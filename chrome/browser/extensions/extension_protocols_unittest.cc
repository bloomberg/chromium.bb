// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/file_util.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_info_map.h"
#include "chrome/browser/extensions/extension_protocols.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/test/mock_resource_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/common/constants.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "net/url_request/url_request_status.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

scoped_refptr<Extension> CreateTestExtension(const std::string& name,
                                             bool incognito_split_mode) {
  DictionaryValue manifest;
  manifest.SetString("name", name);
  manifest.SetString("version", "1");
  manifest.SetString("incognito", incognito_split_mode ? "split" : "spanning");

  base::FilePath path;
  EXPECT_TRUE(file_util::GetCurrentDirectory(&path));

  std::string error;
  scoped_refptr<Extension> extension(
      Extension::Create(path, Manifest::INTERNAL, manifest,
                        Extension::NO_FLAGS, &error));
  EXPECT_TRUE(extension.get()) << error;
  return extension;
}

scoped_refptr<Extension> CreateWebStoreExtension() {
  DictionaryValue manifest;
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
  DictionaryValue manifest;
  manifest.SetString("name", "An extension with web-accessible resources");
  manifest.SetString("version", "2");

  base::ListValue* web_accessible_list = new base::ListValue();
  web_accessible_list->AppendString("test.dat");
  manifest.Set("web_accessible_resources", web_accessible_list);

  base::FilePath path;
  EXPECT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &path));
  path = path.AppendASCII("extensions").AppendASCII("response_headers");

  std::string error;
  scoped_refptr<Extension> extension(
      Extension::Create(path, Manifest::UNPACKED, manifest,
                        Extension::NO_FLAGS, &error));
  EXPECT_TRUE(extension.get()) << error;
  return extension;
}

class ExtensionProtocolTest : public testing::Test {
 public:
  ExtensionProtocolTest()
    : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP),
      resource_context_(&test_url_request_context_) {}

  virtual void SetUp() OVERRIDE {
    testing::Test::SetUp();
    extension_info_map_ = new ExtensionInfoMap();
    net::URLRequestContext* request_context =
        resource_context_.GetRequestContext();
    old_factory_ = request_context->job_factory();
  }

  virtual void TearDown() {
    net::URLRequestContext* request_context =
        resource_context_.GetRequestContext();
    request_context->set_job_factory(old_factory_);
  }

  void SetProtocolHandler(bool incognito) {
    net::URLRequestContext* request_context =
        resource_context_.GetRequestContext();
    job_factory_.SetProtocolHandler(
        kExtensionScheme,
        CreateExtensionProtocolHandler(incognito, extension_info_map_.get()));
    request_context->set_job_factory(&job_factory_);
  }

  void StartRequest(net::URLRequest* request,
                    ResourceType::Type resource_type) {
    content::ResourceRequestInfo::AllocateForTesting(request,
                                                     resource_type,
                                                     &resource_context_,
                                                     -1,
                                                     -1,
                                                     false);
    request->Start();
    base::MessageLoop::current()->Run();
  }

 protected:
  content::TestBrowserThreadBundle thread_bundle_;
  scoped_refptr<ExtensionInfoMap> extension_info_map_;
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
    {"spanning enabled", false, true, false, true},
    {"split enabled", true, true, true, true},
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); ++i) {
    scoped_refptr<Extension> extension =
        CreateTestExtension(cases[i].name, cases[i].incognito_split_mode);
    extension_info_map_->AddExtension(
        extension.get(), base::Time::Now(), cases[i].incognito_enabled);

    // First test a main frame request.
    {
      // It doesn't matter that the resource doesn't exist. If the resource
      // is blocked, we should see ADDRESS_UNREACHABLE. Otherwise, the request
      // should just fail because the file doesn't exist.
      net::URLRequest request(extension->GetResourceURL("404.html"),
                              &test_delegate_,
                              resource_context_.GetRequestContext());
      StartRequest(&request, ResourceType::MAIN_FRAME);
      EXPECT_EQ(net::URLRequestStatus::FAILED, request.status().status());

      if (cases[i].should_allow_main_frame_load) {
        EXPECT_EQ(net::ERR_FILE_NOT_FOUND, request.status().error()) <<
            cases[i].name;
      } else {
        EXPECT_EQ(net::ERR_ADDRESS_UNREACHABLE, request.status().error()) <<
            cases[i].name;
      }
    }

    // Now do a subframe request.
    {
      net::URLRequest request(extension->GetResourceURL("404.html"),
                              &test_delegate_,
                              resource_context_.GetRequestContext());
      StartRequest(&request, ResourceType::SUB_FRAME);
      EXPECT_EQ(net::URLRequestStatus::FAILED, request.status().status());

      if (cases[i].should_allow_sub_frame_load) {
        EXPECT_EQ(net::ERR_FILE_NOT_FOUND, request.status().error()) <<
            cases[i].name;
      } else {
        EXPECT_EQ(net::ERR_ADDRESS_UNREACHABLE, request.status().error()) <<
            cases[i].name;
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
  extension_info_map_->AddExtension(extension.get(), base::Time::Now(), false);

  // First test it with the extension enabled.
  {
    net::URLRequest request(extension->GetResourceURL("webstore_icon_16.png"),
                            &test_delegate_,
                            resource_context_.GetRequestContext());
    StartRequest(&request, ResourceType::MEDIA);
    EXPECT_EQ(net::URLRequestStatus::SUCCESS, request.status().status());
    CheckForContentLengthHeader(&request);
  }

  // And then test it with the extension disabled.
  extension_info_map_->RemoveExtension(extension->id(),
                                       UnloadedExtensionInfo::REASON_DISABLE);
  {
    net::URLRequest request(extension->GetResourceURL("webstore_icon_16.png"),
                            &test_delegate_,
                            resource_context_.GetRequestContext());
    StartRequest(&request, ResourceType::MEDIA);
    EXPECT_EQ(net::URLRequestStatus::SUCCESS, request.status().status());
    CheckForContentLengthHeader(&request);
  }
}

// Tests that a URL request for resource from an extension returns a few
// expected response headers.
TEST_F(ExtensionProtocolTest, ResourceRequestResponseHeaders) {
  // Register a non-incognito extension protocol handler.
  SetProtocolHandler(false);

  scoped_refptr<Extension> extension = CreateTestResponseHeaderExtension();
  extension_info_map_->AddExtension(extension.get(), base::Time::Now(), false);

  {
    net::URLRequest request(extension->GetResourceURL("test.dat"),
                            &test_delegate_,
                            resource_context_.GetRequestContext());
    StartRequest(&request, ResourceType::MEDIA);
    EXPECT_EQ(net::URLRequestStatus::SUCCESS, request.status().status());

    // Check that cache-related headers are set.
    std::string etag;
    request.GetResponseHeaderByName("ETag", &etag);
    EXPECT_TRUE(StartsWithASCII(etag, "\"", false));
    EXPECT_TRUE(EndsWith(etag, "\"", false));

    std::string revalidation_header;
    request.GetResponseHeaderByName("cache-control", &revalidation_header);
    EXPECT_EQ("no-cache", revalidation_header);

    // We set test.dat as web-accessible, so it should have a CORS header.
    std::string access_control;
    request.GetResponseHeaderByName("Access-Control-Allow-Origin",
                                    &access_control);
    EXPECT_EQ("*", access_control);
  }
}

}  // namespace extensions
