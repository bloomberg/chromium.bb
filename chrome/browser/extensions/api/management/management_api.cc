// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/management/management_api.h"

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/api/management/management_api_constants.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_ui_util.h"
#include "chrome/browser/extensions/extension_uninstall_dialog.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/webui/extensions/extension_icon_source.h"
#include "chrome/browser/ui/webui/ntp/core_app_launcher_handler.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/api/management.h"
#include "chrome/common/extensions/chrome_utility_extensions_messages.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/extensions/manifest_url_handler.h"
#include "content/public/browser/utility_process_host.h"
#include "content/public/browser/utility_process_host_client.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/management_policy.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_icon_set.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "extensions/common/manifest_handlers/offline_enabled_info.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/url_pattern.h"

using base::IntToString;
using content::BrowserThread;
using content::UtilityProcessHost;
using content::UtilityProcessHostClient;

namespace keys = extension_management_api_constants;

namespace extensions {

namespace management = api::management;

namespace {

typedef std::vector<linked_ptr<management::ExtensionInfo> > ExtensionInfoList;
typedef std::vector<linked_ptr<management::IconInfo> > IconInfoList;

enum AutoConfirmForTest {
  DO_NOT_SKIP = 0,
  PROCEED,
  ABORT
};

AutoConfirmForTest auto_confirm_for_test = DO_NOT_SKIP;

std::vector<std::string> CreateWarningsList(const Extension* extension) {
  std::vector<std::string> warnings_list;
  PermissionMessages warnings =
      extension->permissions_data()->GetPermissionMessages();
  for (PermissionMessages::const_iterator iter = warnings.begin();
       iter != warnings.end(); ++iter) {
    warnings_list.push_back(base::UTF16ToUTF8(iter->message()));
  }

  return warnings_list;
}

std::vector<management::LaunchType> GetAvailableLaunchTypes(
    const Extension& extension) {
  std::vector<management::LaunchType> launch_type_list;
  if (extension.is_platform_app()) {
    launch_type_list.push_back(management::LAUNCH_TYPE_OPEN_AS_WINDOW);
    return launch_type_list;
  }

  launch_type_list.push_back(management::LAUNCH_TYPE_OPEN_AS_REGULAR_TAB);

#if !defined(OS_MACOSX)
  launch_type_list.push_back(management::LAUNCH_TYPE_OPEN_AS_WINDOW);
#endif

  if (!CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableStreamlinedHostedApps)) {
    launch_type_list.push_back(management::LAUNCH_TYPE_OPEN_AS_PINNED_TAB);
    launch_type_list.push_back(management::LAUNCH_TYPE_OPEN_FULL_SCREEN);
  }
  return launch_type_list;
}

scoped_ptr<management::ExtensionInfo> CreateExtensionInfo(
    const Extension& extension,
    ExtensionSystem* system) {
  scoped_ptr<management::ExtensionInfo> info(new management::ExtensionInfo());
  ExtensionService* service = system->extension_service();

  info->id = extension.id();
  info->name = extension.name();
  info->short_name = extension.short_name();
  info->enabled = service->IsExtensionEnabled(info->id);
  info->offline_enabled = OfflineEnabledInfo::IsOfflineEnabled(&extension);
  info->version = extension.VersionString();
  info->description = extension.description();
  info->options_url = ManifestURL::GetOptionsPage(&extension).spec();
  info->homepage_url.reset(new std::string(
      ManifestURL::GetHomepageURL(&extension).spec()));
  info->may_disable = system->management_policy()->
      UserMayModifySettings(&extension, NULL);
  info->is_app = extension.is_app();
  if (info->is_app) {
    if (extension.is_legacy_packaged_app())
      info->type = management::ExtensionInfo::TYPE_LEGACY_PACKAGED_APP;
    else if (extension.is_hosted_app())
      info->type = management::ExtensionInfo::TYPE_HOSTED_APP;
    else
      info->type = management::ExtensionInfo::TYPE_PACKAGED_APP;
  } else if (extension.is_theme()) {
    info->type = management::ExtensionInfo::TYPE_THEME;
  } else {
    info->type = management::ExtensionInfo::TYPE_EXTENSION;
  }

  if (info->enabled) {
    info->disabled_reason = management::ExtensionInfo::DISABLED_REASON_NONE;
  } else {
    ExtensionPrefs* prefs = ExtensionPrefs::Get(service->profile());
    if (prefs->DidExtensionEscalatePermissions(extension.id())) {
      info->disabled_reason =
          management::ExtensionInfo::DISABLED_REASON_PERMISSIONS_INCREASE;
    } else {
      info->disabled_reason =
          management::ExtensionInfo::DISABLED_REASON_UNKNOWN;
    }
  }

  if (!ManifestURL::GetUpdateURL(&extension).is_empty()) {
    info->update_url.reset(new std::string(
        ManifestURL::GetUpdateURL(&extension).spec()));
  }

  if (extension.is_app()) {
    info->app_launch_url.reset(new std::string(
        AppLaunchInfo::GetFullLaunchURL(&extension).spec()));
  }

  const ExtensionIconSet::IconMap& icons =
      IconsInfo::GetIcons(&extension).map();
  if (!icons.empty()) {
    info->icons.reset(new IconInfoList());
    ExtensionIconSet::IconMap::const_iterator icon_iter;
    for (icon_iter = icons.begin(); icon_iter != icons.end(); ++icon_iter) {
      management::IconInfo* icon_info = new management::IconInfo();
      icon_info->size = icon_iter->first;
      GURL url = ExtensionIconSource::GetIconURL(
          &extension, icon_info->size, ExtensionIconSet::MATCH_EXACTLY, false,
          NULL);
      icon_info->url = url.spec();
      info->icons->push_back(make_linked_ptr<management::IconInfo>(icon_info));
    }
  }

  const std::set<std::string> perms =
      extension.permissions_data()->active_permissions()->GetAPIsAsStrings();
  if (!perms.empty()) {
    std::set<std::string>::const_iterator perms_iter;
    for (perms_iter = perms.begin(); perms_iter != perms.end(); ++perms_iter)
      info->permissions.push_back(*perms_iter);
  }

  if (!extension.is_hosted_app()) {
    // Skip host permissions for hosted apps.
    const URLPatternSet host_perms =
        extension.permissions_data()->active_permissions()->explicit_hosts();
    if (!host_perms.is_empty()) {
      for (URLPatternSet::const_iterator iter = host_perms.begin();
           iter != host_perms.end(); ++iter) {
        info->host_permissions.push_back(iter->GetAsString());
      }
    }
  }

  switch (extension.location()) {
    case Manifest::INTERNAL:
      info->install_type = management::ExtensionInfo::INSTALL_TYPE_NORMAL;
      break;
    case Manifest::UNPACKED:
    case Manifest::COMMAND_LINE:
      info->install_type = management::ExtensionInfo::INSTALL_TYPE_DEVELOPMENT;
      break;
    case Manifest::EXTERNAL_PREF:
    case Manifest::EXTERNAL_REGISTRY:
    case Manifest::EXTERNAL_PREF_DOWNLOAD:
      info->install_type = management::ExtensionInfo::INSTALL_TYPE_SIDELOAD;
      break;
    case Manifest::EXTERNAL_POLICY:
    case Manifest::EXTERNAL_POLICY_DOWNLOAD:
      info->install_type = management::ExtensionInfo::INSTALL_TYPE_ADMIN;
      break;
    case Manifest::NUM_LOCATIONS:
      NOTREACHED();
    case Manifest::INVALID_LOCATION:
    case Manifest::COMPONENT:
    case Manifest::EXTERNAL_COMPONENT:
      info->install_type = management::ExtensionInfo::INSTALL_TYPE_OTHER;
      break;
  }

  info->launch_type = management::LAUNCH_TYPE_NONE;
  if (extension.is_app()) {
    LaunchType launch_type;
    if (extension.is_platform_app()) {
      launch_type = LAUNCH_TYPE_WINDOW;
    } else {
      launch_type =
          GetLaunchType(ExtensionPrefs::Get(service->profile()), &extension);
    }

    switch (launch_type) {
      case LAUNCH_TYPE_PINNED:
        info->launch_type = management::LAUNCH_TYPE_OPEN_AS_PINNED_TAB;
        break;
      case LAUNCH_TYPE_REGULAR:
        info->launch_type = management::LAUNCH_TYPE_OPEN_AS_REGULAR_TAB;
        break;
      case LAUNCH_TYPE_FULLSCREEN:
        info->launch_type = management::LAUNCH_TYPE_OPEN_FULL_SCREEN;
        break;
      case LAUNCH_TYPE_WINDOW:
        info->launch_type = management::LAUNCH_TYPE_OPEN_AS_WINDOW;
        break;
      case LAUNCH_TYPE_INVALID:
      case NUM_LAUNCH_TYPES:
        NOTREACHED();
    }

    info->available_launch_types.reset(new std::vector<management::LaunchType>(
        GetAvailableLaunchTypes(extension)));
  }

  return info.Pass();
}

void AddExtensionInfo(const ExtensionSet& extensions,
                            ExtensionSystem* system,
                            ExtensionInfoList* extension_list,
                            content::BrowserContext* context) {
  for (ExtensionSet::const_iterator iter = extensions.begin();
       iter != extensions.end(); ++iter) {
    const Extension& extension = *iter->get();

    if (ui_util::ShouldNotBeVisible(&extension, context))
      continue;  // Skip built-in extensions/apps.

    extension_list->push_back(make_linked_ptr<management::ExtensionInfo>(
        CreateExtensionInfo(extension, system).release()));
  }
}

}  // namespace

