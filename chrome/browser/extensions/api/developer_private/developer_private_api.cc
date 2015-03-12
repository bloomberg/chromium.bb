// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/developer_private/developer_private_api.h"

#include "apps/app_load_service.h"
#include "apps/saved_files_service.h"
#include "base/base64.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/extensions/api/developer_private/developer_private_mangle.h"
#include "chrome/browser/extensions/api/developer_private/entry_picker.h"
#include "chrome/browser/extensions/api/developer_private/inspectable_views_finder.h"
#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"
#include "chrome/browser/extensions/api/file_handlers/app_file_handler_util.h"
#include "chrome/browser/extensions/chrome_requirements_checker.h"
#include "chrome/browser/extensions/devtools_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_ui_util.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/path_util.h"
#include "chrome/browser/extensions/shared_module_service.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/extensions/updater/extension_updater.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/extensions/extension_icon_source.h"
#include "chrome/common/extensions/api/developer_private.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/api/device_permissions_manager.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/extension_error.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/file_highlighter.h"
#include "extensions/browser/management_policy.h"
#include "extensions/browser/notification_types.h"
#include "extensions/browser/warning_service.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_resource.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/install_warning.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "extensions/common/manifest_handlers/incognito_info.h"
#include "extensions/common/manifest_handlers/offline_enabled_info.h"
#include "extensions/common/manifest_handlers/options_page_info.h"
#include "extensions/common/manifest_url_handlers.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/switches.h"
#include "extensions/grit/extensions_browser_resources.h"
#include "net/base/net_util.h"
#include "storage/browser/fileapi/external_mount_points.h"
#include "storage/browser/fileapi/file_system_context.h"
#include "storage/browser/fileapi/file_system_operation.h"
#include "storage/browser/fileapi/file_system_operation_runner.h"
#include "storage/browser/fileapi/isolated_context.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/web_ui_util.h"

