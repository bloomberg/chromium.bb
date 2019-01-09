// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_image_annotation/core/page_annotator.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace page_image_annotation {

using testing::Eq;

// Tests that destroying subscriptions successfully prevents notifications.
TEST(PageAnnotatorTest, Subscriptions) {
  class TestObserver : public PageAnnotator::Observer {
   public:
    TestObserver(PageAnnotator* const page_annotator)
        : sub_(page_annotator->AddObserver(this)), last_id_(0ul) {}

    void OnImageAdded(const uint64_t node_id) override { last_id_ = node_id; }
    void OnImageModified(const uint64_t node_id) override {
      last_id_ = node_id;
    }
    void OnImageRemoved(const uint64_t node_id) override { last_id_ = node_id; }

    PageAnnotator::Subscription sub_;
    uint64_t last_id_;
  };

  PageAnnotator page_annotator;
  TestObserver o1(&page_annotator), o2(&page_annotator);

  page_annotator.ImageAdded(1ul, "test.jpg");
  EXPECT_THAT(o1.last_id_, Eq(1ul));
  EXPECT_THAT(o2.last_id_, Eq(1ul));

  page_annotator.ImageAdded(2ul, "example.png");
  EXPECT_THAT(o1.last_id_, Eq(2ul));
  EXPECT_THAT(o2.last_id_, Eq(2ul));

  page_annotator.ImageModified(1ul, "demo.gif");
  EXPECT_THAT(o1.last_id_, Eq(1ul));
  EXPECT_THAT(o2.last_id_, Eq(1ul));

  page_annotator.ImageRemoved(2ul);
  EXPECT_THAT(o1.last_id_, Eq(2ul));
  EXPECT_THAT(o2.last_id_, Eq(2ul));

  o1.sub_.Cancel();
  page_annotator.ImageAdded(3ul, "placeholder.bmp");
  EXPECT_THAT(o1.last_id_, Eq(2ul));
  EXPECT_THAT(o2.last_id_, Eq(3ul));

  o2.sub_.Cancel();
  page_annotator.ImageRemoved(1ul);
  EXPECT_THAT(o1.last_id_, Eq(2ul));
  EXPECT_THAT(o2.last_id_, Eq(3ul));
}

// TODO(crbug.com/916363): add more tests when behavior is added to the
//                         PageAnnotator class.

}  // namespace page_image_annotation
