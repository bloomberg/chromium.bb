// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/help/help_handler.h"

#include <stddef.h>

#include <string>

#include "ash/common/system/chromeos/devicetype_utils.h"
#include "base/base_switches.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/i18n/message_formatter.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task_scheduler/post_task.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/obsolete_system/obsolete_system.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/google/core/browser/google_util.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/policy_constants.h"
#include "components/strings/grit/components_chromium_strings.h"
#include "components/strings/grit/components_strings.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/l10n/l10n_util.h"
#include "v8/include/v8-version-string.h"

#if defined(OS_CHROMEOS)
#include "base/files/file_util_proxy.h"
#include "base/i18n/time_formatting.h"
#include "base/sys_info.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service_chromeos.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service_chromeos_factory.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chromeos/image_source.h"
#include "chrome/browser/ui/webui/help/help_utils_chromeos.h"
#include "chrome/browser/ui/webui/help/version_updater_chromeos.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager_client.h"
#include "chromeos/system/statistics_provider.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#endif

using base::ListValue;
using content::BrowserThread;

namespace {

#if defined(OS_CHROMEOS)

// Directory containing the regulatory labels for supported regions.
const char kRegulatoryLabelsDirectory[] = "regulatory_labels";

// File names of the image file and the file containing alt text for the label.
const char kRegulatoryLabelImageFilename[] = "label.png";
const char kRegulatoryLabelTextFilename[] = "label.txt";

// Default region code to use if there's no label for the VPD region code.
const char kDefaultRegionCode[] = "us";

struct RegulatoryLabel {
  const std::string label_text;
  const std::string image_url;
};

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
bool CanChangeChannel(Profile* profile) {
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
    const user_manager::User* user =
        profile ? chromeos::ProfileHelper::Get()->GetUserByProfile(profile)
                : nullptr;
    std::string email =
        user ? user->GetAccountId().GetUserEmail() : std::string();
    size_t at_pos = email.find('@');
    if (at_pos != std::string::npos && at_pos + 1 < email.length())
      domain = email.substr(email.find('@') + 1);
    policy::BrowserPolicyConnectorChromeOS* connector =
        g_browser_process->platform_part()->browser_policy_connector_chromeos();
    return domain == connector->GetEnterpriseDomain();
  } else {
    chromeos::OwnerSettingsServiceChromeOS* service =
        chromeos::OwnerSettingsServiceChromeOSFactory::GetInstance()
            ->GetForBrowserContext(profile);
    // On non managed machines we have local owner who is the only one to change
    // anything. Ensure that ReleaseChannelDelegated is false.
    if (service && service->IsOwner())
      return !value;
  }
  return false;
}

// Returns the path of the regulatory labels directory for a given region, if
// found. Must be called from the blocking pool.
base::FilePath GetRegulatoryLabelDirForRegion(const std::string& region) {
  // Generate the path under the asset dir or URL host to the regulatory files
  // for the region, e.g., "regulatory_labels/us/".
  const base::FilePath region_path =
      base::FilePath(kRegulatoryLabelsDirectory).AppendASCII(region);

  // Check for file existence starting in /usr/share/chromeos-assets/, e.g.,
  // "/usr/share/chromeos-assets/regulatory_labels/us/label.png".
  const base::FilePath asset_dir(chrome::kChromeOSAssetPath);
  if (base::PathExists(asset_dir.Append(region_path)
                           .AppendASCII(kRegulatoryLabelImageFilename))) {
    return region_path;
  }

  return base::FilePath();
}

// Finds the directory for the regulatory label, using the VPD region code.
// Also tries "us" as a fallback region. Must be called from the blocking pool.
base::FilePath FindRegulatoryLabelDir() {
  std::string region;
  base::FilePath region_path;
  // Use the VPD region code to find the label dir.
  if (chromeos::system::StatisticsProvider::GetInstance()->GetMachineStatistic(
          "region", &region) && !region.empty()) {
    region_path = GetRegulatoryLabelDirForRegion(region);
  }

  // Try the fallback region code if no directory was found.
  if (region_path.empty() && region != kDefaultRegionCode)
    region_path = GetRegulatoryLabelDirForRegion(kDefaultRegionCode);

  return region_path;
}