namespace extensions {

namespace developer_private = api::developer_private;

namespace {

const char kNoSuchExtensionError[] = "No such extension.";
const char kSupervisedUserError[] =
    "Supervised users cannot modify extension settings.";
const char kCannotModifyPolicyExtensionError[] =
    "Cannot modify the extension by policy.";
const char kRequiresUserGestureError[] =
    "This action requires a user gesture.";
const char kCouldNotShowSelectFileDialogError[] =
    "Could not show a file chooser.";
const char kFileSelectionCanceled[] =
    "File selection was canceled.";
const char kNoSuchRendererError[] = "No such renderer.";
const char kInvalidPathError[] = "Invalid path.";
const char kManifestKeyIsRequiredError[] =
    "The 'manifestKey' argument is required for manifest files.";

const char kUnpackedAppsFolder[] = "apps_target";
const char kManifestFile[] = "manifest.json";

ExtensionService* GetExtensionService(content::BrowserContext* context) {
  return ExtensionSystem::Get(context)->extension_service();
}

ExtensionUpdater* GetExtensionUpdater(Profile* profile) {
  return GetExtensionService(profile)->updater();
}

GURL GetImageURLFromData(const std::string& contents) {
  std::string contents_base64;
  base::Base64Encode(contents, &contents_base64);

  // TODO(dvh): make use of content::kDataScheme. Filed as crbug/297301.
  const char kDataURLPrefix[] = "data:;base64,";
  return GURL(kDataURLPrefix + contents_base64);
}

GURL GetDefaultImageURL(developer_private::ItemType type) {
  int icon_resource_id;
  switch (type) {
    case developer::ITEM_TYPE_LEGACY_PACKAGED_APP:
    case developer::ITEM_TYPE_HOSTED_APP:
    case developer::ITEM_TYPE_PACKAGED_APP:
      icon_resource_id = IDR_APP_DEFAULT_ICON;
      break;
    default:
      icon_resource_id = IDR_EXTENSION_DEFAULT_ICON;
      break;
  }

  return GetImageURLFromData(
      ResourceBundle::GetSharedInstance().GetRawDataResourceForScale(
          icon_resource_id, ui::SCALE_FACTOR_100P).as_string());
}

// TODO(dvh): This code should be refactored and moved to
// extensions::ImageLoader. Also a resize should be performed to avoid
// potential huge URLs: crbug/297298.
GURL ToDataURL(const base::FilePath& path, developer_private::ItemType type) {
  std::string contents;
  if (path.empty() || !base::ReadFileToString(path, &contents))
    return GetDefaultImageURL(type);

  return GetImageURLFromData(contents);
}

std::string GetExtensionID(const content::RenderViewHost* render_view_host) {
  if (!render_view_host->GetSiteInstance())
    return std::string();

  return render_view_host->GetSiteInstance()->GetSiteURL().host();
}

void BroadcastItemStateChanged(content::BrowserContext* browser_context,
                               developer::EventType event_type,
                               const std::string& item_id) {
  developer::EventData event_data;
  event_data.event_type = event_type;
  event_data.item_id = item_id;

  scoped_ptr<base::ListValue> args(new base::ListValue());
  args->Append(event_data.ToValue().release());
  scoped_ptr<Event> event(new Event(
      developer_private::OnItemStateChanged::kEventName, args.Pass()));
  EventRouter::Get(browser_context)->BroadcastEvent(event.Pass());
}

std::string ReadFileToString(const base::FilePath& path) {
  std::string data;
  ignore_result(base::ReadFileToString(path, &data));
  return data;
}

// Populates the common fields of an extension error.
template <typename ErrorType>
void PopulateErrorBase(const ExtensionError& error,
                       ErrorType* out) {
  CHECK(out);
  out->type = error.type() == ExtensionError::MANIFEST_ERROR ?
      developer::ERROR_TYPE_MANIFEST : developer::ERROR_TYPE_RUNTIME;
  out->extension_id = error.extension_id();
  out->from_incognito = error.from_incognito();
  out->source = base::UTF16ToUTF8(error.source());
  out->message = base::UTF16ToUTF8(error.message());
}

// Given a ManifestError object, converts it into its developer_private
// counterpart.
linked_ptr<developer::ManifestError> ConstructManifestError(
    const ManifestError& error) {
  linked_ptr<developer::ManifestError> result(new developer::ManifestError());
  PopulateErrorBase(error, result.get());
  result->manifest_key = base::UTF16ToUTF8(error.manifest_key());
  if (!error.manifest_specific().empty()) {
    result->manifest_specific.reset(
        new std::string(base::UTF16ToUTF8(error.manifest_specific())));
  }
  return result;
}

// Given a RuntimeError object, converts it into its developer_private
// counterpart.
linked_ptr<developer::RuntimeError> ConstructRuntimeError(
    const RuntimeError& error) {
  linked_ptr<developer::RuntimeError> result(new developer::RuntimeError());
  PopulateErrorBase(error, result.get());
  switch (error.level()) {
    case logging::LOG_INFO:
      result->severity = developer::ERROR_LEVEL_LOG;
      break;
    case logging::LOG_WARNING:
      result->severity = developer::ERROR_LEVEL_WARN;
      break;
    case logging::LOG_ERROR:
      result->severity = developer::ERROR_LEVEL_ERROR;
      break;
    default:
      NOTREACHED();
  }
  result->occurrences = error.occurrences();
  result->render_view_id = error.render_view_id();
  result->render_process_id = error.render_process_id();
  result->can_inspect =
      content::RenderViewHost::FromID(error.render_process_id(),
                                      error.render_view_id()) != nullptr;
  for (const StackFrame& f : error.stack_trace()) {
    linked_ptr<developer::StackFrame> frame(new developer::StackFrame());
    frame->line_number = f.line_number;
    frame->column_number = f.column_number;
    frame->url = base::UTF16ToUTF8(f.source);
    frame->function_name = base::UTF16ToUTF8(f.function);
    result->stack_trace.push_back(frame);
  }
  return result;
}

// Given a Manifest::Type, converts it into its developer_private
// counterpart.
developer::ExtensionType GetExtensionType(Manifest::Type manifest_type) {
  developer::ExtensionType type = developer::EXTENSION_TYPE_EXTENSION;
  switch (manifest_type) {
    case Manifest::TYPE_EXTENSION:
      type = developer::EXTENSION_TYPE_EXTENSION;
      break;
    case Manifest::TYPE_THEME:
      type = developer::EXTENSION_TYPE_THEME;
      break;
    case Manifest::TYPE_HOSTED_APP:
      type = developer::EXTENSION_TYPE_HOSTED_APP;
      break;
    case Manifest::TYPE_LEGACY_PACKAGED_APP:
      type = developer::EXTENSION_TYPE_LEGACY_PACKAGED_APP;
      break;
    case Manifest::TYPE_PLATFORM_APP:
      type = developer::EXTENSION_TYPE_PLATFORM_APP;
      break;
    case Manifest::TYPE_SHARED_MODULE:
      type = developer::EXTENSION_TYPE_SHARED_MODULE;
      break;
    default:
      NOTREACHED();
  }
  return type;
}

// Constructs information about a given |extension|.
scoped_ptr<developer::ExtensionInfo> CreateExtensionInfo(
    const Extension& extension,
    developer::ExtensionState state,
    content::BrowserContext* browser_context) {
  scoped_ptr<developer::ExtensionInfo> info(new developer::ExtensionInfo());

  ExtensionPrefs* prefs = ExtensionPrefs::Get(browser_context);
  // Don't consider the button hidden with the redesign, because "hidden"
  // buttons are now just hidden in the wrench menu.
  info->action_button_hidden =
      !ExtensionActionAPI::GetBrowserActionVisibility(prefs, extension.id()) &&
      !FeatureSwitch::extension_action_redesign()->IsEnabled();

  // Blacklist text.
  int blacklist_text = -1;
  switch (prefs->GetExtensionBlacklistState(extension.id())) {
    case BLACKLISTED_SECURITY_VULNERABILITY:
      blacklist_text = IDS_OPTIONS_BLACKLISTED_SECURITY_VULNERABILITY;
      break;
    case BLACKLISTED_CWS_POLICY_VIOLATION:
      blacklist_text = IDS_OPTIONS_BLACKLISTED_CWS_POLICY_VIOLATION;
      break;
    case BLACKLISTED_POTENTIALLY_UNWANTED:
      blacklist_text = IDS_OPTIONS_BLACKLISTED_POTENTIALLY_UNWANTED;
      break;
    default:
      break;
  }
  if (blacklist_text != -1) {
    info->blacklist_text.reset(
        new std::string(l10n_util::GetStringUTF8(blacklist_text)));
  }

  // Dependent extensions.
  if (extension.is_shared_module()) {
    scoped_ptr<ExtensionSet> dependent_extensions =
        ExtensionSystem::Get(browser_context)->extension_service()->
            shared_module_service()->GetDependentExtensions(&extension);
    for (const scoped_refptr<const Extension>& dependent :
             *dependent_extensions)
      info->dependent_extensions.push_back(dependent->id());
  }

  info->description = extension.description();

  // Disable reasons.
  int disable_reasons = prefs->GetDisableReasons(extension.id());
  info->disable_reasons.suspicious_install =
      (disable_reasons & Extension::DISABLE_NOT_VERIFIED) != 0;
  info->disable_reasons.corrupt_install =
      (disable_reasons & Extension::DISABLE_CORRUPTED) != 0;
  info->disable_reasons.update_required =
      (disable_reasons & Extension::DISABLE_UPDATE_REQUIRED_BY_POLICY) != 0;

  // Error collection.
  Profile* profile = Profile::FromBrowserContext(browser_context);
  ErrorConsole* error_console = ErrorConsole::Get(profile);
  bool error_console_enabled =
      error_console->IsEnabledForChromeExtensionsPage();
  info->error_collection.is_enabled = error_console_enabled;
  info->error_collection.is_active =
      error_console_enabled &&
      error_console->IsReportingEnabledForExtension(extension.id());

  // File access.
  info->file_access.is_enabled = extension.wants_file_access();
  info->file_access.is_active =
      util::AllowFileAccess(extension.id(), browser_context);

  // Home page.
  info->home_page.url = ManifestURL::GetHomepageURL(&extension).spec();
  info->home_page.specified = ManifestURL::SpecifiedHomepageURL(&extension);

  bool is_enabled = state == developer::EXTENSION_STATE_ENABLED;

  // TODO(devlin): This won't work with apps (CORS). We should convert to data
  // urls.
  info->icon_url =
      ExtensionIconSource::GetIconURL(&extension,
                                      extension_misc::EXTENSION_ICON_MEDIUM,
                                      ExtensionIconSet::MATCH_BIGGER,
                                      !is_enabled,
                                      nullptr).spec();

  info->id = extension.id();

  // Incognito access.
  info->incognito_access.is_enabled = extension.can_be_incognito_enabled();
  info->incognito_access.is_active =
      util::IsIncognitoEnabled(extension.id(), browser_context);

  info->installed_by_custodian =
      util::IsExtensionSupervised(&extension, profile);

  // Install warnings (only if unpacked and no error console).
  if (!error_console_enabled &&
      Manifest::IsUnpackedLocation(extension.location())) {
    const std::vector<InstallWarning>& install_warnings =
        extension.install_warnings();
    for (const InstallWarning& warning : install_warnings)
      info->install_warnings.push_back(warning.message);
  }

  // Launch url.
  if (extension.is_app()) {
    info->launch_url.reset(
        new std::string(AppLaunchInfo::GetFullLaunchURL(&extension).spec()));
  }

  // Location.
  if (extension.location() == Manifest::INTERNAL &&
      ManifestURL::UpdatesFromGallery(&extension)) {
    info->location = developer::LOCATION_FROM_STORE;
  } else if (Manifest::IsUnpackedLocation(extension.location())) {
    info->location = developer::LOCATION_UNPACKED;
  } else {
    info->location = developer::LOCATION_UNKNOWN;
  }

  // Location text.
  int location_text = -1;
  if (info->location == developer::LOCATION_UNKNOWN)
    location_text = IDS_OPTIONS_INSTALL_LOCATION_UNKNOWN;
  else if (extension.location() == Manifest::EXTERNAL_REGISTRY)
    location_text = IDS_OPTIONS_INSTALL_LOCATION_3RD_PARTY;
  else if (extension.is_shared_module())
    location_text = IDS_OPTIONS_INSTALL_LOCATION_SHARED_MODULE;
  if (location_text != -1) {
    info->location_text.reset(
        new std::string(l10n_util::GetStringUTF8(location_text)));
  }

  // Runtime/Manifest errors.
  if (error_console_enabled) {
    const ErrorList& errors =
        error_console->GetErrorsForExtension(extension.id());
    for (const ExtensionError* error : errors) {
      switch (error->type()) {
        case ExtensionError::MANIFEST_ERROR:
          info->manifest_errors.push_back(ConstructManifestError(
              static_cast<const ManifestError&>(*error)));
          break;
        case ExtensionError::RUNTIME_ERROR:
          info->runtime_errors.push_back(ConstructRuntimeError(
              static_cast<const RuntimeError&>(*error)));
          break;
        case ExtensionError::NUM_ERROR_TYPES:
          NOTREACHED();
          break;
      }
    }
  }

  ManagementPolicy* management_policy =
      ExtensionSystem::Get(browser_context)->management_policy();
  info->must_remain_installed =
      management_policy->MustRemainInstalled(&extension, nullptr);

  info->name = extension.name();
  info->offline_enabled = OfflineEnabledInfo::IsOfflineEnabled(&extension);

  // Options page.
  if (OptionsPageInfo::HasOptionsPage(&extension)) {
    info->options_page.reset(new developer::OptionsPage());
    info->options_page->open_in_tab =
        OptionsPageInfo::ShouldOpenInTab(&extension);
    info->options_page->url =
        OptionsPageInfo::GetOptionsPage(&extension).spec();
  }

  // Path.
  if (Manifest::IsUnpackedLocation(extension.location())) {
    info->path.reset(new std::string(extension.path().AsUTF8Unsafe()));
    info->prettified_path.reset(new std::string(
      extensions::path_util::PrettifyPath(extension.path()).AsUTF8Unsafe()));
  }

  if (Manifest::IsPolicyLocation(extension.location())) {
    info->policy_text.reset(new std::string(
        l10n_util::GetStringUTF8(IDS_OPTIONS_INSTALL_LOCATION_ENTERPRISE)));
  }

  // Runs on all urls.
  info->run_on_all_urls.is_enabled =
      (FeatureSwitch::scripts_require_action()->IsEnabled() &&
       PermissionsData::ScriptsMayRequireActionForExtension(
           &extension,
           extension.permissions_data()->active_permissions().get())) ||
      extension.permissions_data()->HasWithheldImpliedAllHosts() ||
      util::HasSetAllowedScriptingOnAllUrls(extension.id(), browser_context);
  info->run_on_all_urls.is_active =
      util::AllowedScriptingOnAllUrls(extension.id(), browser_context);

  // Runtime warnings.
  WarningService* warning_service = WarningService::Get(profile);
  std::vector<std::string> warnings =
      warning_service->GetWarningMessagesForExtension(extension.id());
  for (const std::string& warning : warnings)
    info->runtime_warnings.push_back(warning);

  info->state = state;

  info->type = GetExtensionType(extension.manifest()->type());

  info->update_url = ManifestURL::GetUpdateURL(&extension).spec();

  info->user_may_modify =
      management_policy->UserMayModifySettings(&extension, nullptr);

  info->version = extension.GetVersionForDisplay();

  if (state != developer::EXTENSION_STATE_TERMINATED) {
    info->views = InspectableViewsFinder(profile, nullptr).
                      GetViewsForExtension(extension, is_enabled);
  }
  return info.Pass();
}

std::vector<linked_ptr<developer::ExtensionInfo>> GetExtensionsInfo(
    content::BrowserContext* context,
    bool include_disabled,
    bool include_terminated) {
  std::vector<linked_ptr<developer::ExtensionInfo>> list;
  auto add_to_list = [&list, context](const ExtensionSet& extensions,
                                      developer::ExtensionState state) {
    for (const scoped_refptr<const Extension>& extension : extensions) {
      if (ui_util::ShouldDisplayInExtensionSettings(extension.get(), context)) {
        scoped_ptr<developer::ExtensionInfo> info =
            CreateExtensionInfo(*extension, state, context);
        list.push_back(make_linked_ptr(info.release()));
      }
    }
  };

  ExtensionRegistry* registry = ExtensionRegistry::Get(context);
  add_to_list(registry->enabled_extensions(),
              developer::EXTENSION_STATE_ENABLED);
  if (include_disabled) {
    add_to_list(registry->disabled_extensions(),
                developer::EXTENSION_STATE_DISABLED);
  }
  if (include_terminated) {
    add_to_list(registry->terminated_extensions(),
                developer::EXTENSION_STATE_TERMINATED);
  }

  return list;
}

}  // namespace

namespace AllowFileAccess = api::developer_private::AllowFileAccess;
namespace AllowIncognito = api::developer_private::AllowIncognito;
namespace ChoosePath = api::developer_private::ChoosePath;
namespace GetItemsInfo = api::developer_private::GetItemsInfo;
namespace Inspect = api::developer_private::Inspect;
namespace PackDirectory = api::developer_private::PackDirectory;
namespace Reload = api::developer_private::Reload;

static base::LazyInstance<BrowserContextKeyedAPIFactory<DeveloperPrivateAPI> >
    g_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<DeveloperPrivateAPI>*
DeveloperPrivateAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

// static
DeveloperPrivateAPI* DeveloperPrivateAPI::Get(
    content::BrowserContext* context) {
  return GetFactoryInstance()->Get(context);
}

DeveloperPrivateAPI::DeveloperPrivateAPI(content::BrowserContext* context)
    : profile_(Profile::FromBrowserContext(context)) {
  RegisterNotifications();
}

DeveloperPrivateEventRouter::DeveloperPrivateEventRouter(Profile* profile)
    : extension_registry_observer_(this), profile_(profile) {
  registrar_.Add(this,
                 extensions::NOTIFICATION_EXTENSION_VIEW_REGISTERED,
                 content::Source<Profile>(profile_));
  registrar_.Add(this,
                 extensions::NOTIFICATION_EXTENSION_VIEW_UNREGISTERED,
                 content::Source<Profile>(profile_));

  // TODO(limasdf): Use scoped_observer instead.
  ErrorConsole::Get(profile)->AddObserver(this);

  extension_registry_observer_.Add(ExtensionRegistry::Get(profile_));
}

DeveloperPrivateEventRouter::~DeveloperPrivateEventRouter() {
  ErrorConsole::Get(profile_)->RemoveObserver(this);
}

void DeveloperPrivateEventRouter::AddExtensionId(
    const std::string& extension_id) {
  extension_ids_.insert(extension_id);
}

void DeveloperPrivateEventRouter::RemoveExtensionId(
    const std::string& extension_id) {
  extension_ids_.erase(extension_id);
}

void DeveloperPrivateEventRouter::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  Profile* profile = content::Source<Profile>(source).ptr();
  CHECK(profile);
  CHECK(profile_->IsSameProfile(profile));
  developer::EventData event_data;

