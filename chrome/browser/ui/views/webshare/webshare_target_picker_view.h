// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBSHARE_WEBSHARE_TARGET_PICKER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEBSHARE_WEBSHARE_TARGET_PICKER_VIEW_H_

#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "ui/base/models/table_model.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/window/dialog_delegate.h"

// Dialog that presents the user with a list of share target apps. Allows the
// user to pick one target to be opened and have data passed to it.
//
// NOTE: This dialog has *not* been UI-reviewed, and is being used by an
// in-development feature (Web Share) behind a runtime flag. It should not be
// used by any released code until going through UI review.
class WebShareTargetPickerView : public views::DialogDelegateView,
                                 public ui::TableModel {
 public:
  // |targets| is a list of app titles that will be shown in a list. Calls
  // |close_callback| with SHARE if an app was chosen, or CANCEL if the dialog
  // was cancelled.
  WebShareTargetPickerView(
      const std::vector<base::string16>& targets,
      const base::Callback<void(SharePickerResult)>& close_callback);
  ~WebShareTargetPickerView() override;

  // views::View overrides:
  gfx::Size GetPreferredSize() const override;

  // views::WidgetDelegate overrides:
  ui::ModalType GetModalType() const override;
  base::string16 GetWindowTitle() const override;

  // views::DialogDelegate overrides:
  bool Cancel() override;
  bool Accept() override;
  base::string16 GetDialogButtonLabel(ui::DialogButton button) const override;

  // ui::TableModel overrides:
  int RowCount() override;
  base::string16 GetText(int row, int column_id) override;
  void SetObserver(ui::TableModelObserver* observer) override;

 private:
  const std::vector<base::string16> targets_;

  base::Callback<void(SharePickerResult)> close_callback_;

  DISALLOW_COPY_AND_ASSIGN(WebShareTargetPickerView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBSHARE_WEBSHARE_TARGET_PICKER_VIEW_H_
