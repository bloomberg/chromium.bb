// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/certificate_selector.h"

#include <stddef.h>  // For size_t.
#include <vector>

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/certificate_viewer.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "content/public/browser/web_contents.h"
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

namespace chrome {

class CertificateSelector::CertificateTableModel : public ui::TableModel {
 public:
  explicit CertificateTableModel(const net::CertificateList& certificates);

  // ui::TableModel:
  int RowCount() override;
  base::string16 GetText(int index, int column_id) override;
  void SetObserver(ui::TableModelObserver* observer) override;

 private:
  std::vector<base::string16> items_;

  DISALLOW_COPY_AND_ASSIGN(CertificateTableModel);
};

CertificateSelector::CertificateTableModel::CertificateTableModel(
    const net::CertificateList& certificates) {
  for (const scoped_refptr<net::X509Certificate>& cert : certificates) {
    items_.push_back(l10n_util::GetStringFUTF16(
        IDS_CERT_SELECTOR_TABLE_CERT_FORMAT,
        base::UTF8ToUTF16(cert->subject().GetDisplayName()),
        base::UTF8ToUTF16(cert->issuer().GetDisplayName())));
  }
}

int CertificateSelector::CertificateTableModel::RowCount() {
  return items_.size();
}

base::string16 CertificateSelector::CertificateTableModel::GetText(
    int index,
    int column_id) {
  DCHECK_EQ(column_id, 0);
  DCHECK_GE(index, 0);
  DCHECK_LT(static_cast<size_t>(index), items_.size());

  return items_[index];
}

void CertificateSelector::CertificateTableModel::SetObserver(
    ui::TableModelObserver* observer) {
}

CertificateSelector::CertificateSelector(
    const net::CertificateList& certificates,
    content::WebContents* web_contents)
    : certificates_(certificates),
      model_(new CertificateTableModel(certificates)),
      web_contents_(web_contents),
      table_(nullptr),
      view_cert_button_(nullptr) {
  CHECK(web_contents_);
}

CertificateSelector::~CertificateSelector() {
  table_->SetModel(nullptr);
}

void CertificateSelector::Show() {
  constrained_window::ShowWebModalDialogViews(this, web_contents_);

  // Select the first row automatically.  This must be done after the dialog has
  // been created.
  table_->Select(0);
}

void CertificateSelector::InitWithText(const base::string16& text) {
  views::GridLayout* const layout = views::GridLayout::CreatePanel(this);
  SetLayoutManager(layout);

  // The dimensions of the certificate selector table view, in pixels.
  const int kTableViewWidth = 400;
  const int kTableViewHeight = 100;

  const int kColumnSetId = 0;
  views::ColumnSet* const column_set = layout->AddColumnSet(kColumnSetId);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                        views::GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, kColumnSetId);
  scoped_ptr<views::Label> label(new views::Label(text));
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetAllowCharacterBreak(true);
  label->SizeToFit(kTableViewWidth);
  layout->AddView(label.release());

  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  std::vector<ui::TableColumn> columns;
  columns.push_back(ui::TableColumn());
  table_ = new views::TableView(model_.get(), columns, views::TEXT_ONLY,
                                true /* single_selection */);
  table_->SetObserver(this);
  layout->StartRow(1, kColumnSetId);
  layout->AddView(table_->CreateParentIfNecessary(), 1, 1,
                  views::GridLayout::FILL, views::GridLayout::FILL,
                  kTableViewWidth, kTableViewHeight);

  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
}

net::X509Certificate* CertificateSelector::GetSelectedCert() const {
  const int selected = table_->FirstSelectedRow();
  if (selected < 0)  // Nothing is selected in |table_|.
    return nullptr;
  CHECK_LT(static_cast<size_t>(selected), certificates_.size());
  return certificates_[selected].get();
}

bool CertificateSelector::CanResize() const {
  return true;
}

base::string16 CertificateSelector::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_CLIENT_CERT_DIALOG_TITLE);
}

bool CertificateSelector::IsDialogButtonEnabled(ui::DialogButton button) const {
  return button != ui::DIALOG_BUTTON_OK || GetSelectedCert() != nullptr;
}

views::View* CertificateSelector::GetInitiallyFocusedView() {
  DCHECK(table_);
  return table_;
}

views::View* CertificateSelector::CreateExtraView() {
  DCHECK(!view_cert_button_);
  scoped_ptr<views::LabelButton> button(new views::LabelButton(
      this, l10n_util::GetStringUTF16(IDS_PAGEINFO_CERT_INFO_BUTTON)));
  button->SetStyle(views::Button::STYLE_BUTTON);
  view_cert_button_ = button.get();
  return button.release();
}

ui::ModalType CertificateSelector::GetModalType() const {
  return ui::MODAL_TYPE_CHILD;
}

void CertificateSelector::ButtonPressed(views::Button* sender,
                                        const ui::Event& event) {
  if (sender == view_cert_button_) {
    net::X509Certificate* const cert = GetSelectedCert();
    if (cert)
      ShowCertificateViewer(web_contents_,
                            web_contents_->GetTopLevelNativeWindow(), cert);
  }
}

void CertificateSelector::OnSelectionChanged() {
  GetDialogClientView()->ok_button()->SetEnabled(GetSelectedCert() != nullptr);
}

void CertificateSelector::OnDoubleClick() {
  if (Accept())
    GetWidget()->Close();
}

}  // namespace chrome
