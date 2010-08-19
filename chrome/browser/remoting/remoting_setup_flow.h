// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_REMOTING_REMOTING_SETUP_FLOW_H_
#define CHROME_BROWSER_REMOTING_REMOTING_SETUP_FLOW_H_

#include <string>
#include <vector>

#include "app/l10n_util.h"
#include "base/time.h"
#include "chrome/browser/dom_ui/html_dialog_ui.h"
#include "chrome/common/net/gaia/gaia_auth_consumer.h"
#include "chrome/common/net/gaia/gaia_authenticator2.h"
#include "gfx/native_widget_types.h"
#include "grit/generated_resources.h"

class GaiaAuthenticator2;
class RemotingSetupMessageHandler;
class ServiceProcessControl;
class GoogleServiceAuthError;

// The state machine used by Remoting for setup wizard.
class RemotingSetupFlow : public HtmlDialogUIDelegate,
                          public GaiaAuthConsumer {
 public:
  virtual ~RemotingSetupFlow();

  // Runs a flow from |start| to |end|, and does the work of actually showing
  // the HTML dialog.  |container| is kept up-to-date with the lifetime of the
  // flow (e.g it is emptied on dialog close).
  static RemotingSetupFlow* Run(Profile* service);

  // Fills |args| with "user" and "error" arguments by querying |service|.
  static void GetArgsForGaiaLogin(DictionaryValue* args);

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
  virtual void OnDialogClosed(const std::string& json_retval);
  virtual void OnCloseContents(TabContents* source, bool* out_close_dialog) { }
  virtual std::wstring GetDialogTitle() const {
    return l10n_util::GetString(IDS_REMOTING_SETUP_DIALOG_TITLE);
  }
  virtual bool IsDialogModal() const {
    return true;
  }

  // GaiaAuthConsumer implementation.
  virtual void OnClientLoginFailure(
      const GoogleServiceAuthError& error);
  virtual void OnClientLoginSuccess(
      const GaiaAuthConsumer::ClientLoginResult& credentials);
  virtual void OnIssueAuthTokenSuccess(const std::string& service,
                                       const std::string& auth_token);
  virtual void OnIssueAuthTokenFailure(const std::string& service,
                                       const GoogleServiceAuthError& error);

  // Called by RemotingSetupMessageHandler.
  void OnUserSubmittedAuth(const std::string& user,
                           const std::string& password,
                           const std::string& captcha);

 private:
  // Use static Run method to get an instance.
  RemotingSetupFlow(const std::string& args, Profile* profile);

  // Event triggered when the service process was launched.
  void OnProcessLaunched();

  RemotingSetupMessageHandler* message_handler_;

  // The args to pass to the initial page.
  std::string dialog_start_args_;
  Profile* profile_;

  // Fetcher to obtain the Chromoting Directory token.
  scoped_ptr<GaiaAuthenticator2> authenticator_;
  std::string login_;
  std::string remoting_token_;
  std::string sync_token_;

  // Handle to the ServiceProcessControl which talks to the service process.
  ServiceProcessControl* process_control_;

  DISALLOW_COPY_AND_ASSIGN(RemotingSetupFlow);
};

// Open the appropriate setup dialog for the given profile (which can be
// incognito).
void OpenRemotingSetupDialog(Profile* profile);

#endif  // CHROME_BROWSER_REMOTING_REMOTING_SETUP_FLOW_H_
