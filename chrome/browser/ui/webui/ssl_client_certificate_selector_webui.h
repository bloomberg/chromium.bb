// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SSL_CLIENT_CERTIFICATE_SELECTOR_WEBUI_H_
#define CHROME_BROWSER_UI_WEBUI_SSL_CLIENT_CERTIFICATE_SELECTOR_WEBUI_H_
#pragma once

#include <vector>
#include <string>

#include "chrome/browser/ui/webui/html_dialog_ui.h"
#include "chrome/browser/ssl/ssl_client_auth_observer.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "net/base/ssl_cert_request_info.h"

class TabContentsWrapper;

class SSLClientCertificateSelectorWebUI : public SSLClientAuthObserver,
                                          public HtmlDialogUIDelegate,
                                          content::WebUIMessageHandler {
 public:
  // Static factory method.
  static void ShowDialog(
      TabContentsWrapper* wrapper,
      const net::HttpNetworkSession* network_session,
      net::SSLCertRequestInfo* cert_request_info,
      const base::Callback<void(net::X509Certificate*)>& callback);

 private:
  SSLClientCertificateSelectorWebUI(
      TabContentsWrapper* wrapper,
      const net::HttpNetworkSession* network_session,
      net::SSLCertRequestInfo* cert_request_info,
      const base::Callback<void(net::X509Certificate*)>& callback);
  virtual ~SSLClientCertificateSelectorWebUI();

  // SSLClientAuthObserver methods
  virtual void OnCertSelectedByNotification() OVERRIDE;

  // HtmlDialogUIDelegate methods
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

  // WebUIMessageHandler
  virtual void RegisterMessages() OVERRIDE;

  // Closes the dialog from javascript.
  void CloseDialog();

  // Called by JavaScript to get details for dialog.
  void RequestDetails(const base::ListValue* args);

  // Called by JavaScript to show a certificate.
  void ViewCertificate(const base::ListValue* args);

  TabContentsWrapper* wrapper_;

  DISALLOW_COPY_AND_ASSIGN(SSLClientCertificateSelectorWebUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_SSL_CLIENT_CERTIFICATE_SELECTOR_WEBUI_H_
