// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_install_ui_default.h"

#include "base/command_line.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/theme_installed_infobar_delegate.h"
#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/confirm_infobar_delegate.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/simple_message_box.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
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

// Helper class to put up an infobar when installation fails.
class ErrorInfobarDelegate : public ConfirmInfoBarDelegate {
 public:
  ErrorInfobarDelegate(InfoBarTabHelper* infobar_helper,
                       Browser* browser,
                       const CrxInstallerError& error)
      : ConfirmInfoBarDelegate(infobar_helper),
        browser_(browser),
        error_(error) {
  }

 private:
  virtual string16 GetMessageText() const OVERRIDE {
    return error_.message();
  }

  virtual int GetButtons() const OVERRIDE {
    return BUTTON_OK;
  }

  virtual string16 GetLinkText() const OVERRIDE {
    return error_.type() == CrxInstallerError::ERROR_OFF_STORE ?
        l10n_util::GetStringUTF16(IDS_LEARN_MORE) : ASCIIToUTF16("");
  }

  virtual bool LinkClicked(WindowOpenDisposition disposition) OVERRIDE {
    chrome::NavigateParams params(
        browser_,
        GURL("http://support.google.com/chrome_webstore/?p=crx_warning"),
        content::PAGE_TRANSITION_LINK);
    params.disposition = NEW_FOREGROUND_TAB;
    chrome::Navigate(&params);
    return false;
  }

  Browser* browser_;
  CrxInstallerError error_;
};

}  // namespace

ExtensionInstallUIDefault::ExtensionInstallUIDefault(Browser* browser)
    : skip_post_install_ui_(false),
      previous_using_native_theme_(false),
      use_app_installed_bubble_(false) {
  browser_ = browser;

  // Remember the current theme in case the user presses undo.
  if (browser) {
    Profile* profile = browser->profile();
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
                     extension, browser_->profile());
    return;
  }

  // Extensions aren't enabled by default in incognito so we confirm
  // the install in a normal window.
  Profile* current_profile = browser_->profile()->GetOriginalProfile();
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

  chrome::ShowExtensionInstalledBubble(extension, browser, *icon);
}

void ExtensionInstallUIDefault::OnInstallFailure(
    const CrxInstallerError& error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (disable_failure_ui_for_tests || skip_post_install_ui_)
    return;

  Browser* browser = browser::FindLastActiveWithProfile(browser_->profile());
  TabContents* tab_contents = chrome::GetActiveTabContents(browser);
  if (!tab_contents)
    return;
  InfoBarTabHelper* infobar_helper = tab_contents->infobar_tab_helper();
  infobar_helper->AddInfoBar(
      new ErrorInfobarDelegate(infobar_helper, browser, error));
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

  TabContents* tab_contents = chrome::GetActiveTabContents(browser);
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
    TabContents* tab_contents,
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
ExtensionInstallUI* ExtensionInstallUI::Create(Browser* browser) {
  return new ExtensionInstallUIDefault(browser);
}

// static
void ExtensionInstallUI::OpenAppInstalledUI(Browser* browser,
                                            const std::string& app_id) {
  if (NewTabUI::ShouldShowApps()) {
    chrome::NavigateParams params(chrome::GetSingletonTabNavigateParams(
        browser, GURL(chrome::kChromeUINewTabURL)));
    chrome::Navigate(&params);

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
