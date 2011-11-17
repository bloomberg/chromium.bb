// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/favicon/favicon_service.h"
#include "chrome/browser/intents/web_intents_registry.h"
#include "chrome/browser/intents/web_intents_registry_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/intents/web_intent_picker.h"
#include "chrome/browser/ui/intents/web_intent_picker_controller.h"
#include "chrome/browser/ui/intents/web_intent_picker_factory.h"
#include "chrome/browser/ui/tab_contents/test_tab_contents_wrapper.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/test/base/testing_profile.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/tab_contents/test_tab_contents.h"
#include "content/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gfx/codec/png_codec.h"
#include "webkit/glue/web_intent_service_data.h"

using content::BrowserThread;
using testing::_;
using testing::AtMost;
using testing::DoAll;
using testing::Invoke;
using testing::Return;
using testing::SaveArg;

namespace {

const string16 kAction1(ASCIIToUTF16("http://www.example.com/share"));
const string16 kAction2(ASCIIToUTF16("http://www.example.com/foobar"));
const string16 kType(ASCIIToUTF16("image/png"));
const GURL kServiceURL1("http://www.google.com");
const GURL kServiceURL2("http://www.chromium.org");
const GURL kServiceURL3("http://www.test.com");

// Fill the given bmp with valid png data.
void FillDataToBitmap(int w, int h, SkBitmap* bmp) {
  bmp->setConfig(SkBitmap::kARGB_8888_Config, w, h);
  bmp->allocPixels();

  unsigned char* src_data =
      reinterpret_cast<unsigned char*>(bmp->getAddr32(0, 0));
  for (int i = 0; i < w * h; i++) {
    src_data[i * 4 + 0] = static_cast<unsigned char>(i % 255);
    src_data[i * 4 + 1] = static_cast<unsigned char>(i % 255);
    src_data[i * 4 + 2] = static_cast<unsigned char>(i % 255);
    src_data[i * 4 + 3] = static_cast<unsigned char>(i % 255);
  }
}

// Fill the given data buffer with valid png data.
void FillBitmap(int w, int h, std::vector<unsigned char>* output) {
  SkBitmap bitmap;
  FillDataToBitmap(w, h, &bitmap);
  gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, false, output);
}

GURL MakeFaviconURL(const GURL& url) {
  return GURL(url.spec() + "/favicon.png");
}

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

  TabContents* SetInlineDisposition(const GURL& url) { return NULL; }
};

class WebIntentPickerFactoryMock : public WebIntentPickerFactory {
 public:
  MOCK_METHOD3(Create,
               WebIntentPicker*(Browser* browser,
                                TabContentsWrapper* wrapper,
                                WebIntentPickerDelegate* delegate));
  MOCK_METHOD1(ClosePicker, void(WebIntentPicker* picker));
};

class TestWebIntentPickerController : public WebIntentPickerController {
 public:
  TestWebIntentPickerController(TabContentsWrapper* wrapper,
                                WebIntentPickerFactory* factory)
      : WebIntentPickerController(wrapper, factory) {
  }

  MOCK_METHOD1(OnServiceChosen, void(size_t index));
  MOCK_METHOD0(OnCancelled, void(void));
  MOCK_METHOD0(OnClosing, void(void));

  // helper functions to forward to the base class.
  void BaseOnServiceChosen(size_t index) {
    WebIntentPickerController::OnServiceChosen(index);
  }

  void BaseOnCancelled() {
    WebIntentPickerController::OnCancelled();
  }

  void BaseOnClosing() {
    WebIntentPickerController::OnClosing();
  }
};

class WebIntentPickerControllerTest : public TabContentsWrapperTestHarness {
 public:
  WebIntentPickerControllerTest()
      : ui_thread_(BrowserThread::UI, &message_loop_),
        db_thread_(BrowserThread::DB, &message_loop_),
        picker_factory_(NULL),
        delegate_(NULL) {
  }

