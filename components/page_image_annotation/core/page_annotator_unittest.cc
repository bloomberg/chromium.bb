// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_image_annotation/core/page_annotator.h"

#include "base/test/scoped_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace page_image_annotation {

using testing::Eq;

// Tests that the right messages are sent to observers.
TEST(PageAnnotatorTest, Observers) {
  class TestObserver : public PageAnnotator::Observer {
   public:
    TestObserver() : last_added_(0), last_modified_(0), last_removed_(0) {}

    void OnImageAdded(const PageAnnotator::ImageMetadata& metadata) override {
      last_added_ = metadata.node_id;
    }

    void OnImageModified(
        const PageAnnotator::ImageMetadata& metadata) override {
      last_modified_ = metadata.node_id;
    }

    void OnImageRemoved(const uint64_t node_id) override {
      last_removed_ = node_id;
    }

    uint64_t last_added_, last_modified_, last_removed_;
  };

  base::test::ScopedTaskEnvironment test_task_env;

  const auto get_pixels = base::BindRepeating([]() { return SkBitmap(); });

  PageAnnotator page_annotator;

  TestObserver o1;
  page_annotator.AddObserver(&o1);

  page_annotator.ImageAddedOrPossiblyModified({1ul, "test.jpg"}, get_pixels);
  EXPECT_THAT(o1.last_added_, Eq(1ul));

  page_annotator.ImageAddedOrPossiblyModified({2ul, "example.png"}, get_pixels);
  EXPECT_THAT(o1.last_added_, Eq(2ul));

  page_annotator.ImageAddedOrPossiblyModified({1ul, "demo.gif"}, get_pixels);
  EXPECT_THAT(o1.last_added_, Eq(2ul));
  EXPECT_THAT(o1.last_modified_, Eq(1ul));

  page_annotator.ImageRemoved(2ul);
  EXPECT_THAT(o1.last_added_, Eq(2ul));
  EXPECT_THAT(o1.last_modified_, Eq(1ul));
  EXPECT_THAT(o1.last_removed_, Eq(2ul));

  TestObserver o2;
  page_annotator.AddObserver(&o2);

  EXPECT_THAT(o1.last_added_, Eq(2ul));
  EXPECT_THAT(o2.last_added_, Eq(1ul));
}

// TODO(crbug.com/916363): add more tests when behavior is added to the
//                         PageAnnotator class.

}  // namespace page_image_annotation