  switch (type) {
    case extensions::NOTIFICATION_EXTENSION_VIEW_UNREGISTERED: {
      event_data.event_type = developer::EVENT_TYPE_VIEW_UNREGISTERED;
      event_data.item_id = GetExtensionID(
          content::Details<const content::RenderViewHost>(details).ptr());
      break;
    }
    case extensions::NOTIFICATION_EXTENSION_VIEW_REGISTERED: {
      event_data.event_type = developer::EVENT_TYPE_VIEW_REGISTERED;
      event_data.item_id = GetExtensionID(
          content::Details<const content::RenderViewHost>(details).ptr());
      break;
    }
    default:
      NOTREACHED();
      return;
  }

  BroadcastItemStateChanged(profile, event_data.event_type, event_data.item_id);
}

void DeveloperPrivateEventRouter::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  DCHECK(profile_->IsSameProfile(Profile::FromBrowserContext(browser_context)));
  BroadcastItemStateChanged(
      browser_context, developer::EVENT_TYPE_LOADED, extension->id());
}

void DeveloperPrivateEventRouter::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionInfo::Reason reason) {
  DCHECK(profile_->IsSameProfile(Profile::FromBrowserContext(browser_context)));
  BroadcastItemStateChanged(
      browser_context, developer::EVENT_TYPE_UNLOADED, extension->id());
}

