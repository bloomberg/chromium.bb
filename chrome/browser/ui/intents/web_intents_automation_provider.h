// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_INTENTS_WEB_INTENTS_AUTOMATION_PROVIDER_H_
#define CHROME_BROWSER_UI_INTENTS_WEB_INTENTS_AUTOMATION_PROVIDER_H_

#include "chrome/browser/ui/intents/web_intent_picker_controller.h"

// Simulates the contents returned by the CWS when building intents picker
// dialog, when the API key for accessing CWS is not available, such as
// when using automation framework.
// See chrome/browser/automation/testing_automation_provider.h,cc.
class WebIntentsAutomationProvider {
 public:
  // Creates a service provider for |controller|, which must not be NULL.
  explicit WebIntentsAutomationProvider(WebIntentPickerController* controller);
  virtual ~WebIntentsAutomationProvider();

  // Simulates the suggested |extensions| from the Chrome Web Store.
  // The list |extensions| must not be NULL.
  bool FillSuggestedExtensionList(const ListValue* extensions);

  // Sets the saved suggested extensions into the web intents picker.
  void SetSuggestedExtensions();

 private:
  // List of suggested extensions which could be populated from a pyauto test.
  CWSIntentsRegistry::IntentExtensionList suggested_extensions_;

  // A weak pointer of the web intents picker controller which uses this object
  // to build the picker.
  WebIntentPickerController* controller_;

  DISALLOW_COPY_AND_ASSIGN(WebIntentsAutomationProvider);
};

#endif  // CHROME_BROWSER_UI_INTENTS_WEB_INTENTS_AUTOMATION_PROVIDER_H_
