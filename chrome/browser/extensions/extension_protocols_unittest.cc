// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>
#include <string>

#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/previews_state.h"
#include "content/public/test/mock_resource_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/browser/extension_protocols.h"
#include "extensions/browser/info_map.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/file_util.h"
#include "net/base/request_priority.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "net/url_request/url_request_status.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::ResourceType;

namespace extensions {
namespace {

base::FilePath GetTestPath(const std::string& name) {
  base::FilePath path;
  EXPECT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &path));
  return path.AppendASCII("extensions").AppendASCII(name);
}

scoped_refptr<Extension> CreateTestExtension(const std::string& name,
                                             bool incognito_split_mode) {
  base::DictionaryValue manifest;
  manifest.SetString("name", name);
  manifest.SetString("version", "1");
  manifest.SetInteger("manifest_version", 2);
  manifest.SetString("incognito", incognito_split_mode ? "split" : "spanning");

  base::FilePath path = GetTestPath("response_headers");

  std::string error;
  scoped_refptr<Extension> extension(
      Extension::Create(path, Manifest::INTERNAL, manifest,
                        Extension::NO_FLAGS, &error));
  EXPECT_TRUE(extension.get()) << error;
  return extension;
}

scoped_refptr<Extension> CreateWebStoreExtension() {
  base::DictionaryValue manifest;
  manifest.SetString("name", "WebStore");
  manifest.SetString("version", "1");
  manifest.SetString("icons.16", "webstore_icon_16.png");

  base::FilePath path;
  EXPECT_TRUE(PathService::Get(chrome::DIR_RESOURCES, &path));
  path = path.AppendASCII("web_store");

  std::string error;
  scoped_refptr<Extension> extension(
      Extension::Create(path, Manifest::COMPONENT, manifest,
                        Extension::NO_FLAGS, &error));
  EXPECT_TRUE(extension.get()) << error;
  return extension;
}

scoped_refptr<Extension> CreateTestResponseHeaderExtension() {
  base::DictionaryValue manifest;
  manifest.SetString("name", "An extension with web-accessible resources");
  manifest.SetString("version", "2");

  base::ListValue* web_accessible_list = new base::ListValue();
  web_accessible_list->AppendString("test.dat");
  manifest.Set("web_accessible_resources", web_accessible_list);

  base::FilePath path = GetTestPath("response_headers");

  std::string error;
  scoped_refptr<Extension> extension(
      Extension::Create(path, Manifest::UNPACKED, manifest,
                        Extension::NO_FLAGS, &error));
  EXPECT_TRUE(extension.get()) << error;
  return extension;
}

}  // namespace

