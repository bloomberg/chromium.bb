// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/autotest_private/autotest_private_api.h"

#include <memory>
#include <sstream>
#include <utility>

#include "base/lazy_instance.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/autotest_private.h"
#include "extensions/browser/extension_function_registry.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/manifest_handlers/options_page_info.h"
#include "extensions/common/permissions/api_permission_set.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"

#if defined(OS_CHROMEOS)
#include "ash/public/interfaces/ash_message_center_controller.mojom.h"
#include "ash/public/interfaces/constants.mojom.h"
#include "ash/shell.h"
#include "base/base64.h"
#include "base/feature_list.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_pref_names.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/chromeos/login/lock/screen_locker.h"
#include "chrome/browser/chromeos/printing/cups_printers_manager.h"
#include "chrome/browser/chromeos/system/input_device_settings.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/login_screen_client.h"
#include "chrome/browser/ui/views/crostini/crostini_installer_view.h"
#include "chrome/browser/ui/views/crostini/crostini_uninstaller_view.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"
#include "chromeos/printing/printer_configuration.h"
#include "chromeos/services/machine_learning/public/cpp/service_connection.h"
#include "components/arc/arc_prefs.h"
#include "components/user_manager/user_manager.h"
#include "content/public/common/service_manager_connection.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "net/base/filename_util.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/base/ui_base_features.h"
#include "ui/display/screen.h"
#include "ui/message_center/public/cpp/notification.h"
#endif