// Reads the file containing the regulatory label text, if found, relative to
// the asset directory. Must be called from the blocking pool.
std::string ReadRegulatoryLabelText(const base::FilePath& path) {
  base::FilePath text_path(chrome::kChromeOSAssetPath);
  text_path = text_path.Append(path);
  text_path = text_path.AppendASCII(kRegulatoryLabelTextFilename);

  std::string contents;
  if (base::ReadFileToString(text_path, &contents))
    return contents;
  return std::string();
}
#endif  // defined(OS_CHROMEOS)

}  // namespace

HelpHandler::HelpHandler()
    : policy_registrar_(
        g_browser_process->policy_service(),
        policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME, std::string())),
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
    {"aboutTitle", IDS_ABOUT_TITLE},
#if defined(OS_CHROMEOS)
    {"aboutProductTitle", IDS_PRODUCT_OS_NAME},
#else
    {"aboutProductTitle", IDS_PRODUCT_NAME},
#endif
    {"aboutProductDescription", IDS_ABOUT_PRODUCT_DESCRIPTION},
    {"relaunch", IDS_RELAUNCH_BUTTON},
#if defined(OS_CHROMEOS)
    {"relaunchAndPowerwash", IDS_RELAUNCH_AND_POWERWASH_BUTTON},
#endif
    {"productName", IDS_PRODUCT_NAME},
    {"updateCheckStarted", IDS_UPGRADE_CHECK_STARTED},
    {"updating", IDS_UPGRADE_UPDATING},
#if defined(OS_CHROMEOS) || defined(OS_WIN)
    {"updateDisabledByPolicy", IDS_UPGRADE_DISABLED_BY_POLICY},
#endif  // defined(OS_CHROMEOS) || defined(OS_WIN)
#if defined(OS_CHROMEOS)
    {"updateButton", IDS_UPGRADE_BUTTON},
    {"updatingChannelSwitch", IDS_UPGRADE_UPDATING_CHANNEL_SWITCH},
#endif
    {"updateAlmostDone", IDS_UPGRADE_SUCCESSFUL_RELAUNCH},
#if defined(OS_CHROMEOS)
    {"successfulChannelSwitch", IDS_UPGRADE_SUCCESSFUL_CHANNEL_SWITCH},
#endif
    {"getHelpWithChrome", IDS_GET_HELP_USING_CHROME},
    {"reportAnIssue", IDS_REPORT_AN_ISSUE},
