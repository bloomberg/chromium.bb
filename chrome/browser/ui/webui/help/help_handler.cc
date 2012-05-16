// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/help/help_handler.h"

#include <string>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/content_client.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/google_chrome_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "v8/include/v8.h"
#include "webkit/glue/user_agent.h"
#include "webkit/glue/webkit_glue.h"

#if defined(OS_CHROMEOS)
#include "base/i18n/time_formatting.h"
#include "base/file_util_proxy.h"
#include "base/sys_info.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/cros_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#endif

using base::ListValue;
using content::BrowserThread;

namespace {

// Returns the browser version as a string.
string16 BuildBrowserVersionString() {
  chrome::VersionInfo version_info;
  DCHECK(version_info.is_valid());

  std::string browser_version = version_info.Version();
  std::string version_modifier =
      chrome::VersionInfo::GetVersionStringModifier();
  if (!version_modifier.empty())
    browser_version += " " + version_modifier;

#if !defined(GOOGLE_CHROME_BUILD)
  browser_version += " (";
  browser_version += version_info.LastChange();
  browser_version += ")";
#endif

  return UTF8ToUTF16(browser_version);
}

#if defined(OS_CHROMEOS)

bool CanChangeReleaseChannel() {
  // On non managed machines we have local owner who is the only one to change
  // anything.
  if (chromeos::UserManager::Get()->IsCurrentUserOwner())
    return true;
  // On a managed machine we delegate this setting to the users of the same
  // domain only if the policy value is "domain".
  if (g_browser_process->browser_policy_connector()->IsEnterpriseManaged()) {
    bool value = false;
    if (!chromeos::CrosSettings::Get()->GetBoolean(
            chromeos::kReleaseChannelDelegated, &value) || !value)
      return false;
    // Get the currently logged in user and strip the domain part only.
    std::string domain = "";
    std::string user = chromeos::UserManager::Get()->GetLoggedInUser().email();
    size_t at_pos = user.find('@');
    if (at_pos != std::string::npos && at_pos + 1 < user.length())
      domain = user.substr(user.find('@') + 1);
    return domain == g_browser_process->browser_policy_connector()->
        GetEnterpriseDomain();
  }
  return false;
}

// Pointer to a |StringValue| holding the date of the last update to Chromium
// OS. Because this value is obtained by reading a file, it is cached here to
// prevent the need to read from the file system multiple times unnecessarily.
Value* g_last_updated_string = NULL;

#endif  // defined(OS_CHROMEOS)

}  // namespace

HelpHandler::HelpHandler()
    : version_updater_(VersionUpdater::Create()),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
}

HelpHandler::~HelpHandler() {
}