// This test lives in src/chrome instead of src/extensions because it tests
// functionality delegated back to Chrome via ChromeExtensionsBrowserClient.
// See chrome/browser/extensions/chrome_url_request_util.cc.
class ExtensionProtocolTest : public testing::Test {
 public:
  ExtensionProtocolTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP),
        old_factory_(NULL),
        resource_context_(&test_url_request_context_) {}

  void SetUp() override {
    testing::Test::SetUp();
    extension_info_map_ = new InfoMap();
    net::URLRequestContext* request_context =
        resource_context_.GetRequestContext();
    old_factory_ = request_context->job_factory();
  }

  void TearDown() override {
    net::URLRequestContext* request_context =
        resource_context_.GetRequestContext();
    request_context->set_job_factory(old_factory_);
  }

  void SetProtocolHandler(bool is_incognito) {
    net::URLRequestContext* request_context =
        resource_context_.GetRequestContext();
    job_factory_.SetProtocolHandler(
        kExtensionScheme,
        CreateExtensionProtocolHandler(is_incognito,
                                       extension_info_map_.get()));
    request_context->set_job_factory(&job_factory_);
  }

  void StartRequest(net::URLRequest* request,
                    ResourceType resource_type) {
    content::ResourceRequestInfo::AllocateForTesting(
        request, resource_type, &resource_context_,
        /*render_process_id=*/-1,
        /*render_view_id=*/-1,
        /*render_frame_id=*/-1,
        /*is_main_frame=*/resource_type == content::RESOURCE_TYPE_MAIN_FRAME,
        /*parent_is_main_frame=*/false,
        /*allow_download=*/true,
        /*is_async=*/false, content::PREVIEWS_OFF);
    request->Start();
    base::RunLoop().Run();
  }

  // Helper method to create a URLRequest, call StartRequest on it, and return
  // the result. If |extension| hasn't already been added to
  // |extension_info_map_|, this will add it.
  int DoRequest(const Extension& extension, const std::string& relative_path) {
    if (!extension_info_map_->extensions().Contains(extension.id())) {
      extension_info_map_->AddExtension(&extension,
                                        base::Time::Now(),
                                        false,   // incognito_enabled
                                        false);  // notifications_disabled
    }
    std::unique_ptr<net::URLRequest> request(
        resource_context_.GetRequestContext()->CreateRequest(
            extension.GetResourceURL(relative_path), net::DEFAULT_PRIORITY,
            &test_delegate_));
    StartRequest(request.get(), content::RESOURCE_TYPE_MAIN_FRAME);
    return test_delegate_.request_status();
  }

 protected:
  content::TestBrowserThreadBundle thread_bundle_;
  scoped_refptr<InfoMap> extension_info_map_;
  net::URLRequestJobFactoryImpl job_factory_;
  const net::URLRequestJobFactory* old_factory_;
  net::TestDelegate test_delegate_;
  net::TestURLRequestContext test_url_request_context_;
  content::MockResourceContext resource_context_;
};

// Tests that making a chrome-extension request in an incognito context is
// only allowed under the right circumstances (if the extension is allowed
// in incognito, and it's either a non-main-frame request or a split-mode
// extension).
TEST_F(ExtensionProtocolTest, IncognitoRequest) {
  // Register an incognito extension protocol handler.
  SetProtocolHandler(true);

  struct TestCase {
    // Inputs.
    std::string name;
    bool incognito_split_mode;
    bool incognito_enabled;

    // Expected results.
    bool should_allow_main_frame_load;
    bool should_allow_sub_frame_load;
  } cases[] = {
    {"spanning disabled", false, false, false, false},
    {"split disabled", true, false, false, false},
    {"spanning enabled", false, true, false, false},
    {"split enabled", true, true, true, false},
  };

  for (size_t i = 0; i < arraysize(cases); ++i) {
    scoped_refptr<Extension> extension =
        CreateTestExtension(cases[i].name, cases[i].incognito_split_mode);
    extension_info_map_->AddExtension(
        extension.get(), base::Time::Now(), cases[i].incognito_enabled, false);

    // First test a main frame request.
    {
      // It doesn't matter that the resource doesn't exist. If the resource
      // is blocked, we should see BLOCKED_BY_CLIENT. Otherwise, the request
      // should just fail because the file doesn't exist.
      std::unique_ptr<net::URLRequest> request(
          resource_context_.GetRequestContext()->CreateRequest(
              extension->GetResourceURL("404.html"), net::DEFAULT_PRIORITY,
              &test_delegate_));
      StartRequest(request.get(), content::RESOURCE_TYPE_MAIN_FRAME);

      if (cases[i].should_allow_main_frame_load) {
        EXPECT_EQ(net::ERR_FILE_NOT_FOUND, test_delegate_.request_status())
            << cases[i].name;
      } else {
        EXPECT_EQ(net::ERR_BLOCKED_BY_CLIENT, test_delegate_.request_status())
            << cases[i].name;
      }
    }

    // Now do a subframe request.
    {
      // With PlzNavigate, the subframe navigation requests are blocked in
      // ExtensionNavigationThrottle which isn't added in this unit test. This
      // is tested in an integration test in
      // ExtensionResourceRequestPolicyTest.IframeNavigateToInaccessible.
      if (!content::IsBrowserSideNavigationEnabled()) {
        std::unique_ptr<net::URLRequest> request(
            resource_context_.GetRequestContext()->CreateRequest(
                extension->GetResourceURL("404.html"), net::DEFAULT_PRIORITY,
                &test_delegate_));
        StartRequest(request.get(), content::RESOURCE_TYPE_SUB_FRAME);

        if (cases[i].should_allow_sub_frame_load) {
          EXPECT_EQ(net::ERR_FILE_NOT_FOUND, test_delegate_.request_status())
              << cases[i].name;
        } else {
          EXPECT_EQ(net::ERR_BLOCKED_BY_CLIENT, test_delegate_.request_status())
              << cases[i].name;
        }
      }
    }
  }
}

