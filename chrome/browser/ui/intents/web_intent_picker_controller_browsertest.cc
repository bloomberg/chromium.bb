// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/favicon/favicon_service.h"
#include "chrome/browser/intents/web_intents_registry.h"
#include "chrome/browser/intents/web_intents_registry_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/intents/web_intent_picker.h"
#include "chrome/browser/ui/intents/web_intent_picker_controller.h"
#include "chrome/browser/ui/intents/web_intent_picker_model.h"
#include "chrome/browser/ui/intents/web_intent_picker_model_observer.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_intents_dispatcher.h"
#include "content/test/test_url_fetcher_factory.h"
#include "net/base/escape.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/image/image_util.h"
#include "webkit/glue/web_intent_service_data.h"

namespace {

const string16 kAction1(ASCIIToUTF16("http://www.example.com/share"));
const string16 kAction2(ASCIIToUTF16("http://www.example.com/foobar"));
const string16 kType(ASCIIToUTF16("image/png"));
const GURL kServiceURL1("http://www.google.com");
const GURL kServiceURL2("http://www.chromium.org");
const char kCWSResponseEmpty[] =
  "{\"kind\":\"chromewebstore#itemList\",\"total_items\":0,\"start_index\":0,"
  "\"items\":[]}";

const char kCWSResponseResultFormat[] =
  "{\"kind\":\"chromewebstore#itemList\","
    "\"total_items\":1,"
    "\"start_index\":0,"
    "\"items\":[{"
      "\"kind\":\"chromewebstore#item\","
      "\"id\":\"nhkckhebbbncbkefhcpcgepcgfaclehe\","
      "\"type\":\"APPLICATION\","
      "\"num_ratings\":0,"
      "\"average_rating\":0.0,"
      "\"manifest\": \"{\\n"
        "\\\"update_url\\\":\\"
            "\"http://0.tbhome_staging.dserver.download-qa.td.borg.google.com/"
            "service/update2/crx\\\",\\n  "
        "\\\"name\\\": \\\"Sidd's Intent App\\\",\\n  "
        "\\\"description\\\": \\\"Do stuff\\\",\\n  "
        "\\\"version\\\": \\\"1.2.19\\\",\\n  "
        "\\\"app\\\": {\\n    "
          "\\\"urls\\\": [     \\n    ],\\n    "
          "\\\"launch\\\": {\\n      "
            "\\\"web_url\\\": \\\"http://siddharthasaha.net/\\\"\\n    "
          "}\\n  "
        "},\\n  "
        "\\\"icons\\\": {\\n    \\\"128\\\": \\\"icon128.png\\\"\\n  },\\n  "
        "\\\"permissions\\\":" " [\\n    "
          "\\\"unlimitedStorage\\\",\\n    \\\"notifications\\\"\\n  "
        "],\\n"
        "  \\\"intents\\\": {\\n    "
          "\\\"%s\\\" : {\\n      "
            "\\\"type\\\" : [\\\"%s\\\"],\\n      "
            "\\\"path\\\" : \\\"//services/edit\\\",\\n      "
            "\\\"title\\\" : \\\"Sample Editing Intent\\\",\\n      "
            "\\\"disposition\\\" : \\\"inline\\\"\\n    "
          "}\\n  "
        "}\\n"
      "}\\n\","
      "\"family_safe\":true,"
      "\"icon_url\": \"%s\"}]}";

const char kCWSFakeIconURLFormat[] = "http://example.com/%s/icon.png";

class DummyURLFetcherFactory : public content::URLFetcherFactory {
 public:
   DummyURLFetcherFactory() {}
   virtual ~DummyURLFetcherFactory() {}

   virtual content::URLFetcher* CreateURLFetcher(
       int id,
       const GURL& url,
       content::URLFetcher::RequestType request_type,
       content::URLFetcherDelegate* d) OVERRIDE {
     return new TestURLFetcher(id, url, d);
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
        message_loop_started_(false),
        pending_async_completed_(false) {
  }

  virtual void OnModelChanged(WebIntentPickerModel* model) OVERRIDE {
    num_installed_services_ =
        static_cast<int>(model->GetInstalledServiceCount());
  }

  virtual void OnFaviconChanged(
      WebIntentPickerModel* model, size_t index) OVERRIDE {
    num_icons_changed_++;
  }

  virtual void OnExtensionIconChanged(
      WebIntentPickerModel* model, const string16& extension_id) OVERRIDE {
    num_extension_icons_changed_++;
  }

  virtual void OnInlineDisposition(
      WebIntentPickerModel* model, const GURL& url) OVERRIDE {}
  virtual void Close() OVERRIDE {}

  virtual void OnPendingAsyncCompleted() OVERRIDE {
    pending_async_completed_ = true;

    if (message_loop_started_)
      MessageLoop::current()->Quit();
  }

  void WaitForPendingAsync() {
    if (!pending_async_completed_) {
      message_loop_started_ = true;
      ui_test_utils::RunMessageLoop();
    }
  }

  int num_installed_services_;
  int num_icons_changed_;
  int num_extension_icons_changed_;
  bool message_loop_started_;
  bool pending_async_completed_;
};

class IntentsDispatcherMock : public content::WebIntentsDispatcher {
 public:
  explicit IntentsDispatcherMock(const webkit_glue::WebIntentData& intent)
      : intent_(intent),
        dispatched_(false) {}

