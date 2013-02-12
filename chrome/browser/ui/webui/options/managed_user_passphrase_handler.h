// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_MANAGED_USER_PASSPHRASE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_MANAGED_USER_PASSPHRASE_HANDLER_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/options/options_ui.h"

namespace base {
class ListValue;
}

namespace options {

class ManagedUserPassphraseHandler : public OptionsPageUIHandler {
 public:
  ManagedUserPassphraseHandler();
  virtual ~ManagedUserPassphraseHandler();

  // OptionsPageUIHandler implementation.
  virtual void InitializeHandler() OVERRIDE;
  virtual void RegisterMessages() OVERRIDE;
  virtual void GetLocalizedValues(
      base::DictionaryValue* localized_strings) OVERRIDE;

 private:
  // This function is used to set the passphrase of the manager of the managed
  // account. The passphrase is expected as the parameter and is passed in from
  // the UI.
  void SetLocalPassphrase(const base::ListValue* args);

  base::WeakPtrFactory<ManagedUserPassphraseHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ManagedUserPassphraseHandler);
};

}  // namespace options

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_MANAGED_USER_PASSPHRASE_HANDLER_H_
