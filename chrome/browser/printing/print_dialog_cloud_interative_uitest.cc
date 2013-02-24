// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_dialog_cloud.h"
#include "chrome/browser/printing/print_dialog_cloud_internal.h"

#include <functional>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/memory/singleton.h"
#include "base/path_service.h"
#include "base/threading/thread_restrictions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/printing/cloud_print/cloud_print_url.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_controls.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_browser_thread.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_test_job.h"
#include "net/url_request/url_request_test_util.h"

using content::BrowserThread;

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
      base::FilePath test_data_directory;
      PathService::Get(chrome::DIR_TEST_DATA, &test_data_directory);
      base::FilePath test_file =
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
  SimpleTestJob(net::URLRequest* request,
                net::NetworkDelegate* network_delegate)
      : net::URLRequestTestJob(request,
                               network_delegate,
                               test_headers(),
                               TestData::GetInstance()->GetTestData(),
                               true) {}

  virtual void GetResponseInfo(net::HttpResponseInfo* info) OVERRIDE {
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
  virtual ~SimpleTestJob() {}
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
  void set_delegate(net::TestDelegate* delegate) {
    delegate_ = delegate;
  }
  net::TestDelegate* delegate() {
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
  net::TestDelegate* delegate_;

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

  class AutoQuitDelegate : public net::TestDelegate {
   public:
    AutoQuitDelegate() {}

    virtual void OnResponseCompleted(net::URLRequest* request) OVERRIDE {
      BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                              MessageLoop::QuitClosure());
    }
  };

  virtual void SetUp() OVERRIDE {
    TestController::GetInstance()->set_result(false);
    InProcessBrowserTest::SetUp();
  }

  virtual void TearDown() OVERRIDE {
    if (handler_added_) {
      BrowserThread::PostTask(
          BrowserThread::IO, FROM_HERE,
          base::Bind(UnregisterTestHandlers, scheme_, host_name_));
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
      GURL cloud_print_service_url =
          CloudPrintURL(browser()->profile()).
          GetCloudPrintServiceURL();
      scheme_ = cloud_print_service_url.scheme();
      host_name_ = cloud_print_service_url.host();
      BrowserThread::PostTask(
          BrowserThread::IO, FROM_HERE,
          base::Bind(RegisterTestHandlers, scheme_, host_name_));
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
    base::FilePath path_to_pdf =
        test_data_directory_.AppendASCII("printing/cloud_print_uitest.pdf");
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&internal_cloud_print_helpers::CreateDialogFullImpl,
                   browser()->profile(), browser()->window()->GetNativeWindow(),
                   path_to_pdf, string16(), string16(),
                   std::string("application/pdf"), false));
  }

 private:
  static void RegisterTestHandlers(const std::string& scheme,
                       const std::string& host_name) {
    net::URLRequestFilter::GetInstance()->AddHostnameHandler(
        scheme, host_name, &PrintDialogCloudTest::Factory);
  }
  static void UnregisterTestHandlers(const std::string& scheme,
                         const std::string& host_name) {
    net::URLRequestFilter::GetInstance()->RemoveHostnameHandler(scheme,
                                                                host_name);
  }

  bool handler_added_;
  std::string scheme_;
  std::string host_name_;
  base::FilePath test_data_directory_;
  AutoQuitDelegate delegate_;
};

net::URLRequestJob* PrintDialogCloudTest::Factory(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate,
    const std::string& scheme) {
  if (request &&
      (request->url() == TestController::GetInstance()->expected_url())) {
    if (TestController::GetInstance()->use_delegate())
      request->set_delegate(TestController::GetInstance()->delegate());
    TestController::GetInstance()->set_result(true);
    return new SimpleTestJob(request, network_delegate);
  }
  return new net::URLRequestTestJob(request,
                                    network_delegate,
                                    net::URLRequestTestJob::test_headers(),
                                    "",
                                    true);
}

#if defined(OS_WIN)
#define MAYBE_HandlersRegistered DISABLED_HandlersRegistered
#else
#define MAYBE_HandlersRegistered HandlersRegistered
#endif
IN_PROC_BROWSER_TEST_F(PrintDialogCloudTest, MAYBE_HandlersRegistered) {
  AddTestHandlers();

  TestController::GetInstance()->set_use_delegate(true);

  content::RunMessageLoop();

  ASSERT_TRUE(TestController::GetInstance()->result());

  // Close the dialog before finishing the test.
  content::WindowedNotificationObserver tab_closed_observer(
      content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
      content::NotificationService::AllSources());

  // Can't use ui_test_utils::SendKeyPressSync or
  // ui_test_utils::SendKeyPressAndWait due to a race condition with closing
  // the window. See http://crbug.com/111269
  BrowserWindow* window = browser()->window();
  ASSERT_TRUE(window);
  gfx::NativeWindow native_window = window->GetNativeWindow();
  ASSERT_TRUE(native_window);
  bool key_sent = ui_controls::SendKeyPress(native_window, ui::VKEY_ESCAPE,
                                            false, false, false, false);
  EXPECT_TRUE(key_sent);
  if (key_sent)
    tab_closed_observer.Wait();
}
