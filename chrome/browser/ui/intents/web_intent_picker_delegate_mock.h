// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_INTENTS_WEB_INTENT_PICKER_DELEGATE_MOCK_H_
#define CHROME_BROWSER_UI_INTENTS_WEB_INTENT_PICKER_DELEGATE_MOCK_H_

#include "chrome/browser/ui/intents/web_intent_picker_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"

class WebIntentPickerDelegateMock : public WebIntentPickerDelegate {
 public:
  WebIntentPickerDelegateMock();
  virtual ~WebIntentPickerDelegateMock();

  MOCK_METHOD2(
      OnServiceChosen,
      void(const GURL& url,
           webkit_glue::WebIntentServiceData::Disposition disposition));
  MOCK_METHOD1(OnInlineDispositionWebContentsCreated,
               void(content::WebContents* web_contents));
  MOCK_METHOD1(OnExtensionInstallRequested, void(const std::string& id));
  MOCK_METHOD2(OnExtensionLinkClicked,
               void(const std::string& id,
                    WindowOpenDisposition disposition));
  MOCK_METHOD1(OnSuggestionsLinkClicked,
               void(WindowOpenDisposition disposition));
  MOCK_METHOD0(OnUserCancelledPickerDialog, void());
  MOCK_METHOD0(OnChooseAnotherService, void());
  MOCK_METHOD0(OnClosing, void());
};

#endif  // CHROME_BROWSER_UI_INTENTS_WEB_INTENT_PICKER_DELEGATE_MOCK_H_
