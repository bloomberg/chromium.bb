// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/help/help_handler.h"

#include <string>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/google_chrome_strings.h"
#include "components/google/core/browser/google_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/user_agent.h"
#include "grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "v8/include/v8.h"

#if defined(OS_MACOSX)
#include "chrome/browser/mac/obsolete_system.h"
#endif

#if defined(OS_CHROMEOS)
#include "base/files/file_util_proxy.h"
#include "base/i18n/time_formatting.h"
#include "base/prefs/pref_service.h"
#include "base/sys_info.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/help/help_utils_chromeos.h"
#include "chrome/browser/ui/webui/help/version_updater_chromeos.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager_client.h"
#include "components/user_manager/user_manager.h"
#endif

using base::ListValue;
using content::BrowserThread;

namespace {

#if defined(OS_CHROMEOS)

// Returns message that informs user that for update it's better to
// connect to a network of one of the allowed types.
base::string16 GetAllowedConnectionTypesMessage() {
  if (help_utils_chromeos::IsUpdateOverCellularAllowed()) {
    return l10n_util::GetStringUTF16(IDS_UPGRADE_NETWORK_LIST_CELLULAR_ALLOWED);
  } else {
    return l10n_util::GetStringUTF16(
        IDS_UPGRADE_NETWORK_LIST_CELLULAR_DISALLOWED);
  }
}

// Returns true if the device is enterprise managed, false otherwise.
bool IsEnterpriseManaged() {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  return connector->IsEnterpriseManaged();
}

// Returns true if current user can change channel, false otherwise.
bool CanChangeChannel() {
  bool value = false;
  chromeos::CrosSettings::Get()->GetBoolean(chromeos::kReleaseChannelDelegated,
                                            &value);

  // On a managed machine we delegate this setting to the users of the same
  // domain only if the policy value is "domain".
  if (IsEnterpriseManaged()) {
    if (!value)
      return false;
    // Get the currently logged in user and strip the domain part only.
    std::string domain = "";
    std::string user =
        user_manager::UserManager::Get()->GetLoggedInUser()->email();
    size_t at_pos = user.find('@');
    if (at_pos != std::string::npos && at_pos + 1 < user.length())
      domain = user.substr(user.find('@') + 1);
    policy::BrowserPolicyConnectorChromeOS* connector =
        g_browser_process->platform_part()->browser_policy_connector_chromeos();
    return domain == connector->GetEnterpriseDomain();
  } else if (user_manager::UserManager::Get()->IsCurrentUserOwner()) {
    // On non managed machines we have local owner who is the only one to change
    // anything. Ensure that ReleaseChannelDelegated is false.
    return !value;
  }
  return false;
}

#endif  // defined(OS_CHROMEOS)

}  // namespace

HelpHandler::HelpHandler()
    : version_updater_(VersionUpdater::Create()),
      weak_factory_(this) {
}

HelpHandler::~HelpHandler() {
}

