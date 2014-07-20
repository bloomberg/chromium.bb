// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/developer_private/developer_private_api.h"

#include "apps/app_load_service.h"
#include "apps/app_restore_service.h"
#include "apps/app_window.h"
#include "apps/app_window_registry.h"
#include "apps/saved_files_service.h"
#include "base/base64.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_enumerator.h"
#include "base/i18n/file_util_icu.h"
#include "base/lazy_instance.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/extensions/api/developer_private/entry_picker.h"
#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"
#include "chrome/browser/extensions/api/file_handlers/app_file_handler_util.h"
#include "chrome/browser/extensions/devtools_util.h"
#include "chrome/browser/extensions/extension_disabled_ui.h"
#include "chrome/browser/extensions/extension_error_reporter.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_ui_util.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/extensions/updater/extension_updater.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync_file_system/drive_backend_v1/drive_file_sync_service.h"
#include "chrome/browser/sync_file_system/syncable_file_system_util.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/browser/ui/webui/extensions/extension_error_ui_util.h"
#include "chrome/browser/ui/webui/extensions/extension_icon_source.h"
#include "chrome/common/extensions/api/developer_private.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/extensions/manifest_url_handler.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_error.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/management_policy.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_resource.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/install_warning.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "extensions/common/manifest_handlers/incognito_info.h"
#include "extensions/common/manifest_handlers/offline_enabled_info.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/switches.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "net/base/net_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/web_ui_util.h"
#include "webkit/browser/fileapi/external_mount_points.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/browser/fileapi/file_system_operation.h"
#include "webkit/browser/fileapi/file_system_operation_runner.h"
#include "webkit/browser/fileapi/isolated_context.h"
#include "webkit/common/blob/shareable_file_reference.h"

using apps::AppWindow;
using apps::AppWindowRegistry;
using content::RenderViewHost;

