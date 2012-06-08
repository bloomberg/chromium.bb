// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_install_ui_default.h"

#include "base/command_line.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/theme_installed_infobar_delegate.h"
#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/simple_message_box.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/webui/ntp/new_tab_ui.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(USE_ASH)
#include "ash/shell.h"
#endif

using content::BrowserThread;
using content::WebContents;
using extensions::Extension;

namespace {

bool disable_failure_ui_for_tests = false;

}  // namespace

ExtensionInstallUIDefault::ExtensionInstallUIDefault(Profile* profile)
    : profile_(profile),
      skip_post_install_ui_(false),
      previous_using_native_theme_(false),
      use_app_installed_bubble_(false) {
  // Remember the current theme in case the user presses undo.
  if (profile) {
    const Extension* previous_theme =
        ThemeServiceFactory::GetThemeForProfile(profile);
    if (previous_theme)
      previous_theme_id_ = previous_theme->id();
    previous_using_native_theme_ =
        ThemeServiceFactory::GetForProfile(profile)->UsingNativeTheme();
  }
}

ExtensionInstallUIDefault::~ExtensionInstallUIDefault() {
}

void ExtensionInstallUIDefault::OnInstallSuccess(const Extension* extension,
                                                 SkBitmap* icon) {
  if (skip_post_install_ui_)
    return;

  if (extension->is_theme()) {
    ShowThemeInfoBar(previous_theme_id_, previous_using_native_theme_,
                     extension, profile_);
    return;
  }

  // Extensions aren't enabled by default in incognito so we confirm
  // the install in a normal window.
  Profile* current_profile = profile_->GetOriginalProfile();
  Browser* browser = browser::FindOrCreateTabbedBrowser(current_profile);
  if (browser->tab_count() == 0)
    browser->AddBlankTab(true);
  browser->window()->Show();

  bool use_bubble_for_apps = false;

#if defined(TOOLKIT_VIEWS)
  CommandLine* cmdline = CommandLine::ForCurrentProcess();
  use_bubble_for_apps = (use_app_installed_bubble_ ||
                         cmdline->HasSwitch(switches::kAppsNewInstallBubble));
#endif

  if (extension->is_app() && !use_bubble_for_apps) {
    ExtensionInstallUI::OpenAppInstalledUI(browser, extension->id());
    return;
  }

  browser::ShowExtensionInstalledBubble(extension, browser, *icon,
                                        current_profile);
}

void ExtensionInstallUIDefault::OnInstallFailure(const string16& error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (disable_failure_ui_for_tests || skip_post_install_ui_)
    return;

  Browser* browser = browser::FindLastActiveWithProfile(profile_);
  browser::ShowMessageBox(browser ? browser->window()->GetNativeWindow() : NULL,
      l10n_util::GetStringUTF16(IDS_EXTENSION_INSTALL_FAILURE_TITLE), error,
      browser::MESSAGE_BOX_TYPE_WARNING);
}

void ExtensionInstallUIDefault::SetSkipPostInstallUI(bool skip_ui) {
  skip_post_install_ui_ = skip_ui;
}

void ExtensionInstallUIDefault::SetUseAppInstalledBubble(bool use_bubble) {
  use_app_installed_bubble_ = use_bubble;
}

// static
void ExtensionInstallUIDefault::ShowThemeInfoBar(
    const std::string& previous_theme_id, bool previous_using_native_theme,
    const Extension* new_theme, Profile* profile) {
  if (!new_theme->is_theme())
    return;

  // Get last active tabbed browser of profile.
  Browser* browser = browser::FindTabbedBrowser(profile, true);
  if (!browser)
    return;

  TabContentsWrapper* tab_contents = browser->GetSelectedTabContentsWrapper();
  if (!tab_contents)
    return;
  InfoBarTabHelper* infobar_helper = tab_contents->infobar_tab_helper();

  // First find any previous theme preview infobars.
  InfoBarDelegate* old_delegate = NULL;
  for (size_t i = 0; i < infobar_helper->infobar_count(); ++i) {
    InfoBarDelegate* delegate = infobar_helper->GetInfoBarDelegateAt(i);
    ThemeInstalledInfoBarDelegate* theme_infobar =
        delegate->AsThemePreviewInfobarDelegate();
    if (theme_infobar) {
      // If the user installed the same theme twice, ignore the second install
      // and keep the first install info bar, so that they can easily undo to
      // get back the previous theme.
      if (theme_infobar->MatchesTheme(new_theme))
        return;
      old_delegate = delegate;
      break;
    }
  }

  // Then either replace that old one or add a new one.
  InfoBarDelegate* new_delegate = GetNewThemeInstalledInfoBarDelegate(
      tab_contents, new_theme, previous_theme_id, previous_using_native_theme);

  if (old_delegate)
    infobar_helper->ReplaceInfoBar(old_delegate, new_delegate);
  else
    infobar_helper->AddInfoBar(new_delegate);
}

InfoBarDelegate* ExtensionInstallUIDefault::GetNewThemeInstalledInfoBarDelegate(
    TabContentsWrapper* tab_contents,
    const Extension* new_theme,
    const std::string& previous_theme_id,
    bool previous_using_native_theme) {
  Profile* profile = tab_contents->profile();
  return new ThemeInstalledInfoBarDelegate(
      tab_contents->infobar_tab_helper(),
      profile->GetExtensionService(),
      ThemeServiceFactory::GetForProfile(profile),
      new_theme,
      previous_theme_id,
      previous_using_native_theme);
}

// static
ExtensionInstallUI* ExtensionInstallUI::Create(Profile* profile) {
  return new ExtensionInstallUIDefault(profile);
}

// static
void ExtensionInstallUI::OpenAppInstalledUI(Browser* browser,
                                            const std::string& app_id) {
  if (NewTabUI::ShouldShowApps()) {
    browser::NavigateParams params = browser->GetSingletonTabNavigateParams(
        GURL(chrome::kChromeUINewTabURL));
    browser::Navigate(&params);

    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_APP_INSTALLED_TO_NTP,
        content::Source<WebContents>(params.target_contents->web_contents()),
        content::Details<const std::string>(&app_id));
  } else {
#if defined(USE_ASH)
    ash::Shell::GetInstance()->ToggleAppList();

    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_APP_INSTALLED_TO_APPLIST,
        content::Source<Profile>(browser->profile()),
        content::Details<const std::string>(&app_id));
#else
    NOTREACHED();
#endif
  }
}

// static
void ExtensionInstallUI::DisableFailureUIForTests() {
  disable_failure_ui_for_tests = true;
}
