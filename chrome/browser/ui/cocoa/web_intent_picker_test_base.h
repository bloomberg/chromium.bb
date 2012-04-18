// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_WEB_INTENT_PICKER_TEST_BASE_H_
#define CHROME_BROWSER_UI_COCOA_WEB_INTENT_PICKER_TEST_BASE_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/intents/web_intent_picker_delegate.h"
#include "chrome/browser/ui/intents/web_intent_picker_model.h"
#include "testing/gmock/include/gmock/gmock.h"

class TabContentsWrapper;
class WebIntentPickerCocoa;
@class WebIntentPickerSheetController;

class MockIntentPickerDelegate : public WebIntentPickerDelegate {
 public:
  MockIntentPickerDelegate();
  virtual ~MockIntentPickerDelegate();

  MOCK_METHOD2(OnServiceChosen, void(const GURL& url, Disposition disposition));
  MOCK_METHOD1(OnInlineDispositionWebContentsCreated,
      void(content::WebContents* web_contents));
  MOCK_METHOD1(OnExtensionInstallRequested, void(const std::string& id));
  MOCK_METHOD1(OnExtensionLinkClicked, void(const std::string& extension_id));
  MOCK_METHOD0(OnSuggestionsLinkClicked, void());
  MOCK_METHOD0(OnCancelled, void());
  MOCK_METHOD0(OnClosing, void());
};

class WebIntentPickerTestBase {
 public:
  WebIntentPickerTestBase();
  ~WebIntentPickerTestBase();

  void CreateBubble(TabContentsWrapper* wrapper);
  WebIntentPickerSheetController* controller_;  // Weak, owns self.
  NSWindow* window_;  // Weak, owned by controller.
  scoped_ptr<WebIntentPickerCocoa> picker_;
  MockIntentPickerDelegate delegate_;
  WebIntentPickerModel model_;  // The model used by the picker
};

#endif  // CHROME_BROWSER_UI_COCOA_WEB_INTENT_PICKER_TEST_BASE_H_
