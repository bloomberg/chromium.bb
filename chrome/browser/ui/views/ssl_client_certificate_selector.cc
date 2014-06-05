// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/ssl_client_certificate_selector.h"

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/certificate_viewer.h"
#include "chrome/browser/ui/views/constrained_window_views.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/table_model.h"
#include "ui/base/models/table_model_observer.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/table/table_view.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_client_view.h"

#if defined(USE_NSS)
#include "chrome/browser/ui/crypto_module_password_dialog_nss.h"
#endif

///////////////////////////////////////////////////////////////////////////////
// CertificateSelectorTableModel:

class CertificateSelectorTableModel : public ui::TableModel {
 public:
  explicit CertificateSelectorTableModel(
      net::SSLCertRequestInfo* cert_request_info);

  // ui::TableModel implementation:
  virtual int RowCount() OVERRIDE;
  virtual base::string16 GetText(int index, int column_id) OVERRIDE;
  virtual void SetObserver(ui::TableModelObserver* observer) OVERRIDE;

 private:
  std::vector<base::string16> items_;

  DISALLOW_COPY_AND_ASSIGN(CertificateSelectorTableModel);
};

CertificateSelectorTableModel::CertificateSelectorTableModel(
    net::SSLCertRequestInfo* cert_request_info) {
  for (size_t i = 0; i < cert_request_info->client_certs.size(); ++i) {
    net::X509Certificate* cert = cert_request_info->client_certs[i].get();
    base::string16 text = l10n_util::GetStringFUTF16(
        IDS_CERT_SELECTOR_TABLE_CERT_FORMAT,
        base::UTF8ToUTF16(cert->subject().GetDisplayName()),
        base::UTF8ToUTF16(cert->issuer().GetDisplayName()));
    items_.push_back(text);
  }
}

int CertificateSelectorTableModel::RowCount() {
  return items_.size();
}

base::string16 CertificateSelectorTableModel::GetText(int index,
                                                      int column_id) {
  DCHECK_EQ(column_id, 0);
  DCHECK_GE(index, 0);
  DCHECK_LT(index, static_cast<int>(items_.size()));

  return items_[index];
}

void CertificateSelectorTableModel::SetObserver(
    ui::TableModelObserver* observer) {
}

///////////////////////////////////////////////////////////////////////////////
// SSLClientCertificateSelector:

SSLClientCertificateSelector::SSLClientCertificateSelector(
    content::WebContents* web_contents,
    const net::HttpNetworkSession* network_session,
    net::SSLCertRequestInfo* cert_request_info,
    const chrome::SelectCertificateCallback& callback)
    : SSLClientAuthObserver(network_session, cert_request_info, callback),
      model_(new CertificateSelectorTableModel(cert_request_info)),
      web_contents_(web_contents),
      table_(NULL),
      view_cert_button_(NULL) {
  DVLOG(1) << __FUNCTION__;
}

SSLClientCertificateSelector::~SSLClientCertificateSelector() {
  table_->SetModel(NULL);
}

void SSLClientCertificateSelector::Init() {
  views::GridLayout* layout = views::GridLayout::CreatePanel(this);
  SetLayoutManager(layout);

  const int column_set_id = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(column_set_id);
  column_set->AddColumn(
      views::GridLayout::FILL, views::GridLayout::FILL,
      1, views::GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, column_set_id);
  base::string16 text = l10n_util::GetStringFUTF16(
      IDS_CLIENT_CERT_DIALOG_TEXT,
      base::ASCIIToUTF16(cert_request_info()->host_and_port.ToString()));
  views::Label* label = new views::Label(text);
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetAllowCharacterBreak(true);
  layout->AddView(label);

  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  // The dimensions of the certificate selector table view, in pixels.
  static const int kTableViewWidth = 400;
  static const int kTableViewHeight = 100;

  CreateCertTable();
  layout->StartRow(1, column_set_id);
  layout->AddView(table_->CreateParentIfNecessary(), 1, 1,
                  views::GridLayout::FILL,
                  views::GridLayout::FILL, kTableViewWidth, kTableViewHeight);

  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  StartObserving();

  ShowWebModalDialogViews(this, web_contents_);

  // Select the first row automatically.  This must be done after the dialog has
  // been created.
  table_->Select(0);
}

net::X509Certificate* SSLClientCertificateSelector::GetSelectedCert() const {
  int selected = table_->FirstSelectedRow();
  if (selected >= 0 &&
      selected < static_cast<int>(cert_request_info()->client_certs.size()))
    return cert_request_info()->client_certs[selected].get();
  return NULL;
}

