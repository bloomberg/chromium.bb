// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/update_recommended_message_box.h"

#include "app/l10n_util.h"
#include "app/message_box_flags.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/pref_names.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "views/controls/message_box_view.h"
#include "views/window/window.h"

////////////////////////////////////////////////////////////////////////////////
// UpdateRecommendedMessageBox, public:

// static
void UpdateRecommendedMessageBox::ShowMessageBox(
    gfx::NativeWindow parent_window) {
  // When the window closes, it will delete itself.
  new UpdateRecommendedMessageBox(parent_window);
}

bool UpdateRecommendedMessageBox::Accept() {
  // Set the flag to restore the last session on shutdown.
  PrefService* pref_service = g_browser_process->local_state();
  pref_service->SetBoolean(prefs::kRestartLastSessionOnShutdown, true);

  BrowserList::CloseAllBrowsersAndExit();

  return true;
}

int UpdateRecommendedMessageBox::GetDialogButtons() const {
  return MessageBoxFlags::DIALOGBUTTON_OK |
         MessageBoxFlags::DIALOGBUTTON_CANCEL;
}

std::wstring UpdateRecommendedMessageBox::GetDialogButtonLabel(
    MessageBoxFlags::DialogButton button) const {
  DCHECK(button == MessageBoxFlags::DIALOGBUTTON_OK ||
         button == MessageBoxFlags::DIALOGBUTTON_CANCEL);
  return button == MessageBoxFlags::DIALOGBUTTON_OK ?
      l10n_util::GetString(IDS_RESTART_AND_UPDATE) :
      l10n_util::GetString(IDS_NOT_NOW);
}

std::wstring UpdateRecommendedMessageBox::GetWindowTitle() const {
  return l10n_util::GetString(IDS_PRODUCT_NAME);
}

void UpdateRecommendedMessageBox::DeleteDelegate() {
  delete this;
}

bool UpdateRecommendedMessageBox::IsModal() const {
  return true;
}

views::View* UpdateRecommendedMessageBox::GetContentsView() {
  return message_box_view_;
}

////////////////////////////////////////////////////////////////////////////////
// UpdateRecommendedMessageBox, private:

UpdateRecommendedMessageBox::UpdateRecommendedMessageBox(
    gfx::NativeWindow parent_window) {
  const int kDialogWidth = 400;
  // Also deleted when the window closes.
  message_box_view_ = new MessageBoxView(
      MessageBoxFlags::kFlagHasMessage | MessageBoxFlags::kFlagHasOKButton,
      l10n_util::GetStringF(IDS_UPDATE_RECOMMENDED,
                            l10n_util::GetString(IDS_PRODUCT_NAME)),
      std::wstring(),
      kDialogWidth);
  views::Window::CreateChromeWindow(parent_window, gfx::Rect(), this)->Show();
}

UpdateRecommendedMessageBox::~UpdateRecommendedMessageBox() {
}