ExtensionService* ManagementFunction::service() {
  return ExtensionSystem::Get(GetProfile())->extension_service();
}

ExtensionService* AsyncManagementFunction::service() {
  return ExtensionSystem::Get(GetProfile())->extension_service();
}

bool ManagementGetAllFunction::RunSync() {
  ExtensionInfoList extensions;
  ExtensionRegistry* registry = ExtensionRegistry::Get(GetProfile());
  ExtensionSystem* system = ExtensionSystem::Get(GetProfile());

  AddExtensionInfo(registry->enabled_extensions(),
                   system, &extensions, browser_context());
  AddExtensionInfo(registry->disabled_extensions(),
                   system, &extensions, browser_context());
  AddExtensionInfo(registry->terminated_extensions(),
                   system, &extensions, browser_context());

  results_ = management::GetAll::Results::Create(extensions);
  return true;
}

bool ManagementGetFunction::RunSync() {
  scoped_ptr<management::Get::Params> params(
      management::Get::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  const Extension* extension = service()->GetExtensionById(params->id, true);
  if (!extension) {
    error_ = ErrorUtils::FormatErrorMessage(keys::kNoExtensionError,
                                                     params->id);
    return false;
  }

  scoped_ptr<management::ExtensionInfo> info =
      CreateExtensionInfo(*extension, ExtensionSystem::Get(GetProfile()));
  results_ = management::Get::Results::Create(*info);

  return true;
}

bool ManagementGetPermissionWarningsByIdFunction::RunSync() {
  scoped_ptr<management::GetPermissionWarningsById::Params> params(
      management::GetPermissionWarningsById::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  const Extension* extension = service()->GetExtensionById(params->id, true);
  if (!extension) {
    error_ = ErrorUtils::FormatErrorMessage(keys::kNoExtensionError,
                                                     params->id);
    return false;
  }

  std::vector<std::string> warnings = CreateWarningsList(extension);
  results_ = management::GetPermissionWarningsById::Results::Create(warnings);
  return true;
}

namespace {

// This class helps ManagementGetPermissionWarningsByManifestFunction manage
// sending manifest JSON strings to the utility process for parsing.
class SafeManifestJSONParser : public UtilityProcessHostClient {
 public:
  SafeManifestJSONParser(
      ManagementGetPermissionWarningsByManifestFunction* client,
      const std::string& manifest)
      : client_(client),
        manifest_(manifest) {}

  void Start() {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&SafeManifestJSONParser::StartWorkOnIOThread, this));
  }

  void StartWorkOnIOThread() {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    UtilityProcessHost* host = UtilityProcessHost::Create(
        this, base::MessageLoopProxy::current().get());
    host->Send(new ChromeUtilityMsg_ParseJSON(manifest_));
  }

  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE {
    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(SafeManifestJSONParser, message)
      IPC_MESSAGE_HANDLER(ChromeUtilityHostMsg_ParseJSON_Succeeded,
                          OnJSONParseSucceeded)
      IPC_MESSAGE_HANDLER(ChromeUtilityHostMsg_ParseJSON_Failed,
                          OnJSONParseFailed)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
    return handled;
  }

  void OnJSONParseSucceeded(const base::ListValue& wrapper) {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    const base::Value* value = NULL;
    CHECK(wrapper.Get(0, &value));
    if (value->IsType(base::Value::TYPE_DICTIONARY))
      parsed_manifest_.reset(
          static_cast<const base::DictionaryValue*>(value)->DeepCopy());
    else
      error_ = keys::kManifestParseError;

    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&SafeManifestJSONParser::ReportResultFromUIThread, this));
  }

  void OnJSONParseFailed(const std::string& error) {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    error_ = error;
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&SafeManifestJSONParser::ReportResultFromUIThread, this));
  }

  void ReportResultFromUIThread() {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    if (error_.empty() && parsed_manifest_.get())
      client_->OnParseSuccess(parsed_manifest_.Pass());
    else
      client_->OnParseFailure(error_);
  }

 private:
  virtual ~SafeManifestJSONParser() {}

  // The client who we'll report results back to.
  ManagementGetPermissionWarningsByManifestFunction* client_;

  // Data to parse.
  std::string manifest_;

  // Results of parsing.
  scoped_ptr<base::DictionaryValue> parsed_manifest_;

  std::string error_;
};

}  // namespace

