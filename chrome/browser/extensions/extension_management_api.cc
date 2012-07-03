// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_management_api.h"

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/metrics/histogram.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "chrome/browser/extensions/extension_event_names.h"
#include "chrome/browser/extensions/extension_event_router.h"
#include "chrome/browser/extensions/extension_management_api_constants.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/extension_uninstall_dialog.h"
#include "chrome/browser/extensions/management_policy.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/webui/extensions/extension_icon_source.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_utility_messages.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_error_utils.h"
#include "chrome/common/extensions/extension_icon_set.h"
#include "chrome/common/extensions/url_pattern.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/utility_process_host.h"
#include "content/public/browser/utility_process_host_client.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/ui/webui/ntp/app_launcher_handler.h"
#endif

using base::IntToString;
using content::BrowserThread;
using content::UtilityProcessHost;
using content::UtilityProcessHostClient;
using extensions::Extension;
using extensions::PermissionMessages;

namespace events = extension_event_names;
namespace keys = extension_management_api_constants;

namespace {

enum AutoConfirmForTest {
  DO_NOT_SKIP = 0,
  PROCEED,
  ABORT
};

AutoConfirmForTest auto_confirm_for_test = DO_NOT_SKIP;

} // namespace

ExtensionService* ExtensionManagementFunction::service() {
  return profile()->GetExtensionService();
}

ExtensionService* AsyncExtensionManagementFunction::service() {
  return profile()->GetExtensionService();
}

static DictionaryValue* CreateExtensionInfo(const Extension& extension,
                                            ExtensionService* service) {
  DictionaryValue* info = new DictionaryValue();
  bool enabled = service->IsExtensionEnabled(extension.id());
  extension.GetBasicInfo(enabled, info);

  const extensions::ManagementPolicy* policy = ExtensionSystem::Get(
      service->profile())->management_policy();
  info->SetBoolean(keys::kMayDisableKey,
                   policy->UserMayModifySettings(&extension, NULL));

  info->SetBoolean(keys::kIsAppKey, extension.is_app());

  if (!enabled) {
    ExtensionPrefs* prefs = service->extension_prefs();
    bool permissions_escalated =
        prefs->DidExtensionEscalatePermissions(extension.id());
    const char* reason = permissions_escalated ?
        keys::kDisabledReasonPermissionsIncrease : keys::kDisabledReasonUnknown;
    info->SetString(keys::kDisabledReasonKey, reason);
  }

  if (!extension.update_url().is_empty())
    info->SetString(keys::kUpdateUrlKey,
                    extension.update_url().possibly_invalid_spec());
  if (extension.is_app())
    info->SetString(keys::kAppLaunchUrlKey,
                    extension.GetFullLaunchURL().possibly_invalid_spec());

  const ExtensionIconSet::IconMap& icons = extension.icons().map();
  if (!icons.empty()) {
    ListValue* icon_list = new ListValue();
    std::map<ExtensionIconSet::Icons, std::string>::const_iterator icon_iter;
    for (icon_iter = icons.begin(); icon_iter != icons.end(); ++icon_iter) {
      DictionaryValue* icon_info = new DictionaryValue();
      ExtensionIconSet::Icons size = icon_iter->first;
      GURL url = ExtensionIconSource::GetIconURL(
          &extension, size, ExtensionIconSet::MATCH_EXACTLY, false, NULL);
      icon_info->SetInteger(keys::kSizeKey, icon_iter->first);
      icon_info->SetString(keys::kUrlKey, url.spec());
      icon_list->Append(icon_info);
    }
    info->Set(keys::kIconsKey, icon_list);
  }

  const std::set<std::string> perms =
      extension.GetActivePermissions()->GetAPIsAsStrings();
  ListValue* permission_list = new ListValue();
  if (!perms.empty()) {
    std::set<std::string>::const_iterator perms_iter;
    for (perms_iter = perms.begin(); perms_iter != perms.end(); ++perms_iter) {
      StringValue* permission_name = new StringValue(*perms_iter);
      permission_list->Append(permission_name);
    }
  }
  info->Set(keys::kPermissionsKey, permission_list);

  ListValue* host_permission_list = new ListValue();
  if (!extension.is_hosted_app()) {
    // Skip host permissions for hosted apps.
    const URLPatternSet host_perms =
        extension.GetActivePermissions()->explicit_hosts();
    if (!host_perms.is_empty()) {
      URLPatternSet::const_iterator host_perms_iter;
      for (host_perms_iter = host_perms.begin();
           host_perms_iter != host_perms.end();
           ++host_perms_iter) {
        StringValue* name = new StringValue(host_perms_iter->GetAsString());
        host_permission_list->Append(name);
      }
    }
  }
  info->Set(keys::kHostPermissionsKey, host_permission_list);

  return info;
}