#if defined(OS_CHROMEOS)
    {"platform", IDS_PLATFORM_LABEL},
    {"arcVersion", IDS_ARC_VERSION_LABEL},
    {"firmware", IDS_ABOUT_PAGE_FIRMWARE},
    {"showMoreInfo", IDS_SHOW_MORE_INFO},
    {"hideMoreInfo", IDS_HIDE_MORE_INFO},
    {"channel", IDS_ABOUT_PAGE_CHANNEL},
    {"stable", IDS_ABOUT_PAGE_CHANNEL_STABLE},
    {"beta", IDS_ABOUT_PAGE_CHANNEL_BETA},
    {"dev", IDS_ABOUT_PAGE_CHANNEL_DEVELOPMENT},
    {"devChannelDisclaimer", IDS_ABOUT_PAGE_CHANNEL_DEVELOPMENT_DISCLAIMER},
    {"channel-changed", IDS_ABOUT_PAGE_CHANNEL_CHANGED},
    {"currentChannelStable", IDS_ABOUT_PAGE_CURRENT_CHANNEL_STABLE},
    {"currentChannelBeta", IDS_ABOUT_PAGE_CURRENT_CHANNEL_BETA},
    {"currentChannelDev", IDS_ABOUT_PAGE_CURRENT_CHANNEL_DEV},
    {"currentChannel", IDS_ABOUT_PAGE_CURRENT_CHANNEL},
    {"channelChangeButton", IDS_ABOUT_PAGE_CHANNEL_CHANGE_BUTTON},
    {"channelChangeDisallowedMessage",
     IDS_ABOUT_PAGE_CHANNEL_CHANGE_DISALLOWED_MESSAGE},
    {"channelChangePageTitle", IDS_ABOUT_PAGE_CHANNEL_CHANGE_PAGE_TITLE},
    {"channelChangePagePowerwashTitle",
     IDS_ABOUT_PAGE_CHANNEL_CHANGE_PAGE_POWERWASH_TITLE},
    {"channelChangePagePowerwashMessage",
     IDS_ABOUT_PAGE_CHANNEL_CHANGE_PAGE_POWERWASH_MESSAGE},
    {"channelChangePageDelayedChangeTitle",
     IDS_ABOUT_PAGE_CHANNEL_CHANGE_PAGE_DELAYED_CHANGE_TITLE},
    {"channelChangePageUnstableTitle",
     IDS_ABOUT_PAGE_CHANNEL_CHANGE_PAGE_UNSTABLE_TITLE},
    {"channelChangePagePowerwashButton",
     IDS_ABOUT_PAGE_CHANNEL_CHANGE_PAGE_POWERWASH_BUTTON},
    {"channelChangePageChangeButton",
     IDS_ABOUT_PAGE_CHANNEL_CHANGE_PAGE_CHANGE_BUTTON},
    {"channelChangePageCancelButton",
     IDS_ABOUT_PAGE_CHANNEL_CHANGE_PAGE_CANCEL_BUTTON},
    {"userAgent", IDS_VERSION_UI_USER_AGENT},
    {"commandLine", IDS_VERSION_UI_COMMAND_LINE},
    {"buildDate", IDS_VERSION_UI_BUILD_DATE},
#endif
#if defined(OS_MACOSX) || defined(OS_WIN)
    {"learnMore", IDS_LEARN_MORE},
#endif
#if defined(OS_MACOSX)
    {"promote", IDS_ABOUT_CHROME_PROMOTE_UPDATER},
#endif
  };

  for (size_t i = 0; i < arraysize(resources); ++i) {
    localized_strings->SetString(resources[i].name,
                                 l10n_util::GetStringUTF16(resources[i].ids));
  }

#if defined(OS_CHROMEOS)
  localized_strings->SetString("upToDate", ash::SubstituteChromeOSDeviceType(
      IDS_UPGRADE_UP_TO_DATE));
#else
  localized_strings->SetString("upToDate", l10n_util::GetStringUTF16(
      IDS_UPGRADE_UP_TO_DATE));
#endif

  localized_strings->SetString("updateObsoleteSystem",
                               ObsoleteSystem::LocalizedObsoleteString());
  localized_strings->SetString("updateObsoleteSystemURL",
                               ObsoleteSystem::GetLinkURL());

  localized_strings->SetString(
      "browserVersion",
      l10n_util::GetStringFUTF16(IDS_ABOUT_PRODUCT_VERSION,
                                 BuildBrowserVersionString()));

  localized_strings->SetString(
      "productCopyright",
      base::i18n::MessageFormatter::FormatWithNumberedArgs(
          l10n_util::GetStringUTF16(IDS_ABOUT_VERSION_COPYRIGHT),
          base::Time::Now()));

  base::string16 license = l10n_util::GetStringFUTF16(
      IDS_VERSION_UI_LICENSE, base::ASCIIToUTF16(chrome::kChromiumProjectURL),
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

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kDisableNewChannelSwitcherUI)) {
    localized_strings->SetBoolean("disableNewChannelSwitcherUI", true);
  }

  localized_strings->SetString(
      "eolLearnMore", l10n_util::GetStringFUTF16(
                          IDS_ABOUT_PAGE_EOL_LEARN_MORE,
                          base::ASCIIToUTF16(chrome::kEolNotificationURL)));
