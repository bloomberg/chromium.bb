// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_install_ui.h"

#include <map>

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/rand_util.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/extensions/theme_installed_infobar_delegate.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#if defined(TOOLKIT_VIEWS)
#include "chrome/browser/views/app_launcher.h"
#include "chrome/browser/views/extensions/extension_installed_bubble.h"
#elif defined(TOOLKIT_GTK)
#include "chrome/browser/gtk/extension_installed_bubble_gtk.h"
#endif
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_action.h"
#include "chrome/common/extensions/extension_resource.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/url_constants.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"

#if defined(TOOLKIT_GTK)
#include "chrome/browser/extensions/gtk_theme_installed_infobar_delegate.h"
#include "chrome/browser/gtk/gtk_theme_provider.h"
#endif

#if defined(OS_MACOSX)
#include "chrome/browser/cocoa/extension_installed_bubble_bridge.h"
#endif

// static
const int ExtensionInstallUI::kTitleIds[NUM_PROMPT_TYPES] = {
  IDS_EXTENSION_INSTALL_PROMPT_TITLE,
  IDS_EXTENSION_UNINSTALL_PROMPT_TITLE
};
// static
const int ExtensionInstallUI::kHeadingIds[NUM_PROMPT_TYPES] = {
  IDS_EXTENSION_INSTALL_PROMPT_HEADING,
  IDS_EXTENSION_UNINSTALL_PROMPT_HEADING
};
// static
const int ExtensionInstallUI::kButtonIds[NUM_PROMPT_TYPES] = {
  IDS_EXTENSION_PROMPT_INSTALL_BUTTON,
  IDS_EXTENSION_PROMPT_UNINSTALL_BUTTON
};

namespace {

static void GetV2Warnings(Extension* extension,
                          std::vector<string16>* warnings) {
  if (!extension->plugins().empty()) {
    // TODO(aa): This one is a bit awkward. Should we have a separate
    // presentation for this case?
    warnings->push_back(
        l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT2_WARNING_FULL_ACCESS));
    return;
  }

  if (extension->HasAccessToAllHosts()) {
    warnings->push_back(
        l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT2_WARNING_ALL_HOSTS));
  } else {
    std::set<std::string> hosts = extension->GetEffectiveHostPermissions();
    if (hosts.size() == 1) {
      warnings->push_back(
          l10n_util::GetStringFUTF16(IDS_EXTENSION_PROMPT2_WARNING_1_HOST,
                                     UTF8ToUTF16(*hosts.begin())));
    } else if (hosts.size() == 2) {
      warnings->push_back(
          l10n_util::GetStringFUTF16(IDS_EXTENSION_PROMPT2_WARNING_2_HOSTS,
                                     UTF8ToUTF16(*hosts.begin()),
                                     UTF8ToUTF16(*(++hosts.begin()))));
    } else if (hosts.size() == 3) {
      warnings->push_back(
          l10n_util::GetStringFUTF16(IDS_EXTENSION_PROMPT2_WARNING_3_HOSTS,
                                     UTF8ToUTF16(*hosts.begin()),
                                     UTF8ToUTF16(*(++hosts.begin())),
                                     UTF8ToUTF16(*(++++hosts.begin()))));
    } else if (hosts.size() >= 4) {
      warnings->push_back(
          l10n_util::GetStringFUTF16(
              IDS_EXTENSION_PROMPT2_WARNING_4_OR_MORE_HOSTS,
              UTF8ToUTF16(*hosts.begin()),
              UTF8ToUTF16(*(++hosts.begin())),
              IntToString16(hosts.size() - 2)));
    }
  }

  if (extension->HasEffectiveBrowsingHistoryPermission()) {
    warnings->push_back(
        l10n_util::GetStringUTF16(
            IDS_EXTENSION_PROMPT2_WARNING_BROWSING_HISTORY));
  }

  const Extension::SimplePermissions& simple_permissions =
      Extension::GetSimplePermissions();

  for (Extension::SimplePermissions::const_iterator iter =
           simple_permissions.begin();
       iter != simple_permissions.end(); ++iter) {
    if (extension->HasApiPermission(iter->first))
      warnings->push_back(iter->second);
  }
}

}  // namespace

ExtensionInstallUI::ExtensionInstallUI(Profile* profile)
    : profile_(profile),
      ui_loop_(MessageLoop::current()),
      previous_use_system_theme_(false),
      extension_(NULL),
      delegate_(NULL),
      prompt_type_(NUM_PROMPT_TYPES),
      ALLOW_THIS_IN_INITIALIZER_LIST(tracker_(this)) {}

