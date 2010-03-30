// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_install_ui.h"

#include <map>

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/file_util.h"
#include "base/rand_util.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/extensions/theme_installed_infobar_delegate.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#if defined(TOOLKIT_VIEWS)  // TODO(port)
#include "chrome/browser/views/extensions/extension_installed_bubble.h"
#elif defined(TOOLKIT_GTK)
#include "chrome/browser/gtk/extension_installed_bubble_gtk.h"
#endif
#include "chrome/common/extensions/extension.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/platform_util.h"
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
  IDS_EXTENSION_UNINSTALL_PROMPT_TITLE,
  IDS_EXTENSION_ENABLE_INCOGNITO_PROMPT_TITLE
};
// static
const int ExtensionInstallUI::kHeadingIds[NUM_PROMPT_TYPES] = {
  IDS_EXTENSION_INSTALL_PROMPT_HEADING,
  IDS_EXTENSION_UNINSTALL_PROMPT_HEADING,
  IDS_EXTENSION_ENABLE_INCOGNITO_PROMPT_HEADING
};
// static
const int ExtensionInstallUI::kButtonIds[NUM_PROMPT_TYPES] = {
  IDS_EXTENSION_PROMPT_INSTALL_BUTTON,
  IDS_EXTENSION_PROMPT_UNINSTALL_BUTTON,
  IDS_EXTENSION_PROMPT_ENABLE_INCOGNITO_BUTTON
};