void HelpHandler::GetLocalizedValues(base::DictionaryValue* localized_strings) {
  struct L10nResources {
    const char* name;
    int ids;
  };

  static L10nResources resources[] = {
    { "aboutTitle", IDS_ABOUT_TITLE },
#if defined(OS_CHROMEOS)
    { "aboutProductTitle", IDS_PRODUCT_OS_NAME },
#else
    { "aboutProductTitle", IDS_PRODUCT_NAME },
#endif
    { "aboutProductDescription", IDS_ABOUT_PRODUCT_DESCRIPTION },
    { "relaunch", IDS_RELAUNCH_BUTTON },
#if defined(OS_CHROMEOS)
    { "relaunchAndPowerwash", IDS_RELAUNCH_AND_POWERWASH_BUTTON },
#endif
    { "productName", IDS_PRODUCT_NAME },
    { "updateCheckStarted", IDS_UPGRADE_CHECK_STARTED },
    { "upToDate", IDS_UPGRADE_UP_TO_DATE },
    { "updating", IDS_UPGRADE_UPDATING },
#if defined(OS_CHROMEOS)
    { "updateButton", IDS_UPGRADE_BUTTON },
    { "updatingChannelSwitch", IDS_UPGRADE_UPDATING_CHANNEL_SWITCH },
#endif
    { "updateAlmostDone", IDS_UPGRADE_SUCCESSFUL_RELAUNCH },
#if defined(OS_CHROMEOS)
    { "successfulChannelSwitch", IDS_UPGRADE_SUCCESSFUL_CHANNEL_SWITCH },
#endif
    { "getHelpWithChrome", IDS_GET_HELP_USING_CHROME },
    { "reportAnIssue", IDS_REPORT_AN_ISSUE },
#if defined(OS_CHROMEOS)
    { "platform", IDS_PLATFORM_LABEL },
    { "firmware", IDS_ABOUT_PAGE_FIRMWARE },
    { "showMoreInfo", IDS_SHOW_MORE_INFO },
    { "hideMoreInfo", IDS_HIDE_MORE_INFO },
    { "channel", IDS_ABOUT_PAGE_CHANNEL },
    { "stable", IDS_ABOUT_PAGE_CHANNEL_STABLE },
    { "beta", IDS_ABOUT_PAGE_CHANNEL_BETA },
    { "dev", IDS_ABOUT_PAGE_CHANNEL_DEVELOPMENT },
    { "channel-changed", IDS_ABOUT_PAGE_CHANNEL_CHANGED },
    { "currentChannelStable", IDS_ABOUT_PAGE_CURRENT_CHANNEL_STABLE },
    { "currentChannelBeta", IDS_ABOUT_PAGE_CURRENT_CHANNEL_BETA },
    { "currentChannelDev", IDS_ABOUT_PAGE_CURRENT_CHANNEL_DEV },
    { "currentChannel", IDS_ABOUT_PAGE_CURRENT_CHANNEL },
    { "channelChangeButton", IDS_ABOUT_PAGE_CHANNEL_CHANGE_BUTTON },
    { "channelChangeDisallowedMessage",
      IDS_ABOUT_PAGE_CHANNEL_CHANGE_DISALLOWED_MESSAGE },
    { "channelChangePageTitle", IDS_ABOUT_PAGE_CHANNEL_CHANGE_PAGE_TITLE },
    { "channelChangePagePowerwashTitle",
      IDS_ABOUT_PAGE_CHANNEL_CHANGE_PAGE_POWERWASH_TITLE },
    { "channelChangePagePowerwashMessage",
      IDS_ABOUT_PAGE_CHANNEL_CHANGE_PAGE_POWERWASH_MESSAGE },
    { "channelChangePageDelayedChangeTitle",
      IDS_ABOUT_PAGE_CHANNEL_CHANGE_PAGE_DELAYED_CHANGE_TITLE },
    { "channelChangePageUnstableTitle",
      IDS_ABOUT_PAGE_CHANNEL_CHANGE_PAGE_UNSTABLE_TITLE },
    { "channelChangePagePowerwashButton",
      IDS_ABOUT_PAGE_CHANNEL_CHANGE_PAGE_POWERWASH_BUTTON },
    { "channelChangePageChangeButton",
      IDS_ABOUT_PAGE_CHANNEL_CHANGE_PAGE_CHANGE_BUTTON },
    { "channelChangePageCancelButton",
      IDS_ABOUT_PAGE_CHANNEL_CHANGE_PAGE_CANCEL_BUTTON },
    { "webkit", IDS_WEBKIT },
    { "userAgent", IDS_ABOUT_VERSION_USER_AGENT },
    { "commandLine", IDS_ABOUT_VERSION_COMMAND_LINE },
    { "buildDate", IDS_ABOUT_VERSION_BUILD_DATE },
#endif
#if defined(OS_MACOSX)
    { "promote", IDS_ABOUT_CHROME_PROMOTE_UPDATER },
    { "learnMore", IDS_LEARN_MORE },
#endif
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(resources); ++i) {
    localized_strings->SetString(resources[i].name,
                                 l10n_util::GetStringUTF16(resources[i].ids));
  }

#if defined(OS_MACOSX)
  localized_strings->SetString(
      "updateObsoleteSystem",
      ObsoleteSystemMac::LocalizedObsoleteSystemString());
  localized_strings->SetString(
      "updateObsoleteSystemURL",
      chrome::kMac32BitDeprecationURL);
#endif

  localized_strings->SetString(
      "browserVersion",
      l10n_util::GetStringFUTF16(IDS_ABOUT_PRODUCT_VERSION,
                                 BuildBrowserVersionString()));

  base::Time::Exploded exploded_time;
  base::Time::Now().LocalExplode(&exploded_time);
  localized_strings->SetString(
      "productCopyright",
       l10n_util::GetStringFUTF16(IDS_ABOUT_VERSION_COPYRIGHT,
                                  base::IntToString16(exploded_time.year)));

  base::string16 license = l10n_util::GetStringFUTF16(
      IDS_ABOUT_VERSION_LICENSE,
      base::ASCIIToUTF16(chrome::kChromiumProjectURL),
      base::ASCIIToUTF16(chrome::kChromeUICreditsURL));
  localized_strings->SetString("productLicense", license);

#if defined(OS_CHROMEOS)
  base::string16 os_license = l10n_util::GetStringFUTF16(
      IDS_ABOUT_CROS_VERSION_LICENSE,
      base::ASCIIToUTF16(chrome::kChromeUIOSCreditsURL));
  localized_strings->SetString("productOsLicense", os_license);

  base::string16 product_name = l10n_util::GetStringUTF16(IDS_PRODUCT_OS_NAME);
  localized_strings->SetString(
      "channelChangePageDelayedChangeMessage",
      l10n_util::GetStringFUTF16(
          IDS_ABOUT_PAGE_CHANNEL_CHANGE_PAGE_DELAYED_CHANGE_MESSAGE,
          product_name));
  localized_strings->SetString(
      "channelChangePageUnstableMessage",
      l10n_util::GetStringFUTF16(
          IDS_ABOUT_PAGE_CHANNEL_CHANGE_PAGE_UNSTABLE_MESSAGE,
          product_name));

  if (CommandLine::ForCurrentProcess()->
      HasSwitch(chromeos::switches::kDisableNewChannelSwitcherUI)) {
    localized_strings->SetBoolean("disableNewChannelSwitcherUI", true);
  }
#endif

  base::string16 tos = l10n_util::GetStringFUTF16(
      IDS_ABOUT_TERMS_OF_SERVICE, base::UTF8ToUTF16(chrome::kChromeUITermsURL));
  localized_strings->SetString("productTOS", tos);

  localized_strings->SetString("webkitVersion", content::GetWebKitVersion());

  localized_strings->SetString("jsEngine", "V8");
  localized_strings->SetString("jsEngineVersion", v8::V8::GetVersion());

  localized_strings->SetString("userAgentInfo", GetUserAgent());

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
  web_ui()->RegisterMessageCallback("setChannel",
      base::Bind(&HelpHandler::SetChannel, base::Unretained(this)));
  web_ui()->RegisterMessageCallback("relaunchAndPowerwash",
      base::Bind(&HelpHandler::RelaunchAndPowerwash, base::Unretained(this)));
  web_ui()->RegisterMessageCallback("requestUpdate",
      base::Bind(&HelpHandler::RequestUpdate, base::Unretained(this)));
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
      RequestUpdate(NULL);
      break;
    }
    default:
      NOTREACHED();
  }
}