void HelpHandler::GetLocalizedValues(DictionaryValue* localized_strings) {
  DCHECK(localized_strings);
  DCHECK(localized_strings->empty());

  struct L10nResources {
    const char* name;
    int ids;
  };

  static L10nResources resources[] = {
    { "helpTitle", IDS_HELP_TITLE },
    { "aboutTitle", IDS_ABOUT_TAB_TITLE },
#if defined(OS_CHROMEOS)
    { "aboutProductTitle", IDS_PRODUCT_OS_NAME },
#else
    { "aboutProductTitle", IDS_PRODUCT_NAME },
#endif
    { "aboutProductDescription", IDS_ABOUT_PRODUCT_DESCRIPTION },
    { "relaunch", IDS_RELAUNCH_BUTTON },
    { "productName", IDS_PRODUCT_NAME },
    { "productCopyright", IDS_ABOUT_VERSION_COPYRIGHT },
    { "updateCheckStarted", IDS_UPGRADE_CHECK_STARTED },
    { "upToDate", IDS_UPGRADE_UP_TO_DATE },
    { "updating", IDS_UPGRADE_UPDATING },
    { "updateAlmostDone", IDS_UPGRADE_SUCCESSFUL_RELAUNCH },
    { "getHelpWithChrome",  IDS_GET_HELP_USING_CHROME },
    { "reportAnIssue",  IDS_REPORT_AN_ISSUE },
#if defined(OS_CHROMEOS)
    { "platform", IDS_PLATFORM_LABEL },
    { "firmware", IDS_ABOUT_PAGE_FIRMWARE },
    { "showMoreInfo", IDS_SHOW_MORE_INFO },
    { "hideMoreInfo", IDS_HIDE_MORE_INFO },
    { "channel", IDS_ABOUT_PAGE_CHANNEL },
    { "stable", IDS_ABOUT_PAGE_CHANNEL_STABLE },
    { "beta", IDS_ABOUT_PAGE_CHANNEL_BETA },
    { "dev", IDS_ABOUT_PAGE_CHANNEL_DEVELOPMENT },
    { "webkit", IDS_WEBKIT },
    { "userAgent", IDS_ABOUT_VERSION_USER_AGENT },
    { "commandLine", IDS_ABOUT_VERSION_COMMAND_LINE },
    { "lastUpdated", IDS_ABOUT_VERSION_LAST_UPDATED },
#endif
#if defined(OS_MACOSX)
    { "promote", IDS_ABOUT_CHROME_PROMOTE_UPDATER },
#endif
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(resources); ++i) {
    localized_strings->SetString(resources[i].name,
                                 l10n_util::GetStringUTF16(resources[i].ids));
  }

  localized_strings->SetString(
      "browserVersion",
      l10n_util::GetStringFUTF16(IDS_ABOUT_PRODUCT_VERSION,
                                 BuildBrowserVersionString()));

  string16 license = l10n_util::GetStringFUTF16(
      IDS_ABOUT_VERSION_LICENSE,
      UTF8ToUTF16(google_util::StringAppendGoogleLocaleParam(
          chrome::kChromiumProjectURL)),
      ASCIIToUTF16(chrome::kChromeUICreditsURL));
  localized_strings->SetString("productLicense", license);

#if defined(OS_CHROMEOS)
  string16 os_license = l10n_util::GetStringFUTF16(
      IDS_ABOUT_CROS_VERSION_LICENSE,
      ASCIIToUTF16(chrome::kChromeUIOSCreditsURL));
  localized_strings->SetString("productOsLicense", os_license);
#endif

  string16 tos = l10n_util::GetStringFUTF16(
      IDS_ABOUT_TERMS_OF_SERVICE, UTF8ToUTF16(chrome::kChromeUITermsURL));
  localized_strings->SetString("productTOS", tos);

  localized_strings->SetString("webkitVersion",
                               webkit_glue::GetWebKitVersion());

  localized_strings->SetString("jsEngine", "V8");
  localized_strings->SetString("jsEngineVersion", v8::V8::GetVersion());

  localized_strings->SetString("userAgentInfo",
                               content::GetUserAgent(GURL()));

  CommandLine::StringType command_line =
      CommandLine::ForCurrentProcess()->GetCommandLineString();
  localized_strings->SetString("commandLineInfo", command_line);
}

void HelpHandler::RegisterMessages() {
  registrar_.Add(this, chrome::NOTIFICATION_UPGRADE_RECOMMENDED,
                 content::NotificationService::AllSources());

  web_ui()->RegisterMessageCallback("onPageLoaded",
      base::Bind(&HelpHandler::OnPageLoaded, base::Unretained(this)));
  web_ui()->RegisterMessageCallback("relaunchNow",
      base::Bind(&HelpHandler::RelaunchNow, base::Unretained(this)));
  web_ui()->RegisterMessageCallback("openFeedbackDialog",
      base::Bind(&HelpHandler::OpenFeedbackDialog, base::Unretained(this)));
  web_ui()->RegisterMessageCallback("openHelpPage",
      base::Bind(&HelpHandler::OpenHelpPage, base::Unretained(this)));
#if defined(OS_CHROMEOS)
  web_ui()->RegisterMessageCallback("setReleaseTrack",
      base::Bind(&HelpHandler::SetReleaseTrack, base::Unretained(this)));
#endif
#if defined(OS_MACOSX)
  web_ui()->RegisterMessageCallback("promoteUpdater",
      base::Bind(&HelpHandler::PromoteUpdater, base::Unretained(this)));
#endif
}

