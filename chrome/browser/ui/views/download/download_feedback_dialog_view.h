// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DOWNLOAD_DOWNLOAD_FEEDBACK_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_DOWNLOAD_DOWNLOAD_FEEDBACK_DIALOG_VIEW_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/common/pref_names.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {
class MessageBoxView;
}

class Profile;

// Asks the user whether s/he wants to participate in the Safe Browsing
// download feedback program. Shown only for downloads marked DANGEROUS_HOST
// or UNCOMMON_DOWNLOAD. The user should only see this dialog once.
class DownloadFeedbackDialogView : public views::DialogDelegate {
 public:
  // Possible values for prefs::kSafeBrowsingDownloadReportingEnabled pref.
  enum DownloadReportingStatus {
    kDialogNotYetShown,
    kDownloadReportingDisabled,  // Set by Cancel().
    kDownloadReportingEnabled,   // Set by Accept().
    kMaxValue
  };

  static void Show(
      gfx::NativeWindow parent_window,
      Profile* profile,
      const base::Callback<void(DownloadReportingStatus)>& callback);

 private:
  DownloadFeedbackDialogView(
      Profile* profile,
      const base::Callback<void(DownloadReportingStatus)>& callback);
  virtual ~DownloadFeedbackDialogView();

  void ReleaseDialogStatusHold();

  // views::DialogDelegate:
  virtual ui::ModalType GetModalType() const OVERRIDE;
  virtual base::string16 GetWindowTitle() const OVERRIDE;
  virtual void DeleteDelegate() OVERRIDE;
  virtual views::Widget* GetWidget() OVERRIDE;
  virtual const views::Widget* GetWidget() const OVERRIDE;
  virtual views::View* GetContentsView() OVERRIDE;
  virtual int GetDefaultDialogButton() const OVERRIDE;
  virtual base::string16 GetDialogButtonLabel(
      ui::DialogButton button) const OVERRIDE;
  virtual bool Cancel() OVERRIDE;
  virtual bool Accept() OVERRIDE;

  Profile* profile_;
  const base::Callback<void(DownloadReportingStatus)> callback_;
  views::MessageBoxView* explanation_box_view_;
  base::string16 title_text_;
  base::string16 ok_button_text_;
  base::string16 cancel_button_text_;

  DISALLOW_COPY_AND_ASSIGN(DownloadFeedbackDialogView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_DOWNLOAD_DOWNLOAD_FEEDBACK_DIALOG_VIEW_H_
