// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enhanced_bookmarks/image_store.h"

#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_number_conversions.h"
#include "components/enhanced_bookmarks/image_store_util.h"
#include "components/enhanced_bookmarks/persistent_image_store.h"
#include "components/enhanced_bookmarks/test_image_store.h"
#include "testing/platform_test.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "url/gurl.h"

namespace {

gfx::Image CreateImage(int width, int height, int a, int r, int g, int b) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height);
  bitmap.eraseARGB(a, r, g, b);
  gfx::Image image(gfx::Image::CreateFrom1xBitmap(bitmap));

#if defined(OS_IOS)
  // Make sure the image has a kImageRepCocoaTouch.
  image.ToUIImage();
#endif  // defined(OS_IOS)

  return image;
}

gfx::Image GenerateWhiteImage() {
  return CreateImage(42, 24, 255, 255, 255, 255);
}

gfx::Image GenerateBlackImage(int width, int height) {
  return CreateImage(width, height, 255, 0, 0, 0);
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

  scoped_refptr<base::RefCountedMemory> image_1_bytes =
      enhanced_bookmarks::BytesForImage(image_1);
  scoped_refptr<base::RefCountedMemory> image_2_bytes =
      enhanced_bookmarks::BytesForImage(image_2);

  if (image_1_bytes->size() != image_2_bytes->size())
    return false;

  return !memcmp(image_1_bytes->front(),
                 image_2_bytes->front(),
                 image_1_bytes->size());
}

// Factory functions for creating instances of the implementations.
template <class T>
ImageStore* CreateStore(base::ScopedTempDir& folder);

template <>
ImageStore* CreateStore<TestImageStore>(
    base::ScopedTempDir& folder) {
  return new TestImageStore();
}

template <>
ImageStore* CreateStore<PersistentImageStore>(
    base::ScopedTempDir& folder) {
  return new PersistentImageStore(folder.path());
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
    store_.reset(CreateStore<T>(tempDir_));
  }

  virtual void TearDown() OVERRIDE {
    if (store_ && use_persistent_store())
      store_->ClearAll();
  }

  bool use_persistent_store() const { return ShouldPersist<T>(); }
  void ResetStore() { store_.reset(CreateStore<T>(tempDir_)); }

  // The directory the database is saved into.
  base::ScopedTempDir tempDir_;
  // The object the fixture is testing, via its base interface.
  scoped_ptr<ImageStore> store_;

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

TYPED_TEST(ImageStoreUnitTest, GetSize) {
  gfx::Image src_image = GenerateBlackImage();
  const GURL url("foo://bar");
  const GURL image_url("a.jpg");

  int64 size = 0;
  if (this->use_persistent_store()) {
    // File shouldn't exist before we actually start using it since we do lazy
    // initialization.
    EXPECT_EQ(this->store_->GetStoreSizeInBytes(), -1);
  } else {
    EXPECT_LE(this->store_->GetStoreSizeInBytes(), 1024);
  }
  for (int i = 0; i < 100; ++i) {
    this->store_->Insert(
        GURL(url.spec() + '/' + base::IntToString(i)), image_url, src_image);
    EXPECT_GE(this->store_->GetStoreSizeInBytes(), size);
    size = this->store_->GetStoreSizeInBytes();
  }

  if (this->use_persistent_store()) {
    EXPECT_GE(this->store_->GetStoreSizeInBytes(),  90 * 1024); //  90kb
    EXPECT_LE(this->store_->GetStoreSizeInBytes(), 200 * 1024); // 200kb
  } else {
    EXPECT_GE(this->store_->GetStoreSizeInBytes(), 400 * 1024); // 400kb
    EXPECT_LE(this->store_->GetStoreSizeInBytes(), 500 * 1024); // 500kb
  }
}

}  // namespace