namespace {

// We also show the severe warning if the extension has access to any file://
// URLs. They aren't *quite* as dangerous as full access to the system via
// NPAPI, but pretty dang close. Content scripts are currently the only way
// that extension can get access to file:// URLs.
static bool ExtensionHasFileAccess(Extension* extension) {
  for (UserScriptList::const_iterator script =
           extension->content_scripts().begin();
       script != extension->content_scripts().end();
       ++script) {
    for (UserScript::PatternList::const_iterator pattern =
             script->url_patterns().begin();
         pattern != script->url_patterns().end();
         ++pattern) {
      if (pattern->scheme() == chrome::kFileScheme)
        return true;
    }
  }

  return false;
}

// TODO(estade): remove this function when the old install UI is removed. It
// is commented out on linux/gtk due to compiler warnings.
#if !defined(TOOLKIT_GTK)
static std::wstring GetInstallWarning(Extension* extension) {
  // If the extension has a plugin, it's easy: the plugin has the most severe
  // warning.
  if (!extension->plugins().empty() || ExtensionHasFileAccess(extension))
    return l10n_util::GetString(IDS_EXTENSION_PROMPT_WARNING_FULL_ACCESS);

  // Otherwise, we go in descending order of severity: all hosts, several hosts,
  // a single host, no hosts. For each of these, we also have a variation of the
  // message for when api permissions are also requested.
  if (extension->HasAccessToAllHosts()) {
    if (extension->api_permissions().empty())
      return l10n_util::GetString(IDS_EXTENSION_PROMPT_WARNING_ALL_HOSTS);
    else
      return l10n_util::GetString(
          IDS_EXTENSION_PROMPT_WARNING_ALL_HOSTS_AND_BROWSER);
  }

  const std::set<std::string> hosts = extension->GetEffectiveHostPermissions();
  if (hosts.size() > 1) {
    if (extension->api_permissions().empty())
      return l10n_util::GetString(
          IDS_EXTENSION_PROMPT_WARNING_MULTIPLE_HOSTS);
    else
      return l10n_util::GetString(
          IDS_EXTENSION_PROMPT_WARNING_MULTIPLE_HOSTS_AND_BROWSER);
  }

  if (hosts.size() == 1) {
    if (extension->api_permissions().empty())
      return l10n_util::GetStringF(
          IDS_EXTENSION_PROMPT_WARNING_SINGLE_HOST,
          UTF8ToWide(*hosts.begin()));
    else
      return l10n_util::GetStringF(
          IDS_EXTENSION_PROMPT_WARNING_SINGLE_HOST_AND_BROWSER,
          UTF8ToWide(*hosts.begin()));
  }

  DCHECK(hosts.size() == 0);
  if (extension->api_permissions().empty())
    return L"";
  else
    return l10n_util::GetString(IDS_EXTENSION_PROMPT_WARNING_BROWSER);
}
#endif

#if defined(OS_WIN) || defined(TOOLKIT_GTK)
static void GetV2Warnings(Extension* extension,
                          std::vector<string16>* warnings) {
  if (!extension->plugins().empty() || ExtensionHasFileAccess(extension)) {
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

  if (extension->HasApiPermission(Extension::kTabPermission) ||
      extension->HasApiPermission(Extension::kBookmarkPermission)) {
    warnings->push_back(
        l10n_util::GetStringUTF16(
            IDS_EXTENSION_PROMPT2_WARNING_BROWSING_HISTORY));
  }

  // TODO(aa): Geolocation, camera/mic, what else?
}
#endif

}  // namespace

ExtensionInstallUI::ExtensionInstallUI(Profile* profile)
    : profile_(profile),
      ui_loop_(MessageLoop::current()),
      extension_(NULL),
      delegate_(NULL),
      prompt_type_(NUM_PROMPT_TYPES),
      ALLOW_THIS_IN_INITIALIZER_LIST(tracker_(this))
#if defined(TOOLKIT_GTK)
    , previous_use_gtk_theme_(false)
#endif
{}

void ExtensionInstallUI::ConfirmInstall(Delegate* delegate,
                                        Extension* extension) {
  DCHECK(ui_loop_ == MessageLoop::current());
  extension_ = extension;
  delegate_ = delegate;

  // We special-case themes to not show any confirm UI. Instead they are
  // immediately installed, and then we show an infobar (see OnInstallSuccess)
  // to allow the user to revert if they don't like it.
  if (extension->IsTheme()) {
    // Remember the current theme in case the user pressed undo.
    Extension* previous_theme = profile_->GetTheme();
    if (previous_theme)
      previous_theme_id_ = previous_theme->id();

#if defined(TOOLKIT_GTK)
    // On Linux, we also need to take the user's system settings into account
    // to undo theme installation.
    previous_use_gtk_theme_ =
        GtkThemeProvider::GetFrom(profile_)->UseGtkTheme();
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

void ExtensionInstallUI::ConfirmEnableIncognito(Delegate* delegate,
                                                Extension* extension) {
  DCHECK(ui_loop_ == MessageLoop::current());
  extension_ = extension;
  delegate_ = delegate;

  ShowConfirmation(ENABLE_INCOGNITO_PROMPT);
}

void ExtensionInstallUI::OnInstallSuccess(Extension* extension) {
  if (extension->IsTheme()) {
    ShowThemeInfoBar(extension);
    return;
  }

  // GetLastActiveWithProfile will fail on the build bots. This needs to be
  // implemented differently if any test is created which depends on
  // ExtensionInstalledBubble showing.
#if defined(TOOLKIT_VIEWS)
  Browser* browser = BrowserList::GetLastActiveWithProfile(profile_);
  if (!browser)
    return;

  ExtensionInstalledBubble::Show(extension, browser, icon_);
#elif defined(OS_MACOSX)
  if (extension->browser_action() ||
      (extension->page_action() &&
      !extension->page_action()->default_icon_path().empty())) {
    Browser* browser = BrowserList::GetLastActiveWithProfile(profile_);
    DCHECK(browser);
    ExtensionInstalledBubbleCocoa::ShowExtensionInstalledBubble(
        browser->window()->GetNativeHandle(),
        extension, browser, icon_);
  }  else {
    // If the extension is of type GENERIC, launch infobar instead of popup
    // bubble, because we have no guaranteed wrench menu button to point to.
    ShowGenericExtensionInstalledInfoBar(extension);
  }
#elif defined(TOOLKIT_GTK)
  Browser* browser = BrowserList::GetLastActiveWithProfile(profile_);
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

void ExtensionInstallUI::OnOverinstallAttempted(Extension* extension) {
  ShowThemeInfoBar(extension);
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
      NotificationService* service = NotificationService::current();
      service->Notify(NotificationType::EXTENSION_WILL_SHOW_CONFIRM_DIALOG,
          Source<ExtensionInstallUI>(this),
          NotificationService::NoDetails());

#if defined(OS_WIN) || defined(TOOLKIT_GTK)
      std::vector<string16> warnings;
      GetV2Warnings(extension_, &warnings);
      ShowExtensionInstallUIPrompt2Impl(
          profile_, delegate_, extension_, &icon_, warnings);
#else
      ShowExtensionInstallUIPromptImpl(
          profile_, delegate_, extension_, &icon_,
          WideToUTF16Hack(GetInstallWarning(extension_)), INSTALL_PROMPT);
#endif
      break;
    }
    case UNINSTALL_PROMPT: {
      string16 message =
          l10n_util::GetStringUTF16(IDS_EXTENSION_UNINSTALL_CONFIRMATION);
      ShowExtensionInstallUIPromptImpl(profile_, delegate_, extension_, &icon_,
          message, UNINSTALL_PROMPT);
      break;
    }
    case ENABLE_INCOGNITO_PROMPT: {
      string16 message =
          l10n_util::GetStringFUTF16(IDS_EXTENSION_PROMPT_WARNING_INCOGNITO,
          l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));
      ShowExtensionInstallUIPromptImpl(profile_, delegate_, extension_, &icon_,
          message, ENABLE_INCOGNITO_PROMPT);
      break;
    }
    default:
      NOTREACHED() << "Unknown message";
      break;
  }
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
  InfoBarDelegate* new_delegate = GetNewInfoBarDelegate(new_theme,
      tab_contents);

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
      tab_contents, msg, new SkBitmap(icon_));
  tab_contents->AddInfoBar(delegate);
}
#endif

InfoBarDelegate* ExtensionInstallUI::GetNewInfoBarDelegate(
    Extension* new_theme, TabContents* tab_contents) {
#if defined(TOOLKIT_GTK)
  return new GtkThemeInstalledInfoBarDelegate(tab_contents, new_theme,
      previous_theme_id_, previous_use_gtk_theme_);
#else
  return new ThemeInstalledInfoBarDelegate(tab_contents, new_theme,
      previous_theme_id_);
#endif
}
