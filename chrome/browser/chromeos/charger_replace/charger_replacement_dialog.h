// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CHARGER_REPLACE_CHARGER_REPLACEMENT_DIALOG_H_
#define CHROME_BROWSER_CHROMEOS_CHARGER_REPLACE_CHARGER_REPLACEMENT_DIALOG_H_

#include "ui/gfx/native_widget_types.h"
#include "ui/web_dialogs/web_dialog_delegate.h"

namespace chromeos {

class ChargerReplacementHandler;

// Spring charger replacement dialog.
class ChargerReplacementDialog : public ui::WebDialogDelegate {
 public:
  explicit ChargerReplacementDialog(gfx::NativeWindow parent_window);
  virtual ~ChargerReplacementDialog();

  // True if ChargerReplacementDialog should be shown.
  static bool ShouldShowDialog();

  // True if ChargerReplacementDialog is visible.
  static bool IsDialogVisible();

  static void SetFocusOnChargerDialogIfVisible();

  void Show();
  void set_can_close(bool can_close) { can_close_ = can_close; }

 private:
  // ui::WebDialogDelegate implementation.
  virtual ui::ModalType GetDialogModalType() const override;
  virtual base::string16 GetDialogTitle() const override;
  virtual GURL GetDialogContentURL() const override;
  virtual void GetWebUIMessageHandlers(
      std::vector<content::WebUIMessageHandler*>* handlers) const override;
  virtual void GetMinimumDialogSize(gfx::Size* size) const override;
  virtual void GetDialogSize(gfx::Size* size) const override;
  virtual std::string GetDialogArgs() const override;
  virtual bool CanCloseDialog() const override;
  // NOTE: This function deletes this object at the end.
  virtual void OnDialogClosed(const std::string& json_retval) override;
  virtual void OnCloseContents(
      content::WebContents* source, bool* out_close_dialog) override;
  virtual bool ShouldShowDialogTitle() const override;
  virtual bool HandleContextMenu(
      const content::ContextMenuParams& params) override;

  static bool is_window_visible_;
  static gfx::NativeWindow current_window_;

  const gfx::NativeWindow parent_window_;
  bool can_close_;
  ChargerReplacementHandler* charger_replacement_handler_;

  DISALLOW_COPY_AND_ASSIGN(ChargerReplacementDialog);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CHARGER_REPLACE_CHARGER_REPLACEMENT_DIALOG_H_
