// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_CLOUD_PRINT_CLOUD_PRINT_SETUP_FLOW_H_
#define CHROME_BROWSER_PRINTING_CLOUD_PRINT_CLOUD_PRINT_SETUP_FLOW_H_

#include <string>
#include <vector>

#include "base/time.h"
#include "chrome/browser/webui/html_dialog_ui.h"
#include "chrome/common/net/gaia/gaia_auth_consumer.h"
#include "chrome/common/net/gaia/gaia_auth_fetcher.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/native_widget_types.h"

class GaiaAuthFetcher;
class CloudPrintServiceProcessHelper;
class CloudPrintSetupMessageHandler;
class ServiceProcessControl;
class GoogleServiceAuthError;
class Browser;

// This class is responsible for showing a cloud print setup dialog
// and perform operations to fill the content of the dialog and handle
// user actions in the dialog.
//
// It is responsible for:
// 1. Showing the setup dialog.
// 2. Providing the URL for the content of the dialog.
// 3. Providing a data source to provide the content HTML files.
// 4. Providing a message handler to handle user actions in the Web UI.
// 5. Responding to actions received in the message handler.
//
// The architecture for WebUI is designed such that only the message handler
// can access the WebUI. This splits the flow control across the message
// handler and this class. In order to centralize all the flow control and
// content in the WebUI, the WebUI object is given to this object by the
// message handler through the Attach(WebUI*) method.
class CloudPrintSetupFlow : public HtmlDialogUIDelegate,
                            public GaiaAuthConsumer {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}
    // Called when the setup dialog is closed.
    virtual void OnDialogClosed() = 0;
  };
  virtual ~CloudPrintSetupFlow();

  // Runs a flow from |start| to |end|, and does the work of actually showing
  // the HTML dialog.  |container| is kept up-to-date with the lifetime of the
  // flow (e.g it is emptied on dialog close).
  static CloudPrintSetupFlow* OpenDialog(Profile* service, Delegate* delegate,
                                         gfx::NativeWindow parent_window);

  // Focuses the dialog.  This is useful in cases where the dialog has been
  // obscured by a browser window.
  void Focus();

  // HtmlDialogUIDelegate implementation.
  virtual GURL GetDialogContentURL() const;
  virtual void GetWebUIMessageHandlers(
      std::vector<WebUIMessageHandler*>* handlers) const;
  virtual void GetDialogSize(gfx::Size* size) const;
  virtual std::string GetDialogArgs() const;
  virtual void OnDialogClosed(const std::string& json_retval);
  virtual void OnCloseContents(TabContents* source, bool* out_close_dialog);
  virtual std::wstring GetDialogTitle() const;
  virtual bool IsDialogModal() const;
  virtual bool ShouldShowDialogTitle() const;

  // GaiaAuthConsumer implementation.
  virtual void OnClientLoginFailure(
      const GoogleServiceAuthError& error);
  virtual void OnClientLoginSuccess(
      const GaiaAuthConsumer::ClientLoginResult& credentials);

 private:
  friend class CloudPrintServiceProcessHelper;
  friend class CloudPrintSetupMessageHandler;

  // Use static Run method to get an instance.
  CloudPrintSetupFlow(const std::string& args, Profile* profile,
                      Delegate* delegate, bool setup_done);

  // Called CloudPrintSetupMessageHandler when a DOM is attached. This method
  // is called when the HTML page is fully loaded. We then operate on this
  // WebUI object directly.
  void Attach(WebUI* web_ui);

  // Called by CloudPrintSetupMessageHandler when user authentication is
  // registered.
  void OnUserSubmittedAuth(const std::string& user,
                           const std::string& password,
                           const std::string& captcha);

  // Called by CloudPrintSetupMessageHandler when the user clicks on various
  // pieces of UI during setup.
  void OnUserClickedLearnMore();
  void OnUserClickedPrintTestPage();

  // The following methods control which iframe is visible.
  void ShowGaiaLogin(const DictionaryValue& args);
  void ShowGaiaSuccessAndSettingUp();
  void ShowGaiaFailed(const GoogleServiceAuthError& error);
  void ShowSetupDone();
  void ExecuteJavascriptInIFrame(const std::wstring& iframe_xpath,
                                 const std::wstring& js);

  // Pointer to the Web UI. This is provided by CloudPrintSetupMessageHandler
  // when attached.
  WebUI* web_ui_;

  // The args to pass to the initial page.
  std::string dialog_start_args_;
  Profile* profile_;

  // Fetcher to obtain the Chromoting Directory token.
  scoped_ptr<GaiaAuthFetcher> authenticator_;
  std::string login_;
  std::string lsid_;

  // Are we in the done state?
  bool setup_done_;

  // Handle to the ServiceProcessControl which talks to the service process.
  ServiceProcessControl* process_control_;
  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(CloudPrintSetupFlow);
};

#endif  // CHROME_BROWSER_PRINTING_CLOUD_PRINT_CLOUD_PRINT_SETUP_FLOW_H_
