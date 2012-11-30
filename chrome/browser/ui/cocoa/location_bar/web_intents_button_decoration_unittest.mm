// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#import "chrome/browser/ui/cocoa/location_bar/web_intents_button_decoration.h"

#import "chrome/browser/ui/cocoa/cocoa_profile_test.h"
#include "chrome/browser/ui/intents/web_intent_picker_controller.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_intents_dispatcher.h"
#include "ipc/ipc_message.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/glue/web_intent_data.h"
#include "webkit/glue/web_intent_reply_data.h"

class WebIntentsButtonDecorationTest : public CocoaProfileTest {
 public:
  WebIntentsButtonDecorationTest()
      : decoration_(NULL, [NSFont userFontOfSize:12]) {
  }

  void SetWindowDispositionSource(
      WebIntentPickerController* controller,
      content::WebContents* contents,
      content::WebIntentsDispatcher* dispatcher) {
    controller->SetWindowDispositionSource(contents, dispatcher);
  }

  WebIntentsButtonDecoration decoration_;
};

TEST_F(WebIntentsButtonDecorationTest, IdentifiesWebIntentService) {
  scoped_refptr<content::SiteInstance> instance =
      content::SiteInstance::Create(profile());
  scoped_ptr<content::WebContents> contents(content::WebContents::Create(
      profile(), instance.get(), MSG_ROUTING_NONE, NULL));
  WebIntentPickerController::CreateForWebContents(contents.get());

  decoration_.Update(contents.get());
  EXPECT_FALSE(decoration_.IsVisible());

  webkit_glue::WebIntentData data;
  content::WebIntentsDispatcher* dispatcher =
      content::WebIntentsDispatcher::Create(data);
  WebIntentPickerController* web_intent_picker_controller =
      WebIntentPickerController::FromWebContents(contents.get());
  SetWindowDispositionSource(web_intent_picker_controller,
                             contents.get(), dispatcher);
  web_intent_picker_controller->SetLocationBarPickerButtonIndicated();

  decoration_.Update(contents.get());
  EXPECT_TRUE(decoration_.IsVisible());

  dispatcher->SendReply(webkit_glue::WebIntentReply(
      webkit_glue::WEB_INTENT_REPLY_SUCCESS, string16()));
}
