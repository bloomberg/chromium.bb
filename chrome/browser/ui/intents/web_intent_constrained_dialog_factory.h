// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_INTENTS_WEB_INTENT_CONSTRAINED_DIALOG_FACTORY_H_
#define CHROME_BROWSER_UI_INTENTS_WEB_INTENT_CONSTRAINED_DIALOG_FACTORY_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/ui/intents/web_intent_picker_factory.h"

class TabContents;
class WebIntentPickerDelegate;

// A factory for creating a constrained dialog as the intent picker UI for a
// given TabContents.
class WebIntentConstrainedDialogFactory : public WebIntentPickerFactory {
 public:
  WebIntentConstrainedDialogFactory();
  virtual ~WebIntentConstrainedDialogFactory();

  // WebIntentPickerFactory implementation.
  virtual WebIntentPicker* Create(
      TabContents* tab_contents,
      WebIntentPickerDelegate* delegate) OVERRIDE;
  virtual void ClosePicker(WebIntentPicker* picker) OVERRIDE;

 private:
  // Closes the dialog and destroys it.
  void CloseDialog();

  // A weak pointer to the currently active picker, or NULL if there is no
  // active picker.
  WebIntentPicker* picker_;

  DISALLOW_COPY_AND_ASSIGN(WebIntentConstrainedDialogFactory);
};

#endif  // CHROME_BROWSER_UI_INTENTS_WEB_INTENT_CONSTRAINED_DIALOG_FACTORY_H_
