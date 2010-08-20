// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_UI_PASSWORDS_REMOVE_ALL_HANDLER_H_
#define CHROME_BROWSER_DOM_UI_PASSWORDS_REMOVE_ALL_HANDLER_H_

#include "chrome/browser/dom_ui/options_ui.h"

// Chrome passwords and exceptions remove all overlay UI handler.
class PasswordsRemoveAllHandler : public OptionsPageUIHandler {
 public:
  PasswordsRemoveAllHandler();
  virtual ~PasswordsRemoveAllHandler();

  // OptionsUIHandler implementation.
  virtual void GetLocalizedValues(DictionaryValue* localized_strings);

  // DOMMessageHandler implementation.
  virtual void RegisterMessages();

 private:
  DISALLOW_COPY_AND_ASSIGN(PasswordsRemoveAllHandler);
};

#endif  // CHROME_BROWSER_DOM_UI_PASSWORDSREMOVE_ALL_HANDLER_H_
