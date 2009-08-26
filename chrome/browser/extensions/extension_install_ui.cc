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

void ExtensionInstallUI::ConfirmInstall(Delegate* delegate,
                                        Extension* extension,
                                        SkBitmap* install_icon) {
  DCHECK(ui_loop_ == MessageLoop::current());

  // We special-case themes to not show any confirm UI. Instead they are
  // immediately installed, and then we show an infobar (see OnInstallSuccess)
  // to allow the user to revert if they don't like it.
  if (extension->IsTheme()) {
    // Remember the current theme in case the user pressed undo.
    Extension* previous_theme = profile_->GetTheme();
    if (previous_theme)
      previous_theme_id_ = previous_theme->id();

    delegate->ContinueInstall();
    return;
  }

#if defined(OS_WIN) || defined(OS_LINUX)
  ShowExtensionInstallPrompt(profile_, delegate, extension, install_icon);

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
    delegate->AbortInstall();
  } else {
    delegate->ContinueInstall();
  }
#else
  // TODO(port): Implement some UI.
  NOTREACHED();
  delegate->ContinueInstall();
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
  GtkWidget* dialog = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL,
      GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "%s", error.c_str());
  gtk_dialog_run(GTK_DIALOG(dialog));
  gtk_widget_destroy(dialog);
#endif
}

void ExtensionInstallUI::OnOverinstallAttempted(Extension* extension) {
  ShowThemeInfoBar(extension);
}

void ExtensionInstallUI::ShowThemeInfoBar(Extension* new_theme) {
  if (!new_theme->IsTheme())
    return;

  Browser* browser = BrowserList::GetLastActiveWithProfile(profile_);
  if (!browser)
    return;

  TabContents* tab_contents = browser->GetSelectedTabContents();
  if (!tab_contents)
    return;

  // First find any previous theme preview infobars.
  InfoBarDelegate* old_delegate = NULL;
  for (int i = 0; i < tab_contents->infobar_delegate_count(); ++i) {
    InfoBarDelegate* delegate = tab_contents->GetInfoBarDelegateAt(i);
    if (delegate->AsThemePreviewInfobarDelegate()) {
      old_delegate = delegate;
      break;
    }
  }

  // Then either replace that old one or add a new one.
  InfoBarDelegate* new_delegate = new ThemePreviewInfobarDelegate(tab_contents,
      new_theme->name(), previous_theme_id_);

  if (old_delegate)
    tab_contents->ReplaceInfoBar(old_delegate, new_delegate);
  else
    tab_contents->AddInfoBar(new_delegate);
}
