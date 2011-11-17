// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_INTENTS_WEB_INTENT_PICKER_DELEGATE_H_
#define CHROME_BROWSER_UI_INTENTS_WEB_INTENT_PICKER_DELEGATE_H_
#pragma once

// A class used to notify the delegate when the user has chosen a web intent
// service.
class WebIntentPickerDelegate {
 public:
  // Base destructor.
  virtual ~WebIntentPickerDelegate() {}

  // Callback called when the user has chosen a service.
  virtual void OnServiceChosen(size_t index) = 0;

  // Callback called when the user cancels out of the dialog, whether by closing
  // it manually or otherwise purposefully.
  virtual void OnCancelled() = 0;

  // Callback called when the dialog stops showing.
  virtual void OnClosing() = 0;
};

#endif  // CHROME_BROWSER_UI_INTENTS_WEB_INTENT_PICKER_DELEGATE_H_
