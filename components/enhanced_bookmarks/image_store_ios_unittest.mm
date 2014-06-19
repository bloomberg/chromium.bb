// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enhanced_bookmarks/image_store.h"

#import <UIKit/UIKit.h>

#include "base/files/scoped_temp_dir.h"
#include "base/mac/scoped_cftyperef.h"
#include "components/enhanced_bookmarks/image_store_util.h"
#include "components/enhanced_bookmarks/persistent_image_store.h"
#include "components/enhanced_bookmarks/test_image_store.h"
#include "testing/platform_test.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace {

// Generates a gfx::Image with a random UIImage representation. Uses off-center
// circle gradient to make all pixels slightly different in order to detect
// small image alterations.
gfx::Image GenerateRandomUIImage(gfx::Size& size, float scale) {
  UIGraphicsBeginImageContextWithOptions(CGSizeMake(size.width(),
                                                    size.height()),
                                         YES,  // opaque.
                                         scale);
  // Create the gradient's colors.
  CGFloat locations[] = { 0.0, 1.0 };
  CGFloat components[] = { rand()/CGFloat(RAND_MAX),  // Start color r
                           rand()/CGFloat(RAND_MAX),  // g
                           rand()/CGFloat(RAND_MAX),  // b
                           1.0,                       // Alpha
                           rand()/CGFloat(RAND_MAX),  // End color r
                           rand()/CGFloat(RAND_MAX),  // g
                           rand()/CGFloat(RAND_MAX),  // b
                           1.0 };                     // Alpha
  CGPoint center = CGPointMake(size.width() / 3, size.height() / 3);
  CGFloat radius = MAX(size.width(), size.height());

  base::ScopedCFTypeRef<CGColorSpaceRef>
      colorspace(CGColorSpaceCreateDeviceRGB());
  base::ScopedCFTypeRef<CGGradientRef>
      gradient(CGGradientCreateWithColorComponents(colorspace,
                                                   components,
                                                   locations,
                                                   arraysize(locations)));
  CGContextDrawRadialGradient(UIGraphicsGetCurrentContext(),
                              gradient,
                              center,
                              0,
                              center,
                              radius,
                              kCGGradientDrawsAfterEndLocation);
  UIImage* image = UIGraphicsGetImageFromCurrentImageContext();
  UIGraphicsEndImageContext();
  return gfx::Image([image retain]);
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
class ImageStoreUnitTestIOS : public PlatformTest {
 protected:
  ImageStoreUnitTestIOS() {}
  virtual ~ImageStoreUnitTestIOS() {}

  virtual void SetUp() OVERRIDE {
    bool success = temp_dir_.CreateUniqueTempDir();
    ASSERT_TRUE(success);
    store_.reset(CreateStore<T>(temp_dir_));
  }

  virtual void TearDown() OVERRIDE {
    if (store_ && use_persistent_store())
      store_->ClearAll();
  }

  bool use_persistent_store() const { return ShouldPersist<T>(); }
  void ResetStore() { store_.reset(CreateStore<T>(temp_dir_)); }

  // The directory the database is saved into.
  base::ScopedTempDir temp_dir_;
  // The object the fixture is testing, via its base interface.
  scoped_ptr<ImageStore> store_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ImageStoreUnitTestIOS);
};

// The list of implementations of the abstract API that are going to be tested.
typedef testing::Types<TestImageStore,
                       PersistentImageStore> Implementations;

TYPED_TEST_CASE(ImageStoreUnitTestIOS, Implementations);

TYPED_TEST(ImageStoreUnitTestIOS, StoringImagesPreservesScale) {
  CGFloat scales[] = { 0.0, 1.0, 2.0 };
  gfx::Size image_size(42, 24);
  for (unsigned long i = 0; i < arraysize(scales); i++) {
    gfx::Image src_image(GenerateRandomUIImage(image_size, scales[i]));
    const GURL url("foo://bar");
    const GURL image_url("a.jpg");
    this->store_->Insert(url, image_url, src_image);
    std::pair<gfx::Image, GURL> image_info = this->store_->Get(url);

    EXPECT_EQ(image_url, image_info.second);
    EXPECT_TRUE(CompareImages(src_image, image_info.first));
  }
}

}  // namespace
