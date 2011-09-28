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
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/browser/tab_contents/tab_contents.h"
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
  MOCK_METHOD1(SetServiceURLs, void(const std::vector<GURL>& urls));
  MOCK_METHOD2(SetServiceIcon, void(size_t index, const SkBitmap& icon));
  MOCK_METHOD1(SetDefaultServiceIcon, void(size_t index));
  MOCK_METHOD0(Show, void(void));
  MOCK_METHOD0(Close, void(void));
};

class WebIntentPickerFactoryMock : public WebIntentPickerFactory {
 public:
  MOCK_METHOD3(Create,
               WebIntentPicker*(gfx::NativeWindow parent,
                                TabContentsWrapper* wrapper,
                                WebIntentPickerDelegate* delegate));
  MOCK_METHOD1(ClosePicker, void(WebIntentPicker* picker));
};

class WebIntentPickerControllerBrowserTest : public InProcessBrowserTest {
 protected:
  void AddWebIntentService(const string16& action,
                           const GURL& service_url) {
    WebIntentServiceData web_intent_service_data;
    web_intent_service_data.action = action;
    web_intent_service_data.type = kType;
    web_intent_service_data.service_url = service_url;
    web_data_service_->AddWebIntent(web_intent_service_data);
  }

  void SetPickerExpectations(int expected_service_count,
                             int expected_default_favicons) {
    EXPECT_CALL(*picker_factory_, Create(_, _, _)).
        WillOnce(DoAll(SaveArg<2>(&delegate_), Return(&picker_)));
    EXPECT_CALL(picker_,
                SetServiceURLs(VectorIsOfSize(expected_service_count))).
        Times(1);
    EXPECT_CALL(picker_, SetDefaultServiceIcon(_)).
        Times(expected_default_favicons);
    EXPECT_CALL(*picker_factory_, ClosePicker(_));
  }

  void CheckPendingAsync() {
    if (controller_->pending_async_count() > 0) {
      MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(&WebIntentPickerControllerBrowserTest::CheckPendingAsync,
                     base::Unretained(this)));
      return;
    }

    MessageLoop::current()->Quit();
  }

  void WaitForDialogToShow() {
    CheckPendingAsync();
    MessageLoop::current()->Run();
  }

  WebIntentPickerMock picker_;

  // |controller_| takes ownership.
  WebIntentPickerFactoryMock* picker_factory_;

  scoped_ptr<WebIntentPickerController> controller_;
  WebIntentPickerDelegate* delegate_;
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

  picker_factory_ = new WebIntentPickerFactoryMock();
  controller_.reset(new WebIntentPickerController(
      browser()->GetSelectedTabContentsWrapper(), picker_factory_));

  SetPickerExpectations(2, 2);

  controller_->ShowDialog(NULL, kAction1, kType);
  WaitForDialogToShow();

  delegate_->OnServiceChosen(1);
  ASSERT_EQ(2, browser()->tab_count());
  EXPECT_EQ(GURL(kServiceURL2),
            browser()->GetSelectedTabContents()->GetURL());
}
