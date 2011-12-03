// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/md5.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "chrome/renderer/automation/automation_renderer_helper.h"
#include "content/public/renderer/render_view.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebSize.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebWidget.h"

namespace {

const char kThreeBoxesHTML[] =
    "<style> * { margin:0px; padding:0px } </style>"
    "<body>"
    "  <div style='background:red;width:100;height:100'></div>"
    "  <div style='background:green;width:100;height:100'></div>"
    "  <div style='background:blue;width:100;height:100'></div>"
    "</body>";

const char kThreeBoxesMD5[] = "3adefecb4472e6ba1812f9af1fdea3e4";

void CompareSnapshot(const std::vector<unsigned char>& png_data,
                     const std::string& reference_md5) {
  std::string png_data_str(reinterpret_cast<const char*>(&png_data[0]),
                           png_data.size());
  if (CommandLine::ForCurrentProcess()->HasSwitch("dump-test-image")) {
    FilePath path = FilePath().AppendASCII("snapshot" + reference_md5 + ".png");
    EXPECT_EQ(file_util::WriteFile(path, png_data_str.c_str(), png_data.size()),
              static_cast<int>(png_data.size()));
  }
  EXPECT_STREQ(reference_md5.c_str(), base::MD5String(png_data_str).c_str());
}

}  // namespace

typedef ChromeRenderViewTest AutomationRendererHelperTest;

TEST_F(AutomationRendererHelperTest, BasicSnapshot) {
  GetWebWidget()->resize(WebKit::WebSize(100, 300));
  LoadHTML(kThreeBoxesHTML);
  std::vector<unsigned char> png_data;
  std::string error_msg;
  ASSERT_TRUE(AutomationRendererHelper(view_).SnapshotEntirePage(
      view_->GetWebView(), &png_data, &error_msg)) << error_msg;
  CompareSnapshot(png_data, kThreeBoxesMD5);
}

TEST_F(AutomationRendererHelperTest, ScrollingSnapshot) {
  GetWebWidget()->resize(WebKit::WebSize(40, 90));
  LoadHTML(kThreeBoxesHTML);
  std::vector<unsigned char> png_data;
  std::string error_msg;
  ASSERT_TRUE(AutomationRendererHelper(view_).SnapshotEntirePage(
      view_->GetWebView(), &png_data, &error_msg)) << error_msg;
  CompareSnapshot(png_data, kThreeBoxesMD5);
}

TEST_F(AutomationRendererHelperTest, RTLSnapshot) {
  GetWebWidget()->resize(WebKit::WebSize(40, 90));
  const char kThreeBoxesRTLHTML[] =
      "<style> * { margin:0px; padding:0px } </style>"
      "<body dir='rtl'>"
      "  <div style='background:red;width:100;height:100'></div>"
      "  <div style='background:green;width:100;height:100'></div>"
      "  <div style='background:blue;width:100;height:100'></div>"
      "</body>";
  LoadHTML(kThreeBoxesRTLHTML);
  std::vector<unsigned char> png_data;
  std::string error_msg;
  ASSERT_TRUE(AutomationRendererHelper(view_).SnapshotEntirePage(
      view_->GetWebView(), &png_data, &error_msg)) << error_msg;
  CompareSnapshot(png_data, kThreeBoxesMD5);
}