#endif

  base::string16 tos = l10n_util::GetStringFUTF16(
      IDS_ABOUT_TERMS_OF_SERVICE, base::UTF8ToUTF16(chrome::kChromeUITermsURL));
  localized_strings->SetString("productTOS", tos);

  localized_strings->SetString("jsEngine", "V8");
  localized_strings->SetString("jsEngineVersion", V8_VERSION_STRING);

  localized_strings->SetString("userAgentInfo", GetUserAgent());

  base::CommandLine::StringType command_line =
      base::CommandLine::ForCurrentProcess()->GetCommandLineString();
  localized_strings->SetString("commandLineInfo", command_line);
}

void HelpHandler::RegisterMessages() {
  version_updater_.reset(VersionUpdater::Create(web_ui()->GetWebContents()));
  registrar_.Add(this, chrome::NOTIFICATION_UPGRADE_RECOMMENDED,
                 content::NotificationService::AllSources());
  policy_registrar_.Observe(
      policy::key::kDeviceAutoUpdateDisabled,
      base::Bind(&HelpHandler::OnDeviceAutoUpdatePolicyChanged,
                 base::Unretained(this)));

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

#if defined(OS_CHROMEOS)
  // Handler for the product label image, which will be shown if available.
  content::URLDataSource::Add(Profile::FromWebUI(web_ui()),
                              new chromeos::ImageSource());
#endif
}

void HelpHandler::Observe(int type, const content::NotificationSource& source,
                          const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_UPGRADE_RECOMMENDED, type);

  // A version update is installed and ready to go. Refresh the UI so the
  // correct state will be shown.
  RequestUpdate(nullptr);
}

// static
base::string16 HelpHandler::BuildBrowserVersionString() {
  std::string version = version_info::GetVersionNumber();

  std::string modifier = chrome::GetChannelString();
  if (!modifier.empty())
    version += " " + modifier;

#if defined(ARCH_CPU_64_BITS)
  version += " (64-bit)";
#endif

  return base::UTF8ToUTF16(version);
}

void HelpHandler::OnDeviceAutoUpdatePolicyChanged(
    const base::Value* previous_policy,
    const base::Value* current_policy) {
  bool previous_auto_update_disabled = false;
  if (previous_policy)
    CHECK(previous_policy->GetAsBoolean(&previous_auto_update_disabled));

  bool current_auto_update_disabled = false;
  if (current_policy)
    CHECK(current_policy->GetAsBoolean(&current_auto_update_disabled));

  if (current_auto_update_disabled != previous_auto_update_disabled) {
    // Refresh the update status to refresh the status of the UI.
    RefreshUpdateStatus();
  }
}

void HelpHandler::RefreshUpdateStatus() {
  // On Chrome OS, do not check for an update automatically.
#if defined(OS_CHROMEOS)
  static_cast<VersionUpdaterCros*>(version_updater_.get())->GetUpdateStatus(
      base::Bind(&HelpHandler::SetUpdateStatus, base::Unretained(this)));
#else
  RequestUpdate(NULL);
#endif
}

void HelpHandler::OnPageLoaded(const base::ListValue* args) {
#if defined(OS_CHROMEOS)
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, base::TaskTraits().MayBlock().WithPriority(
                     base::TaskPriority::BACKGROUND),
      base::Bind(&chromeos::version_loader::GetVersion,
                 chromeos::version_loader::VERSION_FULL),
      base::Bind(&HelpHandler::OnOSVersion, weak_factory_.GetWeakPtr()));
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, base::TaskTraits().MayBlock().WithPriority(
                     base::TaskPriority::BACKGROUND),
      base::Bind(&chromeos::version_loader::GetARCVersion),
      base::Bind(&HelpHandler::OnARCVersion, weak_factory_.GetWeakPtr()));
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, base::TaskTraits().MayBlock().WithPriority(
                     base::TaskPriority::BACKGROUND),
      base::Bind(&chromeos::version_loader::GetFirmware),
      base::Bind(&HelpHandler::OnOSFirmware, weak_factory_.GetWeakPtr()));

  web_ui()->CallJavascriptFunctionUnsafe(
      "help.HelpPage.updateEnableReleaseChannel",
      base::Value(CanChangeChannel(Profile::FromWebUI(web_ui()))));

  base::Time build_time = base::SysInfo::GetLsbReleaseTime();
  base::string16 build_date = base::TimeFormatFriendlyDate(build_time);
  web_ui()->CallJavascriptFunctionUnsafe("help.HelpPage.setBuildDate",
                                         base::StringValue(build_date));