void DeveloperPrivateEventRouter::OnExtensionWillBeInstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    bool is_update,
    bool from_ephemeral,
    const std::string& old_name) {
  DCHECK(profile_->IsSameProfile(Profile::FromBrowserContext(browser_context)));
  BroadcastItemStateChanged(
      browser_context, developer::EVENT_TYPE_INSTALLED, extension->id());
}

void DeveloperPrivateEventRouter::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    extensions::UninstallReason reason) {
  DCHECK(profile_->IsSameProfile(Profile::FromBrowserContext(browser_context)));
  BroadcastItemStateChanged(
      browser_context, developer::EVENT_TYPE_UNINSTALLED, extension->id());
}

void DeveloperPrivateEventRouter::OnErrorAdded(const ExtensionError* error) {
  // We don't want to handle errors thrown by extensions subscribed to these
  // events (currently only the Apps Developer Tool), because doing so risks
  // entering a loop.
  if (extension_ids_.find(error->extension_id()) != extension_ids_.end())
    return;

  BroadcastItemStateChanged(
      profile_, developer::EVENT_TYPE_ERROR_ADDED, error->extension_id());
}

void DeveloperPrivateAPI::SetLastUnpackedDirectory(const base::FilePath& path) {
  last_unpacked_directory_ = path;
}

void DeveloperPrivateAPI::RegisterNotifications() {
  EventRouter::Get(profile_)->RegisterObserver(
      this, developer_private::OnItemStateChanged::kEventName);
}

DeveloperPrivateAPI::~DeveloperPrivateAPI() {}

void DeveloperPrivateAPI::Shutdown() {}

void DeveloperPrivateAPI::OnListenerAdded(
    const EventListenerInfo& details) {
  if (!developer_private_event_router_) {
    developer_private_event_router_.reset(
        new DeveloperPrivateEventRouter(profile_));
  }

  developer_private_event_router_->AddExtensionId(details.extension_id);
}

void DeveloperPrivateAPI::OnListenerRemoved(
    const EventListenerInfo& details) {
  if (!EventRouter::Get(profile_)->HasEventListener(
          developer_private::OnItemStateChanged::kEventName)) {
    developer_private_event_router_.reset(NULL);
  } else {
    developer_private_event_router_->RemoveExtensionId(details.extension_id);
  }
}

namespace api {

DeveloperPrivateAPIFunction::~DeveloperPrivateAPIFunction() {
}

const Extension* DeveloperPrivateAPIFunction::GetExtensionById(
    const std::string& id) {
  return ExtensionRegistry::Get(browser_context())->GetExtensionById(
      id, ExtensionRegistry::EVERYTHING);
}

bool DeveloperPrivateAutoUpdateFunction::RunSync() {
  ExtensionUpdater* updater = GetExtensionUpdater(GetProfile());
  if (updater)
    updater->CheckNow(ExtensionUpdater::CheckParams());
  SetResult(new base::FundamentalValue(true));
  return true;
}

DeveloperPrivateAutoUpdateFunction::~DeveloperPrivateAutoUpdateFunction() {}

DeveloperPrivateGetExtensionsInfoFunction::
~DeveloperPrivateGetExtensionsInfoFunction() {
}

ExtensionFunction::ResponseAction
DeveloperPrivateGetExtensionsInfoFunction::Run() {
  scoped_ptr<developer::GetExtensionsInfo::Params> params(
      developer::GetExtensionsInfo::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  bool include_disabled = true;
  bool include_terminated = true;
  if (params->options) {
    if (params->options->include_disabled)
      include_disabled = *params->options->include_disabled;
    if (params->options->include_terminated)
      include_terminated = *params->options->include_terminated;
  }

  std::vector<linked_ptr<developer::ExtensionInfo>> list =
      GetExtensionsInfo(browser_context(),
                        include_disabled,
                        include_terminated);

  return RespondNow(ArgumentList(
      developer::GetExtensionsInfo::Results::Create(list)));
}

DeveloperPrivateGetExtensionInfoFunction::
~DeveloperPrivateGetExtensionInfoFunction() {
}

ExtensionFunction::ResponseAction
DeveloperPrivateGetExtensionInfoFunction::Run() {
  scoped_ptr<developer::GetExtensionInfo::Params> params(
      developer::GetExtensionInfo::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context());
  developer::ExtensionState state = developer::EXTENSION_STATE_ENABLED;
  const Extension* extension =
      registry->enabled_extensions().GetByID(params->id);
  if (!extension &&
      (extension = registry->disabled_extensions().GetByID(params->id)) !=
          nullptr) {
    state = developer::EXTENSION_STATE_DISABLED;
  } else if (!extension &&
             (extension =
                  registry->terminated_extensions().GetByID(params->id)) !=
                  nullptr) {
    state = developer::EXTENSION_STATE_TERMINATED;
  } else {
    return RespondNow(Error(kNoSuchExtensionError));
  }

  return RespondNow(OneArgument(CreateExtensionInfo(
      *extension, state, browser_context())->ToValue().release()));
}

DeveloperPrivateGetItemsInfoFunction::DeveloperPrivateGetItemsInfoFunction() {}
DeveloperPrivateGetItemsInfoFunction::~DeveloperPrivateGetItemsInfoFunction() {}

ExtensionFunction::ResponseAction DeveloperPrivateGetItemsInfoFunction::Run() {
  scoped_ptr<developer::GetItemsInfo::Params> params(
      developer::GetItemsInfo::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  ExtensionSet items;
  ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context());
  items.InsertAll(registry->enabled_extensions());

  if (params->include_disabled)
    items.InsertAll(registry->disabled_extensions());
  if (params->include_terminated)
    items.InsertAll(registry->terminated_extensions());

  std::map<std::string, ExtensionResource> resource_map;
  for (const scoped_refptr<const Extension>& item : items) {
    // Don't show component extensions and invisible apps.
    if (ui_util::ShouldDisplayInExtensionSettings(item.get(),
                                                  browser_context())) {
      resource_map[item->id()] =
          IconsInfo::GetIconResource(item.get(),
                                     extension_misc::EXTENSION_ICON_MEDIUM,
                                     ExtensionIconSet::MATCH_BIGGER);
    }
  }

  std::vector<linked_ptr<developer::ExtensionInfo>> list =
      GetExtensionsInfo(browser_context(),
                        params->include_disabled,
                        params->include_terminated);

  for (const linked_ptr<developer::ExtensionInfo>& info : list)
    item_list_.push_back(developer_private_mangle::MangleExtensionInfo(*info));

  content::BrowserThread::PostTask(
      content::BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&DeveloperPrivateGetItemsInfoFunction::GetIconsOnFileThread,
                 this,
                 resource_map));

  return RespondLater();
}

void DeveloperPrivateGetItemsInfoFunction::GetIconsOnFileThread(
    const std::map<std::string, ExtensionResource> resource_map) {
  for (const linked_ptr<developer::ItemInfo>& item : item_list_) {
    auto resource = resource_map.find(item->id);
    if (resource != resource_map.end()) {
      item->icon_url = ToDataURL(resource->second.GetFilePath(),
                                 item->type).spec();
    }
  }

  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(&DeveloperPrivateGetItemsInfoFunction::Finish, this));
}

