// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/message_loop.h"
#include "chrome/browser/chrome_plugin_service_filter.h"
#include "chrome/browser/chromeos/gview_request_interceptor.h"
#include "chrome/browser/plugin_prefs.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_pref_service.h"
#include "content/browser/mock_resource_context.h"
#include "content/browser/plugin_service.h"
#include "content/browser/renderer_host/dummy_resource_handler.h"
#include "content/browser/renderer_host/resource_dispatcher_host_request_info.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_job_factory.h"
#include "net/url_request/url_request_test_job.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/plugins/npapi/plugin_list.h"

namespace chromeos {

namespace {

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

class GViewRequestProtocolFactory
    : public net::URLRequestJobFactory::ProtocolHandler {
 public:
  GViewRequestProtocolFactory() {}
  virtual ~GViewRequestProtocolFactory() {}

  virtual net::URLRequestJob* MaybeCreateJob(net::URLRequest* request) const {
    return new GViewURLRequestTestJob(request);
  }
};

class GViewRequestInterceptorTest : public testing::Test {
 public:
  GViewRequestInterceptorTest()
      : ui_thread_(BrowserThread::UI, &message_loop_),
        io_thread_(BrowserThread::IO, &message_loop_) {}

  virtual void SetUp() {
    content::ResourceContext* resource_context =
        content::MockResourceContext::GetInstance();
    net::URLRequestContext* request_context =
        resource_context->request_context();
    old_factory_ = request_context->job_factory();
    job_factory_.SetProtocolHandler("http", new GViewRequestProtocolFactory);
    job_factory_.AddInterceptor(new GViewRequestInterceptor);
    request_context->set_job_factory(&job_factory_);
    PluginPrefs::RegisterPrefs(&prefs_);
    plugin_prefs_ = new PluginPrefs();
    plugin_prefs_->SetPrefs(&prefs_);
    ChromePluginServiceFilter* filter =
        ChromePluginServiceFilter::GetInstance();
    filter->RegisterResourceContext(plugin_prefs_, resource_context);
    PluginService::GetInstance()->set_filter(filter);

    ASSERT_TRUE(PathService::Get(chrome::FILE_PDF_PLUGIN, &pdf_path_));

    handler_ = new content::DummyResourceHandler();
  }

  virtual void TearDown() {
    content::ResourceContext* resource_context =
        content::MockResourceContext::GetInstance();
    net::URLRequestContext* request_context =
        resource_context->request_context();
    request_context->set_job_factory(old_factory_);
    ChromePluginServiceFilter* filter =
        ChromePluginServiceFilter::GetInstance();
    filter->UnregisterResourceContext(resource_context);
    PluginService::GetInstance()->set_filter(NULL);
  }

  void RegisterPDFPlugin() {
    webkit::WebPluginInfo info;
    info.path = pdf_path_;
    info.enabled = webkit::WebPluginInfo::USER_ENABLED_POLICY_UNMANAGED;
    webkit::npapi::PluginList::Singleton()->RegisterInternalPlugin(info);
    webkit::npapi::PluginList::Singleton()->RefreshPlugins();
  }

  void UnregisterPDFPlugin() {
    webkit::npapi::PluginList::Singleton()->UnregisterInternalPlugin(pdf_path_);
    webkit::npapi::PluginList::Singleton()->RefreshPlugins();
  }

  void SetPDFPluginLoadedState(bool want_loaded) {
    webkit::WebPluginInfo info;
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
  }

  void SetupRequest(net::URLRequest* request) {
    content::ResourceContext* context =
        content::MockResourceContext::GetInstance();
    ResourceDispatcherHostRequestInfo* info =
        new ResourceDispatcherHostRequestInfo(handler_,
                                              ChildProcessInfo::RENDER_PROCESS,
                                              -1,          // child_id
                                              MSG_ROUTING_NONE,
                                              0,           // origin_pid
                                              request->identifier(),
                                              false,       // is_main_frame
                                              -1,          // frame_id
                                              ResourceType::MAIN_FRAME,
                                              PageTransition::LINK,
                                              0,           // upload_size
                                              false,       // is_download
                                              true,        // allow_download
                                              false,       // has_user_gesture
                                              context);
    request->SetUserData(NULL, info);
    request->set_context(context->request_context());
  }