static void AddExtensionInfo(ListValue* list,
                             const ExtensionSet& extensions,
                             ExtensionService* service) {
  for (ExtensionSet::const_iterator i = extensions.begin();
       i != extensions.end(); ++i) {
    const Extension& extension = **i;

    if (extension.location() == Extension::COMPONENT)
      continue;  // Skip built-in extensions.

    list->Append(CreateExtensionInfo(extension, service));
  }
}

bool GetAllExtensionsFunction::RunImpl() {
  ListValue* result = new ListValue();
  result_.reset(result);

  AddExtensionInfo(result, *service()->extensions(), service());
  AddExtensionInfo(result, *service()->disabled_extensions(), service());

  return true;
}

bool GetExtensionByIdFunction::RunImpl() {
  std::string extension_id;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &extension_id));
  const Extension* extension = service()->GetExtensionById(extension_id, true);
  if (!extension) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(keys::kNoExtensionError,
                                                     extension_id);
    return false;
  }
  DictionaryValue* result = CreateExtensionInfo(*extension, service());
  result_.reset(result);

  return true;
}

bool GetPermissionWarningsByIdFunction::RunImpl() {
  std::string ext_id;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &ext_id));

  const Extension* extension = service()->GetExtensionById(ext_id, true);
  if (!extension) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(keys::kNoExtensionError,
                                                     ext_id);
    return false;
  }

  PermissionMessages warnings = extension->GetPermissionMessages();
  ListValue* result = new ListValue();
  for (PermissionMessages::const_iterator i = warnings.begin();
       i < warnings.end(); ++i)
    result->Append(Value::CreateStringValue(i->message()));
  result_.reset(result);
  return true;
}

namespace {

// This class helps GetPermissionWarningsByManifestFunction manage
// sending manifest JSON strings to the utility process for parsing.
class SafeManifestJSONParser : public UtilityProcessHostClient {
 public:
  SafeManifestJSONParser(GetPermissionWarningsByManifestFunction* client,
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
    UtilityProcessHost* host =
        UtilityProcessHost::Create(this, BrowserThread::IO);
    host->EnableZygote();
    host->Send(new ChromeUtilityMsg_ParseJSON(manifest_));
  }

  virtual bool OnMessageReceived(const IPC::Message& message) {
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

  void OnJSONParseSucceeded(const ListValue& wrapper) {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    Value* value = NULL;
    CHECK(wrapper.Get(0, &value));
    if (value->IsType(Value::TYPE_DICTIONARY))
      parsed_manifest_.reset(static_cast<DictionaryValue*>(value)->DeepCopy());
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
      client_->OnParseSuccess(parsed_manifest_.release());
    else
      client_->OnParseFailure(error_);
  }

 private:
  ~SafeManifestJSONParser() {}

  // The client who we'll report results back to.
  GetPermissionWarningsByManifestFunction* client_;

  // Data to parse.
  std::string manifest_;

  // Results of parsing.
  scoped_ptr<DictionaryValue> parsed_manifest_;

  std::string error_;
};

}  // namespace

bool GetPermissionWarningsByManifestFunction::RunImpl() {
  std::string manifest_str;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &manifest_str));

  scoped_refptr<SafeManifestJSONParser> parser =
      new SafeManifestJSONParser(this, manifest_str);
  parser->Start();

  // Matched with a Release() in OnParseSuccess/Failure().
  AddRef();

  // Response is sent async in OnParseSuccess/Failure().
  return true;
}

