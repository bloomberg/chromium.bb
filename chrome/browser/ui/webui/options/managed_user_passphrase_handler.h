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
  // Sets the passphrase of the manager of the managed account. The
  // passphrase is expected as the parameter and is passed in from the UI.
  void SetLocalPassphrase(const base::ListValue* args);

  // Changes the elevation state of the managed user. Before changing the
  // elevation state to true, the passphrase dialog is displayed where the
  // manager of the managed account can enter the passphrase which allows him
  // to modify the settings. It expects as parameter the new elevation state.
  void SetElevated(const base::ListValue* args);

  // Calls the UI with the result of the authentication. |success| is true if
  // the authentication was successful.
  void PassphraseDialogCallback(bool success);

  // Checks if there is already a passphrase specified. It expects as parameter
  // the name of the Javascript function which should be called with the results
  // of this check.
  void IsPassphraseSet(const base::ListValue* args);

  // Resets the passphrase to the empty string.
  void ResetPassphrase(const base::ListValue* args);

  // The name of the Javascript function which should be called after the
  // passphrase has been checked.
  std::string callback_function_name_;
  base::WeakPtrFactory<ManagedUserPassphraseHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ManagedUserPassphraseHandler);
};

}  // namespace options

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_MANAGED_USER_PASSPHRASE_HANDLER_H_