void CheckForContentLengthHeader(net::URLRequest* request) {
  std::string content_length;
  request->GetResponseHeaderByName(net::HttpRequestHeaders::kContentLength,
                                  &content_length);
  EXPECT_FALSE(content_length.empty());
  int length_value = 0;
  EXPECT_TRUE(base::StringToInt(content_length, &length_value));
  EXPECT_GT(length_value, 0);
}

// Tests getting a resource for a component extension works correctly, both when
// the extension is enabled and when it is disabled.
TEST_F(ExtensionProtocolTest, ComponentResourceRequest) {
  // Register a non-incognito extension protocol handler.
  SetProtocolHandler(false);

  scoped_refptr<Extension> extension = CreateWebStoreExtension();
  extension_info_map_->AddExtension(extension.get(),
                                    base::Time::Now(),
                                    false,
                                    false);

  // First test it with the extension enabled.
  {
    std::unique_ptr<net::URLRequest> request(
        resource_context_.GetRequestContext()->CreateRequest(
            extension->GetResourceURL("webstore_icon_16.png"),
            net::DEFAULT_PRIORITY, &test_delegate_));
    StartRequest(request.get(), content::RESOURCE_TYPE_MEDIA);
    EXPECT_EQ(net::OK, test_delegate_.request_status());
    CheckForContentLengthHeader(request.get());
  }

  // And then test it with the extension disabled.
  extension_info_map_->RemoveExtension(extension->id(),
                                       UnloadedExtensionInfo::REASON_DISABLE);
  {
    std::unique_ptr<net::URLRequest> request(
        resource_context_.GetRequestContext()->CreateRequest(
            extension->GetResourceURL("webstore_icon_16.png"),
            net::DEFAULT_PRIORITY, &test_delegate_));
    StartRequest(request.get(), content::RESOURCE_TYPE_MEDIA);
    EXPECT_EQ(net::OK, test_delegate_.request_status());
    CheckForContentLengthHeader(request.get());
  }
}

// Tests that a URL request for resource from an extension returns a few
// expected response headers.
TEST_F(ExtensionProtocolTest, ResourceRequestResponseHeaders) {
  // Register a non-incognito extension protocol handler.
  SetProtocolHandler(false);

  scoped_refptr<Extension> extension = CreateTestResponseHeaderExtension();
  extension_info_map_->AddExtension(extension.get(),
                                    base::Time::Now(),
                                    false,
                                    false);

  {
    std::unique_ptr<net::URLRequest> request(
        resource_context_.GetRequestContext()->CreateRequest(
            extension->GetResourceURL("test.dat"), net::DEFAULT_PRIORITY,
            &test_delegate_));
    StartRequest(request.get(), content::RESOURCE_TYPE_MEDIA);
    EXPECT_EQ(net::OK, test_delegate_.request_status());

    // Check that cache-related headers are set.
    std::string etag;
    request->GetResponseHeaderByName("ETag", &etag);
    EXPECT_TRUE(base::StartsWith(etag, "\"", base::CompareCase::SENSITIVE));
    EXPECT_TRUE(base::EndsWith(etag, "\"", base::CompareCase::SENSITIVE));

    std::string revalidation_header;
    request->GetResponseHeaderByName("cache-control", &revalidation_header);
    EXPECT_EQ("no-cache", revalidation_header);

    // We set test.dat as web-accessible, so it should have a CORS header.
    std::string access_control;
    request->GetResponseHeaderByName("Access-Control-Allow-Origin",
                                    &access_control);
    EXPECT_EQ("*", access_control);
  }
}

