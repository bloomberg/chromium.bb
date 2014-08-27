// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/signed_certificate_timestamps_views.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/ui/views/constrained_window_views.h"
#include "chrome/browser/ui/views/signed_certificate_timestamp_info_view.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/signed_certificate_timestamp_store.h"
#include "content/public/common/signed_certificate_timestamp_id_and_status.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/combobox_model.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"

namespace {

void SignedCertificateTimestampIDsToList(
    const content::SignedCertificateTimestampIDStatusList& sct_ids_list,
    net::SignedCertificateTimestampAndStatusList* sct_list) {
  for (content::SignedCertificateTimestampIDStatusList::const_iterator it =
           sct_ids_list.begin();
       it != sct_ids_list.end();
       ++it) {
    scoped_refptr<net::ct::SignedCertificateTimestamp> sct;
    content::SignedCertificateTimestampStore::GetInstance()->Retrieve(it->id,
                                                                      &sct);
    sct_list->push_back(
        net::SignedCertificateTimestampAndStatus(sct, it->status));
  }
}

}  // namespace

namespace chrome {

void ShowSignedCertificateTimestampsViewer(
    content::WebContents* web_contents,
    const content::SignedCertificateTimestampIDStatusList& sct_ids_list) {
  net::SignedCertificateTimestampAndStatusList sct_list;
  SignedCertificateTimestampIDsToList(sct_ids_list, &sct_list);
  new SignedCertificateTimestampsViews(web_contents, sct_list);
}

}  // namespace chrome

class SCTListModel : public ui::ComboboxModel {
 public:
  explicit SCTListModel(
      const net::SignedCertificateTimestampAndStatusList& sct_list);
  virtual ~SCTListModel();

  // Overridden from ui::ComboboxModel:
  virtual int GetItemCount() const OVERRIDE;
  virtual base::string16 GetItemAt(int index) OVERRIDE;

 private:
  net::SignedCertificateTimestampAndStatusList sct_list_;

  DISALLOW_COPY_AND_ASSIGN(SCTListModel);
};

SCTListModel::SCTListModel(
    const net::SignedCertificateTimestampAndStatusList& sct_list)
    : sct_list_(sct_list) {}

SCTListModel::~SCTListModel() {}

int SCTListModel::GetItemCount() const { return sct_list_.size(); }

base::string16 SCTListModel::GetItemAt(int index) {
  DCHECK_LT(static_cast<size_t>(index), sct_list_.size());
  std::string origin = l10n_util::GetStringUTF8(
      chrome::ct::SCTOriginToResourceID(*(sct_list_[index].sct.get())));

  std::string status = l10n_util::GetStringUTF8(
      chrome::ct::StatusToResourceID(sct_list_[index].status));

  // This formatting string may be internationalized for RTL, etc.
  return l10n_util::GetStringFUTF16(IDS_SCT_CHOOSER_FORMAT,
                                    base::IntToString16(index + 1),
                                    base::UTF8ToUTF16(origin),
                                    base::UTF8ToUTF16(status));
}

SignedCertificateTimestampsViews::SignedCertificateTimestampsViews(
    content::WebContents* web_contents,
    const net::SignedCertificateTimestampAndStatusList& sct_list)
    : sct_info_view_(NULL),
      sct_list_(sct_list) {
  ShowWebModalDialogViews(this, web_contents);
}

SignedCertificateTimestampsViews::~SignedCertificateTimestampsViews() {}

base::string16 SignedCertificateTimestampsViews::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_SCT_VIEWER_TITLE);
}

int SignedCertificateTimestampsViews::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_CANCEL;
}

ui::ModalType SignedCertificateTimestampsViews::GetModalType() const {
  return ui::MODAL_TYPE_CHILD;
}

void SignedCertificateTimestampsViews::OnPerformAction(
    views::Combobox* combobox) {
  DCHECK_EQ(combobox, sct_selector_box_.get());
  DCHECK_LT(combobox->selected_index(), sct_list_model_->GetItemCount());
  ShowSCTInfo(combobox->selected_index());
}

void SignedCertificateTimestampsViews::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  if (details.is_add && details.child == this)
    Init();
}

void SignedCertificateTimestampsViews::Init() {
  views::GridLayout* layout = views::GridLayout::CreatePanel(this);
  SetLayoutManager(layout);

  const int kSelectorBoxLayoutId = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(kSelectorBoxLayoutId);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                        views::GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, kSelectorBoxLayoutId);
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  // Add SCT selector drop-down list.
  layout->StartRow(0, kSelectorBoxLayoutId);
  sct_list_model_.reset(new SCTListModel(sct_list_));
  sct_selector_box_.reset(new views::Combobox(sct_list_model_.get()));
  sct_selector_box_->set_listener(this);
  sct_selector_box_->set_owned_by_client();
  layout->AddView(sct_selector_box_.get());
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  // Add the SCT info view, displaying information about a specific SCT.
  layout->StartRow(0, kSelectorBoxLayoutId);
  sct_info_view_ = new SignedCertificateTimestampInfoView();
  layout->AddView(sct_info_view_);

  sct_info_view_->SetSignedCertificateTimestamp(*(sct_list_[0].sct.get()),
                                                sct_list_[0].status);
}

void SignedCertificateTimestampsViews::ShowSCTInfo(int sct_index) {
  if ((sct_index < 0) || (static_cast<size_t>(sct_index) > sct_list_.size()))
    return;

  sct_info_view_->SetSignedCertificateTimestamp(
      *(sct_list_[sct_index].sct.get()), sct_list_[sct_index].status);
}

void SignedCertificateTimestampsViews::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  GetWidget()->Close();
}
