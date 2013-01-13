// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <iterator>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/favicon/favicon_service.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/intents/default_web_intent_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/intents/web_intent_picker.h"
#include "chrome/browser/ui/intents/web_intent_picker_controller.h"
#include "chrome/browser/ui/intents/web_intent_picker_model.h"
#include "chrome/browser/ui/intents/web_intent_picker_model_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/browser/webdata/web_data_service_factory.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_intents_dispatcher.h"
#include "content/public/test/test_utils.h"
#include "extensions/common/constants.h"
#include "net/base/escape.h"
#include "net/base/mock_host_resolver.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/image/image_util.h"
#include "webkit/glue/web_intent_service_data.h"

namespace {

const string16 kAction1(ASCIIToUTF16("http://webintents.org/share"));
const string16 kAction2(ASCIIToUTF16("http://www.example.com/foobar"));
const string16 kType1(ASCIIToUTF16("image/png"));
const string16 kType2(ASCIIToUTF16("text/*"));
const GURL kServiceURL1("http://www.google.com");
const GURL kServiceURL2("http://www.chromium.org");
const char kDummyExtensionId[] = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
const char kCWSResponseEmpty[] =
  "{\"kind\":\"chromewebstore#itemList\",\"total_items\":0,\"start_index\":0,"
  "\"items\":[]}";

const char kCWSResponseResultFormat[] =
  "{\"kind\":\"chromewebstore#itemList\","
    "\"total_items\":1,"
    "\"start_index\":0,"
    "\"items\":[{"
      "\"kind\":\"chromewebstore#item\","
      "\"id\":\"%s\","
      "\"type\":\"APPLICATION\","
      "\"num_ratings\":0,"
      "\"average_rating\":0.0,"
      "\"manifest\": \"{\\n"
        "\\\"name\\\": \\\"Dummy Share\\\",\\n"
        "\\\"version\\\": \\\"1.0.0.0\\\",\\n"
        "\\\"intents\\\": {\\n"
          "\\\"%s\\\" : {\\n"
            "\\\"type\\\" : [\\\"%s\\\"],\\n"
            "\\\"path\\\" : \\\"share.html\\\",\\n"
            "\\\"title\\\" : \\\"Dummy share!\\\",\\n"
            "\\\"disposition\\\": \\\"inline\\\"\\n"
          "}\\n"
        "}\\n"
      "}\\n\","
      "\"family_safe\":true,"
      "\"icon_url\": \"%s\"}]}";

const char kCWSFakeIconURLFormat[] = "http://example.com/%s/icon.png";

class DummyURLFetcherFactory : public net::URLFetcherFactory {
 public:
  DummyURLFetcherFactory() {}
  virtual ~DummyURLFetcherFactory() {}

  virtual net::URLFetcher* CreateURLFetcher(
      int id,
      const GURL& url,
      net::URLFetcher::RequestType request_type,
      net::URLFetcherDelegate* d) OVERRIDE {
    return new net::TestURLFetcher(id, url, d);
  }
};

}  // namespace

class WebIntentPickerMock : public WebIntentPicker,
                            public WebIntentPickerModelObserver {
 public:
  WebIntentPickerMock()
      : num_installed_services_(0),
        num_icons_changed_(0),
        num_extension_icons_changed_(0),
        num_extensions_installed_(0),
        message_loop_started_(false),
        pending_async_completed_(false),
        num_inline_disposition_(0),
        delegate_(NULL) {
  }

  void MockClose() {
    delegate_->OnClosing();
  }

  // WebIntentPicker implementation.
  virtual void Close() OVERRIDE { StopWaiting(); }
  virtual void SetActionString(const string16& action) OVERRIDE {}
  virtual void OnExtensionInstallSuccess(const std::string& id) OVERRIDE {
    num_extensions_installed_++;
  }
  virtual void OnExtensionInstallFailure(const std::string& id) OVERRIDE {}
  virtual void OnInlineDispositionAutoResize(const gfx::Size& size) OVERRIDE {}
  virtual void OnPendingAsyncCompleted() OVERRIDE {
    StopWaiting();
  }
  virtual void InvalidateDelegate() OVERRIDE {
    delegate_ = NULL;
  }

  // WebIntentPickerModelObserver implementation.
  virtual void OnModelChanged(WebIntentPickerModel* model) OVERRIDE {
    num_installed_services_ =
        static_cast<int>(model->GetInstalledServiceCount());
  }
  virtual void OnFaviconChanged(
      WebIntentPickerModel* model, size_t index) OVERRIDE {
    num_icons_changed_++;
  }
  virtual void OnExtensionIconChanged(
      WebIntentPickerModel* model, const std::string& extension_id) OVERRIDE {
    num_extension_icons_changed_++;
  }
  virtual void OnInlineDisposition(
      const string16& title, const GURL& url) OVERRIDE {
    num_inline_disposition_++;
  }

  void Wait() {
    if (!pending_async_completed_) {
      message_loop_started_ = true;
      content::RunMessageLoop();
      pending_async_completed_ = false;
    }
  }

  void StopWaiting() {
    pending_async_completed_ = true;
    if (message_loop_started_) {
      message_loop_started_ = false;
      MessageLoop::current()->Quit();
    }
  }

  int num_installed_services_;
  int num_icons_changed_;
  int num_extension_icons_changed_;
  int num_extensions_installed_;
  bool message_loop_started_;
  bool pending_async_completed_;
  int num_inline_disposition_;
  WebIntentPickerDelegate* delegate_;
};

