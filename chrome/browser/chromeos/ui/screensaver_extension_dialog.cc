// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/ui/screensaver_extension_dialog.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/chromeos/kiosk_mode/kiosk_mode_settings.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sessions/restore_tab_helper.h"
#include "chrome/browser/ui/views/extensions/extension_dialog.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_file_util.h"

using content::BrowserThread;

namespace {

ScreensaverExtensionDialog* g_instance = NULL;

}  // namespace

namespace browser {

void ShowScreensaverDialog(scoped_refptr<Extension> extension) {
  ScreensaverExtensionDialog::ShowScreensaverDialog(extension);
}

void CloseScreensaverDialog() {
  ScreensaverExtensionDialog::CloseScreensaverDialog();
}

}  // namespace browser

// static
void ScreensaverExtensionDialog::ShowScreensaverDialog(
    scoped_refptr<Extension> extension) {
  if (!g_instance)
    g_instance = new ScreensaverExtensionDialog(extension);
  g_instance->Show();
}

// static
void ScreensaverExtensionDialog::CloseScreensaverDialog() {
  if (g_instance)
    g_instance->Close();
}

ScreensaverExtensionDialog::ScreensaverExtensionDialog(
    scoped_refptr<Extension> extension)
    : screensaver_extension_(extension) {
}

void ScreensaverExtensionDialog::Show() {
  if (!screensaver_extension_)
    return;

  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  Profile* default_profile = ProfileManager::GetDefaultProfile();
  default_profile->GetExtensionService()->AddExtension(screensaver_extension_);
  extension_dialog_ = ExtensionDialog::ShowFullscreen(
      screensaver_extension_->GetFullLaunchURL(),
      default_profile,
      string16(),
      this);
}

void ScreensaverExtensionDialog::Close() {
  if (extension_dialog_) {
    extension_dialog_->Close();
    extension_dialog_ = NULL;
  }
}

ScreensaverExtensionDialog::~ScreensaverExtensionDialog() {
  if (extension_dialog_)
    extension_dialog_->ObserverDestroyed();
}

void ScreensaverExtensionDialog::ExtensionDialogClosing(
    ExtensionDialog* dialog) {
  // Release our reference to the dialog to allow it to close.
  extension_dialog_ = NULL;
}

void ScreensaverExtensionDialog::ExtensionTerminated(
    ExtensionDialog* dialog) {
  // This needs to be run 'slightly' delayed. When we get the extension
  // terminated notification, the extension isn't fully unloaded yet. There
  // is no good way to get around this. The correct solution will be to
  // not need to reload the extension at all - but the current wiring in
  // ExtensionViewsHost makes that not possible.
  MessageLoop::current()->PostTask(FROM_HERE,
      base::Bind(&ScreensaverExtensionDialog::ReloadAndShow,
          base::Unretained(this)));
  dialog->Close();
}

void ScreensaverExtensionDialog::ReloadAndShow() {
  ProfileManager::GetDefaultProfile()->GetExtensionService()->ReloadExtension(
      screensaver_extension_->id());

  Show();
}
