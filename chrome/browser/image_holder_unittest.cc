// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/image_holder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kIconUrl1[] = "http://www.google.com/icon1.jpg";
const char kIconUrl2[] = "http://www.google.com/icon2.jpg";

class TestDelegate : public chrome::ImageHolderDelegate {
 public:
  TestDelegate() : on_fetch_complete_called_(false) {}
  virtual void OnFetchComplete() OVERRIDE {
    on_fetch_complete_called_ = true;
  }
  bool on_fetch_complete_called_;
};

}  // namespace.

namespace chrome {

typedef testing::Test ImageHolderTest;

TEST_F(ImageHolderTest, CreateBitmapFetcherTest) {
  TestDelegate delegate;
  ImageHolder image_holder(GURL(kIconUrl1), GURL(kIconUrl2), NULL, &delegate);

  EXPECT_EQ(GURL(kIconUrl1), image_holder.fetchers_[0]->url());
  EXPECT_EQ(GURL(kIconUrl2), image_holder.fetchers_[1]->url());
  EXPECT_EQ(static_cast<unsigned int>(2), image_holder.fetchers_.size());

  // Adding a dup of an existing URL shouldn't change anything.
  image_holder.CreateBitmapFetcher(GURL(kIconUrl2));
  EXPECT_EQ(GURL(kIconUrl1), image_holder.fetchers_[0]->url());
  EXPECT_EQ(GURL(kIconUrl2), image_holder.fetchers_[1]->url());
  EXPECT_EQ(static_cast<unsigned int>(2), image_holder.fetchers_.size());
}

TEST_F(ImageHolderTest, OnFetchCompleteTest) {
  TestDelegate delegate;
  ImageHolder image_holder(GURL(kIconUrl1), GURL(), NULL, &delegate);

  // Put a real bitmap into "bitmap".  2x2 bitmap of green 32 bit pixels.
  SkBitmap bitmap;
  bitmap.allocN32Pixels(2, 2);
  bitmap.eraseColor(SK_ColorGREEN);

  image_holder.OnFetchComplete(GURL(kIconUrl1), &bitmap);

  // Expect that the app icon has some data in it.
  EXPECT_FALSE(image_holder.low_dpi_image().IsEmpty());

  // Expect that we reported the fetch done to the delegate.
  EXPECT_TRUE(delegate.on_fetch_complete_called_);
}

TEST_F(ImageHolderTest, IsFetchingDoneTest) {
  TestDelegate delegate;
  ImageHolder image_holder1(GURL(kIconUrl1), GURL(kIconUrl2), NULL, &delegate);
  ImageHolder image_holder2(GURL(kIconUrl1), GURL(), NULL, &delegate);
  ImageHolder image_holder3(GURL(), GURL(kIconUrl2), NULL, &delegate);
  ImageHolder image_holder4(GURL(), GURL(), NULL, &delegate);

  // Initially, image holder 4 with no URLs should report done, but no others.
  EXPECT_FALSE(image_holder1.IsFetchingDone());
  EXPECT_FALSE(image_holder2.IsFetchingDone());
  EXPECT_FALSE(image_holder3.IsFetchingDone());
  EXPECT_TRUE(image_holder4.IsFetchingDone());

  // Put a real bitmap into "bitmap".  2x2 bitmap of green 32 bit pixels.
  SkBitmap bitmap;
  bitmap.allocN32Pixels(2, 2);
  bitmap.eraseColor(SK_ColorGREEN);

  // Add the first icon, and image holder 2 should now also report done.
  image_holder1.OnFetchComplete(GURL(kIconUrl1), &bitmap);
  image_holder2.OnFetchComplete(GURL(kIconUrl1), &bitmap);
  image_holder3.OnFetchComplete(GURL(kIconUrl1), &bitmap);
  image_holder4.OnFetchComplete(GURL(kIconUrl1), &bitmap);
  EXPECT_FALSE(image_holder1.IsFetchingDone());
  EXPECT_TRUE(image_holder2.IsFetchingDone());
  EXPECT_FALSE(image_holder3.IsFetchingDone());
  EXPECT_TRUE(image_holder4.IsFetchingDone());

  // Add the second image, and now all 4 should report done.
  image_holder1.OnFetchComplete(GURL(kIconUrl2), &bitmap);
  image_holder2.OnFetchComplete(GURL(kIconUrl2), &bitmap);
  image_holder3.OnFetchComplete(GURL(kIconUrl2), &bitmap);
  image_holder4.OnFetchComplete(GURL(kIconUrl2), &bitmap);
  EXPECT_TRUE(image_holder1.IsFetchingDone());
  EXPECT_TRUE(image_holder2.IsFetchingDone());
  EXPECT_TRUE(image_holder3.IsFetchingDone());
  EXPECT_TRUE(image_holder4.IsFetchingDone());
}

}  // namespace chrome.
