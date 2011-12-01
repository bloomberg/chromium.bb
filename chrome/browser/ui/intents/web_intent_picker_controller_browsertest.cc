// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/favicon/favicon_service.h"
#include "chrome/browser/intents/web_intents_registry.h"
#include "chrome/browser/intents/web_intents_registry_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/intents/web_intent_picker.h"
#include "chrome/browser/ui/intents/web_intent_picker_controller.h"
#include "chrome/browser/ui/intents/web_intent_picker_factory.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/public/browser/intents_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/glue/web_intent_service_data.h"

using testing::_;
using testing::DoAll;
using testing::Return;
using testing::SaveArg;

namespace {

const string16 kAction1(ASCIIToUTF16("http://www.example.com/share"));
const string16 kAction2(ASCIIToUTF16("http://www.example.com/foobar"));
const string16 kType(ASCIIToUTF16("image/png"));
const GURL kServiceURL1("http://www.google.com");
const GURL kServiceURL2("http://www.chromium.org");

MATCHER_P(VectorIsOfSize, n, "") {
  return arg.size() == static_cast<size_t>(n);
}

}  // namespace

class WebIntentPickerMock : public WebIntentPicker {
 public:
  WebIntentPickerMock() : num_urls_(0), num_default_icons_(0) {}

  virtual void SetServiceURLs(const std::vector<GURL>& urls) {
    num_urls_ = urls.size();
  }

  virtual void SetServiceIcon(size_t index, const SkBitmap& icon) {}

  virtual void SetDefaultServiceIcon(size_t index) {
    num_default_icons_++;
  }

  virtual void WaitFor(int target_num_urls, int target_num_default_icons) {
    while (num_urls_ != target_num_urls ||
           num_default_icons_ != target_num_default_icons) {
      MessageLoop::current()->PostTask(FROM_HERE, new MessageLoop::QuitTask());
      ui_test_utils::RunAllPendingInMessageLoop();
    }
  }

  virtual void Close() {}

  TabContents* SetInlineDisposition(const GURL& url) { return NULL; }

  int num_urls_;
  int num_default_icons_;
};


class WebIntentPickerFactoryMock : public WebIntentPickerFactory {
 public:
  explicit WebIntentPickerFactoryMock(WebIntentPickerMock* mock)
      : picker_(mock) {}

  virtual WebIntentPicker* Create(Browser* browser,
                                  TabContentsWrapper* wrapper,
                                  WebIntentPickerDelegate* delegate) {
    return picker_;
  }

  virtual void ClosePicker(WebIntentPicker* picker) {
    if (picker_) {
      picker_->Close();
      picker_ = NULL;
    }
  }

  void Close() {
    picker_ = NULL;
  }

  WebIntentPicker* picker_;
};

class IntentsHostMock : public content::IntentsHost {
 public:
  explicit IntentsHostMock(const webkit_glue::WebIntentData& intent)
      : intent_(intent),
        dispatched_(false) {}

  virtual const webkit_glue::WebIntentData& GetIntent() {
    return intent_;
  }

  virtual void DispatchIntent(TabContents* tab_contents) {
    dispatched_ = true;
  }

  virtual void SendReplyMessage(webkit_glue::WebIntentReplyType reply_type,
                                const string16& data) {
  }

  virtual void RegisterReplyNotification(const base::Closure&) {
  }

  webkit_glue::WebIntentData intent_;
  bool dispatched_;
};

class WebIntentPickerControllerBrowserTest : public InProcessBrowserTest {
 protected:
  void AddWebIntentService(const string16& action,
                           const GURL& service_url) {
    webkit_glue::WebIntentServiceData service;
    service.action = action;
    service.type = kType;
    service.service_url = service_url;
    web_data_service_->AddWebIntentService(service);
  }

  void OnSendReturnMessage(WebIntentPickerController* controller) {
    controller->OnSendReturnMessage();
  }

  void OnServiceChosen(WebIntentPickerController* controller, size_t index) {
    controller->OnServiceChosen(index);
  }

  void SetPickerFactory(WebIntentPickerController* controller,
                        WebIntentPickerFactory* factory) {
    controller->picker_factory_.reset(factory);
  }

  WebIntentPickerMock picker_;

  // The picker controller takes ownership.
  WebIntentPickerFactoryMock* picker_factory_;

  WebDataService* web_data_service_;
  FaviconService* favicon_service_;
};

IN_PROC_BROWSER_TEST_F(WebIntentPickerControllerBrowserTest, ChooseService) {
  web_data_service_ =
      browser()->profile()->GetWebDataService(Profile::EXPLICIT_ACCESS);
  AddWebIntentService(kAction1, kServiceURL1);
  AddWebIntentService(kAction1, kServiceURL2);

  favicon_service_ =
      browser()->profile()->GetFaviconService(Profile::EXPLICIT_ACCESS);

  picker_factory_ = new WebIntentPickerFactoryMock(&picker_);
  WebIntentPickerController* controller = browser()->
      GetSelectedTabContentsWrapper()->web_intent_picker_controller();
  SetPickerFactory(controller, picker_factory_);

  controller->ShowDialog(browser(), kAction1, kType);
  picker_.WaitFor(2, 2);
  EXPECT_EQ(2, picker_.num_urls_);
  EXPECT_EQ(2, picker_.num_default_icons_);

  webkit_glue::WebIntentData intent;
  intent.action = ASCIIToUTF16("a");
  intent.type = ASCIIToUTF16("b");
  IntentsHostMock* host = new IntentsHostMock(intent);
  controller->SetIntentsHost(host);

  OnServiceChosen(controller, 1);
  ASSERT_EQ(2, browser()->tab_count());
  EXPECT_EQ(GURL(kServiceURL2),
            browser()->GetSelectedTabContents()->GetURL());

  EXPECT_TRUE(host->dispatched_);

  OnSendReturnMessage(controller);
  ASSERT_EQ(1, browser()->tab_count());
}
