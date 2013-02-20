// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_MANAGED_USER_PASSPHRASE_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_MANAGED_USER_PASSPHRASE_DIALOG_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "ui/web_dialogs/web_dialog_delegate.h"

class ConstrainedWebDialogDelegate;
class Profile;

namespace content {
class WebContents;
}

namespace base {
class Value;
}

// Called on the UI thread after the Passphrase Dialog was closed. A boolean
// flag is passed which indicates if the authentication was successful or not.
typedef base::Callback<void(bool)> PassphraseCheckedCallback;

// Displays a tab-modal dialog, i.e. a dialog that will block the current page
// but still allow the user to switch to a different page.
// To display the dialog, allocate this object on the heap. It will open the
// dialog from its constructor and then delete itself when the user dismisses
// the dialog.
class ManagedUserPassphraseDialog : public ui::WebDialogDelegate {
 public:
  // Creates a passphrase dialog which will be deleted automatically when the
  // user closes the dialog.
  ManagedUserPassphraseDialog(content::WebContents* web_contents,
                              const PassphraseCheckedCallback& callback);

  // ui::WebDialogDelegate implementation.
  virtual ui::ModalType GetDialogModalType() const OVERRIDE;
  virtual string16 GetDialogTitle() const OVERRIDE;
  virtual GURL GetDialogContentURL() const OVERRIDE;
  virtual void GetWebUIMessageHandlers(
      std::vector<content::WebUIMessageHandler*>* handlers) const OVERRIDE;
  virtual void GetDialogSize(gfx::Size* size) const OVERRIDE;
  virtual std::string GetDialogArgs() const OVERRIDE;
  virtual void OnDialogClosed(const std::string& json_retval) OVERRIDE;
  virtual void OnCloseContents(content::WebContents* source,
                               bool* out_close_dialog) OVERRIDE;
  virtual bool ShouldShowDialogTitle() const OVERRIDE;

 private:
  virtual ~ManagedUserPassphraseDialog();

  // Creates and initializes the WebUIDataSource which is used for this dialog.
  void CreateDataSource(Profile* profile) const;

  PassphraseCheckedCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(ManagedUserPassphraseDialog);
};

#endif  // CHROME_BROWSER_UI_WEBUI_MANAGED_USER_PASSPHRASE_DIALOG_H_