class IntentsDispatcherMock : public content::WebIntentsDispatcher {
 public:
  explicit IntentsDispatcherMock(const webkit_glue::WebIntentData& intent)
      : intent_(intent),
        dispatched_(false),
        replied_(false) {}

  virtual const webkit_glue::WebIntentData& GetIntent() OVERRIDE {
    return intent_;
  }

  virtual void DispatchIntent(content::WebContents* web_contents) OVERRIDE {
    dispatched_ = true;
  }

  virtual void ResetDispatch() OVERRIDE {
  }

  virtual void SendReply(const webkit_glue::WebIntentReply& reply) OVERRIDE {
    replied_ = true;
  }

  virtual void RegisterReplyNotification(
      const base::Callback<void(webkit_glue::WebIntentReplyType)>&) OVERRIDE {
  }

  webkit_glue::WebIntentData intent_;
  bool dispatched_;
  bool replied_;
};

class WebIntentPickerControllerBrowserTest : public InProcessBrowserTest {
 public:
  WebIntentPickerControllerBrowserTest() {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    // We start the test server now instead of in
    // SetUpInProcessBrowserTestFixture so that we can get its port number.
    ASSERT_TRUE(test_server()->Start());

    net::HostPortPair host_port = test_server()->host_port_pair();
    command_line->AppendSwitchASCII(
        switches::kAppsGalleryDownloadURL,
        base::StringPrintf(
            "http://www.example.com:%d/files/extensions/intents/%%s.crx",
            host_port.port()));
    command_line->AppendSwitchASCII(
        switches::kAppsGalleryInstallAutoConfirmForTests, "accept");
  }

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    host_resolver()->AddRule("www.example.com", "127.0.0.1");
  }

  content::WebContents* GetWindowDispositionTarget(
      WebIntentPickerController* controller) {
    return controller->window_disposition_source_;
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    // The FakeURLFetcherFactory will return a NULL URLFetcher if a request is
    // created for a URL it doesn't know and there is no default factory.
    // Instead, use this dummy factory to infinitely delay the request.
    default_url_fetcher_factory_.reset(new DummyURLFetcherFactory);
    fake_url_fetcher_factory_.reset(
        new net::FakeURLFetcherFactory(default_url_fetcher_factory_.get()));

    web_data_service_ = WebDataServiceFactory::GetForProfile(
        GetBrowser()->profile(), Profile::EXPLICIT_ACCESS);
    favicon_service_ = FaviconServiceFactory::GetForProfile(
        GetBrowser()->profile(), Profile::EXPLICIT_ACCESS);
    controller_ = WebIntentPickerController::FromWebContents(
        GetBrowser()->tab_strip_model()->GetActiveWebContents());

    SetupMockPicker();
    controller_->set_model_observer(&picker_);
    picker_.delegate_ = controller_;

    CreateFakeIcon();
  }

  virtual void SetupMockPicker() {
    controller_->set_picker(&picker_);
  }

  virtual Browser* GetBrowser() { return browser(); }

  void AddWebIntentService(const string16& action, const GURL& service_url) {
    webkit_glue::WebIntentServiceData service;
    service.action = action;
    service.type = kType1;
    service.service_url = service_url;
    web_data_service_->AddWebIntentService(service);
  }

  void AddCWSExtensionServiceEmpty(const string16& action) {
    GURL cws_query_url = CWSIntentsRegistry::BuildQueryURL(action, kType1);
    fake_url_fetcher_factory_->SetFakeResponse(cws_query_url.spec(),
                                               kCWSResponseEmpty, true);
  }

  void AddCWSExtensionServiceWithResult(const std::string& extension_id,
                                        const string16& action,
                                        const string16& type) {
    GURL cws_query_url = CWSIntentsRegistry::BuildQueryURL(action, type);
    std::string icon_url;
    std::string escaped_action = net::EscapePath(UTF16ToUTF8(action));
    base::SStringPrintf(&icon_url, kCWSFakeIconURLFormat,
                        escaped_action.c_str());

    std::string response;
    base::SStringPrintf(&response, kCWSResponseResultFormat,
                        extension_id.c_str(),
                        UTF16ToUTF8(action).c_str(), UTF16ToUTF8(type).c_str(),
                        icon_url.c_str());
    fake_url_fetcher_factory_->SetFakeResponse(cws_query_url.spec(), response,
                                               true);

    fake_url_fetcher_factory_->SetFakeResponse(icon_url, icon_response_,
                                               true);
  }

  void SetDefaultService(const string16& action,
                         const std::string& url) {
    DefaultWebIntentService default_service;
    default_service.action = action;
    default_service.type = kType1;
    default_service.user_date = 1000000;
    default_service.service_url = url;
    web_data_service_->AddDefaultWebIntentService(default_service);
  }

  void OnSendReturnMessage(
    webkit_glue::WebIntentReplyType reply_type) {
    controller_->OnSendReturnMessage(reply_type);
  }

  void OnServiceChosen(
      const GURL& url,
      webkit_glue::WebIntentServiceData::Disposition disposition) {
    controller_->OnServiceChosen(url, disposition,
                                 WebIntentPickerDelegate::kEnableDefaults);
  }

  void OnCancelled() {
    controller_->OnUserCancelledPickerDialog();
  }

  void OnExtensionInstallRequested(const std::string& extension_id) {
    controller_->OnExtensionInstallRequested(extension_id);
  }

  extensions::WebstoreInstaller* GetWebstoreInstaller() const {
    return controller_->webstore_installer_.get();
  }

  void CreateFakeIcon() {
    gfx::Image image(gfx::test::CreateImage());
    scoped_refptr<base::RefCountedMemory> image_data = image.As1xPNGBytes();
    DCHECK(image_data->size());

    std::copy(image_data->front(), image_data->front() + image_data->size(),
              std::back_inserter(icon_response_));
  }

  void ClickLocationBarButton(content::WebContents* service_web_contents) {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    WebIntentPickerController* service_controller =
        WebIntentPickerController::FromWebContents(service_web_contents);
    service_controller->LocationBarPickerButtonClicked();
  }

  void CloseCurrentTab() {
    content::WindowedNotificationObserver tab_close_observer(
        content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
        content::NotificationService::AllSources());
    chrome::CloseTab(browser());
    tab_close_observer.Wait();
  }

  WebIntentPickerMock picker_;
  scoped_refptr<WebDataService> web_data_service_;
  FaviconService* favicon_service_;
  WebIntentPickerController* controller_;
  scoped_ptr<DummyURLFetcherFactory> default_url_fetcher_factory_;
  scoped_ptr<net::FakeURLFetcherFactory> fake_url_fetcher_factory_;
  std::string icon_response_;
};

