// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SSL_CLIENT_CERTIFICATE_SELECTOR_H_
#define CHROME_BROWSER_UI_VIEWS_SSL_CLIENT_CERTIFICATE_SELECTOR_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/string16.h"
#include "content/browser/ssl/ssl_client_auth_handler.h"
#include "ui/views/controls/table/table_view_observer.h"
#include "ui/views/window/dialog_delegate.h"
#include "views/controls/button/button.h"
#include "views/view.h"

// This header file exists only for testing.  Chrome should access the
// certificate selector only through the cross-platform interface
// chrome/browser/ssl_client_certificate_selector.h.

namespace views {
class TableView;
class TextButton;
}

class CertificateSelectorTableModel;
class ConstrainedWindow;
class TabContentsWrapper;

class SSLClientCertificateSelector : public SSLClientAuthObserver,
                                     public views::DialogDelegateView,
                                     public views::ButtonListener,
                                     public views::TableViewObserver {
 public:
  SSLClientCertificateSelector(
      TabContentsWrapper* wrapper,
      net::SSLCertRequestInfo* cert_request_info,
      SSLClientAuthHandler* delegate);
  virtual ~SSLClientCertificateSelector();

  void Init();

  net::X509Certificate* GetSelectedCert() const;

  // SSLClientAuthObserver implementation:
  virtual void OnCertSelectedByNotification() OVERRIDE;

  // DialogDelegateView:
  virtual bool CanResize() const OVERRIDE;
  virtual string16 GetWindowTitle() const OVERRIDE;
  virtual void DeleteDelegate() OVERRIDE;
  virtual bool IsDialogButtonEnabled(ui::DialogButton button) const OVERRIDE;
  virtual bool Cancel() OVERRIDE;
  virtual bool Accept() OVERRIDE;
  virtual views::View* GetInitiallyFocusedView() OVERRIDE;
  virtual views::View* GetContentsView() OVERRIDE;
  virtual views::View* GetExtraView() OVERRIDE;

  // views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE;

  // views::TableViewObserver:
  virtual void OnSelectionChanged() OVERRIDE;
  virtual void OnDoubleClick() OVERRIDE;

 private:
  void CreateCertTable();
  void CreateViewCertButton();

  scoped_refptr<net::SSLCertRequestInfo> cert_request_info_;

  scoped_refptr<SSLClientAuthHandler> delegate_;

  scoped_ptr<CertificateSelectorTableModel> model_;

  TabContentsWrapper* wrapper_;

  ConstrainedWindow* window_;
  views::TableView* table_;
  views::TextButton* view_cert_button_;
  views::View* view_cert_button_container_;

  DISALLOW_COPY_AND_ASSIGN(SSLClientCertificateSelector);
};

#endif  // CHROME_BROWSER_UI_VIEWS_SSL_CLIENT_CERTIFICATE_SELECTOR_H_
