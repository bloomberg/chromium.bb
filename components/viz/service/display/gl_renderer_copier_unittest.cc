// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/gl_renderer_copier.h"

#include <stdint.h>

#include <iterator>
#include <memory>

#include "cc/test/test_context_provider.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/vector2d.h"

namespace viz {

class GLRendererCopierTest : public testing::Test {
 public:
  void SetUp() override {
    auto context_provider = cc::TestContextProvider::Create();
    context_provider->BindToCurrentThread();
    copier_ = std::make_unique<GLRendererCopier>(std::move(context_provider),
                                                 nullptr);
  }

  void TearDown() override { copier_.reset(); }

  // These simply forward method calls to GLRendererCopier.
  GLuint TakeCachedObjectOrCreate(const base::UnguessableToken& source,
                                  int which) {
    return copier_->TakeCachedObjectOrCreate(source, which);
  }
  void CacheObjectOrDelete(const base::UnguessableToken& source,
                           int which,
                           int name) {
    copier_->CacheObjectOrDelete(source, which, name);
  }
  std::unique_ptr<GLHelper::ScalerInterface> TakeCachedScalerOrCreate(
      const CopyOutputRequest& request) {
    return copier_->TakeCachedScalerOrCreate(request);
  }
  void CacheScalerOrDelete(const base::UnguessableToken& source,
                           std::unique_ptr<GLHelper::ScalerInterface> scaler) {
    copier_->CacheScalerOrDelete(source, std::move(scaler));
  }
  void FreeUnusedCachedResources() { copier_->FreeUnusedCachedResources(); }

  // These inspect the internal state of the GLRendererCopier's cache.
  size_t GetCopierCacheSize() { return copier_->cache_.size(); }
  bool CacheContainsObject(const base::UnguessableToken& source,
                           int which,
                           GLuint name) {
    return !copier_->cache_.empty() && copier_->cache_.count(source) != 0 &&
           copier_->cache_[source].object_names[which] == name;
  }
  bool CacheContainsScaler(const base::UnguessableToken& source,
                           int scale_from,
                           int scale_to) {
    return !copier_->cache_.empty() && copier_->cache_.count(source) != 0 &&
           copier_->cache_[source].scaler &&
           copier_->cache_[source].scaler->IsSameScaleRatio(
               gfx::Vector2d(scale_from, scale_from),
               gfx::Vector2d(scale_to, scale_to));
  }

  static constexpr int kKeepalivePeriod = GLRendererCopier::kKeepalivePeriod;

 private:
  std::unique_ptr<GLRendererCopier> copier_;
};

// Tests that named objects, such as textures or framebuffers, are only cached
// when the CopyOutputRequest has specified a "source" of requests.
TEST_F(GLRendererCopierTest, ReusesNamedObjects) {
  // With no source set in a copy request, expect to never re-use any textures
  // or framebuffers.
  base::UnguessableToken source;
  for (int which = 0; which < 3; ++which) {
    const GLuint a = TakeCachedObjectOrCreate(source, which);
    EXPECT_NE(0u, a);
    CacheObjectOrDelete(base::UnguessableToken(), which, a);
    const GLuint b = TakeCachedObjectOrCreate(source, which);
    EXPECT_NE(0u, b);
    CacheObjectOrDelete(base::UnguessableToken(), which, b);
    EXPECT_EQ(0u, GetCopierCacheSize());
  }

  // With a source set in the request, objects should now be cached and re-used.
  source = base::UnguessableToken::Create();
  for (int which = 0; which < 3; ++which) {
    const GLuint a = TakeCachedObjectOrCreate(source, which);
    EXPECT_NE(0u, a);
    CacheObjectOrDelete(source, which, a);
    const GLuint b = TakeCachedObjectOrCreate(source, which);
    EXPECT_NE(0u, b);
    EXPECT_EQ(a, b);
    CacheObjectOrDelete(source, which, b);
    EXPECT_EQ(1u, GetCopierCacheSize());
    EXPECT_TRUE(CacheContainsObject(source, which, a));
  }
}

// Tests that scalers are only cached when the CopyOutputRequest has specified a
// "source" of requests, and that different scalers are created if the scale
// ratio changes.
TEST_F(GLRendererCopierTest, ReusesScalers) {
  // With no source set in the request, expect to not cache a scaler.
  const auto request = CopyOutputRequest::CreateStubForTesting();
  ASSERT_FALSE(request->has_source());
  request->SetUniformScaleRatio(2, 1);
  std::unique_ptr<GLHelper::ScalerInterface> scaler =
      TakeCachedScalerOrCreate(*request);
  EXPECT_TRUE(scaler.get());
  CacheScalerOrDelete(base::UnguessableToken(), std::move(scaler));
  EXPECT_FALSE(CacheContainsScaler(base::UnguessableToken(), 2, 1));

  // With a source set in the request, a scaler can now be cached and re-used.
  request->set_source(base::UnguessableToken::Create());
  scaler = TakeCachedScalerOrCreate(*request);
  const auto* a = scaler.get();
  EXPECT_TRUE(a);
  CacheScalerOrDelete(request->source(), std::move(scaler));
  EXPECT_TRUE(CacheContainsScaler(request->source(), 2, 1));
  scaler = TakeCachedScalerOrCreate(*request);
  const auto* b = scaler.get();
  EXPECT_TRUE(b);
  EXPECT_EQ(a, b);
  EXPECT_TRUE(b->IsSameScaleRatio(gfx::Vector2d(2, 2), gfx::Vector2d(1, 1)));
  CacheScalerOrDelete(request->source(), std::move(scaler));
  EXPECT_TRUE(CacheContainsScaler(request->source(), 2, 1));

  // With a source set in the request, but a different scaling ratio needed, the
  // cached scaler should go away and a new one created, and only the new one
  // should ever appear in the cache.
  request->SetUniformScaleRatio(3, 2);
  scaler = TakeCachedScalerOrCreate(*request);
  const auto* c = scaler.get();
  EXPECT_TRUE(c);
  EXPECT_TRUE(c->IsSameScaleRatio(gfx::Vector2d(3, 3), gfx::Vector2d(2, 2)));
  EXPECT_FALSE(CacheContainsScaler(request->source(), 2, 1));
  CacheScalerOrDelete(request->source(), std::move(scaler));
  EXPECT_TRUE(CacheContainsScaler(request->source(), 3, 2));
}

// Tests that cached resources are freed if unused for a while.
TEST_F(GLRendererCopierTest, FreesUnusedResources) {
  // Request a texture, then cache it again.
  const base::UnguessableToken source = base::UnguessableToken::Create();
  const int which = 0;
  const GLuint a = TakeCachedObjectOrCreate(source, which);
  EXPECT_NE(0u, a);
  CacheObjectOrDelete(source, which, a);
  EXPECT_TRUE(CacheContainsObject(source, which, a));

  // Call FreesUnusedCachedResources() the maximum number of times before the
  // cache entry would be considered for freeing.
  for (int i = 0; i < kKeepalivePeriod - 1; ++i) {
    FreeUnusedCachedResources();
    EXPECT_TRUE(CacheContainsObject(source, which, a));
    if (HasFailure())
      break;
  }

  // Calling FreeUnusedCachedResources() just one more time should cause the
  // cache entry to be freed.
  FreeUnusedCachedResources();
  EXPECT_FALSE(CacheContainsObject(source, which, a));
  EXPECT_EQ(0u, GetCopierCacheSize());
}

}  // namespace viz