bool ManagementGetPermissionWarningsByManifestFunction::RunAsync() {
  scoped_ptr<management::GetPermissionWarningsByManifest::Params> params(
      management::GetPermissionWarningsByManifest::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  scoped_refptr<SafeManifestJSONParser> parser =
      new SafeManifestJSONParser(this, params->manifest_str);
  parser->Start();

  // Matched with a Release() in OnParseSuccess/Failure().
  AddRef();

  // Response is sent async in OnParseSuccess/Failure().
  return true;
}

void ManagementGetPermissionWarningsByManifestFunction::OnParseSuccess(
    scoped_ptr<base::DictionaryValue> parsed_manifest) {
  CHECK(parsed_manifest.get());

  scoped_refptr<Extension> extension = Extension::Create(
      base::FilePath(), Manifest::INVALID_LOCATION, *parsed_manifest,
      Extension::NO_FLAGS, &error_);
  if (!extension.get()) {
    OnParseFailure(keys::kExtensionCreateError);
    return;
  }

  std::vector<std::string> warnings = CreateWarningsList(extension.get());
  results_ =
      management::GetPermissionWarningsByManifest::Results::Create(warnings);
  SendResponse(true);

  // Matched with AddRef() in RunAsync().
  Release();
}

void ManagementGetPermissionWarningsByManifestFunction::OnParseFailure(
    const std::string& error) {
  error_ = error;
  SendResponse(false);

  // Matched with AddRef() in RunAsync().
  Release();
}

bool ManagementLaunchAppFunction::RunSync() {
  scoped_ptr<management::LaunchApp::Params> params(
      management::LaunchApp::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  const Extension* extension = service()->GetExtensionById(params->id, true);
  if (!extension) {
    error_ = ErrorUtils::FormatErrorMessage(keys::kNoExtensionError,
                                                     params->id);
    return false;
  }
  if (!extension->is_app()) {
    error_ = ErrorUtils::FormatErrorMessage(keys::kNotAnAppError,
                                                     params->id);
    return false;
  }

  // Look at prefs to find the right launch container.
  // If the user has not set a preference, the default launch value will be
  // returned.
  LaunchContainer launch_container =
      GetLaunchContainer(ExtensionPrefs::Get(GetProfile()), extension);
  OpenApplication(AppLaunchParams(
      GetProfile(), extension, launch_container, NEW_FOREGROUND_TAB));
  CoreAppLauncherHandler::RecordAppLaunchType(
      extension_misc::APP_LAUNCH_EXTENSION_API,
      extension->GetType());

  return true;
}

ManagementSetEnabledFunction::ManagementSetEnabledFunction() {
}

ManagementSetEnabledFunction::~ManagementSetEnabledFunction() {
}

bool ManagementSetEnabledFunction::RunAsync() {
  scoped_ptr<management::SetEnabled::Params> params(
      management::SetEnabled::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  extension_id_ = params->id;

  const Extension* extension =
      ExtensionRegistry::Get(GetProfile())
          ->GetExtensionById(extension_id_, ExtensionRegistry::EVERYTHING);
  if (!extension || ui_util::ShouldNotBeVisible(extension, browser_context())) {
    error_ = ErrorUtils::FormatErrorMessage(
        keys::kNoExtensionError, extension_id_);
    return false;
  }

  const ManagementPolicy* policy =
      ExtensionSystem::Get(GetProfile())->management_policy();
  if (!policy->UserMayModifySettings(extension, NULL) ||
      (!params->enabled && policy->MustRemainEnabled(extension, NULL)) ||
      (params->enabled && policy->MustRemainDisabled(extension, NULL, NULL))) {
    error_ = ErrorUtils::FormatErrorMessage(
        keys::kUserCantModifyError, extension_id_);
    return false;
  }

  bool currently_enabled = service()->IsExtensionEnabled(extension_id_);

  if (!currently_enabled && params->enabled) {
    ExtensionPrefs* prefs = ExtensionPrefs::Get(GetProfile());
    if (prefs->DidExtensionEscalatePermissions(extension_id_)) {
      if (!user_gesture()) {
        error_ = keys::kGestureNeededForEscalationError;
        return false;
      }
      AddRef();  // Matched in InstallUIProceed/InstallUIAbort
      install_prompt_.reset(
          new ExtensionInstallPrompt(GetAssociatedWebContents()));
      install_prompt_->ConfirmReEnable(this, extension);
      return true;
    }
    service()->EnableExtension(extension_id_);
  } else if (currently_enabled && !params->enabled) {
    service()->DisableExtension(extension_id_, Extension::DISABLE_USER_ACTION);
  }

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&ManagementSetEnabledFunction::SendResponse, this, true));

  return true;
}

void ManagementSetEnabledFunction::InstallUIProceed() {
  service()->EnableExtension(extension_id_);
  SendResponse(true);
  Release();
}

void ManagementSetEnabledFunction::InstallUIAbort(bool user_initiated) {
  error_ = keys::kUserDidNotReEnableError;
  SendResponse(false);
  Release();
}

ManagementUninstallFunctionBase::ManagementUninstallFunctionBase() {
}

ManagementUninstallFunctionBase::~ManagementUninstallFunctionBase() {
}

bool ManagementUninstallFunctionBase::Uninstall(
    const std::string& target_extension_id,
    bool show_confirm_dialog) {
  extension_id_ = target_extension_id;
  const Extension* target_extension =
      service()->GetExtensionById(extension_id_, true);
  if (!target_extension ||
      ui_util::ShouldNotBeVisible(target_extension, browser_context())) {
    error_ = ErrorUtils::FormatErrorMessage(
        keys::kNoExtensionError, extension_id_);
    return false;
  }

  if (!ExtensionSystem::Get(GetProfile())
           ->management_policy()
           ->UserMayModifySettings(target_extension, NULL)) {
    error_ = ErrorUtils::FormatErrorMessage(
        keys::kUserCantModifyError, extension_id_);
    return false;
  }

  if (auto_confirm_for_test == DO_NOT_SKIP) {
    if (show_confirm_dialog) {
      AddRef();  // Balanced in ExtensionUninstallAccepted/Canceled
      extension_uninstall_dialog_.reset(ExtensionUninstallDialog::Create(
          GetProfile(), GetCurrentBrowser(), this));
      if (extension_id() != target_extension_id) {
        extension_uninstall_dialog_->ConfirmProgrammaticUninstall(
            target_extension, GetExtension());
      } else {
        // If this is a self uninstall, show the generic uninstall dialog.
        extension_uninstall_dialog_->ConfirmUninstall(target_extension);
      }
    } else {
      Finish(true);
    }
  } else {
    Finish(auto_confirm_for_test == PROCEED);
  }

  return true;
}

// static
void ManagementUninstallFunctionBase::SetAutoConfirmForTest(
    bool should_proceed) {
  auto_confirm_for_test = should_proceed ? PROCEED : ABORT;
}

void ManagementUninstallFunctionBase::Finish(bool should_uninstall) {
  if (should_uninstall) {
    // The extension can be uninstalled in another window while the UI was
    // showing. Do nothing in that case.
    ExtensionRegistry* registry = ExtensionRegistry::Get(GetProfile());
    const Extension* extension = registry->GetExtensionById(
        extension_id_, ExtensionRegistry::EVERYTHING);
    if (!extension) {
      error_ = ErrorUtils::FormatErrorMessage(keys::kNoExtensionError,
                                              extension_id_);
      SendResponse(false);
    } else {
      bool success = service()->UninstallExtension(
          extension_id_, extensions::UNINSTALL_REASON_MANAGEMENT_API, NULL);

      // TODO set error_ if !success
      SendResponse(success);
    }
  } else {
    error_ = ErrorUtils::FormatErrorMessage(
        keys::kUninstallCanceledError, extension_id_);
    SendResponse(false);
  }
}

void ManagementUninstallFunctionBase::ExtensionUninstallAccepted() {
  Finish(true);
  Release();
}

void ManagementUninstallFunctionBase::ExtensionUninstallCanceled() {
  Finish(false);
  Release();
}

ManagementUninstallFunction::ManagementUninstallFunction() {
}

ManagementUninstallFunction::~ManagementUninstallFunction() {
}

bool ManagementUninstallFunction::RunAsync() {
  scoped_ptr<management::Uninstall::Params> params(
      management::Uninstall::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(extension_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  bool show_confirm_dialog = true;
  // By default confirmation dialog isn't shown when uninstalling self, but this
  // can be overridden with showConfirmDialog.
  if (params->id == extension_->id()) {
    show_confirm_dialog = params->options.get() &&
                          params->options->show_confirm_dialog.get() &&
                          *params->options->show_confirm_dialog;
  }
  if (show_confirm_dialog && !user_gesture()) {
    error_ = keys::kGestureNeededForUninstallError;
    return false;
  }
  return Uninstall(params->id, show_confirm_dialog);
}

ManagementUninstallSelfFunction::ManagementUninstallSelfFunction() {
}

ManagementUninstallSelfFunction::~ManagementUninstallSelfFunction() {
}

bool ManagementUninstallSelfFunction::RunAsync() {
  scoped_ptr<management::UninstallSelf::Params> params(
      management::UninstallSelf::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  bool show_confirm_dialog = false;
  if (params->options.get() && params->options->show_confirm_dialog.get())
    show_confirm_dialog = *params->options->show_confirm_dialog;
  return Uninstall(extension_->id(), show_confirm_dialog);
}

ManagementCreateAppShortcutFunction::ManagementCreateAppShortcutFunction() {
}

ManagementCreateAppShortcutFunction::~ManagementCreateAppShortcutFunction() {
}

// static
void ManagementCreateAppShortcutFunction::SetAutoConfirmForTest(
    bool should_proceed) {
  auto_confirm_for_test = should_proceed ? PROCEED : ABORT;
}

void ManagementCreateAppShortcutFunction::OnCloseShortcutPrompt(bool created) {
  if (!created)
    error_ = keys::kCreateShortcutCanceledError;
  SendResponse(created);
  Release();
}

bool ManagementCreateAppShortcutFunction::RunAsync() {
  if (!user_gesture()) {
    error_ = keys::kGestureNeededForCreateAppShortcutError;
    return false;
  }

  scoped_ptr<management::CreateAppShortcut::Params> params(
      management::CreateAppShortcut::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  const Extension* extension = service()->GetExtensionById(params->id, true);
  if (!extension) {
    error_ = ErrorUtils::FormatErrorMessage(keys::kNoExtensionError,
                                            params->id);
    return false;
  }

  if (!extension->is_app()) {
    error_ = ErrorUtils::FormatErrorMessage(keys::kNotAnAppError, params->id);
    return false;
  }

#if defined(OS_MACOSX)
  if (!extension->is_platform_app()) {
    error_ = keys::kCreateOnlyPackagedAppShortcutMac;
    return false;
  }
#endif

  Browser* browser = chrome::FindBrowserWithProfile(
      GetProfile(), chrome::HOST_DESKTOP_TYPE_NATIVE);
  if (!browser) {
    // Shouldn't happen if we have user gesture.
    error_ = keys::kNoBrowserToCreateShortcut;
    return false;
  }

  // Matched with a Release() in OnCloseShortcutPrompt().
  AddRef();

  if (auto_confirm_for_test == DO_NOT_SKIP) {
    chrome::ShowCreateChromeAppShortcutsDialog(
        browser->window()->GetNativeWindow(), browser->profile(), extension,
        base::Bind(&ManagementCreateAppShortcutFunction::OnCloseShortcutPrompt,
           this));
  } else {
    OnCloseShortcutPrompt(auto_confirm_for_test == PROCEED);
  }

  // Response is sent async in OnCloseShortcutPrompt().
  return true;
}

bool ManagementSetLaunchTypeFunction::RunSync() {
  if (!user_gesture()) {
    error_ = keys::kGestureNeededForSetLaunchTypeError;
    return false;
  }

  scoped_ptr<management::SetLaunchType::Params> params(
      management::SetLaunchType::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  const Extension* extension = service()->GetExtensionById(params->id, true);
  if (!extension) {
    error_ =
        ErrorUtils::FormatErrorMessage(keys::kNoExtensionError, params->id);
    return false;
  }

  if (!extension->is_app()) {
    error_ = ErrorUtils::FormatErrorMessage(keys::kNotAnAppError, params->id);
    return false;
  }

  std::vector<management::LaunchType> available_launch_types =
      GetAvailableLaunchTypes(*extension);

  management::LaunchType app_launch_type = params->launch_type;
  if (std::find(available_launch_types.begin(),
                available_launch_types.end(),
                app_launch_type) == available_launch_types.end()) {
    error_ = keys::kLaunchTypeNotAvailableError;
    return false;
  }

  LaunchType launch_type = LAUNCH_TYPE_DEFAULT;
  switch (app_launch_type) {
    case management::LAUNCH_TYPE_OPEN_AS_PINNED_TAB:
      launch_type = LAUNCH_TYPE_PINNED;
      break;
    case management::LAUNCH_TYPE_OPEN_AS_REGULAR_TAB:
      launch_type = LAUNCH_TYPE_REGULAR;
      break;
    case management::LAUNCH_TYPE_OPEN_FULL_SCREEN:
      launch_type = LAUNCH_TYPE_FULLSCREEN;
      break;
    case management::LAUNCH_TYPE_OPEN_AS_WINDOW:
      launch_type = LAUNCH_TYPE_WINDOW;
      break;
    case management::LAUNCH_TYPE_NONE:
      NOTREACHED();
  }

  SetLaunchType(service(), params->id, launch_type);

  return true;
}

ManagementGenerateAppForLinkFunction::ManagementGenerateAppForLinkFunction() {
}

ManagementGenerateAppForLinkFunction::~ManagementGenerateAppForLinkFunction() {
}

void ManagementGenerateAppForLinkFunction::FinishCreateBookmarkApp(
    const Extension* extension,
    const WebApplicationInfo& web_app_info) {
  if (extension) {
    scoped_ptr<management::ExtensionInfo> info =
        CreateExtensionInfo(*extension, ExtensionSystem::Get(GetProfile()));
    results_ = management::GenerateAppForLink::Results::Create(*info);

    SendResponse(true);
    Release();
  } else {
    error_ = keys::kGenerateAppForLinkInstallError;
    SendResponse(false);
    Release();
  }
}

void ManagementGenerateAppForLinkFunction::OnFaviconForApp(
    const favicon_base::FaviconImageResult& image_result) {
  WebApplicationInfo web_app;
  web_app.title = base::UTF8ToUTF16(title_);
  web_app.app_url = launch_url_;

  if (!image_result.image.IsEmpty()) {
    WebApplicationInfo::IconInfo icon;
    icon.data = image_result.image.AsBitmap();
    icon.width = icon.data.width();
    icon.height = icon.data.height();
    web_app.icons.push_back(icon);
  }

  bookmark_app_helper_.reset(new BookmarkAppHelper(service(), web_app, NULL));
  bookmark_app_helper_->Create(base::Bind(
      &ManagementGenerateAppForLinkFunction::FinishCreateBookmarkApp, this));
}

bool ManagementGenerateAppForLinkFunction::RunAsync() {
  if (!user_gesture()) {
    error_ = keys::kGestureNeededForGenerateAppForLinkError;
    return false;
  }

  scoped_ptr<management::GenerateAppForLink::Params> params(
      management::GenerateAppForLink::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  GURL launch_url(params->url);
  if (!launch_url.is_valid() || !launch_url.SchemeIsHTTPOrHTTPS()) {
    error_ = ErrorUtils::FormatErrorMessage(keys::kInvalidURLError,
                                            params->url);
    return false;
  }

  if (params->title.empty()) {
    error_ = keys::kEmptyTitleError;
    return false;
  }

  FaviconService* favicon_service =
      FaviconServiceFactory::GetForProfile(GetProfile(),
                                           Profile::EXPLICIT_ACCESS);
  DCHECK(favicon_service);

  title_ = params->title;
  launch_url_ = launch_url;

  favicon_service->GetFaviconImageForPageURL(
      launch_url,
      base::Bind(&ManagementGenerateAppForLinkFunction::OnFaviconForApp, this),
      &cancelable_task_tracker_);

  // Matched with a Release() in OnExtensionLoaded().
  AddRef();

  // Response is sent async in OnExtensionLoaded().
  return true;
}

ManagementEventRouter::ManagementEventRouter(content::BrowserContext* context)
    : browser_context_(context), extension_registry_observer_(this) {
  extension_registry_observer_.Add(ExtensionRegistry::Get(browser_context_));
}

ManagementEventRouter::~ManagementEventRouter() {}

void ManagementEventRouter::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  BroadcastEvent(extension, management::OnEnabled::kEventName);
}

void ManagementEventRouter::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionInfo::Reason reason) {
  BroadcastEvent(extension, management::OnDisabled::kEventName);
}

void ManagementEventRouter::OnExtensionInstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    bool is_update) {
  BroadcastEvent(extension, management::OnInstalled::kEventName);
}

void ManagementEventRouter::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    extensions::UninstallReason reason) {
  BroadcastEvent(extension, management::OnUninstalled::kEventName);
}

void ManagementEventRouter::BroadcastEvent(const Extension* extension,
                                           const char* event_name) {
  if (ui_util::ShouldNotBeVisible(extension, browser_context_))
    return;  // Don't dispatch events for built-in extenions.
  scoped_ptr<base::ListValue> args(new base::ListValue());
  if (event_name == management::OnUninstalled::kEventName) {
    args->Append(new base::StringValue(extension->id()));
  } else {
    scoped_ptr<management::ExtensionInfo> info =
        CreateExtensionInfo(*extension, ExtensionSystem::Get(browser_context_));
    args->Append(info->ToValue().release());
  }

  EventRouter::Get(browser_context_)
      ->BroadcastEvent(scoped_ptr<Event>(new Event(event_name, args.Pass())));
}

ManagementAPI::ManagementAPI(content::BrowserContext* context)
    : browser_context_(context) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  event_router->RegisterObserver(this, management::OnInstalled::kEventName);
  event_router->RegisterObserver(this, management::OnUninstalled::kEventName);
  event_router->RegisterObserver(this, management::OnEnabled::kEventName);
  event_router->RegisterObserver(this, management::OnDisabled::kEventName);
}

ManagementAPI::~ManagementAPI() {
}

void ManagementAPI::Shutdown() {
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<ManagementAPI> >
    g_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<ManagementAPI>*
ManagementAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

void ManagementAPI::OnListenerAdded(const EventListenerInfo& details) {
  management_event_router_.reset(new ManagementEventRouter(browser_context_));
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

}  // namespace extensions
