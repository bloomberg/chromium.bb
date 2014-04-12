// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/content/dom_distiller_viewer_source.h"

#include "components/dom_distiller/core/url_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace dom_distiller {

const char kTestScheme[] = "myscheme";

class DomDistillerViewerSourceTest : public testing::Test {
 public:
  virtual void SetUp() OVERRIDE {
    source_.reset(new DomDistillerViewerSource(NULL, kTestScheme));
  }

 protected:
  scoped_ptr<DomDistillerViewerSource> source_;
};

TEST_F(DomDistillerViewerSourceTest, TestMimeType) {
  EXPECT_EQ("text/css", source_.get()->GetMimeType(kCssPath));
  EXPECT_EQ("text/html", source_.get()->GetMimeType("anythingelse"));
}

}  // namespace dom_distiller
