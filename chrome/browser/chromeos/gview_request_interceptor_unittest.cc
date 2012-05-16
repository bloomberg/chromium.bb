// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/at_exit.h"
#include "base/message_loop.h"
#include "chrome/browser/chrome_plugin_service_filter.h"
#include "chrome/browser/chromeos/gview_request_interceptor.h"
#include "chrome/browser/plugin_prefs.h"
#include "chrome/browser/plugin_prefs_factory.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_pref_service.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/resource_request_info.h"
#include "content/test/mock_resource_context.h"
#include "content/test/test_browser_thread.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_job_factory.h"
#include "net/url_request/url_request_test_job.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/plugins/npapi/mock_plugin_list.h"

using content::BrowserThread;
using content::PluginService;

namespace chromeos {

namespace {

const char kPdfUrl[] = "http://foo.com/file.pdf";
const char kPptUrl[] = "http://foo.com/file.ppt";
const char kHtmlUrl[] = "http://foo.com/index.html";
const char kPdfBlob[] = "blob:blobinternal:///d17c4eef-28e7-42bd-bafa-78d5cb8";

const char kPdfUrlIntercepted[] =
    "http://docs.google.com/gview?url=http%3A//foo.com/file.pdf";
const char kPptUrlIntercepted[] =
    "http://docs.google.com/gview?url=http%3A//foo.com/file.ppt";

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
    const GURL& request_url = request_->url();
    if (request_url == GURL(kPdfUrl) || request_url == GURL(kPdfBlob)) {
      *mime_type = "application/pdf";
    } else if (request_url == GURL(kPptUrl)) {
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
        file_thread_(BrowserThread::FILE, &message_loop_),
        io_thread_(BrowserThread::IO, &message_loop_),
        plugin_list_(NULL, 0) {}

  virtual void SetUp() {
    net::URLRequestContext* request_context =
        resource_context_.GetRequestContext();
    old_factory_ = request_context->job_factory();
    job_factory_.SetProtocolHandler("http", new GViewRequestProtocolFactory);
    job_factory_.AddInterceptor(new GViewRequestInterceptor);
    request_context->set_job_factory(&job_factory_);
    PluginPrefsFactory::GetInstance()->ForceRegisterPrefsForTest(&prefs_);
    plugin_prefs_ = new PluginPrefs();
    plugin_prefs_->SetPrefs(&prefs_);
    ChromePluginServiceFilter* filter =
        ChromePluginServiceFilter::GetInstance();
    filter->RegisterResourceContext(plugin_prefs_, &resource_context_);
    PluginService::GetInstance()->SetFilter(filter);

    ASSERT_TRUE(PathService::Get(chrome::FILE_PDF_PLUGIN, &pdf_path_));

    PluginService::GetInstance()->SetPluginListForTesting(&plugin_list_);
    PluginService::GetInstance()->Init();
  }

  virtual void TearDown() {
    plugin_prefs_->ShutdownOnUIThread();
    net::URLRequestContext* request_context =
        resource_context_.GetRequestContext();
    request_context->set_job_factory(old_factory_);
    ChromePluginServiceFilter* filter =
        ChromePluginServiceFilter::GetInstance();
    filter->UnregisterResourceContext(&resource_context_);
    PluginService::GetInstance()->SetFilter(NULL);
  }

  void RegisterPDFPlugin() {
    webkit::WebPluginInfo info;
    info.path = pdf_path_;
    plugin_list_.AddPluginToLoad(info);
    plugin_list_.RefreshPlugins();
  }

  void UnregisterPDFPlugin() {
    plugin_list_.ClearPluginsToLoad();
    plugin_list_.RefreshPlugins();
  }

  void SetPDFPluginLoadedState(bool want_loaded) {
    webkit::WebPluginInfo info;
    bool is_loaded = PluginService::GetInstance()->GetPluginInfoByPath(
        pdf_path_, &info);
    if (is_loaded == want_loaded)
      return;

    if (want_loaded) {
      // This "loads" the plug-in even if it's not present on the
      // system - which is OK since we don't actually use it, just
      // need it to be "enabled" for the test.
      RegisterPDFPlugin();
    } else {
      UnregisterPDFPlugin();
    }
    is_loaded = PluginService::GetInstance()->GetPluginInfoByPath(
        pdf_path_, &info);
    ASSERT_EQ(want_loaded, is_loaded);
  }

  void SetupRequest(net::URLRequest* request) {
    content::ResourceRequestInfo::AllocateForTesting(request,
                                                     ResourceType::MAIN_FRAME,
                                                     &resource_context_);
    request->set_context(resource_context_.GetRequestContext());
  }

 protected:
  base::ShadowingAtExitManager at_exit_manager_;  // Deletes PluginService.
  MessageLoopForIO message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;
  content::TestBrowserThread io_thread_;
  webkit::npapi::MockPluginList plugin_list_;
  TestingPrefService prefs_;
  scoped_refptr<PluginPrefs> plugin_prefs_;
  net::URLRequestJobFactory job_factory_;
  const net::URLRequestJobFactory* old_factory_;
  TestDelegate test_delegate_;
  FilePath pdf_path_;
  content::MockResourceContext resource_context_;
};

TEST_F(GViewRequestInterceptorTest, DoNotInterceptHtml) {
  net::URLRequest request(GURL(kHtmlUrl), &test_delegate_);
  SetupRequest(&request);
  request.Start();
  MessageLoop::current()->Run();
  EXPECT_EQ(0, test_delegate_.received_redirect_count());
  EXPECT_EQ(GURL(kHtmlUrl), request.url());
}

TEST_F(GViewRequestInterceptorTest, DoNotInterceptDownload) {
  net::URLRequest request(GURL(kPdfUrl), &test_delegate_);
  SetupRequest(&request);
  request.set_load_flags(net::LOAD_IS_DOWNLOAD);
  request.Start();
  MessageLoop::current()->Run();
  EXPECT_EQ(0, test_delegate_.received_redirect_count());
  EXPECT_EQ(GURL(kPdfUrl), request.url());
}

TEST_F(GViewRequestInterceptorTest, DoNotInterceptPdfWhenEnabled) {
  ASSERT_NO_FATAL_FAILURE(SetPDFPluginLoadedState(true));
  plugin_prefs_->EnablePlugin(true, pdf_path_, MessageLoop::QuitClosure());
  MessageLoop::current()->Run();

  net::URLRequest request(GURL(kPdfUrl), &test_delegate_);
  SetupRequest(&request);
  request.Start();
  MessageLoop::current()->Run();
  EXPECT_EQ(0, test_delegate_.received_redirect_count());
  EXPECT_EQ(GURL(kPdfUrl), request.url());
}

TEST_F(GViewRequestInterceptorTest, InterceptPdfWhenDisabled) {
  ASSERT_NO_FATAL_FAILURE(SetPDFPluginLoadedState(true));
  plugin_prefs_->EnablePlugin(false, pdf_path_, MessageLoop::QuitClosure());
  MessageLoop::current()->Run();

  net::URLRequest request(GURL(kPdfUrl), &test_delegate_);
  SetupRequest(&request);
  request.Start();
  MessageLoop::current()->Run();
  EXPECT_EQ(1, test_delegate_.received_redirect_count());
  EXPECT_EQ(GURL(kPdfUrlIntercepted), request.url());
}

#if !defined(GOOGLE_CHROME_BUILD)
// Official builds have pdf plugin by default, and we cannot unload it, so the
// test fails. Since pdf plugin is always present, we don't need to run this
// test.
TEST_F(GViewRequestInterceptorTest, InterceptPdfWithNoPlugin) {
  ASSERT_NO_FATAL_FAILURE(SetPDFPluginLoadedState(false));

  net::URLRequest request(GURL(kPdfUrl), &test_delegate_);
  SetupRequest(&request);
  request.Start();
  MessageLoop::current()->Run();
  EXPECT_EQ(1, test_delegate_.received_redirect_count());
  EXPECT_EQ(GURL(kPdfUrlIntercepted), request.url());
}
#endif

TEST_F(GViewRequestInterceptorTest, InterceptPowerpoint) {
  net::URLRequest request(GURL(kPptUrl), &test_delegate_);
  SetupRequest(&request);
  request.Start();
  MessageLoop::current()->Run();
  EXPECT_EQ(1, test_delegate_.received_redirect_count());
  EXPECT_EQ(GURL(kPptUrlIntercepted), request.url());
}

TEST_F(GViewRequestInterceptorTest, DoNotInterceptPost) {
  ASSERT_NO_FATAL_FAILURE(SetPDFPluginLoadedState(false));

  net::URLRequest request(GURL(kPdfUrl), &test_delegate_);
  SetupRequest(&request);
  request.set_method("POST");
  request.Start();
  MessageLoop::current()->Run();
  EXPECT_EQ(0, test_delegate_.received_redirect_count());
  EXPECT_EQ(GURL(kPdfUrl), request.url());
}

TEST_F(GViewRequestInterceptorTest, DoNotInterceptBlob) {
  ASSERT_NO_FATAL_FAILURE(SetPDFPluginLoadedState(false));

  net::URLRequest request(GURL(kPdfBlob), &test_delegate_);
  SetupRequest(&request);
  request.Start();
  MessageLoop::current()->Run();
  EXPECT_EQ(0, test_delegate_.received_redirect_count());
  EXPECT_EQ(GURL(kPdfBlob), request.url());
}

}  // namespace

}  // namespace chromeos