#endif  // defined(OS_CHROMEOS)

  RefreshUpdateStatus();

  web_ui()->CallJavascriptFunctionUnsafe(
      "help.HelpPage.setObsoleteSystem",
      base::Value(ObsoleteSystem::IsObsoleteNowOrSoon()));
  web_ui()->CallJavascriptFunctionUnsafe(
      "help.HelpPage.setObsoleteSystemEndOfTheLine",
      base::Value(ObsoleteSystem::IsObsoleteNowOrSoon() &&
                  ObsoleteSystem::IsEndOfTheLine()));

#if defined(OS_CHROMEOS)
  web_ui()->CallJavascriptFunctionUnsafe(
      "help.HelpPage.updateIsEnterpriseManaged",
      base::Value(IsEnterpriseManaged()));
  // First argument to GetChannel() is a flag that indicates whether
  // current channel should be returned (if true) or target channel
  // (otherwise).
  version_updater_->GetChannel(true,
      base::Bind(&HelpHandler::OnCurrentChannel, weak_factory_.GetWeakPtr()));
  version_updater_->GetChannel(false,
      base::Bind(&HelpHandler::OnTargetChannel, weak_factory_.GetWeakPtr()));

  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kDisableEolNotification)) {
    version_updater_->GetEolStatus(
        base::Bind(&HelpHandler::OnEolStatus, weak_factory_.GetWeakPtr()));
  }

  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, base::TaskTraits().MayBlock().WithPriority(
                     base::TaskPriority::BACKGROUND),
      base::Bind(&FindRegulatoryLabelDir),
      base::Bind(&HelpHandler::OnRegulatoryLabelDirFound,
                 weak_factory_.GetWeakPtr()));
#endif
}

#if defined(OS_MACOSX)
void HelpHandler::PromoteUpdater(const base::ListValue* args) {
  version_updater_->PromoteUpdater();
}
#endif

void HelpHandler::RelaunchNow(const base::ListValue* args) {
  DCHECK(args->empty());
  chrome::AttemptRelaunch();
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

  if (!CanChangeChannel(Profile::FromWebUI(web_ui()))) {
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
                                                base::Unretained(this)),
                                     VersionUpdater::PromoteCallback());
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
  VersionUpdater::PromoteCallback promote_callback;
  version_updater_->CheckForUpdate(
      base::Bind(&HelpHandler::SetUpdateStatus, base::Unretained(this)),
#if defined(OS_MACOSX)
      base::Bind(&HelpHandler::SetPromotionState, base::Unretained(this)));
#else
      VersionUpdater::PromoteCallback());
#endif  // OS_MACOSX
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
  case VersionUpdater::DISABLED_BY_ADMIN:
    status_str = "disabled_by_admin";
    break;
  }

  web_ui()->CallJavascriptFunctionUnsafe("help.HelpPage.setUpdateStatus",
                                         base::StringValue(status_str),
                                         base::StringValue(message));

  if (status == VersionUpdater::UPDATING) {
    web_ui()->CallJavascriptFunctionUnsafe("help.HelpPage.setProgress",
                                           base::Value(progress));
  }

#if defined(OS_CHROMEOS)
  if (status == VersionUpdater::FAILED_OFFLINE ||
      status == VersionUpdater::FAILED_CONNECTION_TYPE_DISALLOWED) {
    base::string16 types_msg = GetAllowedConnectionTypesMessage();
    if (!types_msg.empty()) {
      web_ui()->CallJavascriptFunctionUnsafe(
          "help.HelpPage.setAndShowAllowedConnectionTypesMsg",
          base::StringValue(types_msg));
    } else {
      web_ui()->CallJavascriptFunctionUnsafe(
          "help.HelpPage.showAllowedConnectionTypesMsg", base::Value(false));
    }
  } else {
    web_ui()->CallJavascriptFunctionUnsafe(
        "help.HelpPage.showAllowedConnectionTypesMsg", base::Value(false));
  }
