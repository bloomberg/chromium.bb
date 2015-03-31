// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_number_conversions.h"
#include "chrome/common/extensions/manifest_tests/chrome_manifest_test.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/webview_info.h"

using extensions::ErrorUtils;
using extensions::Extension;
using extensions::WebviewInfo;
namespace errors = extensions::manifest_errors;

class WebviewAccessibleResourcesManifestTest : public ChromeManifestTest {
};

TEST_F(WebviewAccessibleResourcesManifestTest, WebviewAccessibleResources) {
  // Manifest version 2 with webview accessible resources specified.
  scoped_refptr<Extension> extension1(
      LoadAndExpectSuccess("webview_accessible_resources_1.json"));
  scoped_refptr<Extension> extension2(
      LoadAndExpectSuccess("webview_accessible_resources_2.json"));
  DCHECK(extension1->id() != extension2->id());
  const WebviewInfo* webview_info1 =
      static_cast<const WebviewInfo*>(extension1->GetManifestData(
          extensions::manifest_keys::kWebviewAccessibleResources));
  const WebviewInfo* webview_info2 =
      static_cast<const WebviewInfo*>(extension2->GetManifestData(
          extensions::manifest_keys::kWebviewAccessibleResources));

  EXPECT_FALSE(webview_info1->IsResourceWebviewAccessible(extension1.get(),
                                                          "fail", "a.html"));
  EXPECT_FALSE(webview_info1->IsResourceWebviewAccessible(extension1.get(),
                                                          "fail", "b.html"));
  EXPECT_FALSE(webview_info1->IsResourceWebviewAccessible(extension1.get(),
                                                          "fail", "c.html"));
  EXPECT_FALSE(webview_info1->IsResourceWebviewAccessible(extension1.get(),
                                                          "fail", "d.html"));

  EXPECT_TRUE(webview_info1->IsResourceWebviewAccessible(extension1.get(),
                                                         "foo", "a.html"));
  EXPECT_TRUE(webview_info1->IsResourceWebviewAccessible(extension1.get(),
                                                         "foo", "b.html"));
  EXPECT_FALSE(webview_info1->IsResourceWebviewAccessible(extension1.get(),
                                                          "foo", "c.html"));
  EXPECT_FALSE(webview_info1->IsResourceWebviewAccessible(extension1.get(),
                                                          "foo", "d.html"));

  EXPECT_TRUE(webview_info1->IsResourceWebviewAccessible(extension1.get(),
                                                         "bar", "a.html"));
  EXPECT_FALSE(webview_info1->IsResourceWebviewAccessible(extension1.get(),
                                                          "bar", "b.html"));
  EXPECT_TRUE(webview_info1->IsResourceWebviewAccessible(extension1.get(),
                                                         "bar", "c.html"));
  EXPECT_FALSE(webview_info1->IsResourceWebviewAccessible(extension1.get(),
                                                          "bar", "d.html"));

  EXPECT_TRUE(webview_info1->IsResourceWebviewAccessible(extension1.get(),
                                                         "foobar", "a.html"));
  EXPECT_TRUE(webview_info1->IsResourceWebviewAccessible(extension1.get(),
                                                         "foobar", "b.html"));
  EXPECT_TRUE(webview_info1->IsResourceWebviewAccessible(extension1.get(),
                                                         "foobar", "c.html"));
  EXPECT_FALSE(webview_info1->IsResourceWebviewAccessible(extension1.get(),
                                                          "foobar", "d.html"));

  // Check that if the owner extension doesn't match the request extension
  // the resource will not be accessible.
  EXPECT_FALSE(webview_info2->IsResourceWebviewAccessible(extension1.get(),
                                                          "foobar", "a.html"));
  EXPECT_FALSE(webview_info1->IsResourceWebviewAccessible(extension2.get(),
                                                          "foobar", "a.html"));
  EXPECT_TRUE(webview_info2->IsResourceWebviewAccessible(extension2.get(),
                                                         "foobar", "a.html"));
  EXPECT_FALSE(
      webview_info1->IsResourceWebviewAccessible(nullptr, "foobar", "a.html"));
  EXPECT_FALSE(
      webview_info2->IsResourceWebviewAccessible(nullptr, "foobar", "a.html"));
}

TEST_F(WebviewAccessibleResourcesManifestTest, InvalidManifest) {
  LoadAndExpectError("webview_accessible_resources_invalid1.json",
                      errors::kInvalidWebview);
  LoadAndExpectError("webview_accessible_resources_invalid2.json",
                      errors::kInvalidWebviewPartitionsList);
  LoadAndExpectError("webview_accessible_resources_invalid3.json",
                      errors::kInvalidWebviewPartitionsList);
  LoadAndExpectError("webview_accessible_resources_invalid4.json",
      ErrorUtils::FormatErrorMessage(
          errors::kInvalidWebviewPartition, base::IntToString(0)));
  LoadAndExpectError("webview_accessible_resources_invalid5.json",
                     errors::kInvalidWebviewPartitionName);
  LoadAndExpectError("webview_accessible_resources_invalid6.json",
                     errors::kInvalidWebviewAccessibleResourcesList);
  LoadAndExpectError("webview_accessible_resources_invalid7.json",
                     errors::kInvalidWebviewAccessibleResourcesList);
  LoadAndExpectError("webview_accessible_resources_invalid8.json",
      ErrorUtils::FormatErrorMessage(
          errors::kInvalidWebviewAccessibleResource, base::IntToString(0)));
}
