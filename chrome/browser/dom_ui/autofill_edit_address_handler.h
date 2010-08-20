// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_UI_AUTOFILL_EDIT_ADDRESS_HANDLER_H_
#define CHROME_BROWSER_DOM_UI_AUTOFILL_EDIT_ADDRESS_HANDLER_H_
#pragma once

#include "base/basictypes.h"
#include "chrome/browser/dom_ui/options_ui.h"

// Handler for the 'Edit Address' overlay.
class AutoFillEditAddressHandler : public OptionsPageUIHandler {
 public:
  AutoFillEditAddressHandler();
  virtual ~AutoFillEditAddressHandler();

  // OptionsPageUIHandler implementation.
  virtual void GetLocalizedValues(DictionaryValue* localized_strings);
  virtual void RegisterMessages();

 private:
  DISALLOW_COPY_AND_ASSIGN(AutoFillEditAddressHandler);
};

#endif  // CHROME_BROWSER_DOM_UI_AUTOFILL_EDIT_ADDRESS_HANDLER_H_