IN_PROC_BROWSER_TEST_F(WebIntentPickerControllerBrowserTest, ChooseService) {
  AddWebIntentService(kAction1, kServiceURL1);
  AddWebIntentService(kAction1, kServiceURL2);
  AddCWSExtensionServiceEmpty(kAction1);

  webkit_glue::WebIntentData intent;
  intent.action = kAction1;
  intent.type = kType1;
  IntentsDispatcherMock dispatcher(intent);
  controller_->SetIntentsDispatcher(&dispatcher);

  controller_->ShowDialog(kAction1, kType1);
  picker_.Wait();
  EXPECT_EQ(2, picker_.num_installed_services_);
  EXPECT_EQ(0, picker_.num_icons_changed_);

  TabStripModel* tab_strip = browser()->tab_strip_model();
  OnServiceChosen(kServiceURL2,
                  webkit_glue::WebIntentServiceData::DISPOSITION_WINDOW);
  ASSERT_EQ(2, tab_strip->count());
  EXPECT_EQ(GURL(kServiceURL2),
            tab_strip->GetActiveWebContents()->GetURL());

  EXPECT_TRUE(GetWindowDispositionTarget(
      WebIntentPickerController::FromWebContents(
          tab_strip->GetActiveWebContents())));

  EXPECT_TRUE(dispatcher.dispatched_);

  OnSendReturnMessage(webkit_glue::WEB_INTENT_REPLY_SUCCESS);
  ASSERT_EQ(1, tab_strip->count());
}

