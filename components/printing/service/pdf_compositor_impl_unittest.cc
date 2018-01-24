// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/callback.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "components/printing/service/pdf_compositor_impl.h"
#include "components/printing/service/public/cpp/pdf_service_mojo_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace printing {

class PdfCompositorImplTest : public testing::Test {
 public:
  PdfCompositorImplTest()
      : task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::IO),
        run_loop_(std::make_unique<base::RunLoop>()),
        is_ready_(false) {}

  void OnIsReadyToCompositeCallback(bool is_ready) {
    is_ready_ = is_ready;
    run_loop_->Quit();
  }

  bool ResultFromCallback() {
    run_loop_->Run();
    run_loop_ = std::make_unique<base::RunLoop>();
    return is_ready_;
  }

 protected:
  base::test::ScopedTaskEnvironment task_environment_;
  std::unique_ptr<base::RunLoop> run_loop_;
  bool is_ready_;
};

TEST_F(PdfCompositorImplTest, IsReadyToComposite) {
  PdfCompositorImpl impl("unittest", nullptr);
  // Frame 2 and 3 are painted.
  impl.AddSubframeContent(2u, mojo::SharedBufferHandle::Create(10),
                          ContentToFrameMap());
  impl.AddSubframeContent(3u, mojo::SharedBufferHandle::Create(10),
                          ContentToFrameMap());

  // Frame 1 contains content 3 which corresponds to frame 2.
  // Frame 1 should be ready as frame 2 is ready.
  ContentToFrameMap subframe_content_map;
  subframe_content_map[3u] = 2u;
  base::flat_set<uint64_t> pending_subframes;
  bool is_ready = impl.IsReadyToComposite(1u, std::move(subframe_content_map),
                                          &pending_subframes);
  EXPECT_TRUE(is_ready);
  EXPECT_TRUE(pending_subframes.empty());

  // If another page of frame 1 needs content 2 which corresponds to frame 3.
  // This page is ready since frame 3 was painted also.
  subframe_content_map.clear();
  subframe_content_map[2u] = 3u;
  is_ready = impl.IsReadyToComposite(1u, std::move(subframe_content_map),
                                     &pending_subframes);
  EXPECT_TRUE(is_ready);
  EXPECT_TRUE(pending_subframes.empty());

  // Frame 1 with content 1, 2 and 3 should not be ready since content 1's
  // content in frame 4 is not painted yet.
  subframe_content_map.clear();
  subframe_content_map[1u] = 4u;
  subframe_content_map[2u] = 3u;
  subframe_content_map[3u] = 2u;
  is_ready = impl.IsReadyToComposite(1u, std::move(subframe_content_map),
                                     &pending_subframes);
  EXPECT_FALSE(is_ready);
  ASSERT_EQ(pending_subframes.size(), 1u);
  EXPECT_EQ(*pending_subframes.begin(), 4u);

  // Add content of frame 4. Now it is ready for composition.
  subframe_content_map.clear();
  subframe_content_map[1u] = 4u;
  subframe_content_map[2u] = 3u;
  subframe_content_map[3u] = 2u;
  impl.AddSubframeContent(4u, mojo::SharedBufferHandle::Create(10),
                          ContentToFrameMap());
  is_ready = impl.IsReadyToComposite(1u, std::move(subframe_content_map),
                                     &pending_subframes);
  EXPECT_TRUE(is_ready);
  EXPECT_TRUE(pending_subframes.empty());
}

