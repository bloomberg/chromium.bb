// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "chrome/browser/thumbnails/thumbnailing_context.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace thumbnails {

typedef testing::Test ThumbnailingContextTest;

TEST_F(ThumbnailingContextTest, TestClipResult) {
  scoped_refptr<ThumbnailingContext> context(
      ThumbnailingContext::CreateThumbnailingContextForTest());

  // Set it to something.
  context->set_clip_result(CLIP_RESULT_SOURCE_IS_SMALLER);
  EXPECT_EQ(CLIP_RESULT_SOURCE_IS_SMALLER, context->clip_result());

  // Make sure it's possible to change it.
  context->set_clip_result(CLIP_RESULT_TALLER_THAN_WIDE);
  EXPECT_EQ(CLIP_RESULT_TALLER_THAN_WIDE, context->clip_result());
}

TEST_F(ThumbnailingContextTest, TestRequestedCopySize) {
  scoped_refptr<ThumbnailingContext> context(
      ThumbnailingContext::CreateThumbnailingContextForTest());

  // Set it to a size.
  gfx::Size the_size(25, 50);
  context->set_requested_copy_size(the_size);
  EXPECT_EQ(the_size, context->requested_copy_size());

  // Try changing to a new size.
  the_size.set_height(500);
  context->set_requested_copy_size(the_size);
  EXPECT_EQ(the_size, context->requested_copy_size());
}

TEST_F(ThumbnailingContextTest, TestBoringScore) {
  scoped_refptr<ThumbnailingContext> context(
      ThumbnailingContext::CreateThumbnailingContextForTest());

  // Set it to something.
  double boring_score = 50.0;
  context->SetBoringScore(boring_score);
  EXPECT_EQ(boring_score, context->score().boring_score);

  // Try changing it.
  boring_score = 25.0;
  context->SetBoringScore(boring_score);
  EXPECT_EQ(boring_score, context->score().boring_score);
}

TEST_F(ThumbnailingContextTest, TestGoodClipping) {
  scoped_refptr<ThumbnailingContext> context(
      ThumbnailingContext::CreateThumbnailingContextForTest());

  // Set to true.
  context->SetGoodClipping(true);
  EXPECT_TRUE(context->score().good_clipping);

  // Try changing to false.
  context->SetGoodClipping(false);
  EXPECT_FALSE(context->score().good_clipping);
}

}  // namespace thumbnails