void ExtensionInstallUI::ConfirmInstall(Delegate* delegate,
                                        Extension* extension) {
  DCHECK(ui_loop_ == MessageLoop::current());
  extension_ = extension;
  delegate_ = delegate;

  // We special-case themes to not show any confirm UI. Instead they are
  // immediately installed, and then we show an infobar (see OnInstallSuccess)
  // to allow the user to revert if they don't like it.
  if (extension->is_theme()) {
    // Remember the current theme in case the user pressed undo.
    Extension* previous_theme = profile_->GetTheme();
    if (previous_theme)
      previous_theme_id_ = previous_theme->id();

#if defined(TOOLKIT_GTK)
    // On Linux, we also need to take the user's system settings into account
    // to undo theme installation.
    previous_use_system_theme_ =
        GtkThemeProvider::GetFrom(profile_)->UseGtkTheme();
#else
    DCHECK(!previous_use_system_theme_);
#endif

    delegate->InstallUIProceed(false);
    return;
  }

  ShowConfirmation(INSTALL_PROMPT);
}

void ExtensionInstallUI::ConfirmUninstall(Delegate* delegate,
                                          Extension* extension) {
  DCHECK(ui_loop_ == MessageLoop::current());
  extension_ = extension;
  delegate_ = delegate;

  ShowConfirmation(UNINSTALL_PROMPT);
}

void ExtensionInstallUI::OnInstallSuccess(Extension* extension) {
  if (extension->is_theme()) {
    ShowThemeInfoBar(previous_theme_id_, previous_use_system_theme_,
                     extension, profile_);
    return;
  }

  // GetLastActiveWithProfile will fail on the build bots. This needs to be
  // implemented differently if any test is created which depends on
  // ExtensionInstalledBubble showing.
  Browser* browser = BrowserList::GetLastActiveWithProfile(profile_);

  if (extension->GetFullLaunchURL().is_valid()) {
    std::string hash_params = "app-id=";
    hash_params += extension->id();

    if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kAppsPanel)) {
#if defined(TOOLKIT_VIEWS)
      AppLauncher::ShowForNewTab(browser, hash_params);
#else
      NOTREACHED();
#endif
    } else {
      std::string url(chrome::kChromeUINewTabURL);
      url += "/#";
      url += hash_params;
      browser->AddTabWithURL(GURL(url), GURL(), PageTransition::TYPED, -1,
                             TabStripModel::ADD_SELECTED, NULL, std::string());
    }

    return;
  }

#if defined(TOOLKIT_VIEWS)
  if (!browser)
    return;

  ExtensionInstalledBubble::Show(extension, browser, icon_);
#elif defined(OS_MACOSX)
  DCHECK(browser);
  // Note that browser actions don't appear in incognito mode initially,
  // so fall back to the generic case.
  if ((extension->browser_action() && !browser->profile()->IsOffTheRecord()) ||
      (extension->page_action() &&
      !extension->page_action()->default_icon_path().empty())) {
    ExtensionInstalledBubbleCocoa::ShowExtensionInstalledBubble(
        browser->window()->GetNativeHandle(),
        extension, browser, icon_);
  }  else {
    // If the extension is of type GENERIC, meaning it doesn't have a UI
    // surface to display for this window, launch infobar instead of popup
    // bubble, because we have no guaranteed wrench menu button to point to.
    ShowGenericExtensionInstalledInfoBar(extension);
  }
#elif defined(TOOLKIT_GTK)
  if (!browser)
    return;
  ExtensionInstalledBubbleGtk::Show(extension, browser, icon_);
#endif  // TOOLKIT_VIEWS
}

void ExtensionInstallUI::OnInstallFailure(const std::string& error) {
  DCHECK(ui_loop_ == MessageLoop::current());

  Browser* browser = BrowserList::GetLastActiveWithProfile(profile_);
  platform_util::SimpleErrorBox(
      browser ? browser->window()->GetNativeHandle() : NULL,
      l10n_util::GetStringUTF16(IDS_EXTENSION_INSTALL_FAILURE_TITLE),
      UTF8ToUTF16(error));
}

