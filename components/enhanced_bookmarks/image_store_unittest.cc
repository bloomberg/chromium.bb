// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enhanced_bookmarks/image_store.h"

#include "base/files/scoped_temp_dir.h"
#include "components/enhanced_bookmarks/image_store_util.h"
#include "components/enhanced_bookmarks/persistent_image_store.h"
#include "components/enhanced_bookmarks/test_image_store.h"
#include "testing/platform_test.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "url/gurl.h"

namespace {

const SkBitmap CreateBitmap(int width, int height, int a, int r, int g, int b) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height);
  bitmap.eraseARGB(a, r, g, b);
  return bitmap;
}

gfx::Image GenerateWhiteImage() {
  return gfx::Image::CreateFrom1xBitmap(
      CreateBitmap(42, 24, 255, 255, 255, 255));
}

gfx::Image GenerateBlackImage(int width, int height) {
  return gfx::Image::CreateFrom1xBitmap(
      CreateBitmap(width, height, 255, 0, 0, 0));
}

gfx::Image GenerateBlackImage() {
  return GenerateBlackImage(42, 24);
}

// Returns true if the two images are identical.
bool CompareImages(const gfx::Image& image_1, const gfx::Image& image_2) {
  if (image_1.IsEmpty() && image_2.IsEmpty())
    return true;

  if (image_1.IsEmpty() || image_2.IsEmpty())
    return false;

  scoped_refptr<base::RefCountedMemory> image_1_png =
      enhanced_bookmarks::BytesForImage(image_1);
  scoped_refptr<base::RefCountedMemory> image_2_png =
      enhanced_bookmarks::BytesForImage(image_2);

  if (image_1_png->size() != image_2_png->size())
    return false;

  return !memcmp(image_1_png->front(),
                 image_2_png->front(),
                 image_1_png->size());
}

// Factory functions for creating instances of the implementations.
template <class T>
scoped_refptr<ImageStore> CreateStore(base::ScopedTempDir& folder);

template <>
scoped_refptr<ImageStore> CreateStore<TestImageStore>(
    base::ScopedTempDir& folder) {
  return scoped_refptr<ImageStore>(new TestImageStore());
}

template <>
scoped_refptr<ImageStore> CreateStore<PersistentImageStore>(
    base::ScopedTempDir& folder) {
  return scoped_refptr<ImageStore>(new PersistentImageStore(folder.path()));
}

// Methods to check if persistence is on or not.
template <class T> bool ShouldPersist();
template <> bool ShouldPersist<TestImageStore>() { return false; }
template <> bool ShouldPersist<PersistentImageStore>() { return true; }

// Test fixture class template for the abstract API.
template <class T>
class ImageStoreUnitTest : public PlatformTest {
 protected:
  ImageStoreUnitTest() {}
  virtual ~ImageStoreUnitTest() {}

  virtual void SetUp() OVERRIDE {
    bool success = tempDir_.CreateUniqueTempDir();
    ASSERT_TRUE(success);
    store_ = CreateStore<T>(tempDir_);
  }

  virtual void TearDown() OVERRIDE {
    if (store_ && use_persistent_store())
      store_->ClearAll();
  }

  bool use_persistent_store() const { return ShouldPersist<T>(); }
  void ResetStore() { store_ = CreateStore<T>(tempDir_); }

  // The directory the database is saved into.
  base::ScopedTempDir tempDir_;
  // The object the fixture is testing, via its base interface.
  scoped_refptr<ImageStore> store_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ImageStoreUnitTest);
};

// The list of implementations of the abstract API that are going to be tested.
typedef testing::Types<TestImageStore,
                       PersistentImageStore> Implementations;

TYPED_TEST_CASE(ImageStoreUnitTest, Implementations);

// All those tests are run on all the implementations.
TYPED_TEST(ImageStoreUnitTest, StartsEmpty) {
  std::set<GURL> all_urls;
  this->store_->GetAllPageUrls(&all_urls);
  EXPECT_EQ(0u, all_urls.size());
}