void DeveloperPrivateGetItemsInfoFunction::Finish() {
  Respond(ArgumentList(developer::GetItemsInfo::Results::Create(item_list_)));
}

ExtensionFunction::ResponseAction
DeveloperPrivateAllowFileAccessFunction::Run() {
  scoped_ptr<AllowFileAccess::Params> params(
      AllowFileAccess::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  const Extension* extension = GetExtensionById(params->extension_id);

  if (!extension)
    return RespondNow(Error(kNoSuchExtensionError));

  if (!user_gesture())
    return RespondNow(Error(kRequiresUserGestureError));

  if (util::IsExtensionSupervised(
          extension, Profile::FromBrowserContext(browser_context()))) {
    return RespondNow(Error(kSupervisedUserError));
  }

  ManagementPolicy* management_policy =
      ExtensionSystem::Get(browser_context())->management_policy();
  if (!management_policy->UserMayModifySettings(extension, nullptr)) {
    LOG(ERROR) << "Attempt to change allow file access of an extension that "
               << "non-usermanagable was made. Extension id : "
               << extension->id();
    return RespondNow(Error(kCannotModifyPolicyExtensionError));
  }

  util::SetAllowFileAccess(extension->id(), browser_context(), params->allow);
  return RespondNow(NoArguments());
}

DeveloperPrivateAllowFileAccessFunction::
    ~DeveloperPrivateAllowFileAccessFunction() {}

ExtensionFunction::ResponseAction
DeveloperPrivateAllowIncognitoFunction::Run() {
  scoped_ptr<AllowIncognito::Params> params(
      AllowIncognito::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  const Extension* extension = GetExtensionById(params->extension_id);

  if (!extension)
    return RespondNow(Error(kNoSuchExtensionError));

  if (!user_gesture())
    return RespondNow(Error(kRequiresUserGestureError));

  // Should this take into account policy settings?
  util::SetIsIncognitoEnabled(
      extension->id(), browser_context(), params->allow);

  return RespondNow(NoArguments());
}

DeveloperPrivateAllowIncognitoFunction::
    ~DeveloperPrivateAllowIncognitoFunction() {}

ExtensionFunction::ResponseAction DeveloperPrivateReloadFunction::Run() {
  scoped_ptr<Reload::Params> params(Reload::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  const Extension* extension = GetExtensionById(params->extension_id);
  if (!extension)
    return RespondNow(Error(kNoSuchExtensionError));

  bool fail_quietly = params->options &&
                      params->options->fail_quietly &&
                      *params->options->fail_quietly;

  ExtensionService* service = GetExtensionService(browser_context());
  if (fail_quietly)
    service->ReloadExtensionWithQuietFailure(params->extension_id);
  else
    service->ReloadExtension(params->extension_id);

  // TODO(devlin): We shouldn't return until the extension has finished trying
  // to reload (and then we could also return the error).
  return RespondNow(NoArguments());
}

bool DeveloperPrivateShowPermissionsDialogFunction::RunSync() {
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &extension_id_));
  CHECK(!extension_id_.empty());
  AppWindowRegistry* registry = AppWindowRegistry::Get(GetProfile());
  DCHECK(registry);
  AppWindow* app_window =
      registry->GetAppWindowForRenderViewHost(render_view_host());
  prompt_.reset(new ExtensionInstallPrompt(app_window->web_contents()));
  const Extension* extension =
      ExtensionRegistry::Get(GetProfile())
          ->GetExtensionById(extension_id_, ExtensionRegistry::EVERYTHING);

  if (!extension)
    return false;

  // Released by InstallUIAbort or InstallUIProceed.
  AddRef();
  std::vector<base::FilePath> retained_file_paths;
  if (extension->permissions_data()->HasAPIPermission(
          APIPermission::kFileSystem)) {
    std::vector<apps::SavedFileEntry> retained_file_entries =
        apps::SavedFilesService::Get(GetProfile())
            ->GetAllFileEntries(extension_id_);
    for (size_t i = 0; i < retained_file_entries.size(); i++) {
      retained_file_paths.push_back(retained_file_entries[i].path);
    }
  }
  std::vector<base::string16> retained_device_messages;
  if (extension->permissions_data()->HasAPIPermission(APIPermission::kUsb)) {
    retained_device_messages =
        extensions::DevicePermissionsManager::Get(GetProfile())
            ->GetPermissionMessageStrings(extension_id_);
  }
  prompt_->ReviewPermissions(
      this, extension, retained_file_paths, retained_device_messages);
  return true;
}

DeveloperPrivateReloadFunction::~DeveloperPrivateReloadFunction() {}

// This is called when the user clicks "Revoke File Access."
void DeveloperPrivateShowPermissionsDialogFunction::InstallUIProceed() {
  Profile* profile = GetProfile();
  extensions::DevicePermissionsManager::Get(profile)->Clear(extension_id_);
  const Extension* extension = ExtensionRegistry::Get(
      profile)->GetExtensionById(extension_id_, ExtensionRegistry::EVERYTHING);
  apps::SavedFilesService::Get(profile)->ClearQueue(extension);
  apps::AppLoadService::Get(profile)
      ->RestartApplicationIfRunning(extension_id_);
  SendResponse(true);
  Release();
}

void DeveloperPrivateShowPermissionsDialogFunction::InstallUIAbort(
    bool user_initiated) {
  SendResponse(true);
  Release();
}

DeveloperPrivateShowPermissionsDialogFunction::
    DeveloperPrivateShowPermissionsDialogFunction() {}

DeveloperPrivateShowPermissionsDialogFunction::
    ~DeveloperPrivateShowPermissionsDialogFunction() {}

ExtensionFunction::ResponseAction DeveloperPrivateInspectFunction::Run() {
  scoped_ptr<developer::Inspect::Params> params(
      developer::Inspect::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  const developer::InspectOptions& options = params->options;

  int render_process_id = 0;
  if (options.render_process_id.as_string &&
      !base::StringToInt(*options.render_process_id.as_string,
                         &render_process_id)) {
    return RespondNow(Error(kNoSuchRendererError));
  } else if (options.render_process_id.as_integer) {
    render_process_id = *options.render_process_id.as_integer;
  }

  int render_view_id = 0;
  if (options.render_view_id.as_string &&
      !base::StringToInt(*options.render_view_id.as_string, &render_view_id)) {
    return RespondNow(Error(kNoSuchRendererError));
  } else if (options.render_view_id.as_integer) {
    render_view_id = *options.render_view_id.as_integer;
  }

  if (render_process_id == -1) {
    // This is a lazy background page.
    const Extension* extension = ExtensionRegistry::Get(
        browser_context())->enabled_extensions().GetByID(options.extension_id);
    if (!extension)
      return RespondNow(Error(kNoSuchExtensionError));

    Profile* profile = Profile::FromBrowserContext(browser_context());
    if (options.incognito)
      profile = profile->GetOffTheRecordProfile();

    // Wakes up the background page and opens the inspect window.
    devtools_util::InspectBackgroundPage(extension, profile);
    return RespondNow(NoArguments());
  }

  content::RenderViewHost* host = content::RenderViewHost::FromID(
      render_process_id, render_view_id);

  if (!host || !content::WebContents::FromRenderViewHost(host))
    return RespondNow(Error(kNoSuchRendererError));

  DevToolsWindow::OpenDevToolsWindow(
      content::WebContents::FromRenderViewHost(host));
  return RespondNow(NoArguments());
}

DeveloperPrivateInspectFunction::~DeveloperPrivateInspectFunction() {}

DeveloperPrivateLoadUnpackedFunction::DeveloperPrivateLoadUnpackedFunction()
    : fail_quietly_(false) {
}

ExtensionFunction::ResponseAction DeveloperPrivateLoadUnpackedFunction::Run() {
  scoped_ptr<developer_private::LoadUnpacked::Params> params(
      developer_private::LoadUnpacked::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  if (!ShowPicker(
           ui::SelectFileDialog::SELECT_FOLDER,
           l10n_util::GetStringUTF16(IDS_EXTENSION_LOAD_FROM_DIRECTORY),
           ui::SelectFileDialog::FileTypeInfo(),
           0 /* file_type_index */)) {
    return RespondNow(Error(kCouldNotShowSelectFileDialogError));
  }

  fail_quietly_ = params->options &&
                  params->options->fail_quietly &&
                  *params->options->fail_quietly;

  AddRef();  // Balanced in FileSelected / FileSelectionCanceled.
  return RespondLater();
}

void DeveloperPrivateLoadUnpackedFunction::FileSelected(
    const base::FilePath& path) {
  scoped_refptr<UnpackedInstaller> installer(
      UnpackedInstaller::Create(GetExtensionService(browser_context())));
  installer->set_be_noisy_on_failure(!fail_quietly_);
  installer->set_completion_callback(
      base::Bind(&DeveloperPrivateLoadUnpackedFunction::OnLoadComplete, this));
  installer->Load(path);

  DeveloperPrivateAPI::Get(browser_context())->SetLastUnpackedDirectory(path);

  Release();  // Balanced in Run().
}

void DeveloperPrivateLoadUnpackedFunction::FileSelectionCanceled() {
  // This isn't really an error, but we should keep it like this for
  // backward compatability.
  Respond(Error(kFileSelectionCanceled));
  Release();  // Balanced in Run().
}

void DeveloperPrivateLoadUnpackedFunction::OnLoadComplete(
    const Extension* extension,
    const base::FilePath& file_path,
    const std::string& error) {
  Respond(extension ? NoArguments() : Error(error));
}

bool DeveloperPrivateChooseEntryFunction::ShowPicker(
    ui::SelectFileDialog::Type picker_type,
    const base::string16& select_title,
    const ui::SelectFileDialog::FileTypeInfo& info,
    int file_type_index) {
  content::WebContents* web_contents = GetSenderWebContents();
  if (!web_contents)
    return false;

  // The entry picker will hold a reference to this function instance,
  // and subsequent sending of the function response) until the user has
  // selected a file or cancelled the picker. At that point, the picker will
  // delete itself.
  new EntryPicker(this,
                  web_contents,
                  picker_type,
                  DeveloperPrivateAPI::Get(browser_context())->
                      GetLastUnpackedDirectory(),
                  select_title,
                  info,
                  file_type_index);
  return true;
}

DeveloperPrivateChooseEntryFunction::~DeveloperPrivateChooseEntryFunction() {}

void DeveloperPrivatePackDirectoryFunction::OnPackSuccess(
    const base::FilePath& crx_file,
    const base::FilePath& pem_file) {
  developer::PackDirectoryResponse response;
  response.message = base::UTF16ToUTF8(
      PackExtensionJob::StandardSuccessMessage(crx_file, pem_file));
  response.status = developer::PACK_STATUS_SUCCESS;
  Respond(OneArgument(response.ToValue().release()));
  Release();  // Balanced in Run().
}

void DeveloperPrivatePackDirectoryFunction::OnPackFailure(
    const std::string& error,
    ExtensionCreator::ErrorType error_type) {
  developer::PackDirectoryResponse response;
  response.message = error;
  if (error_type == ExtensionCreator::kCRXExists) {
    response.item_path = item_path_str_;
    response.pem_path = key_path_str_;
    response.override_flags = ExtensionCreator::kOverwriteCRX;
    response.status = developer::PACK_STATUS_WARNING;
  } else {
    response.status = developer::PACK_STATUS_ERROR;
  }
  Respond(OneArgument(response.ToValue().release()));
  Release();  // Balanced in Run().
}

ExtensionFunction::ResponseAction DeveloperPrivatePackDirectoryFunction::Run() {
  scoped_ptr<PackDirectory::Params> params(
      PackDirectory::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  int flags = params->flags ? *params->flags : 0;
  item_path_str_ = params->path;
  if (params->private_key_path)
    key_path_str_ = *params->private_key_path;

  base::FilePath root_directory =
      base::FilePath::FromUTF8Unsafe(item_path_str_);
  base::FilePath key_file = base::FilePath::FromUTF8Unsafe(key_path_str_);

  developer::PackDirectoryResponse response;
  if (root_directory.empty()) {
    if (item_path_str_.empty())
      response.message = l10n_util::GetStringUTF8(
          IDS_EXTENSION_PACK_DIALOG_ERROR_ROOT_REQUIRED);
    else
      response.message = l10n_util::GetStringUTF8(
          IDS_EXTENSION_PACK_DIALOG_ERROR_ROOT_INVALID);

    response.status = developer::PACK_STATUS_ERROR;
    return RespondNow(OneArgument(response.ToValue().release()));
  }

  if (!key_path_str_.empty() && key_file.empty()) {
    response.message = l10n_util::GetStringUTF8(
        IDS_EXTENSION_PACK_DIALOG_ERROR_KEY_INVALID);
    response.status = developer::PACK_STATUS_ERROR;
    return RespondNow(OneArgument(response.ToValue().release()));
  }

  AddRef();  // Balanced in OnPackSuccess / OnPackFailure.

  // TODO(devlin): Why is PackExtensionJob ref-counted?
  pack_job_ = new PackExtensionJob(this, root_directory, key_file, flags);
  pack_job_->Start();
  return RespondLater();
}

DeveloperPrivatePackDirectoryFunction::DeveloperPrivatePackDirectoryFunction() {
}

DeveloperPrivatePackDirectoryFunction::
~DeveloperPrivatePackDirectoryFunction() {}

DeveloperPrivateLoadUnpackedFunction::~DeveloperPrivateLoadUnpackedFunction() {}

bool DeveloperPrivateLoadDirectoryFunction::RunAsync() {
  // TODO(grv) : add unittests.
  std::string directory_url_str;
  std::string filesystem_name;
  std::string filesystem_path;

  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &filesystem_name));
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(1, &filesystem_path));
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(2, &directory_url_str));

  context_ = content::BrowserContext::GetStoragePartition(
      GetProfile(), render_view_host()->GetSiteInstance())
                 ->GetFileSystemContext();

  // Directory url is non empty only for syncfilesystem.
  if (!directory_url_str.empty()) {
    storage::FileSystemURL directory_url =
        context_->CrackURL(GURL(directory_url_str));
    if (!directory_url.is_valid() ||
        directory_url.type() != storage::kFileSystemTypeSyncable) {
      SetError("DirectoryEntry of unsupported filesystem.");
      return false;
    }
    return LoadByFileSystemAPI(directory_url);
  } else {
    // Check if the DirecotryEntry is the instance of chrome filesystem.
    if (!app_file_handler_util::ValidateFileEntryAndGetPath(filesystem_name,
                                                            filesystem_path,
                                                            render_view_host_,
                                                            &project_base_path_,
                                                            &error_)) {
      SetError("DirectoryEntry of unsupported filesystem.");
      return false;
    }

    // Try to load using the FileSystem API backend, in case the filesystem
    // points to a non-native local directory.
    std::string filesystem_id;
    bool cracked =
        storage::CrackIsolatedFileSystemName(filesystem_name, &filesystem_id);
    CHECK(cracked);
    base::FilePath virtual_path =
        storage::IsolatedContext::GetInstance()
            ->CreateVirtualRootPath(filesystem_id)
            .Append(base::FilePath::FromUTF8Unsafe(filesystem_path));
    storage::FileSystemURL directory_url = context_->CreateCrackedFileSystemURL(
        extensions::Extension::GetBaseURLFromExtensionId(extension_id()),
        storage::kFileSystemTypeIsolated,
        virtual_path);

    if (directory_url.is_valid() &&
        directory_url.type() != storage::kFileSystemTypeNativeLocal &&
        directory_url.type() != storage::kFileSystemTypeRestrictedNativeLocal &&
        directory_url.type() != storage::kFileSystemTypeDragged) {
      return LoadByFileSystemAPI(directory_url);
    }

    Load();
  }

  return true;
}

