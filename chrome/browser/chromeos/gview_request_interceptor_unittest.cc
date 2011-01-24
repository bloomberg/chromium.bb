// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include "base/message_loop.h"
#include "chrome/browser/chromeos/gview_request_interceptor.h"
#include "chrome/browser/plugin_service.h"
#include "chrome/common/chrome_paths.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_test_job.h"
#include "net/url_request/url_request_unittest.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/glue/plugins/plugin_list.h"

namespace chromeos {

class GViewURLRequestTestJob : public net::URLRequestTestJob {
 public:
  explicit GViewURLRequestTestJob(net::URLRequest* request)
      : net::URLRequestTestJob(request, true) {
  }

  virtual bool GetMimeType(std::string* mime_type) const {
    // During the course of a single test, two URLRequestJobs are
    // created -- the first is for the viewable document URL, and the
    // second is for the rediected URL.  In order to test the
    // interceptor, the mime type of the first request must be one of
    // the supported viewable mime types.  So when the net::URLRequestJob
    // is a request for one of the test URLs that point to viewable
    // content, return an appropraite mime type.  Otherwise, return
    // "text/html".
    if (request_->url() == GURL("http://foo.com/file.pdf")) {
      *mime_type = "application/pdf";
    } else if (request_->url() == GURL("http://foo.com/file.ppt")) {
      *mime_type = "application/vnd.ms-powerpoint";
    } else {
      *mime_type = "text/html";
    }
    return true;
  }

 private:
  ~GViewURLRequestTestJob() {}
};

class GViewRequestInterceptorTest : public testing::Test {
 public:
  virtual void SetUp() {
    net::URLRequest::RegisterProtocolFactory("http",
                                        &GViewRequestInterceptorTest::Factory);
    interceptor_ = GViewRequestInterceptor::GetInstance();
    ASSERT_TRUE(PathService::Get(chrome::FILE_PDF_PLUGIN, &pdf_path_));
  }

  virtual void TearDown() {
    net::URLRequest::RegisterProtocolFactory("http", NULL);
    message_loop_.RunAllPending();
  }

  static net::URLRequestJob* Factory(net::URLRequest* request,
                                     const std::string& scheme) {
    return new GViewURLRequestTestJob(request);
  }

  void RegisterPDFPlugin() {
    webkit::npapi::WebPluginInfo info;
    info.path = pdf_path_;
    info.name = ASCIIToUTF16("Internal PDF Plugin");
    info.enabled = webkit::npapi::WebPluginInfo::USER_ENABLED_POLICY_UNMANAGED;
    webkit::npapi::PluginList::Singleton()->RegisterInternalPlugin(info);
    webkit::npapi::PluginList::Singleton()->RefreshPlugins();
  }

  void UnregisterPDFPlugin() {
    webkit::npapi::PluginList::Singleton()->UnregisterInternalPlugin(pdf_path_);
    webkit::npapi::PluginList::Singleton()->RefreshPlugins();
  }

  void SetPDFPluginLoadedState(bool want_loaded, bool* out_is_enabled) {
    webkit::npapi::WebPluginInfo info;
    bool is_loaded =
        webkit::npapi::PluginList::Singleton()->GetPluginInfoByPath(
            pdf_path_, &info);
    if (is_loaded && !want_loaded) {
      UnregisterPDFPlugin();
      is_loaded = webkit::npapi::PluginList::Singleton()->GetPluginInfoByPath(
          pdf_path_, &info);
    } else if (!is_loaded && want_loaded) {
      // This "loads" the plug-in even if it's not present on the
      // system - which is OK since we don't actually use it, just
      // need it to be "enabled" for the test.
      RegisterPDFPlugin();
      is_loaded = webkit::npapi::PluginList::Singleton()->GetPluginInfoByPath(
          pdf_path_, &info);
    }
    EXPECT_EQ(want_loaded, is_loaded);
    *out_is_enabled = webkit::npapi::IsPluginEnabled(info);
  }

 protected:
  MessageLoopForIO message_loop_;
  TestDelegate test_delegate_;
  net::URLRequest::Interceptor* interceptor_;
  FilePath pdf_path_;
};

TEST_F(GViewRequestInterceptorTest, DoNotInterceptHtml) {
  net::URLRequest request(GURL("http://foo.com/index.html"), &test_delegate_);
  request.Start();
  MessageLoop::current()->Run();
  EXPECT_EQ(0, test_delegate_.received_redirect_count());
  EXPECT_EQ(GURL("http://foo.com/index.html"), request.url());
}

TEST_F(GViewRequestInterceptorTest, DoNotInterceptDownload) {
  net::URLRequest request(GURL("http://foo.com/file.pdf"), &test_delegate_);
  request.set_load_flags(net::LOAD_IS_DOWNLOAD);
  request.Start();
  MessageLoop::current()->Run();
  EXPECT_EQ(0, test_delegate_.received_redirect_count());
  EXPECT_EQ(GURL("http://foo.com/file.pdf"), request.url());
}

TEST_F(GViewRequestInterceptorTest, DoNotInterceptPdfWhenEnabled) {
  bool enabled;
  SetPDFPluginLoadedState(true, &enabled);

  if (!enabled) {
    bool pdf_plugin_enabled =
        webkit::npapi::PluginList::Singleton()->EnablePlugin(pdf_path_);
    EXPECT_TRUE(pdf_plugin_enabled);
  }

  net::URLRequest request(GURL("http://foo.com/file.pdf"), &test_delegate_);
  request.Start();
  MessageLoop::current()->Run();
  EXPECT_EQ(0, test_delegate_.received_redirect_count());
  EXPECT_EQ(GURL("http://foo.com/file.pdf"), request.url());
}

TEST_F(GViewRequestInterceptorTest, InterceptPdfWhenDisabled) {
  bool enabled;
  SetPDFPluginLoadedState(true, &enabled);

  if (enabled) {
    bool pdf_plugin_disabled =
        webkit::npapi::PluginList::Singleton()->DisablePlugin(pdf_path_);
    EXPECT_TRUE(pdf_plugin_disabled);
  }

  net::URLRequest request(GURL("http://foo.com/file.pdf"), &test_delegate_);
  request.Start();
  MessageLoop::current()->Run();
  EXPECT_EQ(1, test_delegate_.received_redirect_count());
  EXPECT_EQ(
      GURL("http://docs.google.com/gview?url=http%3A//foo.com/file.pdf"),
      request.url());
}

TEST_F(GViewRequestInterceptorTest, InterceptPdfWithNoPlugin) {
  bool enabled;
  SetPDFPluginLoadedState(false, &enabled);

  net::URLRequest request(GURL("http://foo.com/file.pdf"), &test_delegate_);
  request.Start();
  MessageLoop::current()->Run();
  EXPECT_EQ(1, test_delegate_.received_redirect_count());
  EXPECT_EQ(GURL("http://docs.google.com/gview?url=http%3A//foo.com/file.pdf"),
                 request.url());
}

TEST_F(GViewRequestInterceptorTest, InterceptPowerpoint) {
  net::URLRequest request(GURL("http://foo.com/file.ppt"), &test_delegate_);
  request.Start();
  MessageLoop::current()->Run();
  EXPECT_EQ(1, test_delegate_.received_redirect_count());
  EXPECT_EQ(GURL("http://docs.google.com/gview?url=http%3A//foo.com/file.ppt"),
                 request.url());
}

}  // namespace chromeos
