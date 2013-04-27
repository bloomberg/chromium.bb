// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/web_request/web_request_permissions.h"

#include "base/message_loop.h"
#include "chrome/browser/extensions/extension_info_map.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/extension_test_util.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/test/test_browser_thread.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::ResourceRequestInfo;
using extensions::Extension;
using extensions::Manifest;
using extension_test_util::LoadManifestUnchecked;

class ExtensionWebRequestHelpersTestWithThreadsTest : public testing::Test {
 public:
  ExtensionWebRequestHelpersTestWithThreadsTest()
      : io_thread_(content::BrowserThread::IO, &message_loop_) {}

 protected:
  virtual void SetUp() OVERRIDE;

 protected:
  net::TestURLRequestContext context;

  // This extension has Web Request permissions, but no host permission.
  scoped_refptr<Extension> permissionless_extension_;
  // This extension has Web Request permissions, and *.com a host permission.
  scoped_refptr<Extension> com_extension_;
  scoped_refptr<ExtensionInfoMap> extension_info_map_;

 private:
  MessageLoopForIO message_loop_;
  content::TestBrowserThread io_thread_;
};

void ExtensionWebRequestHelpersTestWithThreadsTest::SetUp() {
  testing::Test::SetUp();

  std::string error;
  permissionless_extension_ = LoadManifestUnchecked("permissions",
                                                    "web_request_no_host.json",
                                                    Manifest::INVALID_LOCATION,
                                                    Extension::NO_FLAGS,
                                                    "ext_id_1",
                                                    &error);
  ASSERT_TRUE(permissionless_extension_) << error;
  com_extension_ =
      LoadManifestUnchecked("permissions",
                            "web_request_com_host_permissions.json",
                            Manifest::INVALID_LOCATION,
                            Extension::NO_FLAGS,
                            "ext_id_2",
                            &error);
  ASSERT_TRUE(com_extension_) << error;
  extension_info_map_ = new ExtensionInfoMap;
  extension_info_map_->AddExtension(permissionless_extension_.get(),
                                    base::Time::Now(),
                                    false /*incognito_enabled*/);
  extension_info_map_->AddExtension(
      com_extension_.get(), base::Time::Now(), false /*incognito_enabled*/);
}

TEST(ExtensionWebRequestHelpersTest, TestHideRequestForURL) {
  MessageLoopForIO message_loop;
  net::TestURLRequestContext context;
  scoped_refptr<ExtensionInfoMap> extension_info_map(new ExtensionInfoMap);
  const char* sensitive_urls[] = {
      "http://clients2.google.com",
      "http://clients22.google.com",
      "https://clients2.google.com",
      "http://clients2.google.com/service/update2/crx",
      "https://clients.google.com",
      "https://test.clients.google.com",
      "https://clients2.google.com/service/update2/crx",
      "http://www.gstatic.com/chrome/extensions/blacklist",
      "https://www.gstatic.com/chrome/extensions/blacklist",
      "notregisteredscheme://www.foobar.com"
  };
  const char* non_sensitive_urls[] = {
      "http://www.google.com/"
  };
  // Check that requests are rejected based on the destination
  for (size_t i = 0; i < arraysize(sensitive_urls); ++i) {
    GURL sensitive_url(sensitive_urls[i]);
    net::TestURLRequest request(sensitive_url, NULL, &context, NULL);
    EXPECT_TRUE(
        WebRequestPermissions::HideRequest(extension_info_map.get(), &request))
        << sensitive_urls[i];
  }
  // Check that requests are accepted if they don't touch sensitive urls.
  for (size_t i = 0; i < arraysize(non_sensitive_urls); ++i) {
    GURL non_sensitive_url(non_sensitive_urls[i]);
    net::TestURLRequest request(non_sensitive_url, NULL, &context, NULL);
    EXPECT_FALSE(
        WebRequestPermissions::HideRequest(extension_info_map.get(), &request))
        << non_sensitive_urls[i];
  }

  // Check protection of requests originating from the frame showing the Chrome
  // WebStore.
  // Normally this request is not protected:
  GURL non_sensitive_url("http://www.google.com/test.js");
  net::TestURLRequest non_sensitive_request(
      non_sensitive_url, NULL, &context, NULL);
  EXPECT_FALSE(WebRequestPermissions::HideRequest(extension_info_map.get(),
                                                  &non_sensitive_request));
  // If the origin is labeled by the WebStoreAppId, it becomes protected.
  int process_id = 42;
  int site_instance_id = 23;
  int frame_id = 17;
  net::TestURLRequest sensitive_request(
      non_sensitive_url, NULL, &context, NULL);
  ResourceRequestInfo::AllocateForTesting(&sensitive_request,
      ResourceType::SCRIPT, NULL, process_id, frame_id);
  extension_info_map->RegisterExtensionProcess(extension_misc::kWebStoreAppId,
      process_id, site_instance_id);
  EXPECT_TRUE(WebRequestPermissions::HideRequest(extension_info_map.get(),
                                                 &sensitive_request));
}

TEST_F(ExtensionWebRequestHelpersTestWithThreadsTest,
       TestCanExtensionAccessURL_HostPermissions) {
  net::TestURLRequest request(
      GURL("http://example.com"), NULL, &context, NULL);

  EXPECT_TRUE(WebRequestPermissions::CanExtensionAccessURL(
      extension_info_map_,
      permissionless_extension_->id(),
      request.url(),
      false /*crosses_incognito*/,
      WebRequestPermissions::DO_NOT_CHECK_HOST));
  EXPECT_FALSE(WebRequestPermissions::CanExtensionAccessURL(
      extension_info_map_,
      permissionless_extension_->id(),
      request.url(),
      false /*crosses_incognito*/,
      WebRequestPermissions::REQUIRE_HOST_PERMISSION));
  EXPECT_TRUE(WebRequestPermissions::CanExtensionAccessURL(
      extension_info_map_,
      com_extension_->id(),
      request.url(),
      false /*crosses_incognito*/,
      WebRequestPermissions::REQUIRE_HOST_PERMISSION));
  EXPECT_FALSE(WebRequestPermissions::CanExtensionAccessURL(
      extension_info_map_,
      com_extension_->id(),
      request.url(),
      false /*crosses_incognito*/,
      WebRequestPermissions::REQUIRE_ALL_URLS));
}