  virtual const webkit_glue::WebIntentData& GetIntent() OVERRIDE {
    return intent_;
  }

  virtual void DispatchIntent(content::WebContents* web_contents) OVERRIDE {
    dispatched_ = true;
  }

  virtual void SendReplyMessage(webkit_glue::WebIntentReplyType reply_type,
                                const string16& data) OVERRIDE {
  }

  virtual void RegisterReplyNotification(
      const base::Callback<void(webkit_glue::WebIntentReplyType)>&) OVERRIDE {
  }

  webkit_glue::WebIntentData intent_;
  bool dispatched_;
};

class WebIntentPickerControllerBrowserTest : public InProcessBrowserTest {
 protected:
  typedef WebIntentPickerModel::Disposition Disposition;

  WebIntentPickerControllerBrowserTest() {}

  virtual void SetUpOnMainThread() OVERRIDE {
    // The FakeURLFetcherFactory will return a NULL URLFetcher if a request is
    // created for a URL it doesn't know and there is no default factory.
    // Instead, use this dummy factory to infinitely delay the request.
    default_url_fetcher_factory_.reset(new DummyURLFetcherFactory);
    fake_url_fetcher_factory_.reset(
        new FakeURLFetcherFactory(default_url_fetcher_factory_.get()));

    web_data_service_ =
        browser()->profile()->GetWebDataService(Profile::EXPLICIT_ACCESS);
    favicon_service_ =
        browser()->profile()->GetFaviconService(Profile::EXPLICIT_ACCESS);
    controller_ = browser()->
        GetSelectedTabContentsWrapper()->web_intent_picker_controller();

    controller_->set_picker(&picker_);
    controller_->set_model_observer(&picker_);

    CreateFakeIcon();
  }

  void AddWebIntentService(const string16& action, const GURL& service_url) {
    webkit_glue::WebIntentServiceData service;
    service.action = action;
    service.type = kType;
    service.service_url = service_url;
    web_data_service_->AddWebIntentService(service);
  }

  void AddCWSExtensionServiceEmpty(const string16& action) {
    GURL cws_query_url = CWSIntentsRegistry::BuildQueryURL(action, kType);
    fake_url_fetcher_factory_->SetFakeResponse(cws_query_url.spec(),
        kCWSResponseEmpty, true);
  }

  void AddCWSExtensionServiceWithResult(const string16& action) {
    GURL cws_query_url = CWSIntentsRegistry::BuildQueryURL(action, kType);
    std::string icon_url;
    std::string escaped_action = net::EscapePath(UTF16ToUTF8(action));
    base::SStringPrintf(&icon_url, kCWSFakeIconURLFormat,
        escaped_action.c_str());

    std::string response;
    base::SStringPrintf(&response, kCWSResponseResultFormat,
        UTF16ToUTF8(action).c_str(), UTF16ToUTF8(kType).c_str(),
        icon_url.c_str());
    fake_url_fetcher_factory_->SetFakeResponse(cws_query_url.spec(), response,
        true);

    fake_url_fetcher_factory_->SetFakeResponse(icon_url, icon_response_,
        true);
  }

  void OnSendReturnMessage(
    webkit_glue::WebIntentReplyType reply_type) {
    controller_->OnSendReturnMessage(reply_type);
  }

  void OnServiceChosen(const GURL& url, Disposition disposition) {
    controller_->OnServiceChosen(url, disposition);
  }

  void OnCancelled() {
    controller_->OnCancelled();
  }

  void CreateFakeIcon() {
    gfx::Image image(gfx::test::CreateImage());
    std::vector<unsigned char> image_data;
    bool result = gfx::PNGEncodedDataFromImage(image, &image_data);
    DCHECK(result);

    std::copy(image_data.begin(), image_data.end(),
        std::back_inserter(icon_response_));
  }