// static
base::string16 HelpHandler::BuildBrowserVersionString() {
  chrome::VersionInfo version_info;
  DCHECK(version_info.is_valid());

  std::string version = version_info.Version();

  std::string modifier = chrome::VersionInfo::GetVersionStringModifier();
  if (!modifier.empty())
    version += " " + modifier;

#if defined(ARCH_CPU_64_BITS)
  version += " (64-bit)";
#endif

  return base::UTF8ToUTF16(version);
}

void HelpHandler::OnPageLoaded(const base::ListValue* args) {
#if defined(OS_CHROMEOS)
  // Version information is loaded from a callback
  loader_.GetVersion(
      chromeos::VersionLoader::VERSION_FULL,
      base::Bind(&HelpHandler::OnOSVersion, base::Unretained(this)),
      &tracker_);
  loader_.GetFirmware(
      base::Bind(&HelpHandler::OnOSFirmware, base::Unretained(this)),
      &tracker_);

  web_ui()->CallJavascriptFunction(
      "help.HelpPage.updateEnableReleaseChannel",
      base::FundamentalValue(CanChangeChannel()));

  base::Time build_time = base::SysInfo::GetLsbReleaseTime();
  base::string16 build_date = base::TimeFormatFriendlyDate(build_time);
  web_ui()->CallJavascriptFunction("help.HelpPage.setBuildDate",
                                   base::StringValue(build_date));
#endif  // defined(OS_CHROMEOS)

  // On Chrome OS, do not check for an update automatically.
#if defined(OS_CHROMEOS)
  static_cast<VersionUpdaterCros*>(version_updater_.get())->GetUpdateStatus(
      base::Bind(&HelpHandler::SetUpdateStatus, base::Unretained(this)));
#else
  RequestUpdate(NULL);
#endif

#if defined(OS_MACOSX)
  web_ui()->CallJavascriptFunction(
      "help.HelpPage.setObsoleteSystem",
      base::FundamentalValue(ObsoleteSystemMac::Is32BitObsoleteNowOrSoon() &&
                             ObsoleteSystemMac::Has32BitOnlyCPU()));
  web_ui()->CallJavascriptFunction(
      "help.HelpPage.setObsoleteSystemEndOfTheLine",
      base::FundamentalValue(ObsoleteSystemMac::Is32BitObsoleteNowOrSoon() &&
                             ObsoleteSystemMac::Is32BitEndOfTheLine()));
#endif

#if defined(OS_CHROMEOS)
  web_ui()->CallJavascriptFunction(
      "help.HelpPage.updateIsEnterpriseManaged",
      base::FundamentalValue(IsEnterpriseManaged()));
  // First argument to GetChannel() is a flag that indicates whether
  // current channel should be returned (if true) or target channel
  // (otherwise).
  version_updater_->GetChannel(true,
      base::Bind(&HelpHandler::OnCurrentChannel, weak_factory_.GetWeakPtr()));
  version_updater_->GetChannel(false,
      base::Bind(&HelpHandler::OnTargetChannel, weak_factory_.GetWeakPtr()));
#endif
}