  virtual void SetUp() {
    TabContentsWrapperTestHarness::SetUp();

    profile()->CreateFaviconService();
    profile()->CreateWebDataService(true);
    web_data_service_ = profile()->GetWebDataService(Profile::EXPLICIT_ACCESS);
    favicon_service_ = profile()->GetFaviconService(Profile::EXPLICIT_ACCESS);
    WebIntentsRegistry *registry =
        WebIntentsRegistryFactory::GetForProfile(profile());
    // TODO(groby): We should not require a call to Initialize() here.
    registry->Initialize(web_data_service_, NULL);

    picker_factory_ = new WebIntentPickerFactoryMock();
    controller_.reset(new TestWebIntentPickerController(contents_wrapper(),
                                                        picker_factory_));
  }

  virtual void TearDown() {
    controller_.reset();

    TabContentsWrapperTestHarness::TearDown();
  }

 protected:
  void AddWebIntentService(const string16& action,
                           const GURL& service_url) {
    webkit_glue::WebIntentServiceData service;
    service.action = action;
    service.type = kType;
    service.service_url = service_url;
    web_data_service_->AddWebIntentService(service);
  }

  void AddFaviconForURL(const GURL& url) {
    std::vector<unsigned char> image_data;
    FillBitmap(16, 16, &image_data);

    favicon_service_->SetFavicon(url,
                                 MakeFaviconURL(url),
                                 image_data,
                                 history::FAVICON);
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
  }

  void CheckPendingAsync() {
    if (controller_->pending_async_count() > 0) {
      MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(&WebIntentPickerControllerTest::CheckPendingAsync,
                     base::Unretained(this)));
      return;
    }

    MessageLoop::current()->Quit();
  }

  void WaitForDialogToShow() {
    CheckPendingAsync();
    MessageLoop::current()->Run();
  }

  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread db_thread_;
  WebIntentPickerMock picker_;

  // |controller_| takes ownership.
  WebIntentPickerFactoryMock* picker_factory_;

  scoped_ptr<TestWebIntentPickerController> controller_;
  WebIntentPickerDelegate* delegate_;
  WebDataService* web_data_service_;
  FaviconService* favicon_service_;
};

TEST_F(WebIntentPickerControllerTest, ShowDialogWith3Services) {
  SetPickerExpectations(3, 3);
  AddWebIntentService(kAction1, kServiceURL1);
  AddWebIntentService(kAction1, kServiceURL2);
  AddWebIntentService(kAction1, kServiceURL3);

  controller_->ShowDialog(NULL, kAction1, kType);
  WaitForDialogToShow();
}

TEST_F(WebIntentPickerControllerTest, ShowDialogWithNoServices) {
  SetPickerExpectations(0, 0);

  EXPECT_CALL(picker_, SetServiceIcon(_, _)).Times(0);

  controller_->ShowDialog(NULL, kAction1, kType);
  WaitForDialogToShow();
}

// TODO(binji) SetServiceIcon isn't called unless I create the HistoryService,
// but when I do, the test hangs...
TEST_F(WebIntentPickerControllerTest, DISABLED_ShowFavicon) {
  SetPickerExpectations(3, 2);
  AddWebIntentService(kAction1, kServiceURL1);
  AddWebIntentService(kAction1, kServiceURL2);
  AddWebIntentService(kAction1, kServiceURL3);
  AddFaviconForURL(kServiceURL1);
  AddFaviconForURL(kServiceURL3);

  EXPECT_CALL(picker_, SetServiceIcon(0, _)).Times(1);
  EXPECT_CALL(picker_, SetDefaultServiceIcon(1)).Times(1);
  EXPECT_CALL(picker_, SetServiceIcon(2, _)).Times(1);

  controller_->ShowDialog(NULL, kAction1, kType);
  WaitForDialogToShow();
}

TEST_F(WebIntentPickerControllerTest, Cancel) {
  SetPickerExpectations(2, 2);
  AddWebIntentService(kAction1, kServiceURL1);
  AddWebIntentService(kAction1, kServiceURL2);

  EXPECT_CALL(*controller_, OnServiceChosen(0))
      .Times(0);
  EXPECT_CALL(*controller_, OnCancelled()).Times(0);
  EXPECT_CALL(*controller_, OnClosing())
      .WillOnce(Invoke(controller_.get(),
                       &TestWebIntentPickerController::BaseOnClosing));

  controller_->ShowDialog(NULL, kAction1, kType);
  WaitForDialogToShow();
  delegate_->OnClosing();
}