#endif  // defined(OS_CHROMEOS)
}

#if defined(OS_MACOSX)
void HelpHandler::SetPromotionState(VersionUpdater::PromotionState state) {
  std::string state_str;
  switch (state) {
  case VersionUpdater::PROMOTE_HIDDEN:
  case VersionUpdater::PROMOTED:
    state_str = "hidden";
    break;
  case VersionUpdater::PROMOTE_ENABLED:
    state_str = "enabled";
    break;
  case VersionUpdater::PROMOTE_DISABLED:
    state_str = "disabled";
    break;
  }

  web_ui()->CallJavascriptFunctionUnsafe("help.HelpPage.setPromotionState",
                                         base::StringValue(state_str));
}
#endif  // defined(OS_MACOSX)

#if defined(OS_CHROMEOS)
void HelpHandler::OnOSVersion(const std::string& version) {
  web_ui()->CallJavascriptFunctionUnsafe("help.HelpPage.setOSVersion",
                                         base::StringValue(version));
}

void HelpHandler::OnARCVersion(const std::string& firmware) {
  web_ui()->CallJavascriptFunctionUnsafe("help.HelpPage.setARCVersion",
                                         base::StringValue(firmware));
}

void HelpHandler::OnOSFirmware(const std::string& firmware) {
  web_ui()->CallJavascriptFunctionUnsafe("help.HelpPage.setOSFirmware",
                                         base::StringValue(firmware));
}

void HelpHandler::OnCurrentChannel(const std::string& channel) {
  web_ui()->CallJavascriptFunctionUnsafe("help.HelpPage.updateCurrentChannel",
                                         base::StringValue(channel));
}

void HelpHandler::OnTargetChannel(const std::string& channel) {
  web_ui()->CallJavascriptFunctionUnsafe("help.HelpPage.updateTargetChannel",
                                         base::StringValue(channel));
}

void HelpHandler::OnRegulatoryLabelDirFound(const base::FilePath& path) {
  if (path.empty())
    return;

  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, base::TaskTraits().MayBlock().WithPriority(
                     base::TaskPriority::BACKGROUND),
      base::Bind(&ReadRegulatoryLabelText, path),
      base::Bind(&HelpHandler::OnRegulatoryLabelTextRead,
                 weak_factory_.GetWeakPtr()));

  // Send the image path to the WebUI.
  OnRegulatoryLabelImageFound(path.AppendASCII(kRegulatoryLabelImageFilename));
}

void HelpHandler::OnRegulatoryLabelImageFound(const base::FilePath& path) {
  std::string url = std::string("chrome://") + chrome::kChromeOSAssetHost +
      "/" + path.MaybeAsASCII();
  web_ui()->CallJavascriptFunctionUnsafe("help.HelpPage.setRegulatoryLabelPath",
                                         base::StringValue(url));
}

void HelpHandler::OnRegulatoryLabelTextRead(const std::string& text) {
  // Remove unnecessary whitespace.
  web_ui()->CallJavascriptFunctionUnsafe(
      "help.HelpPage.setRegulatoryLabelText",
      base::StringValue(base::CollapseWhitespaceASCII(text, true)));
}

void HelpHandler::OnEolStatus(update_engine::EndOfLifeStatus status) {
  // Security only state is no longer supported.
  if (status == update_engine::EndOfLifeStatus::kSecurityOnly ||
      IsEnterpriseManaged()) {
    return;
  }

  if (status == update_engine::EndOfLifeStatus::kSupported) {
    web_ui()->CallJavascriptFunctionUnsafe(
        "help.HelpPage.updateEolMessage", base::StringValue("device_supported"),
        base::StringValue(""));
  } else {
    web_ui()->CallJavascriptFunctionUnsafe(
        "help.HelpPage.updateEolMessage", base::StringValue("device_endoflife"),
        base::StringValue(l10n_util::GetStringUTF16(IDS_ABOUT_PAGE_EOL_EOL)));
  }
}

#endif  // defined(OS_CHROMEOS)
