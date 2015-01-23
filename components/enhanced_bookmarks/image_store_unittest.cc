// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enhanced_bookmarks/image_store.h"

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_number_conversions.h"
#include "components/enhanced_bookmarks/image_record.h"
#include "components/enhanced_bookmarks/image_store_util.h"
#include "components/enhanced_bookmarks/persistent_image_store.h"
#include "components/enhanced_bookmarks/test_image_store.h"
#include "sql/statement.h"
#include "testing/platform_test.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
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

bool CreateV1PersistentImageStoreDB(const base::FilePath& path) {
  sql::Connection db;
  if (!db.Open(path))
    return false;

  if (db.DoesTableExist("images_by_url"))
    return false;

  const char kV1TableSql[] =
      "CREATE TABLE IF NOT EXISTS images_by_url ("
      "page_url LONGVARCHAR NOT NULL,"
      "image_url LONGVARCHAR NOT NULL,"
      "image_data BLOB,"
      "width INTEGER,"
      "height INTEGER"
      ")";
  if (!db.Execute(kV1TableSql))
    return false;

  const char kV1IndexSql[] =
      "CREATE INDEX IF NOT EXISTS images_by_url_idx ON images_by_url(page_url)";
  if (!db.Execute(kV1IndexSql))
    return false;

  sql::Statement statement(db.GetUniqueStatement(
      "INSERT INTO images_by_url "
      "(page_url, image_url, image_data, width, height) "
      "VALUES (?, ?, ?, ?, ?)"));
  statement.BindString(0, "foo://bar");
  statement.BindString(1, "http://a.jpg");
  scoped_refptr<base::RefCountedMemory> image_bytes =
      enhanced_bookmarks::BytesForImage(GenerateWhiteImage());
  statement.BindBlob(2, image_bytes->front(), (int)image_bytes->size());
  statement.BindInt(3, 42);
  statement.BindInt(4, 24);

  return statement.Run();
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

  virtual void SetUp() override {
    bool success = tempDir_.CreateUniqueTempDir();
    ASSERT_TRUE(success);
    store_.reset(CreateStore<T>(tempDir_));
  }

  virtual void TearDown() override {
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
  const enhanced_bookmarks::ImageRecord image(
      GenerateBlackImage(), GURL("http://a.jpg"), SK_ColorBLACK);
  this->store_->Insert(GURL("foo://bar"), image);

  std::set<GURL> all_urls;
  this->store_->GetAllPageUrls(&all_urls);
  EXPECT_EQ(1u, all_urls.size());
  EXPECT_EQ(GURL("foo://bar"), *all_urls.begin());
  EXPECT_TRUE(this->store_->HasKey(GURL("foo://bar")));
}

TYPED_TEST(ImageStoreUnitTest, Retrieve) {
  const GURL url("foo://bar");
  const enhanced_bookmarks::ImageRecord image_in(
      CreateImage(42, 24, 1, 0, 0, 1), GURL("http://a.jpg"), SK_ColorBLUE);
  this->store_->Insert(url, image_in);

  const enhanced_bookmarks::ImageRecord image_out = this->store_->Get(url);
  const gfx::Size size = this->store_->GetSize(url);

  EXPECT_EQ(42, size.width());
  EXPECT_EQ(24, size.height());
  EXPECT_EQ(image_in.url, image_out.url);
  EXPECT_TRUE(CompareImages(image_in.image, image_out.image));
  EXPECT_EQ(SK_ColorBLUE, image_out.dominant_color);
}

TYPED_TEST(ImageStoreUnitTest, Erase) {
  const GURL url("foo://bar");
  const enhanced_bookmarks::ImageRecord image(
      GenerateBlackImage(), GURL("http://a.jpg"), SK_ColorBLACK);
  this->store_->Insert(url, image);
  this->store_->Erase(url);

  EXPECT_FALSE(this->store_->HasKey(url));
  std::set<GURL> all_urls;
  this->store_->GetAllPageUrls(&all_urls);
  EXPECT_EQ(0u, all_urls.size());
}

TYPED_TEST(ImageStoreUnitTest, ClearAll) {
  const GURL url_foo("http://foo");
  const enhanced_bookmarks::ImageRecord black_image(
      GenerateBlackImage(), GURL("http://a.jpg"), SK_ColorBLACK);
  this->store_->Insert(url_foo, black_image);
  const GURL url_bar("http://bar");
  const enhanced_bookmarks::ImageRecord white_image(
      GenerateWhiteImage(), GURL("http://a.jpg"), SK_ColorWHITE);
  this->store_->Insert(url_bar, white_image);

  this->store_->ClearAll();

  EXPECT_FALSE(this->store_->HasKey(url_foo));
  EXPECT_FALSE(this->store_->HasKey(url_bar));
  std::set<GURL> all_urls;
  this->store_->GetAllPageUrls(&all_urls);
  EXPECT_EQ(0u, all_urls.size());
}

TYPED_TEST(ImageStoreUnitTest, Update) {
  const GURL url("foo://bar");
  const enhanced_bookmarks::ImageRecord image1(GenerateWhiteImage(),
                                               GURL("1.jpg"), SK_ColorWHITE);
  this->store_->Insert(url, image1);

  const enhanced_bookmarks::ImageRecord image2(GenerateBlackImage(),
                                               GURL("2.jpg"), SK_ColorBLACK);
  this->store_->Insert(url, image2);

  const enhanced_bookmarks::ImageRecord image_out = this->store_->Get(url);

  EXPECT_TRUE(this->store_->HasKey(url));
  std::set<GURL> all_urls;
  this->store_->GetAllPageUrls(&all_urls);
  EXPECT_EQ(1u, all_urls.size());
  EXPECT_EQ(image2.url, image_out.url);
  EXPECT_TRUE(CompareImages(image2.image, image_out.image));
  EXPECT_EQ(SK_ColorBLACK, image_out.dominant_color);
}

TYPED_TEST(ImageStoreUnitTest, Persistence) {
  const GURL url("foo://bar");
  const enhanced_bookmarks::ImageRecord image_in(
      GenerateBlackImage(), GURL("http://a.jpg"), SK_ColorBLACK);
  this->store_->Insert(url, image_in);

  this->ResetStore();
  if (this->use_persistent_store()) {
    std::set<GURL> all_urls;
    this->store_->GetAllPageUrls(&all_urls);
    EXPECT_EQ(1u, all_urls.size());
    EXPECT_EQ(url, *all_urls.begin());
    EXPECT_TRUE(this->store_->HasKey(url));
    const enhanced_bookmarks::ImageRecord image_out = this->store_->Get(url);

    EXPECT_EQ(image_in.url, image_out.url);
    EXPECT_TRUE(CompareImages(image_in.image, image_out.image));
    EXPECT_EQ(image_in.dominant_color, image_out.dominant_color);
  } else {
    std::set<GURL> all_urls;
    this->store_->GetAllPageUrls(&all_urls);
    EXPECT_EQ(0u, all_urls.size());
    EXPECT_FALSE(this->store_->HasKey(url));
  }
}

TYPED_TEST(ImageStoreUnitTest, MigrationToV2) {
  // Migration is available only with persistent stores.
  if (!this->use_persistent_store())
    return;

  // Set up v1 DB.
  EXPECT_TRUE(CreateV1PersistentImageStoreDB(this->tempDir_.path().Append(
      base::FilePath::FromUTF8Unsafe("BookmarkImageAndUrlStore.db"))));

  const enhanced_bookmarks::ImageRecord image_out =
      this->store_->Get(GURL("foo://bar"));
  EXPECT_EQ(SK_ColorWHITE, image_out.dominant_color);
}

TYPED_TEST(ImageStoreUnitTest, GetSize) {
  const GURL url("foo://bar");
  const enhanced_bookmarks::ImageRecord image_in(
      GenerateBlackImage(), GURL("http://a.jpg"), SK_ColorBLACK);

  int64 size = 0;
  if (this->use_persistent_store()) {
    // File shouldn't exist before we actually start using it since we do lazy
    // initialization.
    EXPECT_EQ(this->store_->GetStoreSizeInBytes(), -1);
  } else {
    EXPECT_LE(this->store_->GetStoreSizeInBytes(), 1024);
  }
  for (int i = 0; i < 100; ++i) {
    this->store_->Insert(GURL(url.spec() + '/' + base::IntToString(i)),
                         image_in);
    EXPECT_GE(this->store_->GetStoreSizeInBytes(), size);
    size = this->store_->GetStoreSizeInBytes();
  }

  if (this->use_persistent_store()) {
    EXPECT_GE(this->store_->GetStoreSizeInBytes(),  80 * 1024); //  80kb
    EXPECT_LE(this->store_->GetStoreSizeInBytes(), 200 * 1024); // 200kb
  } else {
    EXPECT_GE(this->store_->GetStoreSizeInBytes(), 400 * 1024); // 400kb
    EXPECT_LE(this->store_->GetStoreSizeInBytes(), 500 * 1024); // 500kb
  }
}

}  // namespace