IN_PROC_BROWSER_TEST_F(WebIntentPickerControllerBrowserTest,
                       FetchExtensionIcon) {
  AddWebIntentService(kAction1, kServiceURL1);
  AddWebIntentService(kAction1, kServiceURL2);
  AddCWSExtensionServiceWithResult(kDummyExtensionId, kAction1, kType1);

  webkit_glue::WebIntentData intent;
  intent.action = kAction1;
  intent.type = kType1;
  IntentsDispatcherMock dispatcher(intent);
  controller_->SetIntentsDispatcher(&dispatcher);

  controller_->ShowDialog(kAction1, kType1);
  picker_.Wait();
  // Flush all pending worker tasks for PNG decoding.
  content::BrowserThread::GetBlockingPool()->FlushForTesting();
  // Flush all tasks posted from the worker tasks.
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(2, picker_.num_installed_services_);
  EXPECT_EQ(0, picker_.num_icons_changed_);
  EXPECT_EQ(1, picker_.num_extension_icons_changed_);
}

IN_PROC_BROWSER_TEST_F(WebIntentPickerControllerBrowserTest, OpenCancelOpen) {
  AddWebIntentService(kAction1, kServiceURL1);
  AddWebIntentService(kAction1, kServiceURL2);
  AddCWSExtensionServiceEmpty(kAction1);

  webkit_glue::WebIntentData intent;
  intent.action = kAction1;
  intent.type = kType1;
  IntentsDispatcherMock dispatcher(intent);
  controller_->SetIntentsDispatcher(&dispatcher);

  controller_->ShowDialog(kAction1, kType1);
  picker_.Wait();
  OnCancelled();

  controller_->ShowDialog(kAction1, kType1);
  OnCancelled();
}

IN_PROC_BROWSER_TEST_F(WebIntentPickerControllerBrowserTest,
                       CloseTargetTabReturnToSource) {
  AddWebIntentService(kAction1, kServiceURL1);
  AddCWSExtensionServiceEmpty(kAction1);

  TabStripModel* tab_strip = browser()->tab_strip_model();
  GURL original = tab_strip->GetActiveWebContents()->GetURL();

  // Open a new page, but keep focus on original.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabURL), NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  ASSERT_EQ(2, tab_strip->count());
  EXPECT_EQ(original, tab_strip->GetActiveWebContents()->GetURL());

  webkit_glue::WebIntentData intent;
  intent.action = kAction1;
  intent.type = kType1;
  IntentsDispatcherMock dispatcher(intent);
  controller_->SetIntentsDispatcher(&dispatcher);

  controller_->ShowDialog(kAction1, kType1);
  picker_.Wait();
  EXPECT_EQ(1, picker_.num_installed_services_);

  OnServiceChosen(kServiceURL1,
                  webkit_glue::WebIntentServiceData::DISPOSITION_WINDOW);
  ASSERT_EQ(3, tab_strip->count());
  EXPECT_EQ(GURL(kServiceURL1),
            tab_strip->GetActiveWebContents()->GetURL());

  EXPECT_TRUE(dispatcher.dispatched_);

  OnSendReturnMessage(webkit_glue::WEB_INTENT_REPLY_SUCCESS);
  ASSERT_EQ(2, tab_strip->count());
  EXPECT_EQ(original, tab_strip->GetActiveWebContents()->GetURL());

  CloseCurrentTab();
}

class WebIntentPickerControllerIncognitoBrowserTest
    : public WebIntentPickerControllerBrowserTest {
 public:
  WebIntentPickerControllerIncognitoBrowserTest() {}

  virtual void SetUpOnMainThread() OVERRIDE {
    incognito_browser_ = CreateIncognitoBrowser();
    WebIntentPickerControllerBrowserTest::SetUpOnMainThread();
  }

  virtual Browser* GetBrowser() OVERRIDE { return incognito_browser_; }

  int pending_async_count() { return controller_->pending_async_count_; }

 private:
  Browser* incognito_browser_;
};

