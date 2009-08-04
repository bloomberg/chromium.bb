// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_install_ui.h"

#include <map>

#include "app/l10n_util.h"
#include "base/file_util.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/extensions/theme_preview_infobar_delegate.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/extensions/extension.h"
#include "grit/chromium_strings.h"

#if defined(OS_WIN)
#include "app/win_util.h"
#elif defined(OS_MACOSX)
#include "base/scoped_cftyperef.h"
#include "base/sys_string_conversions.h"
#include <CoreFoundation/CFUserNotification.h>
#endif

ExtensionInstallUI::ExtensionInstallUI(Profile* profile)
    : profile_(profile), ui_loop_(MessageLoop::current()) {
}

void ExtensionInstallUI::ConfirmInstall(CrxInstaller* installer,
                                        Extension* extension,
                                        SkBitmap* install_icon) {
  DCHECK(ui_loop_ == MessageLoop::current());

  // We special-case themes to not show any confirm UI. Instead they are
  // immediately installed, and then we show an infobar (see OnInstallSuccess)
  // to allow the user to revert if they don't like it.
  if (extension->IsTheme()) {
    installer->ContinueInstall();
    return;
  }

#if defined(OS_WIN)
  ShowExtensionInstallPrompt(profile_, installer, extension, install_icon);

#elif defined(OS_MACOSX)
  // TODO(port): Implement nicer UI.
  // Using CoreFoundation to do this dialog is unimaginably lame but will do
  // until the UI is redone.
  scoped_cftyperef<CFStringRef> product_name(
      base::SysWideToCFStringRef(l10n_util::GetString(IDS_PRODUCT_NAME)));
  CFOptionFlags response;
  if (kCFUserNotificationAlternateResponse == CFUserNotificationDisplayAlert(
      0, kCFUserNotificationCautionAlertLevel, NULL, NULL, NULL,
      product_name,
      CFSTR("Are you sure you want to install this extension?\n\n"
           "This is a temporary message and it will be removed when "
           "extensions UI is finalized."),
      NULL, CFSTR("Cancel"), NULL, &response)) {
    installer->AbortInstall();
  } else {
    installer->ContinueInstall();
  }
#else
  // TODO(port): Implement some UI.
  NOTREACHED();
  installer->ContinueInstall();
#endif  // OS_*
}

void ExtensionInstallUI::OnInstallSuccess(Extension* extension) {
  ShowThemeInfoBar(extension);
}

void ExtensionInstallUI::OnInstallFailure(const std::string& error) {
  DCHECK(ui_loop_ == MessageLoop::current());

#if defined(OS_WIN)
  win_util::MessageBox(NULL, UTF8ToWide(error), L"Extension Install Error",
      MB_OK | MB_SETFOREGROUND);
#elif defined(OS_MACOSX)
  // There must be a better way to do this, for all platforms.
  scoped_cftyperef<CFStringRef> message_cf(
      base::SysUTF8ToCFStringRef(error));
  CFOptionFlags response;
  CFUserNotificationDisplayAlert(
      0, kCFUserNotificationCautionAlertLevel, NULL, NULL, NULL,
      CFSTR("Extension Install Error"), message_cf,
      NULL, NULL, NULL, &response);
#else
  LOG(ERROR) << "Extension install failed: " << error.c_str();
  NOTREACHED();
#endif
}

void ExtensionInstallUI::OnOverinstallAttempted(Extension* extension) {
  ShowThemeInfoBar(extension);
}

void ExtensionInstallUI::ShowThemeInfoBar(Extension* extension) {
  if (!extension->IsTheme())
    return;

  Browser* browser = BrowserList::GetLastActiveWithProfile(profile_);
  if (!browser)
    return;

  TabContents* tab_contents = browser->GetSelectedTabContents();
  if (!tab_contents)
    return;

  // First remove any previous theme preview infobar.
  for (int i = 0; i < tab_contents->infobar_delegate_count(); ++i) {
    InfoBarDelegate* delegate = tab_contents->GetInfoBarDelegateAt(i);
    if (delegate->AsThemePreviewInfobarDelegate()) {
      tab_contents->RemoveInfoBar(delegate);
      break;
    }
  }

  // Now add the new one.
  tab_contents->AddInfoBar(new ThemePreviewInfobarDelegate(
      tab_contents, extension->name()));
}