///////////////////////////////////////////////////////////////////////////////
// SSLClientAuthObserver implementation:

void SSLClientCertificateSelector::OnCertSelectedByNotification() {
  DVLOG(1) << __FUNCTION__;
  GetWidget()->Close();
}

///////////////////////////////////////////////////////////////////////////////
// DialogDelegateView implementation:

bool SSLClientCertificateSelector::CanResize() const {
  return true;
}

base::string16 SSLClientCertificateSelector::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_CLIENT_CERT_DIALOG_TITLE);
}

void SSLClientCertificateSelector::DeleteDelegate() {
  DVLOG(1) << __FUNCTION__;
  delete this;
}

bool SSLClientCertificateSelector::IsDialogButtonEnabled(
    ui::DialogButton button) const {
  if (button == ui::DIALOG_BUTTON_OK)
    return !!GetSelectedCert();
  return true;
}

bool SSLClientCertificateSelector::Cancel() {
  DVLOG(1) << __FUNCTION__;
  StopObserving();
  CertificateSelected(NULL);
  return true;
}

bool SSLClientCertificateSelector::Accept() {
  DVLOG(1) << __FUNCTION__;
  scoped_refptr<net::X509Certificate> cert = GetSelectedCert();
  if (cert) {
    // Remove the observer before we try unlocking, otherwise we might act on a
    // notification while waiting for the unlock dialog, causing us to delete
    // ourself before the Unlocked callback gets called.
    StopObserving();
#if defined(USE_NSS)
    chrome::UnlockCertSlotIfNecessary(
        cert,
        chrome::kCryptoModulePasswordClientAuth,
        cert_request_info()->host_and_port,
        GetWidget()->GetNativeView(),
        base::Bind(&SSLClientCertificateSelector::Unlocked,
                   base::Unretained(this),
                   cert));
#else
    Unlocked(cert);
#endif
    return false;  // Unlocked() will close the dialog.
  }

  return false;
}

views::View* SSLClientCertificateSelector::GetInitiallyFocusedView() {
  return table_;
}

views::View* SSLClientCertificateSelector::CreateExtraView() {
  DCHECK(!view_cert_button_);
  view_cert_button_ = new views::LabelButton(this,
      l10n_util::GetStringUTF16(IDS_PAGEINFO_CERT_INFO_BUTTON));
  view_cert_button_->SetStyle(views::Button::STYLE_BUTTON);
  return view_cert_button_;
}

ui::ModalType SSLClientCertificateSelector::GetModalType() const {
  return ui::MODAL_TYPE_CHILD;
}

///////////////////////////////////////////////////////////////////////////////
// views::ButtonListener implementation:

void SSLClientCertificateSelector::ButtonPressed(
    views::Button* sender, const ui::Event& event) {
  if (sender == view_cert_button_) {
    net::X509Certificate* cert = GetSelectedCert();
    if (cert)
      ShowCertificateViewer(web_contents_,
                            web_contents_->GetTopLevelNativeWindow(),
                            cert);
  }
}

///////////////////////////////////////////////////////////////////////////////
// views::TableViewObserver implementation:
void SSLClientCertificateSelector::OnSelectionChanged() {
  GetDialogClientView()->ok_button()->SetEnabled(!!GetSelectedCert());
}

void SSLClientCertificateSelector::OnDoubleClick() {
  if (Accept())
    GetWidget()->Close();
}

///////////////////////////////////////////////////////////////////////////////
// SSLClientCertificateSelector private methods:

void SSLClientCertificateSelector::CreateCertTable() {
  std::vector<ui::TableColumn> columns;
  columns.push_back(ui::TableColumn());
  table_ = new views::TableView(model_.get(), columns, views::TEXT_ONLY,
                                true /* single_selection */);
  table_->SetObserver(this);
}

void SSLClientCertificateSelector::Unlocked(net::X509Certificate* cert) {
  DVLOG(1) << __FUNCTION__;
  CertificateSelected(cert);
  GetWidget()->Close();
}

namespace chrome {

void ShowSSLClientCertificateSelector(
    content::WebContents* contents,
    const net::HttpNetworkSession* network_session,
    net::SSLCertRequestInfo* cert_request_info,
    const chrome::SelectCertificateCallback& callback) {
  DVLOG(1) << __FUNCTION__ << " " << contents;
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  (new SSLClientCertificateSelector(
       contents, network_session, cert_request_info, callback))->Init();
}

}  // namespace chrome