bool DeveloperPrivateLoadDirectoryFunction::LoadByFileSystemAPI(
    const storage::FileSystemURL& directory_url) {
  std::string directory_url_str = directory_url.ToGURL().spec();

  size_t pos = 0;
  // Parse the project directory name from the project url. The project url is
  // expected to have project name as the suffix.
  if ((pos = directory_url_str.rfind("/")) == std::string::npos) {
    SetError("Invalid Directory entry.");
    return false;
  }

  std::string project_name;
  project_name = directory_url_str.substr(pos + 1);
  project_base_url_ = directory_url_str.substr(0, pos + 1);

  base::FilePath project_path(GetProfile()->GetPath());
  project_path = project_path.AppendASCII(kUnpackedAppsFolder);
  project_path = project_path.Append(
      base::FilePath::FromUTF8Unsafe(project_name));

  project_base_path_ = project_path;

  content::BrowserThread::PostTask(content::BrowserThread::FILE, FROM_HERE,
      base::Bind(&DeveloperPrivateLoadDirectoryFunction::
                     ClearExistingDirectoryContent,
                 this,
                 project_base_path_));
  return true;
}

void DeveloperPrivateLoadDirectoryFunction::Load() {
  ExtensionService* service = GetExtensionService(GetProfile());
  UnpackedInstaller::Create(service)->Load(project_base_path_);

  // TODO(grv) : The unpacked installer should fire an event when complete
  // and return the extension_id.
  SetResult(new base::StringValue("-1"));
  SendResponse(true);
}