namespace extensions {

namespace developer_private = api::developer_private;

namespace {

const char kUnpackedAppsFolder[] = "apps_target";

ExtensionService* GetExtensionService(Profile* profile) {
  return ExtensionSystem::Get(profile)->extension_service();
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

std::string GetExtensionID(const RenderViewHost* render_view_host) {
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

}  // namespace

namespace AllowFileAccess = api::developer_private::AllowFileAccess;
namespace AllowIncognito = api::developer_private::AllowIncognito;
namespace ChoosePath = api::developer_private::ChoosePath;
namespace Enable = api::developer_private::Enable;
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
                 chrome::NOTIFICATION_EXTENSION_VIEW_REGISTERED,
                 content::Source<Profile>(profile_));
  registrar_.Add(this,
                 chrome::NOTIFICATION_EXTENSION_VIEW_UNREGISTERED,
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
    case chrome::NOTIFICATION_EXTENSION_VIEW_UNREGISTERED: {
      event_data.event_type = developer::EVENT_TYPE_VIEW_UNREGISTERED;
      event_data.item_id = GetExtensionID(
          content::Details<const RenderViewHost>(details).ptr());
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_VIEW_REGISTERED: {
      event_data.event_type = developer::EVENT_TYPE_VIEW_REGISTERED;
      event_data.item_id = GetExtensionID(
          content::Details<const RenderViewHost>(details).ptr());
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

bool DeveloperPrivateAutoUpdateFunction::RunSync() {
  ExtensionUpdater* updater = GetExtensionUpdater(GetProfile());
  if (updater)
    updater->CheckNow(ExtensionUpdater::CheckParams());
  SetResult(new base::FundamentalValue(true));
  return true;
}

DeveloperPrivateAutoUpdateFunction::~DeveloperPrivateAutoUpdateFunction() {}

scoped_ptr<developer::ItemInfo>
DeveloperPrivateGetItemsInfoFunction::CreateItemInfo(const Extension& item,
                                                     bool item_is_enabled) {
  scoped_ptr<developer::ItemInfo> info(new developer::ItemInfo());

  ExtensionSystem* system = ExtensionSystem::Get(GetProfile());
  ExtensionService* service = system->extension_service();
  ExtensionRegistry* registry = ExtensionRegistry::Get(GetProfile());

  info->id = item.id();
  info->name = item.name();
  info->enabled = service->IsExtensionEnabled(info->id);
  info->offline_enabled = OfflineEnabledInfo::IsOfflineEnabled(&item);
  info->version = item.VersionString();
  info->description = item.description();

  if (item.is_app()) {
    if (item.is_legacy_packaged_app())
      info->type = developer::ITEM_TYPE_LEGACY_PACKAGED_APP;
    else if (item.is_hosted_app())
      info->type = developer::ITEM_TYPE_HOSTED_APP;
    else if (item.is_platform_app())
      info->type = developer::ITEM_TYPE_PACKAGED_APP;
    else
      NOTREACHED();
  } else if (item.is_theme()) {
    info->type = developer::ITEM_TYPE_THEME;
  } else if (item.is_extension()) {
    info->type = developer::ITEM_TYPE_EXTENSION;
  } else {
    NOTREACHED();
  }

  if (Manifest::IsUnpackedLocation(item.location())) {
    info->path.reset(
        new std::string(base::UTF16ToUTF8(item.path().LossyDisplayName())));
    // If the ErrorConsole is enabled and the extension is unpacked, use the
    // more detailed errors from the ErrorConsole. Otherwise, use the install
    // warnings (using both is redundant).
    ErrorConsole* error_console = ErrorConsole::Get(GetProfile());
    if (error_console->IsEnabledForAppsDeveloperTools() &&
        item.location() == Manifest::UNPACKED) {
      const ErrorList& errors = error_console->GetErrorsForExtension(item.id());
      if (!errors.empty()) {
        for (ErrorList::const_iterator iter = errors.begin();
             iter != errors.end();
             ++iter) {
          switch ((*iter)->type()) {
            case ExtensionError::MANIFEST_ERROR:
              info->manifest_errors.push_back(
                  make_linked_ptr((*iter)->ToValue().release()));
              break;
            case ExtensionError::RUNTIME_ERROR: {
              const RuntimeError* error =
                  static_cast<const RuntimeError*>(*iter);
              scoped_ptr<base::DictionaryValue> value = error->ToValue();
              bool can_inspect = content::RenderViewHost::FromID(
                                     error->render_process_id(),
                                     error->render_view_id()) != NULL;
              value->SetBoolean("canInspect", can_inspect);
              info->runtime_errors.push_back(make_linked_ptr(value.release()));
              break;
            }
            case ExtensionError::NUM_ERROR_TYPES:
              NOTREACHED();
              break;
          }
        }
      }
    } else {
      for (std::vector<InstallWarning>::const_iterator it =
               item.install_warnings().begin();
           it != item.install_warnings().end();
           ++it) {
        scoped_ptr<developer::InstallWarning> warning(
            new developer::InstallWarning);
        warning->message = it->message;
        info->install_warnings.push_back(make_linked_ptr(warning.release()));
      }
    }
  }

  info->incognito_enabled = util::IsIncognitoEnabled(item.id(), GetProfile());
  info->wants_file_access = item.wants_file_access();
  info->allow_file_access = util::AllowFileAccess(item.id(), GetProfile());
  info->allow_reload = Manifest::IsUnpackedLocation(item.location());
  info->is_unpacked = Manifest::IsUnpackedLocation(item.location());
  info->terminated = registry->terminated_extensions().Contains(item.id());
  info->allow_incognito = item.can_be_incognito_enabled();

  info->homepage_url.reset(new std::string(
      ManifestURL::GetHomepageURL(&item).spec()));
  if (!ManifestURL::GetOptionsPage(&item).is_empty()) {
    info->options_url.reset(
        new std::string(ManifestURL::GetOptionsPage(&item).spec()));
  }

  if (!ManifestURL::GetUpdateURL(&item).is_empty()) {
    info->update_url.reset(
        new std::string(ManifestURL::GetUpdateURL(&item).spec()));
  }

  if (item.is_app()) {
    info->app_launch_url.reset(
        new std::string(AppLaunchInfo::GetFullLaunchURL(&item).spec()));
  }

  info->may_disable = system->management_policy()->
      UserMayModifySettings(&item, NULL);
  info->is_app = item.is_app();
  info->views = GetInspectablePagesForExtension(&item, item_is_enabled);

  return info.Pass();
}

void DeveloperPrivateGetItemsInfoFunction::GetIconsOnFileThread(
    ItemInfoList item_list,
    const std::map<std::string, ExtensionResource> idToIcon) {
  for (ItemInfoList::iterator iter = item_list.begin();
       iter != item_list.end(); ++iter) {
    developer_private::ItemInfo* info = iter->get();
    std::map<std::string, ExtensionResource>::const_iterator resource_ptr
        = idToIcon.find(info->id);
    if (resource_ptr != idToIcon.end()) {
      info->icon_url =
          ToDataURL(resource_ptr->second.GetFilePath(), info->type).spec();
    }
  }

  results_ = developer::GetItemsInfo::Results::Create(item_list);
  content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
      base::Bind(&DeveloperPrivateGetItemsInfoFunction::SendResponse,
                 this,
                 true));
}

void DeveloperPrivateGetItemsInfoFunction::
    GetInspectablePagesForExtensionProcess(
        const Extension* extension,
        const std::set<content::RenderViewHost*>& views,
        ItemInspectViewList* result) {
  bool has_generated_background_page =
      BackgroundInfo::HasGeneratedBackgroundPage(extension);
  for (std::set<content::RenderViewHost*>::const_iterator iter = views.begin();
       iter != views.end(); ++iter) {
    content::RenderViewHost* host = *iter;
    content::WebContents* web_contents =
        content::WebContents::FromRenderViewHost(host);
    ViewType host_type = GetViewType(web_contents);
    if (VIEW_TYPE_EXTENSION_POPUP == host_type ||
        VIEW_TYPE_EXTENSION_DIALOG == host_type)
      continue;

    content::RenderProcessHost* process = host->GetProcess();
    bool is_background_page =
        (web_contents->GetURL() == BackgroundInfo::GetBackgroundURL(extension));
    result->push_back(constructInspectView(
        web_contents->GetURL(),
        process->GetID(),
        host->GetRoutingID(),
        process->GetBrowserContext()->IsOffTheRecord(),
        is_background_page && has_generated_background_page));
  }
}

void DeveloperPrivateGetItemsInfoFunction::GetAppWindowPagesForExtensionProfile(
    const Extension* extension,
    ItemInspectViewList* result) {
  AppWindowRegistry* registry = AppWindowRegistry::Get(GetProfile());
  if (!registry) return;

  const AppWindowRegistry::AppWindowList windows =
      registry->GetAppWindowsForApp(extension->id());

  bool has_generated_background_page =
      BackgroundInfo::HasGeneratedBackgroundPage(extension);
  for (AppWindowRegistry::const_iterator it = windows.begin();
       it != windows.end();
       ++it) {
    content::WebContents* web_contents = (*it)->web_contents();
    RenderViewHost* host = web_contents->GetRenderViewHost();
    content::RenderProcessHost* process = host->GetProcess();
    bool is_background_page =
        (web_contents->GetURL() == BackgroundInfo::GetBackgroundURL(extension));
    result->push_back(constructInspectView(
        web_contents->GetURL(),
        process->GetID(),
        host->GetRoutingID(),
        process->GetBrowserContext()->IsOffTheRecord(),
        is_background_page && has_generated_background_page));
  }
}

linked_ptr<developer::ItemInspectView> DeveloperPrivateGetItemsInfoFunction::
    constructInspectView(
        const GURL& url,
        int render_process_id,
        int render_view_id,
        bool incognito,
        bool generated_background_page) {
  linked_ptr<developer::ItemInspectView> view(new developer::ItemInspectView());

  if (url.scheme() == kExtensionScheme) {
    // No leading slash.
    view->path = url.path().substr(1);
  } else {
    // For live pages, use the full URL.
    view->path = url.spec();
  }

  view->render_process_id = render_process_id;
  view->render_view_id = render_view_id;
  view->incognito = incognito;
  view->generated_background_page = generated_background_page;
  return view;
}

ItemInspectViewList DeveloperPrivateGetItemsInfoFunction::
    GetInspectablePagesForExtension(
        const Extension* extension,
        bool extension_is_enabled) {
  ItemInspectViewList result;
  // Get the extension process's active views.
  ProcessManager* process_manager =
      ExtensionSystem::Get(GetProfile())->process_manager();
  GetInspectablePagesForExtensionProcess(
      extension,
      process_manager->GetRenderViewHostsForExtension(extension->id()),
      &result);

  // Get app window views.
  GetAppWindowPagesForExtensionProfile(extension, &result);

  // Include a link to start the lazy background page, if applicable.
  if (BackgroundInfo::HasLazyBackgroundPage(extension) &&
      extension_is_enabled &&
      !process_manager->GetBackgroundHostForExtension(extension->id())) {
    result.push_back(constructInspectView(
        BackgroundInfo::GetBackgroundURL(extension),
        -1,
        -1,
        false,
        BackgroundInfo::HasGeneratedBackgroundPage(extension)));
  }

  ExtensionService* service = GetExtensionService(GetProfile());
  // Repeat for the incognito process, if applicable. Don't try to get
  // app windows for incognito process.
  if (service->profile()->HasOffTheRecordProfile() &&
      IncognitoInfo::IsSplitMode(extension)) {
    process_manager = ExtensionSystem::Get(
        service->profile()->GetOffTheRecordProfile())->process_manager();
    GetInspectablePagesForExtensionProcess(
        extension,
        process_manager->GetRenderViewHostsForExtension(extension->id()),
        &result);

    if (BackgroundInfo::HasLazyBackgroundPage(extension) &&
        extension_is_enabled &&
        !process_manager->GetBackgroundHostForExtension(extension->id())) {
    result.push_back(constructInspectView(
        BackgroundInfo::GetBackgroundURL(extension),
        -1,
        -1,
        false,
        BackgroundInfo::HasGeneratedBackgroundPage(extension)));
    }
  }

  return result;
}

bool DeveloperPrivateGetItemsInfoFunction::RunAsync() {
  scoped_ptr<developer::GetItemsInfo::Params> params(
      developer::GetItemsInfo::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get() != NULL);

  bool include_disabled = params->include_disabled;
  bool include_terminated = params->include_terminated;

  ExtensionSet items;

  ExtensionRegistry* registry = ExtensionRegistry::Get(GetProfile());

  items.InsertAll(registry->enabled_extensions());

  if (include_disabled) {
    items.InsertAll(registry->disabled_extensions());
  }

  if (include_terminated) {
    items.InsertAll(registry->terminated_extensions());
  }

  ExtensionService* service =
      ExtensionSystem::Get(GetProfile())->extension_service();
  std::map<std::string, ExtensionResource> id_to_icon;
  ItemInfoList item_list;

  for (ExtensionSet::const_iterator iter = items.begin(); iter != items.end();
       ++iter) {
    const Extension& item = *iter->get();

    ExtensionResource item_resource =
        IconsInfo::GetIconResource(&item,
                                   extension_misc::EXTENSION_ICON_MEDIUM,
                                   ExtensionIconSet::MATCH_BIGGER);
    id_to_icon[item.id()] = item_resource;

    // Don't show component extensions and invisible apps.
    if (ui_util::ShouldNotBeVisible(&item, GetProfile()))
      continue;

    item_list.push_back(make_linked_ptr<developer::ItemInfo>(
        CreateItemInfo(
            item, service->IsExtensionEnabled(item.id())).release()));
  }

  content::BrowserThread::PostTask(content::BrowserThread::FILE, FROM_HERE,
      base::Bind(&DeveloperPrivateGetItemsInfoFunction::GetIconsOnFileThread,
                 this,
                 item_list,
                 id_to_icon));

  return true;
}

DeveloperPrivateGetItemsInfoFunction::~DeveloperPrivateGetItemsInfoFunction() {}

bool DeveloperPrivateAllowFileAccessFunction::RunSync() {
  scoped_ptr<AllowFileAccess::Params> params(
      AllowFileAccess::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  EXTENSION_FUNCTION_VALIDATE(user_gesture_);

  ExtensionSystem* system = ExtensionSystem::Get(GetProfile());
  ManagementPolicy* management_policy = system->management_policy();
  const Extension* extension =
      ExtensionRegistry::Get(GetProfile())
          ->GetExtensionById(params->item_id, ExtensionRegistry::EVERYTHING);
  bool result = true;

  if (!extension) {
    result = false;
  } else if (!management_policy->UserMayModifySettings(extension, NULL)) {
    LOG(ERROR) << "Attempt to change allow file access of an extension that "
               << "non-usermanagable was made. Extension id : "
               << extension->id();
    result = false;
  } else {
    util::SetAllowFileAccess(extension->id(), GetProfile(), params->allow);
    result = true;
  }

  return result;
}

DeveloperPrivateAllowFileAccessFunction::
    ~DeveloperPrivateAllowFileAccessFunction() {}

bool DeveloperPrivateAllowIncognitoFunction::RunSync() {
  scoped_ptr<AllowIncognito::Params> params(
      AllowIncognito::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  const Extension* extension =
      ExtensionRegistry::Get(GetProfile())
          ->GetExtensionById(params->item_id, ExtensionRegistry::EVERYTHING);
  bool result = true;

  if (!extension)
    result = false;
  else
    util::SetIsIncognitoEnabled(extension->id(), GetProfile(), params->allow);

  return result;
}

DeveloperPrivateAllowIncognitoFunction::
    ~DeveloperPrivateAllowIncognitoFunction() {}

bool DeveloperPrivateReloadFunction::RunSync() {
  scoped_ptr<Reload::Params> params(Reload::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  CHECK(!params->item_id.empty());
  ExtensionService* service = GetExtensionService(GetProfile());
  service->ReloadExtension(params->item_id);
  return true;
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
  prompt_->ReviewPermissions(this, extension, retained_file_paths);
  return true;
}

DeveloperPrivateReloadFunction::~DeveloperPrivateReloadFunction() {}

// This is called when the user clicks "Revoke File Access."
void DeveloperPrivateShowPermissionsDialogFunction::InstallUIProceed() {
  Profile* profile = GetProfile();
  const Extension* extension = ExtensionRegistry::Get(
      profile)->GetExtensionById(extension_id_, ExtensionRegistry::EVERYTHING);
  apps::SavedFilesService::Get(profile)->ClearQueue(extension);
  if (apps::AppRestoreService::Get(profile)->IsAppRestorable(extension_id_))
    apps::AppLoadService::Get(profile)->RestartApplication(extension_id_);
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

DeveloperPrivateEnableFunction::DeveloperPrivateEnableFunction() {}

bool DeveloperPrivateEnableFunction::RunSync() {
  scoped_ptr<Enable::Params> params(Enable::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::string extension_id = params->item_id;

  const Extension* extension =
      ExtensionRegistry::Get(GetProfile())->GetExtensionById(
          extension_id, ExtensionRegistry::EVERYTHING);
  if (!extension) {
    LOG(ERROR) << "Did not find extension with id " << extension_id;
    return false;
  }
  ExtensionSystem* system = ExtensionSystem::Get(GetProfile());
  ManagementPolicy* policy = system->management_policy();
  bool enable = params->enable;
  if (!policy->UserMayModifySettings(extension, NULL) ||
      (!enable && policy->MustRemainEnabled(extension, NULL)) ||
      (enable && policy->MustRemainDisabled(extension, NULL, NULL))) {
    LOG(ERROR) << "Attempt to change enable state denied by management policy. "
               << "Extension id: " << extension_id.c_str();
    return false;
  }

  ExtensionService* service = system->extension_service();
  if (enable) {
    ExtensionPrefs* prefs = ExtensionPrefs::Get(GetProfile());
    if (prefs->DidExtensionEscalatePermissions(extension_id)) {
      AppWindowRegistry* registry = AppWindowRegistry::Get(GetProfile());
      CHECK(registry);
      AppWindow* app_window =
          registry->GetAppWindowForRenderViewHost(render_view_host());
      if (!app_window) {
        return false;
      }

      ShowExtensionDisabledDialog(
          service, app_window->web_contents(), extension);
    } else if ((prefs->GetDisableReasons(extension_id) &
                  Extension::DISABLE_UNSUPPORTED_REQUIREMENT) &&
               !requirements_checker_.get()) {
      // Recheck the requirements.
      scoped_refptr<const Extension> extension =
          service->GetExtensionById(extension_id, true);
      requirements_checker_.reset(new RequirementsChecker);
      // Released by OnRequirementsChecked.
      AddRef();
      requirements_checker_->Check(
          extension,
          base::Bind(&DeveloperPrivateEnableFunction::OnRequirementsChecked,
                     this, extension_id));
    } else {
      service->EnableExtension(extension_id);

      // Make sure any browser action contained within it is not hidden.
      ExtensionActionAPI::SetBrowserActionVisibility(
          prefs, extension->id(), true);
    }
  } else {
    service->DisableExtension(extension_id, Extension::DISABLE_USER_ACTION);
  }
  return true;
}

void DeveloperPrivateEnableFunction::OnRequirementsChecked(
    const std::string& extension_id,
    std::vector<std::string> requirements_errors) {
  if (requirements_errors.empty()) {
    GetExtensionService(GetProfile())->EnableExtension(extension_id);
  } else {
    ExtensionErrorReporter::GetInstance()->ReportError(
        base::UTF8ToUTF16(JoinString(requirements_errors, ' ')),
        true);  // Be noisy.
  }
  Release();
}

DeveloperPrivateEnableFunction::~DeveloperPrivateEnableFunction() {}

bool DeveloperPrivateInspectFunction::RunSync() {
  scoped_ptr<developer::Inspect::Params> params(
      developer::Inspect::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get() != NULL);
  const developer::InspectOptions& options = params->options;

  int render_process_id;
  base::StringToInt(options.render_process_id, &render_process_id);

  if (render_process_id == -1) {
    // This is a lazy background page. Identify if it is a normal
    // or incognito background page.
    const Extension* extension = ExtensionRegistry::Get(
        GetProfile())->enabled_extensions().GetByID(options.extension_id);
    DCHECK(extension);
    // Wakes up the background page and  opens the inspect window.
    devtools_util::InspectBackgroundPage(extension, GetProfile());
    return false;
  }

  int render_view_id;
  base::StringToInt(options.render_view_id, &render_view_id);
  content::RenderViewHost* host = content::RenderViewHost::FromID(
      render_process_id, render_view_id);

  if (!host) {
    // This can happen if the host has gone away since the page was displayed.
    return false;
  }

  DevToolsWindow::OpenDevToolsWindow(host);
  return true;
}

DeveloperPrivateInspectFunction::~DeveloperPrivateInspectFunction() {}

bool DeveloperPrivateLoadUnpackedFunction::RunAsync() {
  base::string16 select_title =
      l10n_util::GetStringUTF16(IDS_EXTENSION_LOAD_FROM_DIRECTORY);

  // Balanced in FileSelected / FileSelectionCanceled.
  AddRef();
  bool result = ShowPicker(
      ui::SelectFileDialog::SELECT_FOLDER,
      DeveloperPrivateAPI::Get(GetProfile())->GetLastUnpackedDirectory(),
      select_title,
      ui::SelectFileDialog::FileTypeInfo(),
      0);
  return result;
}

void DeveloperPrivateLoadUnpackedFunction::FileSelected(
    const base::FilePath& path) {
  ExtensionService* service = GetExtensionService(GetProfile());
  UnpackedInstaller::Create(service)->Load(path);
  DeveloperPrivateAPI::Get(GetProfile())->SetLastUnpackedDirectory(path);
  SendResponse(true);
  Release();
}

void DeveloperPrivateLoadUnpackedFunction::FileSelectionCanceled() {
  SendResponse(false);
  Release();
}

bool DeveloperPrivateChooseEntryFunction::ShowPicker(
    ui::SelectFileDialog::Type picker_type,
    const base::FilePath& last_directory,
    const base::string16& select_title,
    const ui::SelectFileDialog::FileTypeInfo& info,
    int file_type_index) {
  AppWindowRegistry* registry = AppWindowRegistry::Get(GetProfile());
  DCHECK(registry);
  AppWindow* app_window =
      registry->GetAppWindowForRenderViewHost(render_view_host());
  if (!app_window) {
    return false;
  }

  // The entry picker will hold a reference to this function instance,
  // and subsequent sending of the function response) until the user has
  // selected a file or cancelled the picker. At that point, the picker will
  // delete itself.
  new EntryPicker(this,
                  app_window->web_contents(),
                  picker_type,
                  last_directory,
                  select_title,
                  info,
                  file_type_index);
  return true;
}

bool DeveloperPrivateChooseEntryFunction::RunAsync() {
  return false;
}

DeveloperPrivateChooseEntryFunction::~DeveloperPrivateChooseEntryFunction() {}

void DeveloperPrivatePackDirectoryFunction::OnPackSuccess(
    const base::FilePath& crx_file,
    const base::FilePath& pem_file) {
  developer::PackDirectoryResponse response;
  response.message = base::UTF16ToUTF8(
      PackExtensionJob::StandardSuccessMessage(crx_file, pem_file));
  response.status = developer::PACK_STATUS_SUCCESS;
  results_ = developer::PackDirectory::Results::Create(response);
  SendResponse(true);
  Release();
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
  results_ = developer::PackDirectory::Results::Create(response);
  SendResponse(true);
  Release();
}

bool DeveloperPrivatePackDirectoryFunction::RunAsync() {
  scoped_ptr<PackDirectory::Params> params(
      PackDirectory::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  int flags = params->flags;
  item_path_str_ = params->path;
  key_path_str_ = params->private_key_path;

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
    results_ = developer::PackDirectory::Results::Create(response);
    SendResponse(true);
    return true;
  }

  if (!key_path_str_.empty() && key_file.empty()) {
    response.message = l10n_util::GetStringUTF8(
        IDS_EXTENSION_PACK_DIALOG_ERROR_KEY_INVALID);
    response.status = developer::PACK_STATUS_ERROR;
    results_ = developer::PackDirectory::Results::Create(response);
    SendResponse(true);
    return true;
  }

  // Balanced in OnPackSuccess / OnPackFailure.
  AddRef();

  pack_job_ = new PackExtensionJob(this, root_directory, key_file, flags);
  pack_job_->Start();
  return true;
}

DeveloperPrivatePackDirectoryFunction::DeveloperPrivatePackDirectoryFunction()
{}

DeveloperPrivatePackDirectoryFunction::~DeveloperPrivatePackDirectoryFunction()
{}

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
  if (directory_url_str != "") {
    fileapi::FileSystemURL directory_url =
        context_->CrackURL(GURL(directory_url_str));
    if (!directory_url.is_valid() ||
        directory_url.type() != fileapi::kFileSystemTypeSyncable) {
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
        fileapi::CrackIsolatedFileSystemName(filesystem_name, &filesystem_id);
    CHECK(cracked);
    base::FilePath virtual_path =
        fileapi::IsolatedContext::GetInstance()
            ->CreateVirtualRootPath(filesystem_id)
            .Append(base::FilePath::FromUTF8Unsafe(filesystem_path));
    fileapi::FileSystemURL directory_url = context_->CreateCrackedFileSystemURL(
        extensions::Extension::GetBaseURLFromExtensionId(extension_id()),
        fileapi::kFileSystemTypeIsolated,
        virtual_path);

    if (directory_url.is_valid() &&
        directory_url.type() != fileapi::kFileSystemTypeNativeLocal &&
        directory_url.type() != fileapi::kFileSystemTypeRestrictedNativeLocal &&
        directory_url.type() != fileapi::kFileSystemTypeDragged) {
      return LoadByFileSystemAPI(directory_url);
    }

    Load();
  }

  return true;
}

bool DeveloperPrivateLoadDirectoryFunction::LoadByFileSystemAPI(
    const fileapi::FileSystemURL& directory_url) {
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
  fileapi::FileSystemURL url = context_->CrackURL(project_url);

  context_->operation_runner()->ReadDirectory(
      url, base::Bind(&DeveloperPrivateLoadDirectoryFunction::
                      ReadDirectoryByFileSystemAPICb,
                      this, project_path, destination_path));
}

void DeveloperPrivateLoadDirectoryFunction::ReadDirectoryByFileSystemAPICb(
    const base::FilePath& project_path,
    const base::FilePath& destination_path,
    base::File::Error status,
    const fileapi::FileSystemOperation::FileEntryList& file_list,
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
    fileapi::FileSystemURL url = context_->CrackURL(project_url);

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
    const scoped_refptr<webkit_blob::ShareableFileReference>& file_ref) {
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

bool DeveloperPrivateChoosePathFunction::RunAsync() {
  scoped_ptr<developer::ChoosePath::Params> params(
      developer::ChoosePath::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get() != NULL);

  ui::SelectFileDialog::Type type = ui::SelectFileDialog::SELECT_FOLDER;
  ui::SelectFileDialog::FileTypeInfo info;
  if (params->select_type == developer::SELECT_TYPE_FILE) {
    type = ui::SelectFileDialog::SELECT_OPEN_FILE;
  }
  base::string16 select_title;

  int file_type_index = 0;
  if (params->file_type == developer::FILE_TYPE_LOAD) {
    select_title = l10n_util::GetStringUTF16(IDS_EXTENSION_LOAD_FROM_DIRECTORY);
  } else if (params->file_type == developer::FILE_TYPE_PEM) {
    select_title = l10n_util::GetStringUTF16(
        IDS_EXTENSION_PACK_DIALOG_SELECT_KEY);
    info.extensions.push_back(std::vector<base::FilePath::StringType>());
    info.extensions.front().push_back(FILE_PATH_LITERAL("pem"));
    info.extension_description_overrides.push_back(
        l10n_util::GetStringUTF16(
            IDS_EXTENSION_PACK_DIALOG_KEY_FILE_TYPE_DESCRIPTION));
    info.include_all_files = true;
    file_type_index = 1;
  } else {
    NOTREACHED();
  }

  // Balanced by FileSelected / FileSelectionCanceled.
  AddRef();
  bool result = ShowPicker(
      type,
      DeveloperPrivateAPI::Get(GetProfile())->GetLastUnpackedDirectory(),
      select_title,
      info,
      file_type_index);
  return result;
}

void DeveloperPrivateChoosePathFunction::FileSelected(
    const base::FilePath& path) {
  SetResult(new base::StringValue(base::UTF16ToUTF8(path.LossyDisplayName())));
  SendResponse(true);
  Release();
}

void DeveloperPrivateChoosePathFunction::FileSelectionCanceled() {
  SendResponse(false);
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

bool DeveloperPrivateRequestFileSourceFunction::RunAsync() {
  scoped_ptr<developer::RequestFileSource::Params> params(
      developer::RequestFileSource::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get() != NULL);

  base::DictionaryValue* dict = NULL;
  if (!params->dict->GetAsDictionary(&dict)) {
    NOTREACHED();
    return false;
  }

  AddRef();  // Balanced in LaunchCallback().
  error_ui_util::HandleRequestFileSource(
      dict,
      GetProfile(),
      base::Bind(&DeveloperPrivateRequestFileSourceFunction::LaunchCallback,
                 base::Unretained(this)));
  return true;
}

void DeveloperPrivateRequestFileSourceFunction::LaunchCallback(
    const base::DictionaryValue& results) {
  SetResult(results.DeepCopy());
  SendResponse(true);
  Release();  // Balanced in RunAsync().
}

DeveloperPrivateOpenDevToolsFunction::DeveloperPrivateOpenDevToolsFunction() {}
DeveloperPrivateOpenDevToolsFunction::~DeveloperPrivateOpenDevToolsFunction() {}

bool DeveloperPrivateOpenDevToolsFunction::RunAsync() {
  scoped_ptr<developer::OpenDevTools::Params> params(
      developer::OpenDevTools::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get() != NULL);

  base::DictionaryValue* dict = NULL;
  if (!params->dict->GetAsDictionary(&dict)) {
    NOTREACHED();
    return false;
  }

  error_ui_util::HandleOpenDevTools(dict);

  return true;
}

}  // namespace api

}  // namespace extensions
