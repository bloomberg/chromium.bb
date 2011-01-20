// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/tab_contents_ssl_helper.h"

#include "app/l10n_util.h"
#include "base/basictypes.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/certificate_viewer.h"
#include "chrome/browser/ssl/ssl_add_cert_handler.h"
#include "chrome/browser/ssl/ssl_client_auth_handler.h"
#include "chrome/browser/ssl_client_certificate_selector.h"
#include "chrome/browser/tab_contents/infobar_delegate.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_source.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "net/base/net_errors.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

SkBitmap* GetCertIcon() {
  // TODO(davidben): use a more appropriate icon.
  return ResourceBundle::GetSharedInstance().GetBitmapNamed(
      IDR_INFOBAR_SAVE_PASSWORD);
}

class SSLCertAddedInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  SSLCertAddedInfoBarDelegate(TabContents* tab_contents,
                              net::X509Certificate* cert)
      : ConfirmInfoBarDelegate(tab_contents),
        tab_contents_(tab_contents),
        cert_(cert) {
  }

  virtual ~SSLCertAddedInfoBarDelegate() {
  }

  // Overridden from ConfirmInfoBarDelegate:
  virtual string16 GetMessageText() const {
    // TODO(evanm): GetDisplayName should return UTF-16.
    return l10n_util::GetStringFUTF16(
        IDS_ADD_CERT_SUCCESS_INFOBAR_LABEL,
        UTF8ToUTF16(cert_->issuer().GetDisplayName()));
  }

  virtual SkBitmap* GetIcon() const {
    return GetCertIcon();
  }

  virtual int GetButtons() const {
    return BUTTON_OK;
  }

  virtual string16 GetButtonLabel(InfoBarButton button) const {
    switch (button) {
      case BUTTON_OK:
        return l10n_util::GetStringUTF16(IDS_ADD_CERT_SUCCESS_INFOBAR_BUTTON);
      default:
        return string16();
    }
  }

  virtual Type GetInfoBarType() {
    return PAGE_ACTION_TYPE;
  }

  virtual bool Accept() {
    ShowCertificateViewer(tab_contents_->GetMessageBoxRootWindow(), cert_);
    return false;  // Hiding the infobar just as the dialog opens looks weird.
  }

  virtual void InfoBarClosed() {
    // ConfirmInfoBarDelegate doesn't delete itself.
    delete this;
  }

 private:
  // The TabContents we are attached to
  TabContents* tab_contents_;
  // The cert we added.
  scoped_refptr<net::X509Certificate> cert_;
};

}  // namespace

class TabContentsSSLHelper::SSLAddCertData : public NotificationObserver {
 public:
  SSLAddCertData(TabContents* tab, SSLAddCertHandler* handler)
      : tab_(tab),
        handler_(handler),
        infobar_delegate_(NULL) {
    // Listen for disappearing InfoBars.
    Source<TabContents> tc_source(tab_);
    registrar_.Add(this, NotificationType::TAB_CONTENTS_INFOBAR_REMOVED,
                   tc_source);
    registrar_.Add(this, NotificationType::TAB_CONTENTS_INFOBAR_REPLACED,
                   tc_source);
  }
  ~SSLAddCertData() {}

  // Displays |delegate| as an infobar in |tab_|, replacing our current one if
  // still active.
  void ShowInfoBar(InfoBarDelegate* delegate) {
    if (infobar_delegate_) {
      tab_->ReplaceInfoBar(infobar_delegate_, delegate);
    } else {
      tab_->AddInfoBar(delegate);
    }
    infobar_delegate_ = delegate;
  }

  void ShowErrorInfoBar(const string16& message) {
    ShowInfoBar(
        new SimpleAlertInfoBarDelegate(tab_, message, GetCertIcon(), true));
  }

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    switch (type.value) {
      case NotificationType::TAB_CONTENTS_INFOBAR_REMOVED:
        InfoBarClosed(Details<InfoBarDelegate>(details).ptr());
        break;
      case NotificationType::TAB_CONTENTS_INFOBAR_REPLACED:
        typedef std::pair<InfoBarDelegate*, InfoBarDelegate*>
            InfoBarDelegatePair;
        InfoBarClosed(Details<InfoBarDelegatePair>(details).ptr()->first);
        break;
      default:
        NOTREACHED();
        break;
    }
  }

 private:
  void InfoBarClosed(InfoBarDelegate* delegate) {
    if (infobar_delegate_ == delegate)
      infobar_delegate_ = NULL;
  }

  // The TabContents we are attached to.
  TabContents* tab_;
  // The handler we call back to.
  scoped_refptr<SSLAddCertHandler> handler_;
  // The current InfoBarDelegate we're displaying.
  InfoBarDelegate* infobar_delegate_;

  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(SSLAddCertData);
};

TabContentsSSLHelper::TabContentsSSLHelper(TabContents* tab_contents)
    : tab_contents_(tab_contents) {
}

TabContentsSSLHelper::~TabContentsSSLHelper() {
}

void TabContentsSSLHelper::ShowClientCertificateRequestDialog(
    scoped_refptr<SSLClientAuthHandler> handler) {
  browser::ShowSSLClientCertificateSelector(
      tab_contents_, handler->cert_request_info(), handler);
}

void TabContentsSSLHelper::OnVerifyClientCertificateError(
    scoped_refptr<SSLAddCertHandler> handler, int error_code) {
  SSLAddCertData* add_cert_data = GetAddCertData(handler);
  // Display an infobar with the error message.
  // TODO(davidben): Display a more user-friendly error string.
  add_cert_data->ShowErrorInfoBar(
      l10n_util::GetStringFUTF16(IDS_ADD_CERT_ERR_INVALID_CERT,
                                 base::IntToString16(-error_code),
                                 ASCIIToUTF16(net::ErrorToString(error_code))));
}

void TabContentsSSLHelper::AskToAddClientCertificate(
    scoped_refptr<SSLAddCertHandler> handler) {
  NOTREACHED();  // Not implemented yet.
}

void TabContentsSSLHelper::OnAddClientCertificateSuccess(
    scoped_refptr<SSLAddCertHandler> handler) {
  SSLAddCertData* add_cert_data = GetAddCertData(handler);
  // Display an infobar to inform the user.
  add_cert_data->ShowInfoBar(
      new SSLCertAddedInfoBarDelegate(tab_contents_, handler->cert()));
}

void TabContentsSSLHelper::OnAddClientCertificateError(
    scoped_refptr<SSLAddCertHandler> handler, int error_code) {
  SSLAddCertData* add_cert_data = GetAddCertData(handler);
  // Display an infobar with the error message.
  // TODO(davidben): Display a more user-friendly error string.
  add_cert_data->ShowErrorInfoBar(
      l10n_util::GetStringFUTF16(IDS_ADD_CERT_ERR_FAILED,
                                 base::IntToString16(-error_code),
                                 ASCIIToUTF16(net::ErrorToString(error_code))));
}

void TabContentsSSLHelper::OnAddClientCertificateFinished(
    scoped_refptr<SSLAddCertHandler> handler) {
  // Clean up.
  request_id_to_add_cert_data_.erase(handler->network_request_id());
}

TabContentsSSLHelper::SSLAddCertData* TabContentsSSLHelper::GetAddCertData(
    SSLAddCertHandler* handler) {
  // Find/create the slot.
  linked_ptr<SSLAddCertData>& ptr_ref =
      request_id_to_add_cert_data_[handler->network_request_id()];
  // Fill it if necessary.
  if (!ptr_ref.get())
    ptr_ref.reset(new SSLAddCertData(tab_contents_, handler));
  return ptr_ref.get();
}