 protected:
  MessageLoopForIO message_loop_;
  BrowserThread ui_thread_;
  BrowserThread io_thread_;
  TestingPrefService prefs_;
  scoped_refptr<PluginPrefs> plugin_prefs_;
  net::URLRequestJobFactory job_factory_;
  const net::URLRequestJobFactory* old_factory_;
  scoped_refptr<ResourceHandler> handler_;
  TestDelegate test_delegate_;
  FilePath pdf_path_;
};

TEST_F(GViewRequestInterceptorTest, DoNotInterceptHtml) {
  net::URLRequest request(GURL("http://foo.com/index.html"), &test_delegate_);
  SetupRequest(&request);
  request.Start();
  MessageLoop::current()->Run();
  EXPECT_EQ(0, test_delegate_.received_redirect_count());
  EXPECT_EQ(GURL("http://foo.com/index.html"), request.url());
}

TEST_F(GViewRequestInterceptorTest, DoNotInterceptDownload) {
  net::URLRequest request(GURL("http://foo.com/file.pdf"), &test_delegate_);
  SetupRequest(&request);
  request.set_load_flags(net::LOAD_IS_DOWNLOAD);
  request.Start();
  MessageLoop::current()->Run();
  EXPECT_EQ(0, test_delegate_.received_redirect_count());
  EXPECT_EQ(GURL("http://foo.com/file.pdf"), request.url());
}

TEST_F(GViewRequestInterceptorTest, DoNotInterceptPdfWhenEnabled) {
  SetPDFPluginLoadedState(true);
  webkit::npapi::PluginList::Singleton()->EnablePlugin(pdf_path_);

  net::URLRequest request(GURL("http://foo.com/file.pdf"), &test_delegate_);
  SetupRequest(&request);
  request.Start();
  MessageLoop::current()->Run();
  EXPECT_EQ(0, test_delegate_.received_redirect_count());
  EXPECT_EQ(GURL("http://foo.com/file.pdf"), request.url());
}

TEST_F(GViewRequestInterceptorTest, InterceptPdfWhenDisabled) {
  SetPDFPluginLoadedState(true);
  webkit::npapi::PluginList::Singleton()->DisablePlugin(pdf_path_);

  net::URLRequest request(GURL("http://foo.com/file.pdf"), &test_delegate_);
  SetupRequest(&request);
  request.Start();
  MessageLoop::current()->Run();
  EXPECT_EQ(1, test_delegate_.received_redirect_count());
  EXPECT_EQ(
      GURL("http://docs.google.com/gview?url=http%3A//foo.com/file.pdf"),
      request.url());
}

TEST_F(GViewRequestInterceptorTest, InterceptPdfWithNoPlugin) {
  SetPDFPluginLoadedState(false);

  net::URLRequest request(GURL("http://foo.com/file.pdf"), &test_delegate_);
  SetupRequest(&request);
  request.Start();
  MessageLoop::current()->Run();
  EXPECT_EQ(1, test_delegate_.received_redirect_count());
  EXPECT_EQ(GURL("http://docs.google.com/gview?url=http%3A//foo.com/file.pdf"),
                 request.url());
}

TEST_F(GViewRequestInterceptorTest, InterceptPowerpoint) {
  net::URLRequest request(GURL("http://foo.com/file.ppt"), &test_delegate_);
  SetupRequest(&request);
  request.Start();
  MessageLoop::current()->Run();
  EXPECT_EQ(1, test_delegate_.received_redirect_count());
  EXPECT_EQ(GURL("http://docs.google.com/gview?url=http%3A//foo.com/file.ppt"),
                 request.url());
}

}  // namespace

}  // namespace chromeos