void HelpHandler::Observe(int type, const content::NotificationSource& source,
                          const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_UPGRADE_RECOMMENDED: {
      // A version update is installed and ready to go. Refresh the UI so the
      // correct state will be shown.
      version_updater_->CheckForUpdate(
          base::Bind(&HelpHandler::SetUpdateStatus, base::Unretained(this))
#if defined(OS_MACOSX)
          , base::Bind(&HelpHandler::SetPromotionState, base::Unretained(this))
#endif
          );
      break;
    }
    default:
      NOTREACHED();
  }
}

void HelpHandler::OnPageLoaded(const ListValue* args) {
#if defined(OS_CHROMEOS)
  // Version information is loaded from a callback
  loader_.GetVersion(&consumer_, base::Bind(&HelpHandler::OnOSVersion,
                                            base::Unretained(this)),
                     chromeos::VersionLoader::VERSION_FULL);
  loader_.GetFirmware(&consumer_, base::Bind(&HelpHandler::OnOSFirmware,
                                             base::Unretained(this)));

  scoped_ptr<base::Value> can_change_channel_value(
      base::Value::CreateBooleanValue(CanChangeReleaseChannel()));
  web_ui()->CallJavascriptFunction(
      "help.HelpPage.updateEnableReleaseChannel", *can_change_channel_value);

  if (g_last_updated_string == NULL) {
    // If |g_last_updated_string| is |NULL|, the date has not yet been assigned.
    // Get the date of the last lsb-release file modification.
    base::FileUtilProxy::GetFileInfo(
        BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE),
        base::SysInfo::GetLsbReleaseFilePath(),
        base::Bind(&HelpHandler::ProcessLsbFileInfo,
                   weak_factory_.GetWeakPtr()));
  } else {
    web_ui()->CallJavascriptFunction("help.HelpPage.setLastUpdated",
                                     *g_last_updated_string);
  }
#endif  // defined(OS_CHROMEOS)

  version_updater_->CheckForUpdate(
      base::Bind(&HelpHandler::SetUpdateStatus, base::Unretained(this))
#if defined(OS_MACOSX)
      , base::Bind(&HelpHandler::SetPromotionState, base::Unretained(this))
#endif
      );

#if defined(OS_CHROMEOS)
  version_updater_->GetReleaseChannel(
      base::Bind(&HelpHandler::OnReleaseChannel, base::Unretained(this)));
#endif
}

#if defined(OS_MACOSX)
void HelpHandler::PromoteUpdater(const ListValue* args) {
  version_updater_->PromoteUpdater();
}
#endif

void HelpHandler::RelaunchNow(const ListValue* args) {
  DCHECK(args->empty());
  version_updater_->RelaunchBrowser();
}

void HelpHandler::OpenFeedbackDialog(const ListValue* args) {
  DCHECK(args->empty());
  Browser* browser = browser::FindBrowserWithWebContents(
      web_ui()->GetWebContents());
  browser->OpenFeedbackDialog();
}

void HelpHandler::OpenHelpPage(const base::ListValue* args) {
  DCHECK(args->empty());
  Browser* browser = browser::FindBrowserWithWebContents(
      web_ui()->GetWebContents());
  browser->ShowHelpTab();
}

#if defined(OS_CHROMEOS)

void HelpHandler::SetReleaseTrack(const ListValue* args) {
  if (!CanChangeReleaseChannel()) {
    LOG(WARNING) << "Non-owner tried to change release track.";
    return;
  }

  const std::string channel = UTF16ToUTF8(ExtractStringValue(args));
  version_updater_->SetReleaseChannel(channel);
  // On enterprise machines we can only use SetReleaseChannel to store the
  // user choice in the lsb-release file but we can not modify the policy blob.
  // Therefore we only call SetString if the device is locally owned and the
  // currently logged in user is the owner.
  if (chromeos::UserManager::Get()->IsCurrentUserOwner()) {
    chromeos::CrosSettings::Get()->SetString(chromeos::kReleaseChannel,
                                             channel);
  }
}