TEST_F(PdfCompositorImplTest, MultiLayerDependency) {
  PdfCompositorImpl impl("unittest", nullptr);
  // Frame 3 has content 1 which refers to subframe 1.
  ContentToFrameMap subframe_content_map;
  subframe_content_map[1u] = 1u;
  impl.AddSubframeContent(3u, mojo::SharedBufferHandle::Create(10),
                          std::move(subframe_content_map));

  // Frame 5 has content 3 which refers to subframe 3.
  // Although frame 3's content is added, its subframe 1's content is not added.
  // So frame 5 is not ready.
  subframe_content_map.clear();
  subframe_content_map[3u] = 3u;
  base::flat_set<uint64_t> pending_subframes;
  bool is_ready = impl.IsReadyToComposite(5u, std::move(subframe_content_map),
                                          &pending_subframes);
  EXPECT_FALSE(is_ready);
  ASSERT_EQ(pending_subframes.size(), 1u);
  EXPECT_EQ(*pending_subframes.begin(), 1u);

  // Frame 6 is not ready either since it needs frame 5 to be ready.
  subframe_content_map.clear();
  subframe_content_map[1u] = 5u;
  is_ready = impl.IsReadyToComposite(6u, std::move(subframe_content_map),
                                     &pending_subframes);
  EXPECT_FALSE(is_ready);
  ASSERT_EQ(pending_subframes.size(), 1u);
  EXPECT_EQ(*pending_subframes.begin(), 5u);

  // When frame 1's content is added, frame 5 is ready.
  impl.AddSubframeContent(1u, mojo::SharedBufferHandle::Create(10),
                          ContentToFrameMap());
  subframe_content_map.clear();
  subframe_content_map[3u] = 3u;
  is_ready = impl.IsReadyToComposite(5u, std::move(subframe_content_map),
                                     &pending_subframes);
  EXPECT_TRUE(is_ready);
  EXPECT_TRUE(pending_subframes.empty());

  // Add frame 5's content.
  subframe_content_map.clear();
  subframe_content_map[3u] = 3u;
  impl.AddSubframeContent(5u, mojo::SharedBufferHandle::Create(10),
                          std::move(subframe_content_map));

  // Frame 6 is ready too.
  subframe_content_map.clear();
  subframe_content_map[1u] = 5u;
  is_ready = impl.IsReadyToComposite(6u, std::move(subframe_content_map),
                                     &pending_subframes);
  EXPECT_TRUE(is_ready);
  EXPECT_TRUE(pending_subframes.empty());
}

TEST_F(PdfCompositorImplTest, DependencyLoop) {
  PdfCompositorImpl impl("unittest", nullptr);
  // Frame 3 has content 1, which refers to frame 1.
  // Frame 1 has content 3, which refers to frame 3.
  ContentToFrameMap subframe_content_map;
  subframe_content_map[3u] = 3u;
  impl.AddSubframeContent(1u, mojo::SharedBufferHandle::Create(10),
                          std::move(subframe_content_map));

  subframe_content_map.clear();
  subframe_content_map[1u] = 1u;
  impl.AddSubframeContent(3u, mojo::SharedBufferHandle::Create(10),
                          std::move(subframe_content_map));

  // Both frame 1 and 3 are painted, frame 5 should be ready.
  base::flat_set<uint64_t> pending_subframes;
  subframe_content_map.clear();
  subframe_content_map[1u] = 3u;
  bool is_ready = impl.IsReadyToComposite(5u, std::move(subframe_content_map),
                                          &pending_subframes);
  EXPECT_TRUE(is_ready);
  EXPECT_TRUE(pending_subframes.empty());

  // Frame 6 has content 7, which refers to frame 7.
  subframe_content_map.clear();
  subframe_content_map[7u] = 7u;
  impl.AddSubframeContent(6, mojo::SharedBufferHandle::Create(10),
                          std::move(subframe_content_map));
  // Frame 7 should be ready since frame 6's own content is added and it only
  // depends on frame 7.
  subframe_content_map.clear();
  subframe_content_map[6u] = 6u;
  is_ready = impl.IsReadyToComposite(7u, std::move(subframe_content_map),
                                     &pending_subframes);
  EXPECT_TRUE(is_ready);
  EXPECT_TRUE(pending_subframes.empty());
}

}  // namespace printing