  WebIntentPickerMock picker_;
  WebDataService* web_data_service_;
  FaviconService* favicon_service_;
  WebIntentPickerController* controller_;
  scoped_ptr<DummyURLFetcherFactory> default_url_fetcher_factory_;
  scoped_ptr<FakeURLFetcherFactory> fake_url_fetcher_factory_;
  std::string icon_response_;
};

IN_PROC_BROWSER_TEST_F(WebIntentPickerControllerBrowserTest, ChooseService) {
  AddWebIntentService(kAction1, kServiceURL1);
  AddWebIntentService(kAction1, kServiceURL2);
  AddCWSExtensionServiceEmpty(kAction1);

  controller_->ShowDialog(browser(), kAction1, kType);
  picker_.WaitForPendingAsync();
  EXPECT_EQ(2, picker_.num_installed_services_);
  EXPECT_EQ(0, picker_.num_icons_changed_);

  webkit_glue::WebIntentData intent;
  intent.action = ASCIIToUTF16("a");
  intent.type = ASCIIToUTF16("b");
  IntentsDispatcherMock dispatcher(intent);
  controller_->SetIntentsDispatcher(&dispatcher);

  OnServiceChosen(kServiceURL2, WebIntentPickerModel::DISPOSITION_WINDOW);
  ASSERT_EQ(2, browser()->tab_count());
  EXPECT_EQ(GURL(kServiceURL2),
            browser()->GetSelectedWebContents()->GetURL());

  EXPECT_TRUE(dispatcher.dispatched_);

  OnSendReturnMessage(webkit_glue::WEB_INTENT_REPLY_SUCCESS);
  ASSERT_EQ(1, browser()->tab_count());
}

IN_PROC_BROWSER_TEST_F(WebIntentPickerControllerBrowserTest,
                       FetchExtensionIcon) {
  AddWebIntentService(kAction1, kServiceURL1);
  AddWebIntentService(kAction1, kServiceURL2);
  AddCWSExtensionServiceWithResult(kAction1);

  controller_->ShowDialog(browser(), kAction1, kType);
  picker_.WaitForPendingAsync();
  EXPECT_EQ(2, picker_.num_installed_services_);
  EXPECT_EQ(0, picker_.num_icons_changed_);
  EXPECT_EQ(1, picker_.num_extension_icons_changed_);
}

IN_PROC_BROWSER_TEST_F(WebIntentPickerControllerBrowserTest, OpenCancelOpen) {
  AddWebIntentService(kAction1, kServiceURL1);
  AddWebIntentService(kAction1, kServiceURL2);
  AddCWSExtensionServiceEmpty(kAction1);

  controller_->ShowDialog(browser(), kAction1, kType);
  picker_.WaitForPendingAsync();
  OnCancelled();

  controller_->ShowDialog(browser(), kAction1, kType);
  OnCancelled();
}

IN_PROC_BROWSER_TEST_F(WebIntentPickerControllerBrowserTest,
                       CloseTargetTabReturnToSource) {
  AddWebIntentService(kAction1, kServiceURL1);
  AddCWSExtensionServiceEmpty(kAction1);

  GURL original = browser()->GetSelectedWebContents()->GetURL();

  // Open a new page, but keep focus on original.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabURL), NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  ASSERT_EQ(2, browser()->tab_count());
  EXPECT_EQ(original, browser()->GetSelectedWebContents()->GetURL());

  controller_->ShowDialog(browser(), kAction1, kType);
  picker_.WaitForPendingAsync();
  EXPECT_EQ(1, picker_.num_installed_services_);

  webkit_glue::WebIntentData intent;
  intent.action = ASCIIToUTF16("a");
  intent.type = ASCIIToUTF16("b");
  IntentsDispatcherMock dispatcher(intent);
  controller_->SetIntentsDispatcher(&dispatcher);

  OnServiceChosen(kServiceURL1, WebIntentPickerModel::DISPOSITION_WINDOW);
  ASSERT_EQ(3, browser()->tab_count());
  EXPECT_EQ(GURL(kServiceURL1),
            browser()->GetSelectedWebContents()->GetURL());

  EXPECT_TRUE(dispatcher.dispatched_);

  OnSendReturnMessage(webkit_glue::WEB_INTENT_REPLY_SUCCESS);
  ASSERT_EQ(2, browser()->tab_count());
  EXPECT_EQ(original, browser()->GetSelectedWebContents()->GetURL());
}