void ExtensionInstallUI::OnImageLoaded(
    SkBitmap* image, ExtensionResource resource, int index) {
  if (image)
    icon_ = *image;
  else
    icon_ = SkBitmap();
  if (icon_.empty()) {
    icon_ = *ResourceBundle::GetSharedInstance().GetBitmapNamed(
        IDR_EXTENSION_DEFAULT_ICON);
  }

  switch (prompt_type_) {
    case INSTALL_PROMPT: {
      // TODO(jcivelli): http://crbug.com/44771 We should not show an install
      //                 dialog when installing an app from the gallery.
      NotificationService* service = NotificationService::current();
      service->Notify(NotificationType::EXTENSION_WILL_SHOW_CONFIRM_DIALOG,
          Source<ExtensionInstallUI>(this),
          NotificationService::NoDetails());

      std::vector<string16> warnings;
      GetV2Warnings(extension_, &warnings);
      ShowExtensionInstallUIPrompt2Impl(
          profile_, delegate_, extension_, &icon_, warnings);
      break;
    }
    case UNINSTALL_PROMPT: {
      string16 message =
          l10n_util::GetStringUTF16(IDS_EXTENSION_UNINSTALL_CONFIRMATION);
      ShowExtensionInstallUIPromptImpl(profile_, delegate_, extension_, &icon_,
          message, UNINSTALL_PROMPT);
      break;
    }
    default:
      NOTREACHED() << "Unknown message";
      break;
  }
}

void ExtensionInstallUI::ShowThemeInfoBar(
    const std::string& previous_theme_id, bool previous_use_system_theme,
    Extension* new_theme, Profile* profile) {
  if (!new_theme->is_theme())
    return;

  // Get last active normal browser of profile.
  Browser* browser = BrowserList::FindBrowserWithType(profile,
                                                      Browser::TYPE_NORMAL,
                                                      true);
  if (!browser)
    return;

  TabContents* tab_contents = browser->GetSelectedTabContents();
  if (!tab_contents)
    return;

  // First find any previous theme preview infobars.
  InfoBarDelegate* old_delegate = NULL;
  for (int i = 0; i < tab_contents->infobar_delegate_count(); ++i) {
    InfoBarDelegate* delegate = tab_contents->GetInfoBarDelegateAt(i);
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
  InfoBarDelegate* new_delegate =
      GetNewThemeInstalledInfoBarDelegate(
          tab_contents, new_theme,
          previous_theme_id, previous_use_system_theme);

  if (old_delegate)
    tab_contents->ReplaceInfoBar(old_delegate, new_delegate);
  else
    tab_contents->AddInfoBar(new_delegate);
}

void ExtensionInstallUI::ShowConfirmation(PromptType prompt_type) {
  // Load the image asynchronously. For the response, check OnImageLoaded.
  prompt_type_ = prompt_type;
  ExtensionResource image =
      extension_->GetIconPath(Extension::EXTENSION_ICON_LARGE);
  tracker_.LoadImage(extension_, image,
                     gfx::Size(Extension::EXTENSION_ICON_LARGE,
                               Extension::EXTENSION_ICON_LARGE),
                     ImageLoadingTracker::DONT_CACHE);
}

#if defined(OS_MACOSX)
void ExtensionInstallUI::ShowGenericExtensionInstalledInfoBar(
    Extension* new_extension) {
  Browser* browser = BrowserList::GetLastActiveWithProfile(profile_);
  if (!browser)
    return;

  TabContents* tab_contents = browser->GetSelectedTabContents();
  if (!tab_contents)
    return;

  std::wstring msg = l10n_util::GetStringF(IDS_EXTENSION_INSTALLED_HEADING,
                                           UTF8ToWide(new_extension->name())) +
         L" " + l10n_util::GetString(IDS_EXTENSION_INSTALLED_MANAGE_INFO_MAC);
  InfoBarDelegate* delegate = new SimpleAlertInfoBarDelegate(
      tab_contents, msg, new SkBitmap(icon_), true);
  tab_contents->AddInfoBar(delegate);
}
#endif

InfoBarDelegate* ExtensionInstallUI::GetNewThemeInstalledInfoBarDelegate(
    TabContents* tab_contents, Extension* new_theme,
    const std::string& previous_theme_id, bool previous_use_system_theme) {
#if defined(TOOLKIT_GTK)
  return new GtkThemeInstalledInfoBarDelegate(tab_contents, new_theme,
      previous_theme_id, previous_use_system_theme);
#else
  return new ThemeInstalledInfoBarDelegate(tab_contents, new_theme,
      previous_theme_id);
#endif
}