IN_PROC_BROWSER_TEST_F(WebIntentPickerControllerIncognitoBrowserTest,
                       ShowDialogShouldntCrash) {
  webkit_glue::WebIntentData intent;
  intent.action = kAction1;
  intent.type = kType1;
  IntentsDispatcherMock dispatcher(intent);
  controller_->SetIntentsDispatcher(&dispatcher);

  controller_->ShowDialog(kAction1, kType1);
  // This should do nothing for now.
  EXPECT_EQ(0, pending_async_count());
}

IN_PROC_BROWSER_TEST_F(WebIntentPickerControllerBrowserTest,
                       ExtensionInstallSuccess) {
  const char extension_id[] = "ooodacpbmglpoagccnepcbfhfhpdgddn";
  AddCWSExtensionServiceWithResult(extension_id, kAction1, kType2);

  webkit_glue::WebIntentData intent;
  intent.action = kAction1;
  intent.type = kType2;
  IntentsDispatcherMock dispatcher(intent);
  controller_->SetIntentsDispatcher(&dispatcher);

  controller_->ShowDialog(kAction1, kType2);
  picker_.Wait();

  TabStripModel* tab_strip = browser()->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  OnExtensionInstallRequested(extension_id);
  picker_.Wait();
  EXPECT_EQ(1, picker_.num_extensions_installed_);
  const extensions::Extension* extension = browser()->profile()->
      GetExtensionService()->GetExtensionById(extension_id, false);
  EXPECT_TRUE(extension);

  // Installing an extension should also choose it. Since this extension uses
  // window disposition, it will create a new tab.
  EXPECT_EQ(2, tab_strip->count());
  EXPECT_EQ(0, picker_.num_inline_disposition_);

  CloseCurrentTab();
}

// Tests that inline install of an extension using inline disposition works
// and brings up content as inline content.
IN_PROC_BROWSER_TEST_F(WebIntentPickerControllerBrowserTest,
                       ExtensionInstallSuccessInline) {
  const char extension_id[] = "nnhendkbgefomfgdlnmfhhmihihlljpi";
  AddCWSExtensionServiceWithResult(extension_id, kAction1, kType2);

  webkit_glue::WebIntentData intent;
  intent.action = kAction1;
  intent.type = kType2;
  IntentsDispatcherMock dispatcher(intent);
  controller_->SetIntentsDispatcher(&dispatcher);

  controller_->ShowDialog(kAction1, kType2);
  picker_.Wait();

  OnExtensionInstallRequested(extension_id);
  picker_.Wait();
  EXPECT_EQ(1, picker_.num_extensions_installed_);
  const extensions::Extension* extension = browser()->profile()->
      GetExtensionService()->GetExtensionById(extension_id, false);
  EXPECT_TRUE(extension);

  // Installing an extension should also choose it. Since this extension uses
  // inline disposition, it will create no tabs and invoke OnInlineDisposition.
  TabStripModel* tab_strip = browser()->tab_strip_model();
  EXPECT_EQ(1, tab_strip->count());
  EXPECT_EQ(1, picker_.num_inline_disposition_);
}

IN_PROC_BROWSER_TEST_F(WebIntentPickerControllerBrowserTest,
                       WebstoreInstallerLifetime) {
  const char extension_id[] = "nnhendkbgefomfgdlnmfhhmihihlljpi";
  AddCWSExtensionServiceWithResult(extension_id, kAction1, kType2);

  webkit_glue::WebIntentData intent;
  intent.action = kAction1;
  intent.type = kType2;
  IntentsDispatcherMock dispatcher(intent);
  controller_->SetIntentsDispatcher(&dispatcher);

  controller_->ShowDialog(kAction1, kType2);
  picker_.Wait();

  // We haven't yet created a WebstoreInstaller.
  EXPECT_FALSE(GetWebstoreInstaller());

  // While extension install is pending, we have a WebstoreInstaller.
  OnExtensionInstallRequested(extension_id);
  EXPECT_TRUE(GetWebstoreInstaller());

  // After extension install the WebstoreInstaller is cleaned up.
  picker_.Wait();
  EXPECT_EQ(1, picker_.num_extensions_installed_);
  EXPECT_FALSE(GetWebstoreInstaller());
}

