// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_INTENTS_WEB_INTENT_PICKER_FACTORY_IMPL_H_
#define CHROME_BROWSER_UI_INTENTS_WEB_INTENT_PICKER_FACTORY_IMPL_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/ui/intents/web_intent_picker_factory.h"

class TabContents;
class WebIntentPickerDelegate;

// A factory for creating the web intent picker UI.
class WebIntentPickerFactoryImpl : public WebIntentPickerFactory {
 public:
  WebIntentPickerFactoryImpl();
  virtual ~WebIntentPickerFactoryImpl();

  // WebIntentPickerFactory implementation.
  virtual WebIntentPicker* Create(
      gfx::NativeWindow parent,
      TabContentsWrapper* wrapper,
      WebIntentPickerDelegate* delegate) OVERRIDE;
  virtual void ClosePicker(WebIntentPicker* picker) OVERRIDE;

 private:
  // Close the picker and destroys it.
  void Close();

  // A weak pointer to the currently active picker, or NULL if there is no
  // active picker.
  WebIntentPicker* picker_;

  DISALLOW_COPY_AND_ASSIGN(WebIntentPickerFactoryImpl);
};

#endif  // CHROME_BROWSER_UI_INTENTS_WEB_INTENT_PICKER_FACTORY_IMPL_H_