void DeveloperPrivateLoadDirectoryFunction::ClearExistingDirectoryContent(
    const base::FilePath& project_path) {

  // Clear the project directory before copying new files.
  base::DeleteFile(project_path, true /*recursive*/);

  pending_copy_operations_count_ = 1;

  content::BrowserThread::PostTask(content::BrowserThread::IO, FROM_HERE,
      base::Bind(&DeveloperPrivateLoadDirectoryFunction::
                 ReadDirectoryByFileSystemAPI,
                 this, project_path, project_path.BaseName()));
}

void DeveloperPrivateLoadDirectoryFunction::ReadDirectoryByFileSystemAPI(
    const base::FilePath& project_path,
    const base::FilePath& destination_path) {
  GURL project_url = GURL(project_base_url_ + destination_path.AsUTF8Unsafe());
  storage::FileSystemURL url = context_->CrackURL(project_url);

  context_->operation_runner()->ReadDirectory(
      url, base::Bind(&DeveloperPrivateLoadDirectoryFunction::
                      ReadDirectoryByFileSystemAPICb,
                      this, project_path, destination_path));
}

void DeveloperPrivateLoadDirectoryFunction::ReadDirectoryByFileSystemAPICb(
    const base::FilePath& project_path,
    const base::FilePath& destination_path,
    base::File::Error status,
    const storage::FileSystemOperation::FileEntryList& file_list,
    bool has_more) {
  if (status != base::File::FILE_OK) {
    DLOG(ERROR) << "Error in copying files from sync filesystem.";
    return;
  }

  // We add 1 to the pending copy operations for both files and directories. We
  // release the directory copy operation once all the files under the directory
  // are added for copying. We do that to ensure that pendingCopyOperationsCount
  // does not become zero before all copy operations are finished.
  // In case the directory happens to be executing the last copy operation it
  // will call SendResponse to send the response to the API. The pending copy
  // operations of files are released by the CopyFile function.
  pending_copy_operations_count_ += file_list.size();

  for (size_t i = 0; i < file_list.size(); ++i) {
    if (file_list[i].is_directory) {
      ReadDirectoryByFileSystemAPI(project_path.Append(file_list[i].name),
                                   destination_path.Append(file_list[i].name));
      continue;
    }

    GURL project_url = GURL(project_base_url_ +
        destination_path.Append(file_list[i].name).AsUTF8Unsafe());
    storage::FileSystemURL url = context_->CrackURL(project_url);

    base::FilePath target_path = project_path;
    target_path = target_path.Append(file_list[i].name);

    context_->operation_runner()->CreateSnapshotFile(
        url,
        base::Bind(&DeveloperPrivateLoadDirectoryFunction::SnapshotFileCallback,
            this,
            target_path));
  }

  if (!has_more) {
    // Directory copy operation released here.
    pending_copy_operations_count_--;

    if (!pending_copy_operations_count_) {
      content::BrowserThread::PostTask(
          content::BrowserThread::UI, FROM_HERE,
          base::Bind(&DeveloperPrivateLoadDirectoryFunction::SendResponse,
                     this,
                     success_));
    }
  }
}

void DeveloperPrivateLoadDirectoryFunction::SnapshotFileCallback(
    const base::FilePath& target_path,
    base::File::Error result,
    const base::File::Info& file_info,
    const base::FilePath& src_path,
    const scoped_refptr<storage::ShareableFileReference>& file_ref) {
  if (result != base::File::FILE_OK) {
    SetError("Error in copying files from sync filesystem.");
    success_ = false;
    return;
  }

  content::BrowserThread::PostTask(content::BrowserThread::FILE, FROM_HERE,
      base::Bind(&DeveloperPrivateLoadDirectoryFunction::CopyFile,
                 this,
                 src_path,
                 target_path));
}

void DeveloperPrivateLoadDirectoryFunction::CopyFile(
    const base::FilePath& src_path,
    const base::FilePath& target_path) {
  if (!base::CreateDirectory(target_path.DirName())) {
    SetError("Error in copying files from sync filesystem.");
    success_ = false;
  }

  if (success_)
    base::CopyFile(src_path, target_path);

  CHECK(pending_copy_operations_count_ > 0);
  pending_copy_operations_count_--;

  if (!pending_copy_operations_count_) {
    content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
        base::Bind(&DeveloperPrivateLoadDirectoryFunction::Load,
                   this));
  }
}

DeveloperPrivateLoadDirectoryFunction::DeveloperPrivateLoadDirectoryFunction()
    : pending_copy_operations_count_(0), success_(true) {}

DeveloperPrivateLoadDirectoryFunction::~DeveloperPrivateLoadDirectoryFunction()
    {}