// Test that an explicit intent does not trigger loading intents from the
// registry (skips the picker), and creates the intent service handler
// immediately.
IN_PROC_BROWSER_TEST_F(WebIntentPickerControllerBrowserTest,
                       ExplicitIntentTest) {
  // Install a target service for the explicit intent.
  const char extension_id[] = "ooodacpbmglpoagccnepcbfhfhpdgddn";
  AddCWSExtensionServiceWithResult(extension_id, kAction1, kType2);

  webkit_glue::WebIntentData intent;
  intent.action = kAction1;
  intent.type = kType2;
  IntentsDispatcherMock dispatcher(intent);
  controller_->SetIntentsDispatcher(&dispatcher);

  controller_->ShowDialog(kAction1, kType2);
  picker_.Wait();

  TabStripModel* tab_strip = browser()->tab_strip_model();
  OnExtensionInstallRequested(extension_id);
  picker_.Wait();
  ASSERT_EQ(1, picker_.num_extensions_installed_);
  // The intent launches a new tab.
  ASSERT_EQ(2, tab_strip->count());

  // Make the controller think nothing is being shown.
  picker_.MockClose();
  SetupMockPicker();

  // Now call the explicit intent.
  webkit_glue::WebIntentData explicitIntent;
  explicitIntent.action = kAction1;
  explicitIntent.type = kType2;
  explicitIntent.service = GURL(StringPrintf("%s://%s/%s",
                                             extensions::kExtensionScheme,
                                             extension_id,
                                             "share.html"));
  IntentsDispatcherMock dispatcher2(explicitIntent);
  controller_->SetIntentsDispatcher(&dispatcher2);
  ui_test_utils::WindowedTabAddedNotificationObserver new_tab_observer((
      content::Source<content::WebContentsDelegate>(browser())));
  controller_->ShowDialog(kAction1, kType2);
  new_tab_observer.Wait();

  content::WebContents* service_web_contents = new_tab_observer.GetTab();
  DCHECK(service_web_contents);

  // Location bar button should not be shown for explicit intents.
  WebIntentPickerController* service_controller =
      WebIntentPickerController::FromWebContents(service_web_contents);
  DCHECK(service_controller);
  EXPECT_FALSE(service_controller->ShowLocationBarPickerButton());

  EXPECT_EQ(3, tab_strip->count());
  EXPECT_EQ(0, picker_.num_inline_disposition_);
  EXPECT_FALSE(dispatcher2.replied_);

  // num_installed_services_ would be 2 if the intent wasn't explicit.
  EXPECT_EQ(1, picker_.num_installed_services_);

  // Close tabs to get rid of them before the dispatchers go out of scope at the
  // end of this method.
  CloseCurrentTab();
  CloseCurrentTab();
}

// Test that an explicit intent for non-installed extension won't
// complete.
IN_PROC_BROWSER_TEST_F(WebIntentPickerControllerBrowserTest,
                       ExplicitIntentNoExtensionTest) {
  AddWebIntentService(kAction1, kServiceURL1);
  AddWebIntentService(kAction1, kServiceURL2);
  AddCWSExtensionServiceWithResult(kDummyExtensionId, kAction1, kType1);

  webkit_glue::WebIntentData intent;
  intent.action = kAction1;
  intent.type = kType1;
  intent.service = GURL(StringPrintf("%s://%s/%s",
                                     extensions::kExtensionScheme,
                                     kDummyExtensionId,
                                     UTF16ToASCII(kAction1).c_str()));
  IntentsDispatcherMock dispatcher(intent);
  controller_->SetIntentsDispatcher(&dispatcher);
  controller_->ShowDialog(kAction1, kType1);
  picker_.Wait();

  TabStripModel* tab_strip = browser()->tab_strip_model();
  EXPECT_EQ(1, tab_strip->count());
  EXPECT_EQ(0, picker_.num_inline_disposition_);
  EXPECT_TRUE(dispatcher.replied_);

  // num_installed_services_ would be 2 if the intent wasn't explicit.
  EXPECT_EQ(0, picker_.num_installed_services_);
}

