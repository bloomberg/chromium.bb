// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_install_ui.h"

#include <map>

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/file_util.h"
#include "base/rand_util.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/extensions/theme_installed_infobar_delegate.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/extensions/extension.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"

#if defined(OS_WIN)
#include "app/win_util.h"
#elif defined(OS_MACOSX)
#include "base/scoped_cftyperef.h"
#include "base/sys_string_conversions.h"
#include <CoreFoundation/CFUserNotification.h>
#elif defined(TOOLKIT_GTK)
#include "chrome/browser/extensions/gtk_theme_installed_infobar_delegate.h"
#include "chrome/browser/gtk/gtk_theme_provider.h"
#endif

namespace {

#if defined(OS_WIN) || defined(TOOLKIT_GTK)

static std::wstring GetInstallWarning(Extension* extension) {
  // If the extension has a plugin, it's easy: the plugin has the most severe
  // warning.
  if (!extension->plugins().empty())
    return l10n_util::GetString(IDS_EXTENSION_PROMPT_WARNING_NEW_FULL_ACCESS);

  // Otherwise, we go in descending order of severity: all hosts, several hosts,
  // a single host, no hosts. For each of these, we also have a variation of the
  // message for when api permissions are also requested.
  if (extension->HasAccessToAllHosts()) {
    if (extension->api_permissions().empty())
      return l10n_util::GetString(IDS_EXTENSION_PROMPT_WARNING_NEW_ALL_HOSTS);
    else
      return l10n_util::GetString(
          IDS_EXTENSION_PROMPT_WARNING_NEW_ALL_HOSTS_AND_BROWSER);
  }

  const std::set<std::string> hosts = extension->GetEffectiveHostPermissions();
  if (hosts.size() > 1) {
    if (extension->api_permissions().empty())
      return l10n_util::GetString(
          IDS_EXTENSION_PROMPT_WARNING_NEW_MULTIPLE_HOSTS);
    else
      return l10n_util::GetString(
          IDS_EXTENSION_PROMPT_WARNING_NEW_MULTIPLE_HOSTS_AND_BROWSER);
  }

  if (hosts.size() == 1) {
    if (extension->api_permissions().empty())
      return l10n_util::GetStringF(
          IDS_EXTENSION_PROMPT_WARNING_NEW_SINGLE_HOST,
          UTF8ToWide(*hosts.begin()));
    else
      return l10n_util::GetStringF(
          IDS_EXTENSION_PROMPT_WARNING_NEW_SINGLE_HOST_AND_BROWSER,
          UTF8ToWide(*hosts.begin()));
  }

  DCHECK(hosts.size() == 0);
  if (extension->api_permissions().empty())
    return L"";
  else
    return l10n_util::GetString(IDS_EXTENSION_PROMPT_WARNING_NEW_BROWSER);
}

#endif

}  // namespace

ExtensionInstallUI::ExtensionInstallUI(Profile* profile)
    : profile_(profile), ui_loop_(MessageLoop::current())
#if defined(TOOLKIT_GTK)
    ,previous_use_gtk_theme_(false)
#endif
{
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

#if defined(TOOLKIT_GTK)
    // On linux, we also need to take the user's system settings into account
    // to undo theme installation.
    previous_use_gtk_theme_ =
        GtkThemeProvider::GetFrom(profile_)->UseGtkTheme();
#endif

    delegate->ContinueInstall();
    return;
  }

#if defined(OS_WIN) || defined(TOOLKIT_GTK)
  if (!install_icon) {
    install_icon = ResourceBundle::GetSharedInstance().GetBitmapNamed(
        IDR_DEFAULT_EXTENSION_ICON_128);
  }

  ShowExtensionInstallPrompt(profile_, delegate, extension, install_icon,
                             GetInstallWarning(extension));

#elif defined(OS_MACOSX)
  // TODO(port): Implement nicer UI.
  // Using CoreFoundation to do this dialog is unimaginably lame but will do
  // until the UI is redone.
  scoped_cftyperef<CFStringRef> confirm_title(base::SysWideToCFStringRef(
      l10n_util::GetString(IDS_EXTENSION_PROMPT_TITLE)));

  // Build the confirmation prompt, including a heading, a random humorous
  // warning, and a severe warning.
  const string16& confirm_format(ASCIIToUTF16("$1\n\n$2\n\n$3"));
  std::vector<string16> subst;
  subst.push_back(l10n_util::GetStringFUTF16(IDS_EXTENSION_PROMPT_HEADING,
      UTF8ToUTF16(extension->name())));
  string16 warnings[] = {
    l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_WARNING_1),
    l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_WARNING_2),
    l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_WARNING_3)
  };
  subst.push_back(warnings[base::RandInt(0, arraysize(warnings) - 1)]);
  subst.push_back(l10n_util::GetStringUTF16(
      IDS_EXTENSION_PROMPT_WARNING_SEVERE));
  scoped_cftyperef<CFStringRef> confirm_prompt(base::SysUTF16ToCFStringRef(
      ReplaceStringPlaceholders(confirm_format, subst, NULL)));

  scoped_cftyperef<CFStringRef> confirm_cancel(base::SysWideToCFStringRef(
      l10n_util::GetString(IDS_EXTENSION_PROMPT_CANCEL_BUTTON)));

  CFOptionFlags response;
  CFUserNotificationDisplayAlert(
      0, kCFUserNotificationCautionAlertLevel,
      NULL, // TODO(port): show the install_icon instead of a default.
      NULL, NULL, // Sound URL, localization URL.
      confirm_title,
      confirm_prompt,
      NULL, // Default button.
      confirm_cancel,
      NULL, // Other button.
      &response);
  if (response == kCFUserNotificationAlternateResponse) {
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
      0, kCFUserNotificationNoteAlertLevel, NULL, NULL, NULL,
      CFSTR("Extension Install Error"), message_cf,
      NULL, NULL, NULL, &response);
#else
  GtkWidget* dialog = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL,
      GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "%s", error.c_str());
  g_signal_connect(dialog, "response", G_CALLBACK(gtk_widget_destroy), NULL);
  gtk_widget_show_all(dialog);
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
  InfoBarDelegate* new_delegate =
#if defined(TOOLKIT_GTK)
      new GtkThemeInstalledInfoBarDelegate(
          tab_contents,
          new_theme->name(), previous_theme_id_, previous_use_gtk_theme_);
#else
      new ThemeInstalledInfoBarDelegate(tab_contents,
                                        new_theme->name(), previous_theme_id_);
#endif

  if (old_delegate)
    tab_contents->ReplaceInfoBar(old_delegate, new_delegate);
  else
    tab_contents->AddInfoBar(new_delegate);
}
