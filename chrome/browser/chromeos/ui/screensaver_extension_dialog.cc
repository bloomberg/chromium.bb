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

void ShowScreensaverDialog() {
  ScreensaverExtensionDialog::ShowScreensaverDialog();
}

void CloseScreensaverDialog() {
  ScreensaverExtensionDialog::CloseScreensaverDialog();
}

}  // namespace browser

// static
void ScreensaverExtensionDialog::ShowScreensaverDialog() {
  if (!g_instance)
    g_instance = new ScreensaverExtensionDialog();
  g_instance->Show();
}

// static
void ScreensaverExtensionDialog::CloseScreensaverDialog() {
  if (g_instance)
    g_instance->Close();
}

ScreensaverExtensionDialog::ScreensaverExtensionDialog()
    : screensaver_extension_(NULL),
      loading_extension_(false) {
}

void ScreensaverExtensionDialog::LoadExtension() {
  // If the helper is not initialized, call us again when it is.
  // We can't get the screensaver path till the helper is ready.
  if (!chromeos::KioskModeSettings::Get()->is_initialized()) {
    chromeos::KioskModeSettings::Get()->Initialize(
        base::Bind(&ScreensaverExtensionDialog::LoadExtension,
                   base::Unretained(this)));
    return;
  }

  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  std::string error;

  scoped_refptr<Extension> screensaver_extension =
      extension_file_util::LoadExtension(
          FilePath(chromeos::KioskModeSettings::Get()->GetScreensaverPath()),
          Extension::COMPONENT,
          Extension::NO_FLAGS,
          &error);

  if (!screensaver_extension) {
    LOG(ERROR) << "Could not load screensaver extension from: "
               << chromeos::KioskModeSettings::Get()->GetScreensaverPath();
    return;
  }

  BrowserThread::PostTask(BrowserThread::UI,
                          FROM_HERE,
                          base::Bind(
                              &ScreensaverExtensionDialog::SetExtensionAndShow,
                              base::Unretained(this),
                              screensaver_extension));
}

void ScreensaverExtensionDialog::SetExtensionAndShow(
    scoped_refptr<Extension> extension) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  screensaver_extension_ = extension;
  loading_extension_ = false;
  Show();
}

void ScreensaverExtensionDialog::Show() {
  // Whenever we're loading the extension, Show() will
  // be called after the load finishes, so return.
  if (loading_extension_)
    return;

  if (!screensaver_extension_) {
    loading_extension_ = true;
    BrowserThread::PostTask(BrowserThread::FILE,
                            FROM_HERE,
                            base::Bind(
                                &ScreensaverExtensionDialog::LoadExtension,
                                base::Unretained(this)));
    return;
  }

  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  Profile* default_profile = ProfileManager::GetDefaultProfile();
  if (default_profile->GetExtensionService()->AddExtension(
          screensaver_extension_)) {
    extension_dialog_ = ExtensionDialog::ShowFullscreen(
        screensaver_extension_->GetFullLaunchURL(),
        default_profile,
        string16(),
        this);
  } else {
    LOG(ERROR) << "Couldn't add screensaver extension to profile.";
  }
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
