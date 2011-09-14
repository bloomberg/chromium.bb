// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_INTENTS_WEB_INTENT_PICKER_FACTORY_H_
#define CHROME_BROWSER_UI_INTENTS_WEB_INTENT_PICKER_FACTORY_H_
#pragma once

#include "ui/gfx/native_widget_types.h"

class TabContentsWrapper;
class WebIntentPicker;
class WebIntentPickerDelegate;

// Interface to create and close the WebIntentPicker UI.
class WebIntentPickerFactory {
 public:
  virtual ~WebIntentPickerFactory() {}

  // Creates a new WebIntentPicker. The picker is owned by the factory.
  virtual WebIntentPicker* Create(gfx::NativeWindow parent,
                                  TabContentsWrapper* tab_contents,
                                  WebIntentPickerDelegate* delegate) = 0;

  // Closes and destroys the picker.
  virtual void ClosePicker(WebIntentPicker* picker) = 0;
};

#endif  // CHROME_BROWSER_UI_INTENTS_WEB_INTENT_PICKER_FACTORY_H_