#if defined(OS_MACOSX)
void HelpHandler::PromoteUpdater(const base::ListValue* args) {
  version_updater_->PromoteUpdater();
}
#endif

void HelpHandler::RelaunchNow(const base::ListValue* args) {
  DCHECK(args->empty());
  version_updater_->RelaunchBrowser();
}

void HelpHandler::OpenFeedbackDialog(const base::ListValue* args) {
  DCHECK(args->empty());
  Browser* browser = chrome::FindBrowserWithWebContents(
      web_ui()->GetWebContents());
  chrome::OpenFeedbackDialog(browser);
}

void HelpHandler::OpenHelpPage(const base::ListValue* args) {
  DCHECK(args->empty());
  Browser* browser = chrome::FindBrowserWithWebContents(
      web_ui()->GetWebContents());
  chrome::ShowHelp(browser, chrome::HELP_SOURCE_WEBUI);
}

#if defined(OS_CHROMEOS)

void HelpHandler::SetChannel(const base::ListValue* args) {
  DCHECK(args->GetSize() == 2);

  if (!CanChangeChannel()) {
    LOG(WARNING) << "Non-owner tried to change release track.";
    return;
  }

  base::string16 channel;
  bool is_powerwash_allowed;
  if (!args->GetString(0, &channel) ||
      !args->GetBoolean(1, &is_powerwash_allowed)) {
    LOG(ERROR) << "Can't parse SetChannel() args";
    return;
  }

  version_updater_->SetChannel(base::UTF16ToUTF8(channel),
                               is_powerwash_allowed);
  if (user_manager::UserManager::Get()->IsCurrentUserOwner()) {
    // Check for update after switching release channel.
    version_updater_->CheckForUpdate(base::Bind(&HelpHandler::SetUpdateStatus,
                                                base::Unretained(this)));
  }
}