ExtensionFunction::ResponseAction DeveloperPrivateChoosePathFunction::Run() {
  scoped_ptr<developer::ChoosePath::Params> params(
      developer::ChoosePath::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  ui::SelectFileDialog::Type type = ui::SelectFileDialog::SELECT_FOLDER;
  ui::SelectFileDialog::FileTypeInfo info;

  if (params->select_type == developer::SELECT_TYPE_FILE)
    type = ui::SelectFileDialog::SELECT_OPEN_FILE;
  base::string16 select_title;

  int file_type_index = 0;
  if (params->file_type == developer::FILE_TYPE_LOAD) {
    select_title = l10n_util::GetStringUTF16(IDS_EXTENSION_LOAD_FROM_DIRECTORY);
  } else if (params->file_type == developer::FILE_TYPE_PEM) {
    select_title = l10n_util::GetStringUTF16(
        IDS_EXTENSION_PACK_DIALOG_SELECT_KEY);
    info.extensions.push_back(std::vector<base::FilePath::StringType>(
        1, FILE_PATH_LITERAL("pem")));
    info.extension_description_overrides.push_back(
        l10n_util::GetStringUTF16(
            IDS_EXTENSION_PACK_DIALOG_KEY_FILE_TYPE_DESCRIPTION));
    info.include_all_files = true;
    file_type_index = 1;
  } else {
    NOTREACHED();
  }

  if (!ShowPicker(
           type,
           select_title,
           info,
           file_type_index)) {
    return RespondNow(Error(kCouldNotShowSelectFileDialogError));
  }

  AddRef();  // Balanced by FileSelected / FileSelectionCanceled.
  return RespondLater();
}

void DeveloperPrivateChoosePathFunction::FileSelected(
    const base::FilePath& path) {
  Respond(OneArgument(new base::StringValue(path.LossyDisplayName())));
  Release();
}

void DeveloperPrivateChoosePathFunction::FileSelectionCanceled() {
  // This isn't really an error, but we should keep it like this for
  // backward compatability.
  Respond(Error(kFileSelectionCanceled));
  Release();
}

DeveloperPrivateChoosePathFunction::~DeveloperPrivateChoosePathFunction() {}

bool DeveloperPrivateIsProfileManagedFunction::RunSync() {
  SetResult(new base::FundamentalValue(GetProfile()->IsSupervised()));
  return true;
}

DeveloperPrivateIsProfileManagedFunction::
    ~DeveloperPrivateIsProfileManagedFunction() {
}

DeveloperPrivateRequestFileSourceFunction::
    DeveloperPrivateRequestFileSourceFunction() {}

DeveloperPrivateRequestFileSourceFunction::
    ~DeveloperPrivateRequestFileSourceFunction() {}

ExtensionFunction::ResponseAction
DeveloperPrivateRequestFileSourceFunction::Run() {
  params_ = developer::RequestFileSource::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_);

  const developer::RequestFileSourceProperties& properties =
      params_->properties;
  const Extension* extension = GetExtensionById(properties.extension_id);
  if (!extension)
    return RespondNow(Error(kNoSuchExtensionError));

  // Under no circumstances should we ever need to reference a file outside of
  // the extension's directory. If it tries to, abort.
  base::FilePath path_suffix =
      base::FilePath::FromUTF8Unsafe(properties.path_suffix);
  if (path_suffix.empty() || path_suffix.ReferencesParent())
    return RespondNow(Error(kInvalidPathError));

  if (properties.path_suffix == kManifestFile && !properties.manifest_key)
    return RespondNow(Error(kManifestKeyIsRequiredError));

  base::PostTaskAndReplyWithResult(
      content::BrowserThread::GetBlockingPool(),
      FROM_HERE,
      base::Bind(&ReadFileToString, extension->path().Append(path_suffix)),
      base::Bind(&DeveloperPrivateRequestFileSourceFunction::Finish, this));

  return RespondLater();
}

void DeveloperPrivateRequestFileSourceFunction::Finish(
    const std::string& file_contents) {
  const developer::RequestFileSourceProperties& properties =
      params_->properties;
  const Extension* extension = GetExtensionById(properties.extension_id);
  if (!extension) {
    Respond(Error(kNoSuchExtensionError));
    return;
  }

  developer::RequestFileSourceResponse response;
  base::FilePath path_suffix =
      base::FilePath::FromUTF8Unsafe(properties.path_suffix);
  base::FilePath path = extension->path().Append(path_suffix);
  response.title = base::StringPrintf("%s: %s",
                                      extension->name().c_str(),
                                      path.BaseName().AsUTF8Unsafe().c_str());
  response.message = properties.message;

  scoped_ptr<FileHighlighter> highlighter;
  if (properties.path_suffix == kManifestFile) {
    highlighter.reset(new ManifestHighlighter(
        file_contents,
        *properties.manifest_key,
        properties.manifest_specific ?
            *properties.manifest_specific : std::string()));
  } else {
    highlighter.reset(new SourceHighlighter(
        file_contents,
        properties.line_number ? *properties.line_number : 0));
  }

  response.before_highlight = highlighter->GetBeforeFeature();
  response.highlight = highlighter->GetFeature();
  response.after_highlight = highlighter->GetAfterFeature();

  Respond(OneArgument(response.ToValue().release()));
}

DeveloperPrivateOpenDevToolsFunction::DeveloperPrivateOpenDevToolsFunction() {}
DeveloperPrivateOpenDevToolsFunction::~DeveloperPrivateOpenDevToolsFunction() {}

ExtensionFunction::ResponseAction
DeveloperPrivateOpenDevToolsFunction::Run() {
  scoped_ptr<developer::OpenDevTools::Params> params(
      developer::OpenDevTools::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  const developer::OpenDevToolsProperties& properties = params->properties;

  content::RenderViewHost* rvh =
      content::RenderViewHost::FromID(properties.render_process_id,
                                      properties.render_view_id);
  content::WebContents* web_contents =
      rvh ? content::WebContents::FromRenderViewHost(rvh) : nullptr;
  // It's possible that the render view was closed since we last updated the
  // links. Handle this gracefully.
  if (!web_contents)
    return RespondNow(Error(kNoSuchRendererError));

  // If we include a url, we should inspect it specifically (and not just the
  // render view).
  if (properties.url) {
    // Line/column numbers are reported in display-friendly 1-based numbers,
    // but are inspected in zero-based numbers.
    // Default to the first line/column.
    DevToolsWindow::OpenDevToolsWindow(
        web_contents,
        DevToolsToggleAction::Reveal(
            base::UTF8ToUTF16(*properties.url),
            properties.line_number ? *properties.line_number - 1 : 0,
            properties.column_number ? *properties.column_number - 1 : 0));
  } else {
    DevToolsWindow::OpenDevToolsWindow(web_contents);
  }

  // Once we open the inspector, we focus on the appropriate tab...
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);

  // ... but some pages (popups and apps) don't have tabs, and some (background
  // pages) don't have an associated browser. For these, the inspector opens in
  // a new window, and our work is done.
  if (!browser || !browser->is_type_tabbed())
    return RespondNow(NoArguments());

  TabStripModel* tab_strip = browser->tab_strip_model();
  tab_strip->ActivateTabAt(tab_strip->GetIndexOfWebContents(web_contents),
                           false);  // Not through direct user gesture.
  return RespondNow(NoArguments());
}

}  // namespace api

}  // namespace extensions
