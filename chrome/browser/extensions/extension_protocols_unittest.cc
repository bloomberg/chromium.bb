// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/file_util.h"
#include "base/message_loop.h"
#include "chrome/browser/extensions/extension_info_map.h"
#include "chrome/browser/extensions/extension_protocols.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/test/mock_resource_context.h"
#include "content/public/test/test_browser_thread.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_status.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;
using extensions::Extension;

namespace {

scoped_refptr<Extension> CreateTestExtension(const std::string& name,
                                             bool incognito_split_mode) {
  DictionaryValue manifest;
  manifest.SetString("name", name);
  manifest.SetString("version", "1");
  manifest.SetString("incognito", incognito_split_mode ? "split" : "spanning");

  FilePath path;
  EXPECT_TRUE(file_util::GetCurrentDirectory(&path));

  std::string error;
  scoped_refptr<Extension> extension(
      Extension::Create(path, Extension::INTERNAL, manifest,
                        Extension::NO_FLAGS, &error));
  EXPECT_TRUE(extension.get()) << error;
  return extension;
}

class ExtensionProtocolTest : public testing::Test {
 public:
  ExtensionProtocolTest()
      : ui_thread_(BrowserThread::UI, &message_loop_),
        file_thread_(BrowserThread::FILE, &message_loop_),
        io_thread_(BrowserThread::IO, &message_loop_) {}

  virtual void SetUp() {
    extension_info_map_ = new ExtensionInfoMap();
    net::URLRequestContext* request_context =
        resource_context_.GetRequestContext();
    old_factory_ = request_context->job_factory();
    // Register an incognito extension protocol handler.
    job_factory_.SetProtocolHandler(
        chrome::kExtensionScheme,
        CreateExtensionProtocolHandler(true, extension_info_map_));
    request_context->set_job_factory(&job_factory_);
  }

  virtual void TearDown() {
    net::URLRequestContext* request_context =
        resource_context_.GetRequestContext();
    request_context->set_job_factory(old_factory_);
  }

  void StartRequest(net::URLRequest* request,
                    ResourceType::Type resource_type) {
    content::ResourceRequestInfo::AllocateForTesting(request,
                                                     resource_type,
                                                     &resource_context_,
                                                     -1,
                                                     -1);
    request->set_context(resource_context_.GetRequestContext());
    request->Start();
    MessageLoop::current()->Run();
  }

 protected:
  MessageLoopForIO message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;
  content::TestBrowserThread io_thread_;
  scoped_refptr<ExtensionInfoMap> extension_info_map_;
  net::URLRequestJobFactory job_factory_;
  const net::URLRequestJobFactory* old_factory_;
  TestDelegate test_delegate_;
  content::MockResourceContext resource_context_;
};

// Tests that making a chrome-extension request in an incognito context is
// only allowed under the right circumstances (if the extension is allowed
// in incognito, and it's either a non-main-frame request or a split-mode
// extension).
TEST_F(ExtensionProtocolTest, IncognitoRequest) {
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
        extension, base::Time::Now(), cases[i].incognito_enabled);

    // First test a main frame request.
    {
      // It doesn't matter that the resource doesn't exist. If the resource
      // is blocked, we should see ADDRESS_UNREACHABLE. Otherwise, the request
      // should just fail because the file doesn't exist.
      net::URLRequest request(extension->GetResourceURL("404.html"),
                              &test_delegate_);
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
                              &test_delegate_);
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

}  // namespace