#endif  // defined(OS_CHROMEOS)

void HelpHandler::SetUpdateStatus(VersionUpdater::Status status,
                                  int progress, const string16& message) {
  // Only UPDATING state should have progress set.
  DCHECK(status == VersionUpdater::UPDATING || progress == 0);

  std::string status_str;
  switch (status) {
  case VersionUpdater::CHECKING:
    status_str = "checking";
    break;
  case VersionUpdater::UPDATING:
    status_str = "updating";
    break;
  case VersionUpdater::NEARLY_UPDATED:
    status_str = "nearly_updated";
    break;
  case VersionUpdater::UPDATED:
    status_str = "updated";
    break;
  case VersionUpdater::FAILED:
    status_str = "failed";
    break;
  case VersionUpdater::DISABLED:
    status_str = "disabled";
    break;
  }

  scoped_ptr<Value> status_value(Value::CreateStringValue(status_str));
  scoped_ptr<Value> message_value(Value::CreateStringValue(message));
  web_ui()->CallJavascriptFunction("help.HelpPage.setUpdateStatus",
                                   *status_value, *message_value);

  if (status == VersionUpdater::UPDATING) {
    scoped_ptr<Value> progress_value(Value::CreateIntegerValue(progress));
    web_ui()->CallJavascriptFunction("help.HelpPage.setProgress",
                                     *progress_value);
  }
}

#if defined(OS_MACOSX)
void HelpHandler::SetPromotionState(VersionUpdater::PromotionState state) {
  std::string state_str;
  switch (state) {
  case VersionUpdater::PROMOTE_HIDDEN:
    state_str = "hidden";
    break;
  case VersionUpdater::PROMOTE_ENABLED:
    state_str = "enabled";
    break;
  case VersionUpdater::PROMOTE_DISABLED:
    state_str = "disabled";
    break;
  }

  scoped_ptr<Value> state_value(Value::CreateStringValue(state_str));
  web_ui()->CallJavascriptFunction("help.HelpPage.setPromotionState",
                                   *state_value);
}
#endif  // defined(OS_MACOSX)

#if defined(OS_CHROMEOS)
void HelpHandler::OnOSVersion(chromeos::VersionLoader::Handle handle,
                              std::string version) {
  scoped_ptr<Value> version_string(Value::CreateStringValue(version));
  web_ui()->CallJavascriptFunction("help.HelpPage.setOSVersion",
                                   *version_string);
}

void HelpHandler::OnOSFirmware(chromeos::VersionLoader::Handle handle,
                               std::string firmware) {
  scoped_ptr<Value> firmware_string(Value::CreateStringValue(firmware));
  web_ui()->CallJavascriptFunction("help.HelpPage.setOSFirmware",
                                   *firmware_string);
}

void HelpHandler::OnReleaseChannel(const std::string& channel) {
  scoped_ptr<Value> channel_string(Value::CreateStringValue(channel));
  web_ui()->CallJavascriptFunction(
      "help.HelpPage.updateSelectedChannel", *channel_string);
}

void HelpHandler::ProcessLsbFileInfo(
    base::PlatformFileError error, const base::PlatformFileInfo& file_info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // If |g_last_updated_string| is not |NULL|, then the file's information has
  // already been retrieved by another tab.
  if (g_last_updated_string == NULL) {
    base::Time time;
    if (error == base::PLATFORM_FILE_OK) {
      // Retrieves the approximate time at which Chrome OS was last updated.
      // Each time a new build is created, /etc/lsb-release is modified with the
      // new version numbers of the release.
      time = file_info.last_modified;
    } else {
      // If the time of the last update cannot be retrieved, return and do not
      // display the "Last Updated" section.
      return;
    }

    // Note that this string will be internationalized.
    string16 last_updated = base::TimeFormatFriendlyDate(time);
    g_last_updated_string = Value::CreateStringValue(last_updated);
  }

  web_ui()->CallJavascriptFunction("help.HelpPage.setLastUpdated",
                                   *g_last_updated_string);
}
#endif // defined(OS_CHROMEOS)
