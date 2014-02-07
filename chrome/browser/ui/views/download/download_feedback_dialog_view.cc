// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/download/download_feedback_dialog_view.h"

#include "base/prefs/pref_service.h"
#include "base/supports_user_data.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/constrained_window_views.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/message_box_view.h"
#include "ui/views/widget/widget.h"

namespace {

const void* kDialogStatusKey = &kDialogStatusKey;

class DialogStatusData : public base::SupportsUserData::Data {
 public:
  DialogStatusData() : currently_shown_(false) {}
  virtual ~DialogStatusData() {}
  bool currently_shown() const { return currently_shown_; }
  void set_currently_shown(bool shown) { currently_shown_ = shown; }
 private:
  bool currently_shown_;
};

}  // namespace

// static
void DownloadFeedbackDialogView::Show(
    gfx::NativeWindow parent_window,
    Profile* profile,
    const base::Callback<void(DownloadReportingStatus)>& callback) {
  // This dialog should only be shown if it hasn't been shown before.
  DCHECK(profile->GetPrefs()->GetInteger(
      prefs::kSafeBrowsingDownloadReportingEnabled) == kDialogNotYetShown);

  // Only one dialog should be shown at a time, so check to see if another one
  // is open. If another one is open, treat this parallel call as if reporting
  // is disabled (to be conservative).
  DialogStatusData* data =
      static_cast<DialogStatusData*>(profile->GetUserData(kDialogStatusKey));
  if (data == NULL) {
    data = new DialogStatusData();
    profile->SetUserData(kDialogStatusKey, data);
  }
  if (data->currently_shown() == false) {
    data->set_currently_shown(true);
    DownloadFeedbackDialogView* window =
        new DownloadFeedbackDialogView(profile, callback);
    CreateBrowserModalDialogViews(window, parent_window)->Show();
  } else {
    callback.Run(kDownloadReportingDisabled);
  }
}

void DownloadFeedbackDialogView::ReleaseDialogStatusHold() {
  DialogStatusData* data =
      static_cast<DialogStatusData*>(profile_->GetUserData(kDialogStatusKey));
  DCHECK(data);
  data->set_currently_shown(false);
}

DownloadFeedbackDialogView::DownloadFeedbackDialogView(
    Profile* profile,
    const base::Callback<void(DownloadReportingStatus)>& callback)
    : profile_(profile),
      callback_(callback),
      explanation_box_view_(new views::MessageBoxView(
          views::MessageBoxView::InitParams(l10n_util::GetStringUTF16(
              IDS_FEEDBACK_SERVICE_DIALOG_EXPLANATION)))),
      title_text_(l10n_util::GetStringUTF16(IDS_FEEDBACK_SERVICE_DIALOG_TITLE)),
      ok_button_text_(l10n_util::GetStringUTF16(
          IDS_FEEDBACK_SERVICE_DIALOG_OK_BUTTON_LABEL)),
      cancel_button_text_(l10n_util::GetStringUTF16(
          IDS_FEEDBACK_SERVICE_DIALOG_CANCEL_BUTTON_LABEL)) {
}

DownloadFeedbackDialogView::~DownloadFeedbackDialogView() {}

int DownloadFeedbackDialogView::GetDefaultDialogButton() const {
  return ui::DIALOG_BUTTON_CANCEL;
}

base::string16 DownloadFeedbackDialogView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  return (button == ui::DIALOG_BUTTON_OK) ?
      ok_button_text_ : cancel_button_text_;
}

bool DownloadFeedbackDialogView::Cancel() {
  profile_->GetPrefs()->SetInteger(
      prefs::kSafeBrowsingDownloadReportingEnabled, kDownloadReportingDisabled);
  ReleaseDialogStatusHold();
  callback_.Run(kDownloadReportingDisabled);
  return true;
}

bool DownloadFeedbackDialogView::Accept() {
  profile_->GetPrefs()->SetInteger(
      prefs::kSafeBrowsingDownloadReportingEnabled, kDownloadReportingEnabled);
  ReleaseDialogStatusHold();
  callback_.Run(kDownloadReportingEnabled);
  return true;
}

ui::ModalType DownloadFeedbackDialogView::GetModalType() const {
  return ui::MODAL_TYPE_WINDOW;
}

base::string16 DownloadFeedbackDialogView::GetWindowTitle() const {
  return title_text_;
}

void DownloadFeedbackDialogView::DeleteDelegate() {
  delete this;
}

views::Widget* DownloadFeedbackDialogView::GetWidget() {
  return explanation_box_view_->GetWidget();
}

const views::Widget* DownloadFeedbackDialogView::GetWidget() const {
  return explanation_box_view_->GetWidget();
}

views::View* DownloadFeedbackDialogView::GetContentsView() {
  return explanation_box_view_;
}