// Tests that a URL request for main frame or subframe from an extension
// succeeds, but subresources fail. See http://crbug.com/312269.
TEST_F(ExtensionProtocolTest, AllowFrameRequests) {
  // Register a non-incognito extension protocol handler.
  SetProtocolHandler(false);

  scoped_refptr<Extension> extension = CreateTestExtension("foo", false);
  extension_info_map_->AddExtension(extension.get(),
                                    base::Time::Now(),
                                    false,
                                    false);

  // All MAIN_FRAME requests should succeed. SUB_FRAME requests that are not
  // explicitly listed in web_accesible_resources or same-origin to the parent
  // should not succeed.
  {
    std::unique_ptr<net::URLRequest> request(
        resource_context_.GetRequestContext()->CreateRequest(
            extension->GetResourceURL("test.dat"), net::DEFAULT_PRIORITY,
            &test_delegate_));
    StartRequest(request.get(), content::RESOURCE_TYPE_MAIN_FRAME);
    EXPECT_EQ(net::OK, test_delegate_.request_status());
  }
  {
    // With PlzNavigate, the subframe navigation requests are blocked in
    // ExtensionNavigationThrottle which isn't added in this unit test. This is
    // tested in an integration test in
    // ExtensionResourceRequestPolicyTest.IframeNavigateToInaccessible.
    if (!content::IsBrowserSideNavigationEnabled()) {
      std::unique_ptr<net::URLRequest> request(
          resource_context_.GetRequestContext()->CreateRequest(
              extension->GetResourceURL("test.dat"), net::DEFAULT_PRIORITY,
              &test_delegate_));
      StartRequest(request.get(), content::RESOURCE_TYPE_SUB_FRAME);
      EXPECT_EQ(net::ERR_BLOCKED_BY_CLIENT, test_delegate_.request_status());
    }
  }

  // And subresource types, such as media, should fail.
  {
    std::unique_ptr<net::URLRequest> request(
        resource_context_.GetRequestContext()->CreateRequest(
            extension->GetResourceURL("test.dat"), net::DEFAULT_PRIORITY,
            &test_delegate_));
    StartRequest(request.get(), content::RESOURCE_TYPE_MEDIA);
    EXPECT_EQ(net::ERR_BLOCKED_BY_CLIENT, test_delegate_.request_status());
  }
}


TEST_F(ExtensionProtocolTest, MetadataFolder) {
  SetProtocolHandler(false);

  base::FilePath extension_dir = GetTestPath("metadata_folder");
  std::string error;
  scoped_refptr<Extension> extension =
      file_util::LoadExtension(extension_dir, Manifest::INTERNAL,
                               Extension::NO_FLAGS, &error);
  ASSERT_NE(extension.get(), nullptr) << "error: " << error;

  // Loading "/test.html" should succeed.
  EXPECT_EQ(net::OK, DoRequest(*extension, "test.html"));

  // Loading "/_metadata/verified_contents.json" should fail.
  base::FilePath relative_path =
      base::FilePath(kMetadataFolder).Append(kVerifiedContentsFilename);
  EXPECT_TRUE(base::PathExists(extension_dir.Append(relative_path)));
  EXPECT_EQ(net::ERR_FAILED,
            DoRequest(*extension, relative_path.AsUTF8Unsafe()));

  // Loading "/_metadata/a.txt" should also fail.
  relative_path = base::FilePath(kMetadataFolder).AppendASCII("a.txt");
  EXPECT_TRUE(base::PathExists(extension_dir.Append(relative_path)));
  EXPECT_EQ(net::ERR_FAILED,
            DoRequest(*extension, relative_path.AsUTF8Unsafe()));
}

}  // namespace extensions
