// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/intents/web_intent_picker.h"
#include "chrome/browser/ui/intents/web_intent_picker_controller.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_intents_dispatcher.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/glue/web_intent_data.h"

class WebIntentPickerBrowserTest : public ExtensionBrowserTest {
 public:
  WebIntentPickerBrowserTest()
      : replied_(false),
        reply_type_(webkit_glue::WEB_INTENT_REPLY_INVALID) {}

  // Install two extensions that handle a share intent --
  // one inline, the other window disposition.
  virtual void InstallExtensions() {
    DCHECK(browser());
    DCHECK(browser()->profile());
    EXPECT_TRUE(InstallExtension(test_data_dir_.AppendASCII(
        "intents/nnhendkbgefomfgdlnmfhhmihihlljpi.crx"), 1));
    EXPECT_TRUE(InstallExtension(test_data_dir_.AppendASCII(
        "intents/ooodacpbmglpoagccnepcbfhfhpdgddn.crx"), 1));
  }

  void WebIntentReplied(webkit_glue::WebIntentReplyType reply_type) {
    replied_ = true;
    reply_type_ = reply_type;
  }

  // Creates a web intents dispatcher and hooks it up to notify the test when it
  // returns.
  content::WebIntentsDispatcher* GetDispatcher(
      const webkit_glue::WebIntentData& data) {
    content::WebIntentsDispatcher* dispatcher =
        content::WebIntentsDispatcher::Create(data);
    dispatcher->RegisterReplyNotification(
        base::Bind(&WebIntentPickerBrowserTest::WebIntentReplied,
                   base::Unretained(this)));
    return dispatcher;
  }

  WebIntentPicker* GetPicker(WebIntentPickerController* controller) {
    return controller->picker_;
  }

  bool PickerShown(WebIntentPickerController* controller) {
    return controller->picker_shown_;
  }

  void Reset(WebIntentPickerController* controller) {
    controller->Reset();
  }

  webkit_glue::WebIntentReplyType WaitForReply() {
    while (!replied_)
      content::RunAllPendingInMessageLoop();
    return reply_type_;
  }

  // Synchronously closes the tab that's currently open.
  void CloseCurrentTab() {
    content::WindowedNotificationObserver tab_close_observer(
        content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
        content::NotificationService::AllSources());
    chrome::CloseTab(browser());
    tab_close_observer.Wait();
  }

  bool replied_;
  webkit_glue::WebIntentReplyType reply_type_;
};

// TODO(rouslan): Enable this test after code patch.
IN_PROC_BROWSER_TEST_F(WebIntentPickerBrowserTest, DISABLED_TestShowPicker) {
  InstallExtensions();

  webkit_glue::WebIntentData data(ASCIIToUTF16("http://webintents.org/share"),
                                  ASCIIToUTF16("text/uri-list"),
                                  ASCIIToUTF16("payload"));
  content::WebIntentsDispatcher* dispatcher = GetDispatcher(data);
  content::WebContents* web_contents = chrome::GetActiveWebContents(browser());
  DCHECK(web_contents);
  web_contents->GetDelegate()->WebIntentDispatch(web_contents, dispatcher);

  WebIntentPickerController* controller =
      WebIntentPickerController::FromWebContents(web_contents);
  while (GetPicker(controller) == NULL)
    content::RunAllPendingInMessageLoop();

  EXPECT_TRUE(PickerShown(controller));

  CloseCurrentTab();

  // Picker lifetime controls should tolerate destruction of the tab (and
  // controller and model) it is running on.
}

// TODO(groby): Enable this test after code patch.
IN_PROC_BROWSER_TEST_F(WebIntentPickerBrowserTest,
                       DISABLED_TestUpdateAfterClose) {
  InstallExtensions();

  webkit_glue::WebIntentData data(ASCIIToUTF16("http://webintents.org/share"),
                                  ASCIIToUTF16("text/uri-list"),
                                  ASCIIToUTF16("payload"));
  content::WebIntentsDispatcher* dispatcher = GetDispatcher(data);
  content::WebContents* web_contents = chrome::GetActiveWebContents(browser());
  DCHECK(web_contents);
  web_contents->GetDelegate()->WebIntentDispatch(web_contents, dispatcher);

  WebIntentPickerController* controller =
      WebIntentPickerController::FromWebContents(web_contents);
  while (GetPicker(controller) == NULL)
    content::RunAllPendingInMessageLoop();

  // Picker should get rid of with getting state update messages after a
  // controller reset.
  Reset(controller);
  GetPicker(controller)->OnPendingAsyncCompleted();
}