// Test that explicit intents won't load uninstalled non-extensions.
IN_PROC_BROWSER_TEST_F(WebIntentPickerControllerBrowserTest,
                       ExplicitIntentNonExtensionTest) {
  AddWebIntentService(kAction1, kServiceURL1);
  AddWebIntentService(kAction1, kServiceURL2);
  AddCWSExtensionServiceWithResult(kDummyExtensionId, kAction1, kType1);

  webkit_glue::WebIntentData intent;
  intent.action = kAction1;
  intent.type = kType1;
  intent.service = GURL("http://www.uninstalled.com/");
  IntentsDispatcherMock dispatcher(intent);
  controller_->SetIntentsDispatcher(&dispatcher);
  controller_->ShowDialog(kAction1, kType1);
  picker_.Wait();

  TabStripModel* tab_strip = browser()->tab_strip_model();
  EXPECT_EQ(1, tab_strip->count());
  EXPECT_EQ(0, picker_.num_inline_disposition_);

  // num_installed_services_ would be 2 if the intent wasn't explicit.
  EXPECT_EQ(0, picker_.num_installed_services_);
  EXPECT_TRUE(dispatcher.replied_);
}

IN_PROC_BROWSER_TEST_F(WebIntentPickerControllerBrowserTest,
                       DefaultsTest) {
  AddWebIntentService(kAction1, kServiceURL1);
  AddWebIntentService(kAction1, kServiceURL2);
  AddCWSExtensionServiceEmpty(kAction1);

  SetDefaultService(kAction1, kServiceURL1.spec());

  webkit_glue::WebIntentData intent;
  intent.action = kAction1;
  intent.type = kType1;
  IntentsDispatcherMock dispatcher(intent);
  controller_->SetIntentsDispatcher(&dispatcher);

  ui_test_utils::WindowedTabAddedNotificationObserver new_tab_observer((
      content::Source<content::WebContentsDelegate>(browser())));
  controller_->ShowDialog(kAction1, kType1);
  new_tab_observer.Wait();
  WebIntentPickerController* service_controller =
      WebIntentPickerController::FromWebContents(new_tab_observer.GetTab());
  EXPECT_TRUE(service_controller->ShowLocationBarPickerButton());

  EXPECT_EQ(2, picker_.num_installed_services_);

  // The tab is shown immediately without needing to call OnServiceChosen.
  TabStripModel* tab_strip = browser()->tab_strip_model();
  ASSERT_EQ(2, tab_strip->count());
  EXPECT_EQ(GURL(kServiceURL1),
            tab_strip->GetActiveWebContents()->GetURL());

  EXPECT_TRUE(dispatcher.dispatched_);

  CloseCurrentTab();
}

IN_PROC_BROWSER_TEST_F(WebIntentPickerControllerBrowserTest,
                       ChooseAnotherService) {
  AddWebIntentService(kAction1, kServiceURL1);
  AddWebIntentService(kAction1, kServiceURL2);
  AddCWSExtensionServiceEmpty(kAction1);

  // Bring up the picker to get the test-installed services so we can create a
  // default with the right defaulting fingerprint.
  SetDefaultService(kAction1, kServiceURL1.spec());

  webkit_glue::WebIntentData intent;
  intent.action = kAction1;
  intent.type = kType1;
  IntentsDispatcherMock dispatcher(intent);
  controller_->SetIntentsDispatcher(&dispatcher);

  ui_test_utils::WindowedTabAddedNotificationObserver new_tab_observer((
      content::Source<content::WebContentsDelegate>(browser())));
  controller_->ShowDialog(kAction1, kType1);
  new_tab_observer.Wait();
  content::WebContents* service_web_contents = new_tab_observer.GetTab();

  EXPECT_EQ(2, picker_.num_installed_services_);

  // The tab is shown immediately without needing to call OnServiceChosen.
  TabStripModel* tab_strip = browser()->tab_strip_model();
  ASSERT_EQ(2, tab_strip->count());
  EXPECT_EQ(GURL(kServiceURL1),
            tab_strip->GetActiveWebContents()->GetURL());

  // Simulate click on the location bar use-another-service button.
  content::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_TAB_CLOSING,
      content::NotificationService::AllSources());
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(&WebIntentPickerControllerBrowserTest::ClickLocationBarButton,
                 base::Unretained(this),
                 service_web_contents));
  observer.Wait();
  picker_.Wait();

  // The service tab is closed and the picker is shown again
  // on the original tab.
  EXPECT_EQ(1, tab_strip->count());
  EXPECT_EQ(2, picker_.num_installed_services_);
  EXPECT_EQ(0, picker_.num_inline_disposition_);
}