void GetPermissionWarningsByManifestFunction::OnParseSuccess(
    DictionaryValue* parsed_manifest) {
  CHECK(parsed_manifest);

  scoped_refptr<Extension> extension = Extension::Create(
      FilePath(), Extension::INVALID, *parsed_manifest, Extension::NO_FLAGS,
      &error_);
  if (!extension.get()) {
    OnParseFailure(keys::kExtensionCreateError);
    return;
  }

  PermissionMessages warnings = extension->GetPermissionMessages();
  ListValue* result = new ListValue();
  for (PermissionMessages::const_iterator i = warnings.begin();
       i < warnings.end(); ++i)
    result->Append(Value::CreateStringValue(i->message()));
  result_.reset(result);
  SendResponse(true);

  // Matched with AddRef() in RunImpl().
  Release();
}

void GetPermissionWarningsByManifestFunction::OnParseFailure(
    const std::string& error) {
  error_ = error;
  SendResponse(false);

  // Matched with AddRef() in RunImpl().
  Release();
}

bool LaunchAppFunction::RunImpl() {
  std::string extension_id;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &extension_id));
  const Extension* extension = service()->GetExtensionById(extension_id, true);
  if (!extension) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(keys::kNoExtensionError,
                                                     extension_id);
    return false;
  }
  if (!extension->is_app()) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(keys::kNotAnAppError,
                                                     extension_id);
    return false;
  }

  // Look at prefs to find the right launch container.
  // |default_pref_value| is set to LAUNCH_REGULAR so that if
  // the user has not set a preference, we open the app in a tab.
  extension_misc::LaunchContainer launch_container =
      service()->extension_prefs()->GetLaunchContainer(
          extension, ExtensionPrefs::LAUNCH_DEFAULT);
  application_launch::OpenApplication(profile(), extension, launch_container,
                                      GURL(), NEW_FOREGROUND_TAB, NULL);
#if !defined(OS_ANDROID)
  AppLauncherHandler::RecordAppLaunchType(
      extension_misc::APP_LAUNCH_EXTENSION_API);
#endif

  return true;
}

SetEnabledFunction::SetEnabledFunction() {
}

SetEnabledFunction::~SetEnabledFunction() {
}

bool SetEnabledFunction::RunImpl() {
  bool enable;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &extension_id_));
  EXTENSION_FUNCTION_VALIDATE(args_->GetBoolean(1, &enable));

  const Extension* extension = service()->GetExtensionById(extension_id_, true);
  if (!extension) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(
        keys::kNoExtensionError, extension_id_);
    return false;
  }

  const extensions::ManagementPolicy* policy = ExtensionSystem::Get(
      profile())->management_policy();
  if (!policy->UserMayModifySettings(extension, NULL)) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(
        keys::kUserCantModifyError, extension_id_);
    return false;
  }

  bool currently_enabled = service()->IsExtensionEnabled(extension_id_);

  if (!currently_enabled && enable) {
    ExtensionPrefs* prefs = service()->extension_prefs();
    if (prefs->DidExtensionEscalatePermissions(extension_id_)) {
      if (!user_gesture()) {
        error_ = keys::kGestureNeededForEscalationError;
        return false;
      }
      AddRef(); // Matched in InstallUIProceed/InstallUIAbort
      install_prompt_.reset(
          chrome::CreateExtensionInstallPromptWithBrowser(GetCurrentBrowser()));
      install_prompt_->ConfirmReEnable(this, extension);
      return true;
    }
    service()->EnableExtension(extension_id_);
  } else if (currently_enabled && !enable) {
    service()->DisableExtension(extension_id_, Extension::DISABLE_USER_ACTION);
  }

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&SetEnabledFunction::SendResponse, this, true));

  return true;
}

void SetEnabledFunction::InstallUIProceed() {
  service()->EnableExtension(extension_id_);
  SendResponse(true);
  Release();
}

void SetEnabledFunction::InstallUIAbort(bool user_initiated) {
  error_ = keys::kUserDidNotReEnableError;
  SendResponse(false);
  Release();
}

UninstallFunction::UninstallFunction() {
}

UninstallFunction::~UninstallFunction() {
}