TYPED_TEST(ImageStoreUnitTest, StoreOne) {
  this->store_->Insert(GURL("foo://bar"), GURL("a.jpg"), GenerateBlackImage());

  std::set<GURL> all_urls;
  this->store_->GetAllPageUrls(&all_urls);
  EXPECT_EQ(1u, all_urls.size());
  EXPECT_EQ(GURL("foo://bar"), *all_urls.begin());
  EXPECT_TRUE(this->store_->HasKey(GURL("foo://bar")));
}

TYPED_TEST(ImageStoreUnitTest, Retrieve) {
  gfx::Image src_image = GenerateBlackImage(42, 24);
  const GURL url("foo://bar");
  const GURL image_url("a.jpg");
  this->store_->Insert(url, image_url, src_image);

  std::pair<gfx::Image, GURL> image_info = this->store_->Get(url);
  gfx::Size size = this->store_->GetSize(url);

  EXPECT_EQ(size.width(), 42);
  EXPECT_EQ(size.height(), 24);
  EXPECT_EQ(image_url, image_info.second);
  EXPECT_TRUE(CompareImages(src_image, image_info.first));
}

TYPED_TEST(ImageStoreUnitTest, Erase) {
  gfx::Image src_image = GenerateBlackImage();
  const GURL url("foo://bar");
  const GURL image_url("a.jpg");
  this->store_->Insert(url, image_url, src_image);
  this->store_->Erase(url);

  EXPECT_FALSE(this->store_->HasKey(url));
  std::set<GURL> all_urls;
  this->store_->GetAllPageUrls(&all_urls);
  EXPECT_EQ(0u, all_urls.size());
}

TYPED_TEST(ImageStoreUnitTest, ClearAll) {
  const GURL url_foo("http://foo");
  this->store_->Insert(url_foo, GURL("foo.jpg"), GenerateBlackImage());
  const GURL url_bar("http://bar");
  this->store_->Insert(url_foo, GURL("bar.jpg"), GenerateWhiteImage());

  this->store_->ClearAll();

  EXPECT_FALSE(this->store_->HasKey(url_foo));
  EXPECT_FALSE(this->store_->HasKey(url_bar));
  std::set<GURL> all_urls;
  this->store_->GetAllPageUrls(&all_urls);
  EXPECT_EQ(0u, all_urls.size());
}

TYPED_TEST(ImageStoreUnitTest, Update) {
  gfx::Image src_image1 = GenerateWhiteImage();
  gfx::Image src_image2 = GenerateBlackImage();
  const GURL url("foo://bar");
  const GURL image_url1("1.jpg");
  this->store_->Insert(url, image_url1, src_image1);

  const GURL image_url2("2.jpg");
  this->store_->Insert(url, image_url2, src_image2);

  std::pair<gfx::Image, GURL> image_info = this->store_->Get(url);

  EXPECT_TRUE(this->store_->HasKey(url));
  std::set<GURL> all_urls;
  this->store_->GetAllPageUrls(&all_urls);
  EXPECT_EQ(1u, all_urls.size());
  EXPECT_EQ(image_url2, image_info.second);
  EXPECT_TRUE(CompareImages(src_image2, image_info.first));
}

TYPED_TEST(ImageStoreUnitTest, Persistence) {
  gfx::Image src_image = GenerateBlackImage();
  const GURL url("foo://bar");
  const GURL image_url("a.jpg");
  this->store_->Insert(url, image_url, src_image);

  this->ResetStore();
  if (this->use_persistent_store()) {
    std::set<GURL> all_urls;
    this->store_->GetAllPageUrls(&all_urls);
    EXPECT_EQ(1u, all_urls.size());
    EXPECT_EQ(GURL("foo://bar"), *all_urls.begin());
    EXPECT_TRUE(this->store_->HasKey(GURL("foo://bar")));
    std::pair<gfx::Image, GURL> image_info = this->store_->Get(url);

    EXPECT_EQ(image_url, image_info.second);
    EXPECT_TRUE(CompareImages(src_image, image_info.first));
  } else {
    std::set<GURL> all_urls;
    this->store_->GetAllPageUrls(&all_urls);
    EXPECT_EQ(0u, all_urls.size());
    EXPECT_FALSE(this->store_->HasKey(GURL("foo://bar")));
  }
}

}  // namespace