void HelpHandler::RelaunchAndPowerwash(const base::ListValue* args) {
  DCHECK(args->empty());

  if (IsEnterpriseManaged())
    return;

  PrefService* prefs = g_browser_process->local_state();
  prefs->SetBoolean(prefs::kFactoryResetRequested, true);
  prefs->CommitPendingWrite();

  // Perform sign out. Current chrome process will then terminate, new one will
  // be launched (as if it was a restart).
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->RequestRestart();
}

#endif  // defined(OS_CHROMEOS)

void HelpHandler::RequestUpdate(const base::ListValue* args) {
  version_updater_->CheckForUpdate(
      base::Bind(&HelpHandler::SetUpdateStatus, base::Unretained(this))
#if defined(OS_MACOSX)
      , base::Bind(&HelpHandler::SetPromotionState, base::Unretained(this))
#endif
      );
}

void HelpHandler::SetUpdateStatus(VersionUpdater::Status status,
                                  int progress, const base::string16& message) {
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
  case VersionUpdater::FAILED_OFFLINE:
  case VersionUpdater::FAILED_CONNECTION_TYPE_DISALLOWED:
    status_str = "failed";
    break;
  case VersionUpdater::DISABLED:
    status_str = "disabled";
    break;
  }

  web_ui()->CallJavascriptFunction("help.HelpPage.setUpdateStatus",
                                   base::StringValue(status_str),
                                   base::StringValue(message));

  if (status == VersionUpdater::UPDATING) {
    web_ui()->CallJavascriptFunction("help.HelpPage.setProgress",
                                     base::FundamentalValue(progress));
  }

#if defined(OS_CHROMEOS)
  if (status == VersionUpdater::FAILED_OFFLINE ||
      status == VersionUpdater::FAILED_CONNECTION_TYPE_DISALLOWED) {
    base::string16 types_msg = GetAllowedConnectionTypesMessage();
    if (!types_msg.empty()) {
      web_ui()->CallJavascriptFunction(
          "help.HelpPage.setAndShowAllowedConnectionTypesMsg",
          base::StringValue(types_msg));
    } else {
      web_ui()->CallJavascriptFunction(
          "help.HelpPage.showAllowedConnectionTypesMsg",
          base::FundamentalValue(false));
    }
  } else {
    web_ui()->CallJavascriptFunction(
        "help.HelpPage.showAllowedConnectionTypesMsg",
        base::FundamentalValue(false));
  }
#endif  // defined(OS_CHROMEOS)
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

  web_ui()->CallJavascriptFunction("help.HelpPage.setPromotionState",
                                   base::StringValue(state_str));
}
#endif  // defined(OS_MACOSX)

#if defined(OS_CHROMEOS)
void HelpHandler::OnOSVersion(const std::string& version) {
  web_ui()->CallJavascriptFunction("help.HelpPage.setOSVersion",
                                   base::StringValue(version));
}

void HelpHandler::OnOSFirmware(const std::string& firmware) {
  web_ui()->CallJavascriptFunction("help.HelpPage.setOSFirmware",
                                   base::StringValue(firmware));
}

void HelpHandler::OnCurrentChannel(const std::string& channel) {
  web_ui()->CallJavascriptFunction(
      "help.HelpPage.updateCurrentChannel", base::StringValue(channel));
}

void HelpHandler::OnTargetChannel(const std::string& channel) {
  web_ui()->CallJavascriptFunction(
      "help.HelpPage.updateTargetChannel", base::StringValue(channel));
}

#endif // defined(OS_CHROMEOS)
