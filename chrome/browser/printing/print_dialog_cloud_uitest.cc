// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_dialog_cloud.h"
#include "chrome/browser/printing/print_dialog_cloud_internal.h"

#include <functional>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/singleton.h"
#include "base/threading/thread_restrictions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/printing/cloud_print/cloud_print_url.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "content/browser/browser_thread.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_test_job.h"
#include "net/url_request/url_request_test_util.h"

namespace {

class TestData {
 public:
  static TestData* GetInstance() {
    return Singleton<TestData>::get();
  }

  const char* GetTestData() {
    // Fetching this data blocks the IO thread, but we don't really care because
    // this is a test.
    base::ThreadRestrictions::ScopedAllowIO allow_io;

    if (test_data_.empty()) {
      FilePath test_data_directory;
      PathService::Get(chrome::DIR_TEST_DATA, &test_data_directory);
      FilePath test_file =
          test_data_directory.AppendASCII("printing/cloud_print_uitest.html");
      file_util::ReadFileToString(test_file, &test_data_);
    }
    return test_data_.c_str();
  }
 private:
  TestData() {}

  std::string test_data_;

  friend struct DefaultSingletonTraits<TestData>;
};

// A simple test net::URLRequestJob. We don't care what it does, only that
// whether it starts and finishes.
class SimpleTestJob : public net::URLRequestTestJob {
 public:
  explicit SimpleTestJob(net::URLRequest* request)
      : net::URLRequestTestJob(request, test_headers(),
                               TestData::GetInstance()->GetTestData(), true) {}

  virtual void GetResponseInfo(net::HttpResponseInfo* info) {
    net::URLRequestTestJob::GetResponseInfo(info);
    if (request_->url().SchemeIsSecure()) {
      // Make up a fake certificate for this response since we don't have
      // access to the real SSL info.
      const char* kCertIssuer = "Chrome Internal";
      const int kLifetimeDays = 100;

      info->ssl_info.cert =
          new net::X509Certificate(request_->url().GetWithEmptyPath().spec(),
                                   kCertIssuer,
                                   base::Time::Now(),
                                   base::Time::Now() +
                                   base::TimeDelta::FromDays(kLifetimeDays));
      info->ssl_info.cert_status = 0;
      info->ssl_info.security_bits = -1;
    }
  }

 private:
  ~SimpleTestJob() {}
};

class TestController {
 public:
  static TestController* GetInstance() {
    return Singleton<TestController>::get();
  }
  void set_result(bool value) {
    result_ = value;
  }
  bool result() {
    return result_;
  }
  void set_expected_url(const GURL& url) {
    expected_url_ = url;
  }
  const GURL expected_url() {
    return expected_url_;
  }
  void set_delegate(TestDelegate* delegate) {
    delegate_ = delegate;
  }
  TestDelegate* delegate() {
    return delegate_;
  }
  void set_use_delegate(bool value) {
    use_delegate_ = value;
  }
  bool use_delegate() {
    return use_delegate_;
  }
 private:
  TestController()
      : result_(false),
        use_delegate_(false),
        delegate_(NULL) {}

  bool result_;
  bool use_delegate_;
  GURL expected_url_;
  TestDelegate* delegate_;

  friend struct DefaultSingletonTraits<TestController>;
};

}  // namespace

class PrintDialogCloudTest : public InProcessBrowserTest {
 public:
  PrintDialogCloudTest() : handler_added_(false) {
    PathService::Get(chrome::DIR_TEST_DATA, &test_data_directory_);
  }

  // Must be static for handing into AddHostnameHandler.
  static net::URLRequest::ProtocolFactory Factory;

  class AutoQuitDelegate : public TestDelegate {
   public:
    AutoQuitDelegate() {}

    virtual void OnResponseCompleted(net::URLRequest* request) {
      BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                              new MessageLoop::QuitTask());
    }
  };

  virtual void SetUp() {
    TestController::GetInstance()->set_result(false);
    InProcessBrowserTest::SetUp();
  }

  virtual void TearDown() {
    if (handler_added_) {
      net::URLRequestFilter* filter = net::URLRequestFilter::GetInstance();
      filter->RemoveHostnameHandler(scheme_, host_name_);
      handler_added_ = false;
      TestController::GetInstance()->set_delegate(NULL);
    }
    InProcessBrowserTest::TearDown();
  }

  // Normally this is something I would expect could go into SetUp(),
  // but there seems to be some timing or ordering related issue with
  // the test harness that made that flaky.  Calling this from the
  // individual test functions seems to fix that.
  void AddTestHandlers() {
    if (!handler_added_) {
      net::URLRequestFilter* filter = net::URLRequestFilter::GetInstance();
      GURL cloud_print_service_url =
          CloudPrintURL(browser()->profile()).
          GetCloudPrintServiceURL();
      scheme_ = cloud_print_service_url.scheme();
      host_name_ = cloud_print_service_url.host();
      filter->AddHostnameHandler(scheme_, host_name_,
                                 &PrintDialogCloudTest::Factory);
      handler_added_ = true;

      GURL cloud_print_dialog_url =
          CloudPrintURL(browser()->profile()).
          GetCloudPrintServiceDialogURL();
      TestController::GetInstance()->set_expected_url(cloud_print_dialog_url);
      TestController::GetInstance()->set_delegate(&delegate_);
    }

    CreateDialogForTest();
  }

  void CreateDialogForTest() {
    FilePath path_to_pdf =
        test_data_directory_.AppendASCII("printing/cloud_print_uitest.pdf");
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        NewRunnableFunction(&PrintDialogCloud::CreateDialogImpl,
                            path_to_pdf,
                            string16(),
                            std::string("application/pdf"),
                            true));
  }

  bool handler_added_;
  std::string scheme_;
  std::string host_name_;
  FilePath test_data_directory_;
  AutoQuitDelegate delegate_;
};

net::URLRequestJob* PrintDialogCloudTest::Factory(net::URLRequest* request,
                                                  const std::string& scheme) {
  if (TestController::GetInstance()->use_delegate())
    request->set_delegate(TestController::GetInstance()->delegate());
  if (request &&
      (request->url() == TestController::GetInstance()->expected_url())) {
    TestController::GetInstance()->set_result(true);
  }
  return new SimpleTestJob(request);
}

IN_PROC_BROWSER_TEST_F(PrintDialogCloudTest, HandlersRegistered) {
  BrowserList::SetLastActive(browser());
  ASSERT_TRUE(BrowserList::GetLastActive());

  AddTestHandlers();

  TestController::GetInstance()->set_use_delegate(true);

  ui_test_utils::RunMessageLoop();

  ASSERT_TRUE(TestController::GetInstance()->result());
}

#if defined(OS_CHROMEOS)
// Disabled until the extern URL is live so that the Print menu item
// can be enabled for Chromium OS.
IN_PROC_BROWSER_TEST_F(PrintDialogCloudTest, DISABLED_DialogGrabbed) {
  BrowserList::SetLastActive(browser());
  ASSERT_TRUE(BrowserList::GetLastActive());

  AddTestHandlers();

  // This goes back one step further for the Chrome OS case, to making
  // sure 'window.print()' gets to the right place.
  ASSERT_TRUE(browser()->GetSelectedTabContents());
  ASSERT_TRUE(browser()->GetSelectedTabContents()->render_view_host());

  string16 window_print = ASCIIToUTF16("window.print()");
  browser()->GetSelectedTabContents()->render_view_host()->
      ExecuteJavascriptInWebFrame(string16(), window_print);

  ui_test_utils::RunMessageLoop();

  ASSERT_TRUE(TestController::GetInstance()->result());
}
#endif
