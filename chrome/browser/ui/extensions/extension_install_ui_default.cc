// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extension_install_ui_default.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/extension_installed_bubble.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/simple_message_box.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/install/crx_install_error.h"
#include "extensions/common/extension.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/ui/extensions/extension_installed_notification.h"
#else
#include "chrome/common/url_constants.h"
#include "content/public/browser/notification_service.h"
#endif

using content::BrowserThread;
using content::WebContents;
using extensions::Extension;

namespace {

Browser* FindOrCreateVisibleBrowser(Profile* profile) {
  // TODO(mpcomplete): remove this workaround for http://crbug.com/244246
  // after fixing http://crbug.com/38676.
  if (!IncognitoModePrefs::CanOpenBrowser(profile))
    return NULL;
  chrome::ScopedTabbedBrowserDisplayer displayer(profile);
  Browser* browser = displayer.browser();
  if (browser->tab_strip_model()->count() == 0)
    chrome::AddTabAt(browser, GURL(), -1, true);
  return browser;
}

void ShowExtensionInstalledBubble(const extensions::Extension* extension,
                                  Profile* profile,
                                  const SkBitmap& icon) {
  Browser* browser = FindOrCreateVisibleBrowser(profile);
  if (browser)
    ExtensionInstalledBubble::ShowBubble(extension, browser, icon);
}

// Helper class to put up an infobar when installation fails.
class ErrorInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  // Creates an error infobar and delegate and adds the infobar to
  // |infobar_service|.
  static void Create(InfoBarService* infobar_service,
                     const extensions::CrxInstallError& error);

 private:
  explicit ErrorInfoBarDelegate(const extensions::CrxInstallError& error);
  ~ErrorInfoBarDelegate() override;

  // ConfirmInfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  base::string16 GetMessageText() const override;
  int GetButtons() const override;
  base::string16 GetLinkText() const override;
  GURL GetLinkURL() const override;

  extensions::CrxInstallError error_;

  DISALLOW_COPY_AND_ASSIGN(ErrorInfoBarDelegate);
};

// static
void ErrorInfoBarDelegate::Create(InfoBarService* infobar_service,
                                  const extensions::CrxInstallError& error) {
  infobar_service->AddInfoBar(infobar_service->CreateConfirmInfoBar(
      std::unique_ptr<ConfirmInfoBarDelegate>(
          new ErrorInfoBarDelegate(error))));
}

ErrorInfoBarDelegate::ErrorInfoBarDelegate(
    const extensions::CrxInstallError& error)
    : ConfirmInfoBarDelegate(), error_(error) {
}

ErrorInfoBarDelegate::~ErrorInfoBarDelegate() {
}

infobars::InfoBarDelegate::InfoBarIdentifier
ErrorInfoBarDelegate::GetIdentifier() const {
  return INSTALLATION_ERROR_INFOBAR_DELEGATE;
}

base::string16 ErrorInfoBarDelegate::GetMessageText() const {
  return error_.message();
}

int ErrorInfoBarDelegate::GetButtons() const {
  return BUTTON_OK;
}

base::string16 ErrorInfoBarDelegate::GetLinkText() const {
  return (error_.type() == extensions::CrxInstallError::ERROR_OFF_STORE)
             ? l10n_util::GetStringUTF16(IDS_LEARN_MORE)
             : base::string16();
}

GURL ErrorInfoBarDelegate::GetLinkURL() const {
  return GURL("https://support.google.com/chrome_webstore/?p=crx_warning");
}

}  // namespace

ExtensionInstallUIDefault::ExtensionInstallUIDefault(
    content::BrowserContext* context)
    : profile_(Profile::FromBrowserContext(context)),
      skip_post_install_ui_(false),
      use_app_installed_bubble_(false) {}

ExtensionInstallUIDefault::~ExtensionInstallUIDefault() {}

void ExtensionInstallUIDefault::OnInstallSuccess(const Extension* extension,
                                                 const SkBitmap* icon) {
  if (skip_post_install_ui_ || extension->is_theme())
    return;

  if (!profile_) {
    // TODO(zelidrag): Figure out what exact conditions cause crash
    // http://crbug.com/159437 and write browser test to cover it.
    NOTREACHED();
    return;
  }

  // Extensions aren't enabled by default in incognito so we confirm
  // the install in a normal window.
  Profile* current_profile = profile_->GetOriginalProfile();
  if (extension->is_app()) {
    bool use_bubble = false;

#if defined(TOOLKIT_VIEWS)  || defined(OS_MACOSX)
    use_bubble = use_app_installed_bubble_;
#endif

    if (use_bubble) {
      ShowExtensionInstalledBubble(extension, current_profile, *icon);
      return;
    }

#if defined(OS_CHROMEOS)
    ExtensionInstalledNotification::Show(extension, current_profile);
#else  // defined(OS_CHROMEOS)
    OpenAppInstalledUI(extension->id());
#endif  // defined(OS_CHROMEOS)
    return;
  }

  ShowExtensionInstalledBubble(extension, current_profile, *icon);
}

void ExtensionInstallUIDefault::OnInstallFailure(
    const extensions::CrxInstallError& error) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (disable_failure_ui_for_tests() || skip_post_install_ui_)
    return;

  Browser* browser = chrome::FindLastActiveWithProfile(profile_);
  if (!browser)  // Can be NULL in unittests.
    return;
  WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (!web_contents)
    return;
  ErrorInfoBarDelegate::Create(InfoBarService::FromWebContents(web_contents),
                               error);
}

void ExtensionInstallUIDefault::OpenAppInstalledUI(const std::string& app_id) {
#if defined(OS_CHROMEOS)
  // Notification always enabled on ChromeOS, so always handled in
  // OnInstallSuccess.
  NOTREACHED();
#else
  Profile* current_profile = profile_->GetOriginalProfile();
  Browser* browser = FindOrCreateVisibleBrowser(current_profile);
  if (browser) {
    chrome::NavigateParams params(chrome::GetSingletonTabNavigateParams(
        browser, GURL(chrome::kChromeUIAppsURL)));
    chrome::Navigate(&params);

    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_APP_INSTALLED_TO_NTP,
        content::Source<WebContents>(params.target_contents),
        content::Details<const std::string>(&app_id));
  }
#endif
}

void ExtensionInstallUIDefault::SetUseAppInstalledBubble(bool use_bubble) {
  use_app_installed_bubble_ = use_bubble;
}

void ExtensionInstallUIDefault::SetSkipPostInstallUI(bool skip_ui) {
  skip_post_install_ui_ = skip_ui;
}

gfx::NativeWindow ExtensionInstallUIDefault::GetDefaultInstallDialogParent() {
  Browser* browser = chrome::FindLastActiveWithProfile(profile_);
  if (browser) {
    content::WebContents* contents =
        browser->tab_strip_model()->GetActiveWebContents();
    return contents->GetTopLevelNativeWindow();
  }
  return NULL;
}