bool UninstallFunction::RunImpl() {
  bool show_confirm_dialog = false;

  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &extension_id_));

  if (HasOptionalArgument(1)) {
    DictionaryValue* options = NULL;
    EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(1, &options));

    if (options->HasKey(keys::kShowConfirmDialogKey)) {
      EXTENSION_FUNCTION_VALIDATE(options->GetBoolean(
          keys::kShowConfirmDialogKey, &show_confirm_dialog));
    }
  }

  const Extension* extension = service()->GetExtensionById(extension_id_, true);
  if (!extension) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(
        keys::kNoExtensionError, extension_id_);
    return false;
  }

  if (!ExtensionSystem::Get(
      profile())->management_policy()->UserMayModifySettings(extension, NULL)) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(
        keys::kUserCantModifyError, extension_id_);
    return false;
  }

  if (auto_confirm_for_test == DO_NOT_SKIP) {
    if (show_confirm_dialog) {
      AddRef(); // Balanced in ExtensionUninstallAccepted/Canceled
      extension_uninstall_dialog_.reset(ExtensionUninstallDialog::Create(
          GetCurrentBrowser(), this));
      extension_uninstall_dialog_->ConfirmUninstall(extension);
    } else {
      Finish(true);
    }
  } else {
    Finish(auto_confirm_for_test == PROCEED);
  }

  return true;
}

// static
void UninstallFunction::SetAutoConfirmForTest(bool should_proceed) {
  auto_confirm_for_test = should_proceed ? PROCEED : ABORT;
}

void UninstallFunction::Finish(bool should_uninstall) {
  if (should_uninstall) {
    bool success = service()->UninstallExtension(
        extension_id_,
        false, /* external uninstall */
        NULL);

    // TODO set error_ if !success
    SendResponse(success);
  } else {
    error_ = ExtensionErrorUtils::FormatErrorMessage(
        keys::kUninstallCanceledError, extension_id_);
    SendResponse(false);
  }

}

void UninstallFunction::ExtensionUninstallAccepted() {
  Finish(true);
  Release();
}

void UninstallFunction::ExtensionUninstallCanceled() {
  Finish(false);
  Release();
}

ExtensionManagementEventRouter::ExtensionManagementEventRouter(Profile* profile)
    : profile_(profile) {}

ExtensionManagementEventRouter::~ExtensionManagementEventRouter() {}

void ExtensionManagementEventRouter::Init() {
  int types[] = {
    chrome::NOTIFICATION_EXTENSION_INSTALLED,
    chrome::NOTIFICATION_EXTENSION_UNINSTALLED,
    chrome::NOTIFICATION_EXTENSION_LOADED,
    chrome::NOTIFICATION_EXTENSION_UNLOADED
  };

  CHECK(registrar_.IsEmpty());
  for (size_t i = 0; i < arraysize(types); i++) {
    registrar_.Add(this,
                   types[i],
                   content::Source<Profile>(profile_));
  }
}

void ExtensionManagementEventRouter::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  const char* event_name = NULL;
  Profile* profile = content::Source<Profile>(source).ptr();
  CHECK(profile);
  CHECK(profile_->IsSameProfile(profile));

  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_INSTALLED:
      event_name = events::kOnExtensionInstalled;
      break;
    case chrome::NOTIFICATION_EXTENSION_UNINSTALLED:
      event_name = events::kOnExtensionUninstalled;
      break;
    case chrome::NOTIFICATION_EXTENSION_LOADED:
      event_name = events::kOnExtensionEnabled;
      break;
    case chrome::NOTIFICATION_EXTENSION_UNLOADED:
      event_name = events::kOnExtensionDisabled;
      break;
    default:
      NOTREACHED();
      return;
  }

  ListValue args;
  if (event_name == events::kOnExtensionUninstalled) {
    args.Append(Value::CreateStringValue(
        *content::Details<const std::string>(details).ptr()));
  } else {
    const Extension* extension = NULL;
    if (event_name == events::kOnExtensionDisabled) {
      extension = content::Details<extensions::UnloadedExtensionInfo>(
          details)->extension;
    } else {
      extension = content::Details<const Extension>(details).ptr();
    }
    CHECK(extension);
    ExtensionService* service = profile->GetExtensionService();
    args.Append(CreateExtensionInfo(*extension, service));
  }

  std::string args_json;
  base::JSONWriter::Write(&args, &args_json);

  profile->GetExtensionEventRouter()->DispatchEventToRenderers(
      event_name, args_json, NULL, GURL());
}
