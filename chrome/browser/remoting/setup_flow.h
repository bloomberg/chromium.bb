// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_REMOTING_SETUP_FLOW_H_
#define CHROME_BROWSER_REMOTING_SETUP_FLOW_H_

#include <string>
#include <vector>

#include "app/l10n_util.h"
#include "base/time.h"
#include "chrome/browser/dom_ui/html_dialog_ui.h"
#include "gfx/native_widget_types.h"
#include "grit/generated_resources.h"

namespace remoting_setup {

// The state machine used by Remoting for setup wizard.
class SetupFlow : public HtmlDialogUIDelegate {
 public:
  virtual ~SetupFlow();

  // Runs a flow from |start| to |end|, and does the work of actually showing
  // the HTML dialog.  |container| is kept up-to-date with the lifetime of the
  // flow (e.g it is emptied on dialog close).
  static SetupFlow* Run(Profile* service);

  // Fills |args| with "user" and "error" arguments by querying |service|.
  static void GetArgsForGaiaLogin(DictionaryValue* args);

  // Triggers a state machine transition to advance_state.
  void Advance();

  // Focuses the dialog.  This is useful in cases where the dialog has been
  // obscured by a browser window.
  void Focus();

  // HtmlDialogUIDelegate implementation.
  // Get the HTML file path for the content to load in the dialog.
  virtual GURL GetDialogContentURL() const {
    return GURL("chrome://remotingresources/setup");
  }

  // HtmlDialogUIDelegate implementation.
  virtual void GetDOMMessageHandlers(
      std::vector<DOMMessageHandler*>* handlers) const;

  // HtmlDialogUIDelegate implementation.
  // Get the size of the dialog.
  virtual void GetDialogSize(gfx::Size* size) const;

  // HtmlDialogUIDelegate implementation.
  // Gets the JSON string input to use when opening the dialog.
  virtual std::string GetDialogArgs() const {
    return dialog_start_args_;
  }

  // HtmlDialogUIDelegate implementation.
  // A callback to notify the delegate that the dialog closed.
  virtual void OnDialogClosed(const std::string& json_retval);

  // HtmlDialogUIDelegate implementation.
  virtual void OnCloseContents(TabContents* source, bool* out_close_dialog) { }

  // HtmlDialogUIDelegate implementation.
  virtual std::wstring GetDialogTitle() const {
    return l10n_util::GetString(IDS_REMOTING_SETUP_DIALOG_TITLE);
  }

  // HtmlDialogUIDelegate implementation.
  virtual bool IsDialogModal() const {
    return false;
  }

  void OnUserClickedCustomize() {
    // TODO(pranavk): Implement this method.
  }

  bool ClickCustomizeOk() {
    // TODO(pranavk): Implement this method.
    return true;
  }

  void ClickCustomizeCancel() {
    // TODO(pranavk): Implement this method.
  }

 private:

  // Use static Run method to get an instance.
  SetupFlow(const std::string& args, Profile* profile);

  DISALLOW_COPY_AND_ASSIGN(SetupFlow);

  std::string dialog_start_args_;  // The args to pass to the initial page.

  Profile* profile_;
};

}  // namespace remoting_setup

#endif  // CHROME_BROWSER_REMOTING_SETUP_FLOW_H_