namespace extensions {
namespace {

std::unique_ptr<base::ListValue> GetHostPermissions(const Extension* ext,
                                                    bool effective_perm) {
  const PermissionsData* permissions_data = ext->permissions_data();
  const URLPatternSet& pattern_set =
      effective_perm ? permissions_data->GetEffectiveHostPermissions()
                     : permissions_data->active_permissions().explicit_hosts();

  auto permissions = std::make_unique<base::ListValue>();
  for (URLPatternSet::const_iterator perm = pattern_set.begin();
       perm != pattern_set.end(); ++perm) {
    permissions->AppendString(perm->GetAsString());
  }

  return permissions;
}

std::unique_ptr<base::ListValue> GetAPIPermissions(const Extension* ext) {
  auto permissions = std::make_unique<base::ListValue>();
  std::set<std::string> perm_list =
      ext->permissions_data()->active_permissions().GetAPIsAsStrings();
  for (std::set<std::string>::const_iterator perm = perm_list.begin();
       perm != perm_list.end(); ++perm) {
    permissions->AppendString(*perm);
  }
  return permissions;
}

bool IsTestMode(content::BrowserContext* context) {
  return AutotestPrivateAPI::GetFactoryInstance()->Get(context)->test_mode();
}

#if defined(OS_CHROMEOS)
std::string ConvertToString(message_center::NotificationType type) {
  switch (type) {
    case message_center::NOTIFICATION_TYPE_SIMPLE:
      return "simple";
    case message_center::NOTIFICATION_TYPE_BASE_FORMAT:
      return "base_format";
    case message_center::NOTIFICATION_TYPE_IMAGE:
      return "image";
    case message_center::NOTIFICATION_TYPE_MULTIPLE:
      return "multiple";
    case message_center::NOTIFICATION_TYPE_PROGRESS:
      return "progress";
    case message_center::NOTIFICATION_TYPE_CUSTOM:
      return "custom";
  }
  return "unknown";
}

std::unique_ptr<base::DictionaryValue> MakeDictionaryFromNotification(
    const message_center::Notification& notification) {
  auto result = std::make_unique<base::DictionaryValue>();
  result->SetString("id", notification.id());
  result->SetString("type", ConvertToString(notification.type()));
  result->SetString("title", notification.title());
  result->SetString("message", notification.message());
  result->SetInteger("priority", notification.priority());
  result->SetInteger("progress", notification.progress());
  return result;
}

constexpr char kCrostiniNotAvailableForCurrentUserError[] =
    "Crostini is not available for the current user";

#else

constexpr char kOnlyAvailableOnChromeOSError[] = "Only available on ChromeOS";

#endif

}  // namespace

AutotestPrivateLogoutFunction::~AutotestPrivateLogoutFunction() = default;

ExtensionFunction::ResponseAction AutotestPrivateLogoutFunction::Run() {
  DVLOG(1) << "AutotestPrivateLogoutFunction";
  if (!IsTestMode(browser_context()))
    chrome::AttemptUserExit();
  return RespondNow(NoArguments());
}

AutotestPrivateRestartFunction::~AutotestPrivateRestartFunction() = default;

ExtensionFunction::ResponseAction AutotestPrivateRestartFunction::Run() {
  DVLOG(1) << "AutotestPrivateRestartFunction";
  if (!IsTestMode(browser_context()))
    chrome::AttemptRestart();
  return RespondNow(NoArguments());
}

AutotestPrivateShutdownFunction::~AutotestPrivateShutdownFunction() = default;

ExtensionFunction::ResponseAction AutotestPrivateShutdownFunction::Run() {
  std::unique_ptr<api::autotest_private::Shutdown::Params> params(
      api::autotest_private::Shutdown::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  DVLOG(1) << "AutotestPrivateShutdownFunction " << params->force;

  if (!IsTestMode(browser_context()))
    chrome::AttemptExit();
  return RespondNow(NoArguments());
}

AutotestPrivateLoginStatusFunction::~AutotestPrivateLoginStatusFunction() =
    default;

ExtensionFunction::ResponseAction AutotestPrivateLoginStatusFunction::Run() {
  DVLOG(1) << "AutotestPrivateLoginStatusFunction";

#if defined(OS_CHROMEOS)
  LoginScreenClient::Get()->login_screen()->IsReadyForPassword(base::BindOnce(
      &AutotestPrivateLoginStatusFunction::OnIsReadyForPassword, this));
  return RespondLater();
#else
  return RespondNow(OneArgument(std::make_unique<base::DictionaryValue>()));
#endif
}

#if defined(OS_CHROMEOS)
void AutotestPrivateLoginStatusFunction::OnIsReadyForPassword(bool is_ready) {
  auto result = std::make_unique<base::DictionaryValue>();
  const user_manager::UserManager* user_manager =
      user_manager::UserManager::Get();

  // default_screen_locker()->locked() is set when the UI is ready, so this
  // tells us both views based lockscreen UI and screenlocker are ready.
  const bool is_screen_locked =
      !!chromeos::ScreenLocker::default_screen_locker() &&
      chromeos::ScreenLocker::default_screen_locker()->locked();

  if (user_manager) {
    result->SetBoolean("isLoggedIn", user_manager->IsUserLoggedIn());
    result->SetBoolean("isOwner", user_manager->IsCurrentUserOwner());
    result->SetBoolean("isScreenLocked", is_screen_locked);
    result->SetBoolean("isReadyForPassword", is_ready);
    if (user_manager->IsUserLoggedIn()) {
      result->SetBoolean("isRegularUser",
                         user_manager->IsLoggedInAsUserWithGaiaAccount());
      result->SetBoolean("isGuest", user_manager->IsLoggedInAsGuest());
      result->SetBoolean("isKiosk", user_manager->IsLoggedInAsKioskApp());

      const user_manager::User* user = user_manager->GetActiveUser();
      result->SetString("email", user->GetAccountId().GetUserEmail());
      result->SetString("displayEmail", user->display_email());

      std::string user_image;
      switch (user->image_index()) {
        case user_manager::User::USER_IMAGE_EXTERNAL:
          user_image = "file";
          break;

        case user_manager::User::USER_IMAGE_PROFILE:
          user_image = "profile";
          break;

        default:
          user_image = base::IntToString(user->image_index());
          break;
      }
      result->SetString("userImage", user_image);
    }
  }
  Respond(OneArgument(std::move(result)));
}
#endif

AutotestPrivateLockScreenFunction::~AutotestPrivateLockScreenFunction() =
    default;

ExtensionFunction::ResponseAction AutotestPrivateLockScreenFunction::Run() {
  DVLOG(1) << "AutotestPrivateLockScreenFunction";
#if defined(OS_CHROMEOS)
  chromeos::DBusThreadManager::Get()
      ->GetSessionManagerClient()
      ->RequestLockScreen();
#endif
  return RespondNow(NoArguments());
}

AutotestPrivateGetExtensionsInfoFunction::
    ~AutotestPrivateGetExtensionsInfoFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateGetExtensionsInfoFunction::Run() {
  DVLOG(1) << "AutotestPrivateGetExtensionsInfoFunction";

  ExtensionService* service =
      ExtensionSystem::Get(browser_context())->extension_service();
  ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context());
  const ExtensionSet& extensions = registry->enabled_extensions();
  const ExtensionSet& disabled_extensions = registry->disabled_extensions();
  ExtensionActionManager* extension_action_manager =
      ExtensionActionManager::Get(browser_context());

  auto extensions_values = std::make_unique<base::ListValue>();
  ExtensionList all;
  all.insert(all.end(), extensions.begin(), extensions.end());
  all.insert(all.end(), disabled_extensions.begin(), disabled_extensions.end());
  for (ExtensionList::const_iterator it = all.begin(); it != all.end(); ++it) {
    const Extension* extension = it->get();
    std::string id = extension->id();
    std::unique_ptr<base::DictionaryValue> extension_value(
        new base::DictionaryValue);
    extension_value->SetString("id", id);
    extension_value->SetString("version", extension->VersionString());
    extension_value->SetString("name", extension->name());
    extension_value->SetString("publicKey", extension->public_key());
    extension_value->SetString("description", extension->description());
    extension_value->SetString(
        "backgroundUrl", BackgroundInfo::GetBackgroundURL(extension).spec());
    extension_value->SetString(
        "optionsUrl", OptionsPageInfo::GetOptionsPage(extension).spec());

    extension_value->Set("hostPermissions",
                         GetHostPermissions(extension, false));
    extension_value->Set("effectiveHostPermissions",
                         GetHostPermissions(extension, true));
    extension_value->Set("apiPermissions", GetAPIPermissions(extension));

    Manifest::Location location = extension->location();
    extension_value->SetBoolean("isComponent", location == Manifest::COMPONENT);
    extension_value->SetBoolean("isInternal", location == Manifest::INTERNAL);
    extension_value->SetBoolean("isUserInstalled",
                                location == Manifest::INTERNAL ||
                                    Manifest::IsUnpackedLocation(location));
    extension_value->SetBoolean("isEnabled", service->IsExtensionEnabled(id));
    extension_value->SetBoolean(
        "allowedInIncognito", util::IsIncognitoEnabled(id, browser_context()));
    extension_value->SetBoolean(
        "hasPageAction",
        extension_action_manager->GetPageAction(*extension) != NULL);

    extensions_values->Append(std::move(extension_value));
  }

  std::unique_ptr<base::DictionaryValue> return_value(
      new base::DictionaryValue);
  return_value->Set("extensions", std::move(extensions_values));
  return RespondNow(OneArgument(std::move(return_value)));
}

static int AccessArray(const volatile int arr[], const volatile int* index) {
  return arr[*index];
}

AutotestPrivateSimulateAsanMemoryBugFunction::
    ~AutotestPrivateSimulateAsanMemoryBugFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateSimulateAsanMemoryBugFunction::Run() {
  DVLOG(1) << "AutotestPrivateSimulateAsanMemoryBugFunction";
  if (!IsTestMode(browser_context())) {
    // This array is volatile not to let compiler optimize us out.
    volatile int testarray[3] = {0, 0, 0};

    // Cause Address Sanitizer to abort this process.
    volatile int index = 5;
    AccessArray(testarray, &index);
  }
  return RespondNow(NoArguments());
}

AutotestPrivateSetTouchpadSensitivityFunction::
    ~AutotestPrivateSetTouchpadSensitivityFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateSetTouchpadSensitivityFunction::Run() {
  std::unique_ptr<api::autotest_private::SetTouchpadSensitivity::Params> params(
      api::autotest_private::SetTouchpadSensitivity::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  DVLOG(1) << "AutotestPrivateSetTouchpadSensitivityFunction " << params->value;

#if defined(OS_CHROMEOS)
  chromeos::system::InputDeviceSettings::Get()->SetTouchpadSensitivity(
      params->value);
#endif
  return RespondNow(NoArguments());
}

AutotestPrivateSetTapToClickFunction::~AutotestPrivateSetTapToClickFunction() =
    default;

ExtensionFunction::ResponseAction AutotestPrivateSetTapToClickFunction::Run() {
  std::unique_ptr<api::autotest_private::SetTapToClick::Params> params(
      api::autotest_private::SetTapToClick::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  DVLOG(1) << "AutotestPrivateSetTapToClickFunction " << params->enabled;

#if defined(OS_CHROMEOS)
  chromeos::system::InputDeviceSettings::Get()->SetTapToClick(params->enabled);
#endif
  return RespondNow(NoArguments());
}

AutotestPrivateSetThreeFingerClickFunction::
    ~AutotestPrivateSetThreeFingerClickFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateSetThreeFingerClickFunction::Run() {
  std::unique_ptr<api::autotest_private::SetThreeFingerClick::Params> params(
      api::autotest_private::SetThreeFingerClick::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  DVLOG(1) << "AutotestPrivateSetThreeFingerClickFunction " << params->enabled;

#if defined(OS_CHROMEOS)
  chromeos::system::InputDeviceSettings::Get()->SetThreeFingerClick(
      params->enabled);
#endif
  return RespondNow(NoArguments());
}

AutotestPrivateSetTapDraggingFunction::
    ~AutotestPrivateSetTapDraggingFunction() = default;

ExtensionFunction::ResponseAction AutotestPrivateSetTapDraggingFunction::Run() {
  std::unique_ptr<api::autotest_private::SetTapDragging::Params> params(
      api::autotest_private::SetTapDragging::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  DVLOG(1) << "AutotestPrivateSetTapDraggingFunction " << params->enabled;

#if defined(OS_CHROMEOS)
  chromeos::system::InputDeviceSettings::Get()->SetTapDragging(params->enabled);
#endif
  return RespondNow(NoArguments());
}

AutotestPrivateSetNaturalScrollFunction::
    ~AutotestPrivateSetNaturalScrollFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateSetNaturalScrollFunction::Run() {
  std::unique_ptr<api::autotest_private::SetNaturalScroll::Params> params(
      api::autotest_private::SetNaturalScroll::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  DVLOG(1) << "AutotestPrivateSetNaturalScrollFunction " << params->enabled;

#if defined(OS_CHROMEOS)
  chromeos::system::InputDeviceSettings::Get()->SetNaturalScroll(
      params->enabled);
#endif
  return RespondNow(NoArguments());
}

AutotestPrivateSetMouseSensitivityFunction::
    ~AutotestPrivateSetMouseSensitivityFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateSetMouseSensitivityFunction::Run() {
  std::unique_ptr<api::autotest_private::SetMouseSensitivity::Params> params(
      api::autotest_private::SetMouseSensitivity::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  DVLOG(1) << "AutotestPrivateSetMouseSensitivityFunction " << params->value;

#if defined(OS_CHROMEOS)
  chromeos::system::InputDeviceSettings::Get()->SetMouseSensitivity(
      params->value);
#endif
  return RespondNow(NoArguments());
}

AutotestPrivateSetPrimaryButtonRightFunction::
    ~AutotestPrivateSetPrimaryButtonRightFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateSetPrimaryButtonRightFunction::Run() {
  std::unique_ptr<api::autotest_private::SetPrimaryButtonRight::Params> params(
      api::autotest_private::SetPrimaryButtonRight::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  DVLOG(1) << "AutotestPrivateSetPrimaryButtonRightFunction " << params->right;

#if defined(OS_CHROMEOS)
  chromeos::system::InputDeviceSettings::Get()->SetPrimaryButtonRight(
      params->right);
#endif
  return RespondNow(NoArguments());
}

AutotestPrivateSetMouseReverseScrollFunction::
    ~AutotestPrivateSetMouseReverseScrollFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateSetMouseReverseScrollFunction::Run() {
  std::unique_ptr<api::autotest_private::SetMouseReverseScroll::Params> params(
      api::autotest_private::SetMouseReverseScroll::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  DVLOG(1) << "AutotestPrivateSetMouseReverseScrollFunction "
           << params->enabled;

#if defined(OS_CHROMEOS)
  chromeos::system::InputDeviceSettings::Get()->SetMouseReverseScroll(
      params->enabled);
#endif
  return RespondNow(NoArguments());
}

AutotestPrivateGetVisibleNotificationsFunction::
    AutotestPrivateGetVisibleNotificationsFunction() = default;
AutotestPrivateGetVisibleNotificationsFunction::
    ~AutotestPrivateGetVisibleNotificationsFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateGetVisibleNotificationsFunction::Run() {
  DVLOG(1) << "AutotestPrivateGetVisibleNotificationsFunction";
#if defined(OS_CHROMEOS)
  auto* connection = content::ServiceManagerConnection::GetForProcess();
  connection->GetConnector()->BindInterface(ash::mojom::kServiceName,
                                            &controller_);
  controller_->GetActiveNotifications(base::BindOnce(
      &AutotestPrivateGetVisibleNotificationsFunction::OnGotNotifications,
      this));
  return RespondLater();
#else
  return RespondNow(OneArgument(std::make_unique<base::ListValue>()));
#endif
}

#if defined(OS_CHROMEOS)
void AutotestPrivateGetVisibleNotificationsFunction::OnGotNotifications(
    const std::vector<message_center::Notification>& notifications) {
  auto values = std::make_unique<base::ListValue>();
  for (const auto& notification : notifications) {
    values->Append(MakeDictionaryFromNotification(notification));
  }
  Respond(OneArgument(std::move(values)));
}

// static
std::string AutotestPrivateGetPrinterListFunction::GetPrinterType(
    chromeos::CupsPrintersManager::PrinterClass type) {
  switch (type) {
    case chromeos::CupsPrintersManager::PrinterClass::kConfigured:
      return "configured";
    case chromeos::CupsPrintersManager::PrinterClass::kEnterprise:
      return "enterprise";
    case chromeos::CupsPrintersManager::PrinterClass::kAutomatic:
      return "automatic";
    case chromeos::CupsPrintersManager::PrinterClass::kDiscovered:
      return "discovered";
    default:
      return "unknown";
  }
}
#endif

AutotestPrivateGetPrinterListFunction::
    ~AutotestPrivateGetPrinterListFunction() = default;

ExtensionFunction::ResponseAction AutotestPrivateGetPrinterListFunction::Run() {
  DVLOG(1) << "AutotestPrivateGetPrinterListFunction";
  auto values = std::make_unique<base::ListValue>();
#if defined(OS_CHROMEOS)
  Profile* profile = ProfileManager::GetActiveUserProfile();
  std::unique_ptr<chromeos::CupsPrintersManager> printers_manager =
      chromeos::CupsPrintersManager::Create(profile);
  std::vector<chromeos::CupsPrintersManager::PrinterClass> printer_type = {
      chromeos::CupsPrintersManager::PrinterClass::kConfigured,
      chromeos::CupsPrintersManager::PrinterClass::kEnterprise,
      chromeos::CupsPrintersManager::PrinterClass::kAutomatic};
  for (const auto& type : printer_type) {
    std::vector<chromeos::Printer> printer_list =
        printers_manager->GetPrinters(type);
    for (const auto& printer : printer_list) {
      auto result = std::make_unique<base::DictionaryValue>();
      result->SetString("printerName", printer.display_name());
      result->SetString("printerId", printer.id());
      result->SetString("printerType", GetPrinterType(type));
      values->Append(std::move(result));
    }
  }
#endif
  return RespondNow(OneArgument(std::move(values)));
}

AutotestPrivateUpdatePrinterFunction::AutotestPrivateUpdatePrinterFunction() =
    default;
AutotestPrivateUpdatePrinterFunction::~AutotestPrivateUpdatePrinterFunction() =
    default;

ExtensionFunction::ResponseAction AutotestPrivateUpdatePrinterFunction::Run() {
  std::unique_ptr<api::autotest_private::UpdatePrinter::Params> params(
      api::autotest_private::UpdatePrinter::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  DVLOG(1) << "AutotestPrivateUpdatePrinterFunction";
#if defined(OS_CHROMEOS)
  const api::autotest_private::Printer& js_printer = params->printer;
  chromeos::Printer printer(js_printer.printer_id ? *js_printer.printer_id
                                                  : "");
  printer.set_display_name(js_printer.printer_name);
  if (js_printer.printer_desc)
    printer.set_description(*js_printer.printer_desc);

  if (js_printer.printer_make_and_model)
    printer.set_make_and_model(*js_printer.printer_make_and_model);

  if (js_printer.printer_uri)
    printer.set_uri(*js_printer.printer_uri);

  if (js_printer.printer_ppd) {
    const GURL ppd =
        net::FilePathToFileURL(base::FilePath(*js_printer.printer_ppd));
    if (ppd.is_valid())
      printer.mutable_ppd_reference()->user_supplied_ppd_url = ppd.spec();
    else
      LOG(ERROR) << "Invalid ppd path: " << *js_printer.printer_ppd;
  }
  auto printers_manager = chromeos::CupsPrintersManager::Create(
      ProfileManager::GetActiveUserProfile());
  printers_manager->UpdateConfiguredPrinter(printer);
#endif
  return RespondNow(NoArguments());
}

AutotestPrivateRemovePrinterFunction::AutotestPrivateRemovePrinterFunction() =
    default;
AutotestPrivateRemovePrinterFunction::~AutotestPrivateRemovePrinterFunction() =
    default;

ExtensionFunction::ResponseAction AutotestPrivateRemovePrinterFunction::Run() {
  std::unique_ptr<api::autotest_private::RemovePrinter::Params> params(
      api::autotest_private::RemovePrinter::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  DVLOG(1) << "AutotestPrivateRemovePrinterFunction";
#if defined(OS_CHROMEOS)
  auto printers_manager = chromeos::CupsPrintersManager::Create(
      ProfileManager::GetActiveUserProfile());
  printers_manager->RemoveConfiguredPrinter(params->printer_id);
#endif
  return RespondNow(NoArguments());
}

AutotestPrivateGetPlayStoreStateFunction::
    ~AutotestPrivateGetPlayStoreStateFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateGetPlayStoreStateFunction::Run() {
  DVLOG(1) << "AutotestPrivateGetPlayStoreStateFunction";
  api::autotest_private::PlayStoreState play_store_state;
  play_store_state.allowed = false;
#if defined(OS_CHROMEOS)
  Profile* profile = ProfileManager::GetActiveUserProfile();
  if (arc::IsArcAllowedForProfile(profile)) {
    play_store_state.allowed = true;
    play_store_state.enabled =
        std::make_unique<bool>(arc::IsArcPlayStoreEnabledForProfile(profile));
    play_store_state.managed = std::make_unique<bool>(
        arc::IsArcPlayStoreEnabledPreferenceManagedForProfile(profile));
  }
#endif
  return RespondNow(OneArgument(play_store_state.ToValue()));
}

AutotestPrivateSetPlayStoreEnabledFunction::
    ~AutotestPrivateSetPlayStoreEnabledFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateSetPlayStoreEnabledFunction::Run() {
  DVLOG(1) << "AutotestPrivateSetPlayStoreEnabledFunction";
#if defined(OS_CHROMEOS)
  std::unique_ptr<api::autotest_private::SetPlayStoreEnabled::Params> params(
      api::autotest_private::SetPlayStoreEnabled::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  Profile* profile = ProfileManager::GetActiveUserProfile();
  if (arc::IsArcAllowedForProfile(profile)) {
    if (!arc::SetArcPlayStoreEnabledForProfile(profile, params->enabled)) {
      return RespondNow(
          Error("ARC enabled state cannot be changed for the current user"));
    }
    profile->GetPrefs()->SetBoolean(arc::prefs::kArcLocationServiceEnabled,
                                    true);
    return RespondNow(NoArguments());
  } else {
    return RespondNow(Error("ARC is not available for the current user"));
  }
#else
  return RespondNow(Error(kOnlyAvailableOnChromeOSError));
#endif
}

AutotestPrivateGetHistogramFunction::~AutotestPrivateGetHistogramFunction() =
    default;

ExtensionFunction::ResponseAction AutotestPrivateGetHistogramFunction::Run() {
  DVLOG(1) << "AutotestPrivateGetHistogramFunction";
  std::unique_ptr<api::autotest_private::GetHistogram::Params> params(
      api::autotest_private::GetHistogram::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  const base::HistogramBase* histogram =
      base::StatisticsRecorder::FindHistogram(params->name);
  if (!histogram) {
    return RespondNow(
        Error(base::StrCat({"Histogram ", params->name, " not found"})));
  }

  std::unique_ptr<base::HistogramSamples> samples =
      histogram->SnapshotSamples();
  api::autotest_private::Histogram result;

  for (std::unique_ptr<base::SampleCountIterator> it = samples->Iterator();
       !it->Done(); it->Next()) {
    base::HistogramBase::Sample min = 0;
    int64_t max = 0;
    base::HistogramBase::Count count = 0;
    it->Get(&min, &max, &count);

    api::autotest_private::HistogramBucket bucket;
    bucket.min = min;
    bucket.max = max;
    bucket.count = count;
    result.buckets.push_back(std::move(bucket));
  }

  return RespondNow(OneArgument(result.ToValue()));
}

AutotestPrivateIsAppShownFunction::~AutotestPrivateIsAppShownFunction() =
    default;

ExtensionFunction::ResponseAction AutotestPrivateIsAppShownFunction::Run() {
  DVLOG(1) << "AutotestPrivateIsAppShownFunction";
#if defined(OS_CHROMEOS)
  std::unique_ptr<api::autotest_private::IsAppShown::Params> params(
      api::autotest_private::IsAppShown::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  ChromeLauncherController* const controller =
      ChromeLauncherController::instance();
  if (!controller)
    return RespondNow(Error("Controller not available"));

  const ash::ShelfItem* item =
      controller->GetItem(ash::ShelfID(params->app_id));
  const bool window_attached =
      item && item->status == ash::ShelfItemStatus::STATUS_RUNNING;
  return RespondNow(
      OneArgument(std::make_unique<base::Value>(window_attached)));
#else
  return RespondNow(Error(kOnlyAvailableOnChromeOSError));
#endif
}

AutotestPrivateLaunchAppFunction::~AutotestPrivateLaunchAppFunction() = default;

ExtensionFunction::ResponseAction AutotestPrivateLaunchAppFunction::Run() {
  DVLOG(1) << "AutotestPrivateLaunchAppFunction";
#if defined(OS_CHROMEOS)
  std::unique_ptr<api::autotest_private::LaunchApp::Params> params(
      api::autotest_private::LaunchApp::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  ChromeLauncherController* const controller =
      ChromeLauncherController::instance();
  if (!controller)
    return RespondNow(Error("Controller not available"));
  controller->LaunchApp(ash::ShelfID(params->app_id),
                        ash::ShelfLaunchSource::LAUNCH_FROM_APP_LIST,
                        0, /* event_flags */
                        display::Screen::GetScreen()->GetPrimaryDisplay().id());
  return RespondNow(NoArguments());
#else
  return RespondNow(Error(kOnlyAvailableOnChromeOSError));
#endif
}

AutotestPrivateSetCrostiniEnabledFunction::
    ~AutotestPrivateSetCrostiniEnabledFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateSetCrostiniEnabledFunction::Run() {
  DVLOG(1) << "AutotestPrivateSetCrostiniEnabledFunction";
#if defined(OS_CHROMEOS)
  std::unique_ptr<api::autotest_private::SetCrostiniEnabled::Params> params(
      api::autotest_private::SetCrostiniEnabled::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  Profile* profile = Profile::FromBrowserContext(browser_context());
  if (!IsCrostiniUIAllowedForProfile(profile))
    return RespondNow(Error(kCrostiniNotAvailableForCurrentUserError));

  // Set the preference to indicate Crostini is enabled/disabled.
  profile->GetPrefs()->SetBoolean(crostini::prefs::kCrostiniEnabled,
                                  params->enabled);
  // Set the flag to indicate we are in testing mode so that Chrome doesn't
  // try to start the VM/container itself.
  crostini::CrostiniManager* crostini_manager =
      crostini::CrostiniManager::GetForProfile(profile);
  crostini_manager->set_skip_restart_for_testing();
  return RespondNow(NoArguments());
#else
  return RespondNow(Error(kOnlyAvailableOnChromeOSError));
#endif
}

AutotestPrivateRunCrostiniInstallerFunction::
    ~AutotestPrivateRunCrostiniInstallerFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateRunCrostiniInstallerFunction::Run() {
  DVLOG(1) << "AutotestPrivateInstallCrostiniFunction";
#if defined(OS_CHROMEOS)
  Profile* profile = Profile::FromBrowserContext(browser_context());
  if (!IsCrostiniUIAllowedForProfile(profile))
    return RespondNow(Error(kCrostiniNotAvailableForCurrentUserError));

  // Run GUI installer which will install crostini vm / container and
  // start terminal app on completion.  After starting the installer,
  // we call RestartCrostini and we will be put in the pending restarters
  // queue and be notified on success/otherwise of installation.
  CrostiniInstallerView::Show(profile);
  CrostiniInstallerView::GetActiveViewForTesting()->Accept();
  crostini::CrostiniManager::GetForProfile(profile)->RestartCrostini(
      kCrostiniDefaultVmName, kCrostiniDefaultContainerName,
      base::BindOnce(
          &AutotestPrivateRunCrostiniInstallerFunction::CrostiniRestarted,
          this));

  return RespondLater();
#else
  return RespondNow(Error(kOnlyAvailableOnChromeOSError));
#endif
}

#if defined(OS_CHROMEOS)
void AutotestPrivateRunCrostiniInstallerFunction::CrostiniRestarted(
    crostini::ConciergeClientResult result) {
  if (result == crostini::ConciergeClientResult::SUCCESS) {
    Respond(NoArguments());
  } else {
    Respond(Error("Error installing crostini"));
  }
}
#endif

AutotestPrivateRunCrostiniUninstallerFunction::
    ~AutotestPrivateRunCrostiniUninstallerFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateRunCrostiniUninstallerFunction::Run() {
  DVLOG(1) << "AutotestPrivateRunCrostiniUninstallerFunction";
#if defined(OS_CHROMEOS)
  Profile* profile = Profile::FromBrowserContext(browser_context());
  if (!IsCrostiniUIAllowedForProfile(profile))
    return RespondNow(Error(kCrostiniNotAvailableForCurrentUserError));

  // Run GUI uninstaller which will remove crostini vm / container. We then
  // receive the callback with the result when that is complete.
  crostini::CrostiniManager::GetForProfile(profile)->AddRemoveCrostiniCallback(
      base::BindOnce(
          &AutotestPrivateRunCrostiniUninstallerFunction::CrostiniRemoved,
          this));
  CrostiniUninstallerView::Show(profile);
  CrostiniUninstallerView::GetActiveViewForTesting()->Accept();
  return RespondLater();
#else
  return RespondNow(Error(kOnlyAvailableOnChromeOSError));
#endif
}

#if defined(OS_CHROMEOS)
void AutotestPrivateRunCrostiniUninstallerFunction::CrostiniRemoved(
    crostini::ConciergeClientResult result) {
  if (result == crostini::ConciergeClientResult::SUCCESS) {
    Respond(NoArguments());
  } else {
    Respond(Error("Error uninstalling crostini"));
  }
}
#endif

AutotestPrivateTakeScreenshotFunction::
    ~AutotestPrivateTakeScreenshotFunction() = default;

ExtensionFunction::ResponseAction AutotestPrivateTakeScreenshotFunction::Run() {
  DVLOG(1) << "AutotestPrivateTakeScreenshotFunction";
#if defined(OS_CHROMEOS)
  auto grabber = std::make_unique<ui::ScreenshotGrabber>();
  // TODO(mash): Fix for mash, http://crbug.com/557397
  aura::Window* primary_root = ash::Shell::GetPrimaryRootWindow();
  // Pass the ScreenshotGrabber to the callback so that it stays alive for the
  // duration of the operation, it'll then get deallocated when the callback
  // completes.
  grabber->TakeScreenshot(
      primary_root, primary_root->bounds(),
      base::BindOnce(&AutotestPrivateTakeScreenshotFunction::ScreenshotTaken,
                     this, base::Passed(&grabber)));
  return RespondLater();
#else
  return RespondNow(Error(kOnlyAvailableOnChromeOSError));
#endif
}

#if defined(OS_CHROMEOS)
void AutotestPrivateTakeScreenshotFunction::ScreenshotTaken(
    std::unique_ptr<ui::ScreenshotGrabber> grabber,
    ui::ScreenshotResult screenshot_result,
    scoped_refptr<base::RefCountedMemory> png_data) {
  if (screenshot_result == ui::ScreenshotResult::SUCCESS) {
    // Base64 encode the result so we can return it as a string.
    std::string base64Png(png_data->front(),
                          png_data->front() + png_data->size());
    base::Base64Encode(base64Png, &base64Png);
    Respond(OneArgument(std::make_unique<base::Value>(base64Png)));
  } else {
    Respond(Error(base::StrCat(
        {"Error taking screenshot ",
         base::NumberToString(static_cast<int>(screenshot_result))})));
  }
}
#endif

AutotestPrivateBootstrapMachineLearningServiceFunction::
    ~AutotestPrivateBootstrapMachineLearningServiceFunction() = default;

AutotestPrivateBootstrapMachineLearningServiceFunction::
    AutotestPrivateBootstrapMachineLearningServiceFunction() {}

ExtensionFunction::ResponseAction
AutotestPrivateBootstrapMachineLearningServiceFunction::Run() {
  DVLOG(1) << "AutotestPrivateBootstrapMachineLearningServiceFunction";
#if defined(OS_CHROMEOS)
  // Load a model. This will first bootstrap the Mojo connection to ML Service.
  chromeos::machine_learning::ServiceConnection::GetInstance()->LoadModel(
      chromeos::machine_learning::mojom::ModelSpec::New(
          chromeos::machine_learning::mojom::ModelId::TEST_MODEL),
      mojo::MakeRequest(&model_),
      base::BindOnce(
          &AutotestPrivateBootstrapMachineLearningServiceFunction::ModelLoaded,
          this));
  model_.set_connection_error_handler(base::BindOnce(
      &AutotestPrivateBootstrapMachineLearningServiceFunction::ConnectionError,
      this));
  return RespondLater();
#else
  return RespondNow(Error(kOnlyAvailableOnChromeOSError));
#endif
}

#if defined(OS_CHROMEOS)
void AutotestPrivateBootstrapMachineLearningServiceFunction::ModelLoaded(
    chromeos::machine_learning::mojom::LoadModelResult result) {
  if (result == chromeos::machine_learning::mojom::LoadModelResult::OK) {
    Respond(NoArguments());
  } else {
    Respond(Error(base::StrCat(
        {"Model load error ", (std::ostringstream() << result).str()})));
  }
}

void AutotestPrivateBootstrapMachineLearningServiceFunction::ConnectionError() {
  Respond(Error("ML Service connection error"));
}
#endif

static base::LazyInstance<BrowserContextKeyedAPIFactory<AutotestPrivateAPI>>::
    DestructorAtExit g_autotest_private_api_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<AutotestPrivateAPI>*
AutotestPrivateAPI::GetFactoryInstance() {
  return g_autotest_private_api_factory.Pointer();
}

template <>
KeyedService*
BrowserContextKeyedAPIFactory<AutotestPrivateAPI>::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new AutotestPrivateAPI();
}

AutotestPrivateAPI::AutotestPrivateAPI() : test_mode_(false) {}

AutotestPrivateAPI::~AutotestPrivateAPI() {}

}  // namespace extensions
