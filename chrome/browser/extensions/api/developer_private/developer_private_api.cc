// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/developer_private/developer_private_api.h"

#include "apps/app_load_service.h"
#include "apps/app_restore_service.h"
#include "apps/saved_files_service.h"
#include "apps/shell_window.h"
#include "apps/shell_window_registry.h"
#include "base/base64.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_enumerator.h"
#include "base/i18n/file_util_icu.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/extensions/api/developer_private/developer_private_api_factory.h"
#include "chrome/browser/extensions/api/developer_private/entry_picker.h"
#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"
#include "chrome/browser/extensions/devtools_util.h"
#include "chrome/browser/extensions/extension_disabled_ui.h"
#include "chrome/browser/extensions/extension_error_reporter.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/management_policy.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/extensions/updater/extension_updater.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync_file_system/drive_backend_v1/drive_file_sync_service.h"
#include "chrome/browser/sync_file_system/syncable_file_system_util.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/browser/ui/webui/extensions/extension_icon_source.h"
#include "chrome/common/extensions/api/developer_private.h"
#include "chrome/common/extensions/background_info.h"
#include "chrome/common/extensions/incognito_handler.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/extensions/manifest_handlers/icons_handler.h"
#include "chrome/common/extensions/manifest_handlers/offline_enabled_info.h"
#include "chrome/common/extensions/manifest_url_handler.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_resource.h"
#include "extensions/common/install_warning.h"
#include "extensions/common/switches.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "net/base/net_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/webui/web_ui_util.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/browser/fileapi/file_system_operation.h"
#include "webkit/browser/fileapi/file_system_operation_runner.h"
#include "webkit/common/blob/shareable_file_reference.h"

using apps::ShellWindow;
using apps::ShellWindowRegistry;
using content::RenderViewHost;

namespace extensions {

namespace developer_private = api::developer_private;

namespace {

const base::FilePath::CharType kUnpackedAppsFolder[]
    = FILE_PATH_LITERAL("apps_target");

ExtensionUpdater* GetExtensionUpdater(Profile* profile) {
    return profile->GetExtensionService()->updater();
}

GURL GetImageURLFromData(std::string contents) {
  std::string contents_base64;
  if (!base::Base64Encode(contents, &contents_base64))
    return GURL();

  // TODO(dvh): make use of chrome::kDataScheme. Filed as crbug/297301.
  const char kDataURLPrefix[] = "data:image;base64,";
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

std::vector<base::FilePath> ListFolder(const base::FilePath path) {
  base::FileEnumerator files(path, false,
      base::FileEnumerator::DIRECTORIES | base::FileEnumerator::FILES);
  std::vector<base::FilePath> paths;

  for (base::FilePath current_path = files.Next(); !current_path.empty();
       current_path = files.Next()) {
    paths.push_back(current_path);
  }
  return paths;
}

bool ValidateFolderName(const base::FilePath::StringType& name) {
  base::FilePath::StringType name_sanitized(name);
  file_util::ReplaceIllegalCharactersInPath(&name_sanitized, '_');
  return name == name_sanitized;
}

const Extension* GetExtensionByPath(const ExtensionSet* extensions,
                                    const base::FilePath& path) {
  base::FilePath extension_path = base::MakeAbsoluteFilePath(path);
  for (ExtensionSet::const_iterator iter = extensions->begin();
       iter != extensions->end(); ++iter) {
    if ((*iter)->path() == extension_path)
      return iter->get();
  }
  return NULL;
}

std::string GetExtensionID(const RenderViewHost* render_view_host) {
  if (!render_view_host->GetSiteInstance())
    return std::string();

  return render_view_host->GetSiteInstance()->GetSiteURL().host();
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

DeveloperPrivateAPI* DeveloperPrivateAPI::Get(Profile* profile) {
  return DeveloperPrivateAPIFactory::GetForProfile(profile);
}

DeveloperPrivateAPI::DeveloperPrivateAPI(Profile* profile) : profile_(profile) {
  RegisterNotifications();
}

DeveloperPrivateEventRouter::DeveloperPrivateEventRouter(Profile* profile)
: profile_(profile) {
  int types[] = {
    chrome::NOTIFICATION_EXTENSION_INSTALLED,
    chrome::NOTIFICATION_EXTENSION_UNINSTALLED,
    chrome::NOTIFICATION_EXTENSION_LOADED,
    chrome::NOTIFICATION_EXTENSION_UNLOADED,
    chrome::NOTIFICATION_EXTENSION_VIEW_REGISTERED,
    chrome::NOTIFICATION_EXTENSION_VIEW_UNREGISTERED
  };

  CHECK(registrar_.IsEmpty());
  for (size_t i = 0; i < arraysize(types); ++i) {
    registrar_.Add(this,
                   types[i],
                   content::Source<Profile>(profile_));
  }
}


DeveloperPrivateEventRouter::~DeveloperPrivateEventRouter() {}

void DeveloperPrivateEventRouter::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  const char* event_name = NULL;
  Profile* profile = content::Source<Profile>(source).ptr();
  CHECK(profile);
  CHECK(profile_->IsSameProfile(profile));
  developer::EventData event_data;
  const Extension* extension = NULL;

  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_INSTALLED:
      event_data.event_type = developer::EVENT_TYPE_INSTALLED;
      extension =
          content::Details<const InstalledExtensionInfo>(details)->extension;
      break;
    case chrome::NOTIFICATION_EXTENSION_UNINSTALLED:
      event_data.event_type = developer::EVENT_TYPE_UNINSTALLED;
      extension = content::Details<const Extension>(details).ptr();
      break;
    case chrome::NOTIFICATION_EXTENSION_LOADED:
      event_data.event_type = developer::EVENT_TYPE_LOADED;
      extension = content::Details<const Extension>(details).ptr();
      break;
    case chrome::NOTIFICATION_EXTENSION_UNLOADED:
      event_data.event_type = developer::EVENT_TYPE_UNLOADED;
      extension =
          content::Details<const UnloadedExtensionInfo>(details)->extension;
      break;
    case chrome::NOTIFICATION_EXTENSION_VIEW_UNREGISTERED:
      event_data.event_type = developer::EVENT_TYPE_VIEW_UNREGISTERED;
      event_data.item_id = GetExtensionID(
          content::Details<const RenderViewHost>(details).ptr());
      break;
    case chrome::NOTIFICATION_EXTENSION_VIEW_REGISTERED:
      event_data.event_type = developer::EVENT_TYPE_VIEW_REGISTERED;
      event_data.item_id = GetExtensionID(
          content::Details<const RenderViewHost>(details).ptr());
      break;
    default:
      NOTREACHED();
      return;
  }

  if (extension)
    event_data.item_id = extension->id();

  scoped_ptr<ListValue> args(new ListValue());
  args->Append(event_data.ToValue().release());

  event_name = developer_private::OnItemStateChanged::kEventName;
  scoped_ptr<Event> event(new Event(event_name, args.Pass()));
  ExtensionSystem::Get(profile)->event_router()->BroadcastEvent(event.Pass());
}

void DeveloperPrivateAPI::SetLastUnpackedDirectory(const base::FilePath& path) {
  last_unpacked_directory_ = path;
}

void DeveloperPrivateAPI::RegisterNotifications() {
  ExtensionSystem::Get(profile_)->event_router()->RegisterObserver(
      this, developer_private::OnItemStateChanged::kEventName);
}

DeveloperPrivateAPI::~DeveloperPrivateAPI() {}

void DeveloperPrivateAPI::Shutdown() {}

void DeveloperPrivateAPI::OnListenerAdded(
    const EventListenerInfo& details) {
  if (!developer_private_event_router_)
    developer_private_event_router_.reset(
        new DeveloperPrivateEventRouter(profile_));
}

void DeveloperPrivateAPI::OnListenerRemoved(
    const EventListenerInfo& details) {
  if (!ExtensionSystem::Get(profile_)->event_router()->HasEventListener(
          developer_private::OnItemStateChanged::kEventName))
    developer_private_event_router_.reset(NULL);
}

namespace api {

bool DeveloperPrivateAutoUpdateFunction::RunImpl() {
  ExtensionUpdater* updater = GetExtensionUpdater(profile());
  if (updater)
    updater->CheckNow(ExtensionUpdater::CheckParams());
  SetResult(new base::FundamentalValue(true));
  return true;
}

DeveloperPrivateAutoUpdateFunction::~DeveloperPrivateAutoUpdateFunction() {}

scoped_ptr<developer::ItemInfo>
  DeveloperPrivateGetItemsInfoFunction::CreateItemInfo(
      const Extension& item,
      bool item_is_enabled) {
  scoped_ptr<developer::ItemInfo> info(new developer::ItemInfo());

  ExtensionSystem* system = ExtensionSystem::Get(profile());
  ExtensionService* service = profile()->GetExtensionService();

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
        new std::string(UTF16ToUTF8(item.path().LossyDisplayName())));
    for (std::vector<extensions::InstallWarning>::const_iterator it =
             item.install_warnings().begin();
         it != item.install_warnings().end(); ++it) {
      developer::InstallWarning* warning = new developer::InstallWarning();
      warning->message = it->message;
      info->install_warnings.push_back(make_linked_ptr(warning));
    }
  }

  info->incognito_enabled = service->IsIncognitoEnabled(item.id());
  info->wants_file_access = item.wants_file_access();
  info->allow_file_access = service->AllowFileAccess(&item);
  info->allow_reload = Manifest::IsUnpackedLocation(item.location());
  info->is_unpacked = Manifest::IsUnpackedLocation(item.location());
  info->terminated = service->terminated_extensions()->Contains(item.id());
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
    info->app_launch_url.reset(new std::string(
        extensions::AppLaunchInfo::GetFullLaunchURL(&item).spec()));
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

void DeveloperPrivateGetItemsInfoFunction::
    GetShellWindowPagesForExtensionProfile(
        const Extension* extension,
        ItemInspectViewList* result) {
  ShellWindowRegistry* registry = ShellWindowRegistry::Get(profile());
  if (!registry) return;

  const ShellWindowRegistry::ShellWindowList windows =
      registry->GetShellWindowsForApp(extension->id());

  bool has_generated_background_page =
      BackgroundInfo::HasGeneratedBackgroundPage(extension);
  for (ShellWindowRegistry::const_iterator it = windows.begin();
       it != windows.end(); ++it) {
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
  ExtensionProcessManager* process_manager =
      ExtensionSystem::Get(profile())->process_manager();
  GetInspectablePagesForExtensionProcess(
      extension,
      process_manager->GetRenderViewHostsForExtension(extension->id()),
      &result);

  // Get shell window views
  GetShellWindowPagesForExtensionProfile(extension, &result);

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

  ExtensionService* service = profile()->GetExtensionService();
  // Repeat for the incognito process, if applicable. Don't try to get
  // shell windows for incognito process.
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

bool DeveloperPrivateGetItemsInfoFunction::RunImpl() {
  scoped_ptr<developer::GetItemsInfo::Params> params(
      developer::GetItemsInfo::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get() != NULL);

  bool include_disabled = params->include_disabled;
  bool include_terminated = params->include_terminated;

  ExtensionSet items;

  ExtensionService* service = profile()->GetExtensionService();

  items.InsertAll(*service->extensions());

  if (include_disabled) {
    items.InsertAll(*service->disabled_extensions());
  }

  if (include_terminated) {
    items.InsertAll(*service->terminated_extensions());
  }

  std::map<std::string, ExtensionResource> id_to_icon;
  ItemInfoList item_list;

  for (ExtensionSet::const_iterator iter = items.begin();
       iter != items.end(); ++iter) {
    const Extension& item = *iter->get();

    ExtensionResource item_resource =
        IconsInfo::GetIconResource(&item,
                                   extension_misc::EXTENSION_ICON_MEDIUM,
                                   ExtensionIconSet::MATCH_BIGGER);
    id_to_icon[item.id()] = item_resource;

    // Don't show component extensions and invisible apps.
    if (item.ShouldNotBeVisible())
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

bool DeveloperPrivateAllowFileAccessFunction::RunImpl() {
  scoped_ptr<AllowFileAccess::Params> params(
      AllowFileAccess::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  EXTENSION_FUNCTION_VALIDATE(user_gesture_);

  ExtensionSystem* system = ExtensionSystem::Get(profile());
  ManagementPolicy* management_policy = system->management_policy();
  ExtensionService* service = profile()->GetExtensionService();
  const Extension* extension = service->GetInstalledExtension(params->item_id);
  bool result = true;

  if (!extension) {
    result = false;
  } else if (!management_policy->UserMayModifySettings(extension, NULL)) {
    LOG(ERROR) << "Attempt to change allow file access of an extension that "
               << "non-usermanagable was made. Extension id : "
               << extension->id();
    result = false;
  } else {
    service->SetAllowFileAccess(extension, params->allow);
    result = true;
  }

  return result;
}

DeveloperPrivateAllowFileAccessFunction::
    ~DeveloperPrivateAllowFileAccessFunction() {}

bool DeveloperPrivateAllowIncognitoFunction::RunImpl() {
  scoped_ptr<AllowIncognito::Params> params(
      AllowIncognito::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  ExtensionService* service = profile()->GetExtensionService();
  const Extension* extension = service->GetInstalledExtension(params->item_id);
  bool result = true;

  if (!extension)
    result = false;
  else
    service->SetIsIncognitoEnabled(extension->id(), params->allow);

  return result;
}

DeveloperPrivateAllowIncognitoFunction::
    ~DeveloperPrivateAllowIncognitoFunction() {}


bool DeveloperPrivateReloadFunction::RunImpl() {
  scoped_ptr<Reload::Params> params(Reload::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  ExtensionService* service = profile()->GetExtensionService();
  CHECK(!params->item_id.empty());
  service->ReloadExtension(params->item_id);
  return true;
}

bool DeveloperPrivateShowPermissionsDialogFunction::RunImpl() {
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &extension_id_));
  ExtensionService* service = profile()->GetExtensionService();
  CHECK(!extension_id_.empty());
  ShellWindowRegistry* registry = ShellWindowRegistry::Get(profile());
  DCHECK(registry);
  ShellWindow* shell_window = registry->GetShellWindowForRenderViewHost(
      render_view_host());
  prompt_.reset(new ExtensionInstallPrompt(shell_window->web_contents()));
  const Extension* extension = service->GetInstalledExtension(extension_id_);

  if (!extension)
    return false;

  // Released by InstallUIAbort or InstallUIProceed.
  AddRef();
  std::vector<base::FilePath> retained_file_paths;
  if (extension->HasAPIPermission(extensions::APIPermission::kFileSystem)) {
    std::vector<apps::SavedFileEntry> retained_file_entries =
        apps::SavedFilesService::Get(profile())->GetAllFileEntries(
            extension_id_);
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
  apps::SavedFilesService::Get(profile())->ClearQueue(
      profile()->GetExtensionService()->GetExtensionById(extension_id_, true));
  if (apps::AppRestoreService::Get(profile())->IsAppRestorable(extension_id_))
    apps::AppLoadService::Get(profile())->RestartApplication(extension_id_);
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

bool DeveloperPrivateEnableFunction::RunImpl() {
  scoped_ptr<Enable::Params> params(Enable::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::string extension_id = params->item_id;

  ExtensionSystem* system = ExtensionSystem::Get(profile());
  ManagementPolicy* management_policy = system->management_policy();
  ExtensionService* service = profile()->GetExtensionService();

  const Extension* extension = service->GetInstalledExtension(extension_id);
  if (!extension ||
      !management_policy->UserMayModifySettings(extension, NULL)) {
    LOG(ERROR) << "Attempt to enable an extension that is non-usermanagable "
               "was made. Extension id: " << extension_id.c_str();
    return false;
  }

  if (params->enable) {
    ExtensionPrefs* prefs = service->extension_prefs();
    if (prefs->DidExtensionEscalatePermissions(extension_id)) {
      ShellWindowRegistry* registry = ShellWindowRegistry::Get(profile());
      CHECK(registry);
      ShellWindow* shell_window = registry->GetShellWindowForRenderViewHost(
          render_view_host());
      if (!shell_window) {
        return false;
      }

      ShowExtensionDisabledDialog(
          service, shell_window->web_contents(), extension);
    } else if ((prefs->GetDisableReasons(extension_id) &
                  Extension::DISABLE_UNSUPPORTED_REQUIREMENT) &&
               !requirements_checker_.get()) {
      // Recheck the requirements.
      scoped_refptr<const Extension> extension =
          service->GetExtensionById(extension_id,
                                     true );// include_disabled
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
    std::string extension_id,
    std::vector<std::string> requirements_errors) {
  if (requirements_errors.empty()) {
    ExtensionService* service = profile()->GetExtensionService();
    service->EnableExtension(extension_id);
  } else {
    ExtensionErrorReporter::GetInstance()->ReportError(
        UTF8ToUTF16(JoinString(requirements_errors, ' ')),
        true /* be noisy */);
  }
  Release();
}

DeveloperPrivateEnableFunction::~DeveloperPrivateEnableFunction() {}

bool DeveloperPrivateInspectFunction::RunImpl() {
  scoped_ptr<developer::Inspect::Params> params(
      developer::Inspect::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get() != NULL);
  const developer::InspectOptions& options = params->options;

  int render_process_id;
  base::StringToInt(options.render_process_id, &render_process_id);

  if (render_process_id == -1) {
    // This is a lazy background page. Identify if it is a normal
    // or incognito background page.
    ExtensionService* service = profile()->GetExtensionService();
    if (options.incognito)
      service = ExtensionSystem::Get(
          service->profile()->GetOffTheRecordProfile())->extension_service();
    const Extension* extension = service->extensions()->GetByID(
        options.extension_id);
    DCHECK(extension);
    // Wakes up the background page and  opens the inspect window.
    devtools_util::InspectBackgroundPage(extension, profile());
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

bool DeveloperPrivateLoadUnpackedFunction::RunImpl() {
  string16 select_title =
      l10n_util::GetStringUTF16(IDS_EXTENSION_LOAD_FROM_DIRECTORY);

  // Balanced in FileSelected / FileSelectionCanceled.
  AddRef();
  bool result = ShowPicker(
      ui::SelectFileDialog::SELECT_FOLDER,
      DeveloperPrivateAPI::Get(profile())->GetLastUnpackedDirectory(),
      select_title,
      ui::SelectFileDialog::FileTypeInfo(),
      0);
  return result;
}

void DeveloperPrivateLoadUnpackedFunction::FileSelected(
    const base::FilePath& path) {
  ExtensionService* service = profile()->GetExtensionService();
  UnpackedInstaller::Create(service)->Load(path);
  DeveloperPrivateAPI::Get(profile())->SetLastUnpackedDirectory(path);
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
    const string16& select_title,
    const ui::SelectFileDialog::FileTypeInfo& info,
    int file_type_index) {
  ShellWindowRegistry* registry = ShellWindowRegistry::Get(profile());
  DCHECK(registry);
  ShellWindow* shell_window = registry->GetShellWindowForRenderViewHost(
      render_view_host());
  if (!shell_window) {
    return false;
  }

  // The entry picker will hold a reference to this function instance,
  // and subsequent sending of the function response) until the user has
  // selected a file or cancelled the picker. At that point, the picker will
  // delete itself.
  new EntryPicker(this, shell_window->web_contents(), picker_type,
  last_directory, select_title, info, file_type_index);
  return true;
}

bool DeveloperPrivateChooseEntryFunction::RunImpl() { return false; }

DeveloperPrivateChooseEntryFunction::~DeveloperPrivateChooseEntryFunction() {}

void DeveloperPrivatePackDirectoryFunction::OnPackSuccess(
    const base::FilePath& crx_file,
    const base::FilePath& pem_file) {
  developer::PackDirectoryResponse response;
  response.message =
      UTF16ToUTF8(PackExtensionJob::StandardSuccessMessage(crx_file, pem_file));
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

bool DeveloperPrivatePackDirectoryFunction::RunImpl() {
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

bool DeveloperPrivateExportSyncfsFolderToLocalfsFunction::RunImpl() {
  // TODO(grv) : add unittests.
  base::FilePath::StringType project_name;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &project_name));
  if (!ValidateFolderName(project_name)) {
    DLOG(INFO) << "Invalid project_name : [" << project_name << "]";
    return false;
  }

  context_ = content::BrowserContext::GetStoragePartition(profile(),
      render_view_host()->GetSiteInstance())->GetFileSystemContext();

  base::FilePath project_path(profile()->GetPath());
  project_path = project_path.Append(kUnpackedAppsFolder);
  project_path = project_path.Append(project_name);

  content::BrowserThread::PostTask(content::BrowserThread::FILE, FROM_HERE,
      base::Bind(&DeveloperPrivateExportSyncfsFolderToLocalfsFunction::
                     ClearPrexistingDirectoryContent,
                 this,
                 project_path));

  return true;
}

void DeveloperPrivateExportSyncfsFolderToLocalfsFunction::
    ClearPrexistingDirectoryContent(const base::FilePath& project_path) {

  // Clear the project directory before copying new files.
  base::DeleteFile(project_path, true/*recursive*/);

  pendingCopyOperationsCount_ = 1;

  content::BrowserThread::PostTask(content::BrowserThread::IO, FROM_HERE,
      base::Bind(&DeveloperPrivateExportSyncfsFolderToLocalfsFunction::
                 ReadSyncFileSystemDirectory,
                 this, project_path, project_path.BaseName()));
}

void DeveloperPrivateExportSyncfsFolderToLocalfsFunction::
    ReadSyncFileSystemDirectory(const base::FilePath& project_path,
                                const base::FilePath& destination_path) {
  std::string origin_url(
      Extension::GetBaseURLFromExtensionId(extension_id()).spec());
  fileapi::FileSystemURL url(sync_file_system::CreateSyncableFileSystemURL(
      GURL(origin_url),
      destination_path));

  context_->operation_runner()->ReadDirectory(
      url, base::Bind(&DeveloperPrivateExportSyncfsFolderToLocalfsFunction::
                      ReadSyncFileSystemDirectoryCb,
                      this, project_path, destination_path));
}

void DeveloperPrivateExportSyncfsFolderToLocalfsFunction::
    ReadSyncFileSystemDirectoryCb(
    const base::FilePath& project_path,
    const base::FilePath& destination_path,
    base::PlatformFileError status,
    const fileapi::FileSystemOperation::FileEntryList& file_list,
    bool has_more) {

  if (status != base::PLATFORM_FILE_OK) {
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
  pendingCopyOperationsCount_ += file_list.size();

  for (size_t i = 0; i < file_list.size(); ++i) {
    if (file_list[i].is_directory) {
      ReadSyncFileSystemDirectory(project_path.Append(file_list[i].name),
                                  destination_path.Append(file_list[i].name));
      continue;
    }

    std::string origin_url(
        Extension::GetBaseURLFromExtensionId(extension_id()).spec());
    fileapi::FileSystemURL url(sync_file_system::CreateSyncableFileSystemURL(
        GURL(origin_url),
        destination_path.Append(file_list[i].name)));
    base::FilePath target_path = project_path;
    target_path = target_path.Append(file_list[i].name);

    context_->operation_runner()->CreateSnapshotFile(
        url,
        base::Bind(
            &DeveloperPrivateExportSyncfsFolderToLocalfsFunction::
                SnapshotFileCallback,
            this,
            target_path));

  }

  // Directory copy operation released here.
  pendingCopyOperationsCount_--;

  if (!pendingCopyOperationsCount_) {
    content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
        base::Bind(&DeveloperPrivateExportSyncfsFolderToLocalfsFunction::
                       SendResponse,
                   this,
                   success_));
  }
}

void DeveloperPrivateExportSyncfsFolderToLocalfsFunction::SnapshotFileCallback(
    const base::FilePath& target_path,
    base::PlatformFileError result,
    const base::PlatformFileInfo& file_info,
    const base::FilePath& src_path,
    const scoped_refptr<webkit_blob::ShareableFileReference>& file_ref) {
  if (result != base::PLATFORM_FILE_OK) {
    SetError("Error in copying files from sync filesystem.");
    success_ = false;
    return;
  }

  content::BrowserThread::PostTask(content::BrowserThread::FILE, FROM_HERE,
      base::Bind(&DeveloperPrivateExportSyncfsFolderToLocalfsFunction::CopyFile,
                 this,
                 src_path,
                 target_path));
}

void DeveloperPrivateExportSyncfsFolderToLocalfsFunction::CopyFile(
    const base::FilePath& src_path,
    const base::FilePath& target_path) {
  if (!file_util::CreateDirectory(target_path.DirName())) {
    SetError("Error in copying files from sync filesystem.");
    success_ = false;
  }

  if (success_)
    base::CopyFile(src_path, target_path);

  CHECK(pendingCopyOperationsCount_ > 0);
  pendingCopyOperationsCount_--;

  if (!pendingCopyOperationsCount_) {
    content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
        base::Bind(&DeveloperPrivateExportSyncfsFolderToLocalfsFunction::
                       SendResponse,
                   this,
                   success_));
  }
}

DeveloperPrivateExportSyncfsFolderToLocalfsFunction::
    DeveloperPrivateExportSyncfsFolderToLocalfsFunction()
    : pendingCopyOperationsCount_(0), success_(true) {}

DeveloperPrivateExportSyncfsFolderToLocalfsFunction::
    ~DeveloperPrivateExportSyncfsFolderToLocalfsFunction() {}

bool DeveloperPrivateLoadProjectFunction::RunImpl() {
  // TODO(grv) : add unit tests.
  base::FilePath::StringType project_name;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &project_name));
  if (!ValidateFolderName(project_name)) {
    DLOG(INFO) << "Invalid project_name : [" << project_name << "]";
    return false;
  }

  base::FilePath path(profile()->GetPath());
  path = path.Append(kUnpackedAppsFolder);
  // TODO(grv) : Sanitize / check project_name.
  path = path.Append(project_name);
  ExtensionService* service = profile()->GetExtensionService();
  UnpackedInstaller::Create(service)->Load(path);

  const ExtensionSet* extensions = service->extensions();
  // Released by GetUnpackedExtension.
  AddRef();
  content::BrowserThread::PostTask(content::BrowserThread::FILE, FROM_HERE,
      base::Bind(&DeveloperPrivateLoadProjectFunction::GetUnpackedExtension,
                 this, path, extensions));
  return true;
}

void DeveloperPrivateLoadProjectFunction::GetUnpackedExtension(
    const base::FilePath& path,
    const ExtensionSet* extensions) {
  const Extension* extension = GetExtensionByPath(extensions, path);
  bool success = true;
  if (extension) {
    SetResult(new base::StringValue(extension->id()));
  } else {
    SetError("unable to load the project");
    success = false;
  }
  content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
      base::Bind(&DeveloperPrivateLoadProjectFunction::SendResponse,
                 this,
                 success));
  Release();
}

DeveloperPrivateLoadProjectFunction::DeveloperPrivateLoadProjectFunction() {}

DeveloperPrivateLoadProjectFunction::~DeveloperPrivateLoadProjectFunction() {}

bool DeveloperPrivateChoosePathFunction::RunImpl() {

  scoped_ptr<developer::ChoosePath::Params> params(
      developer::ChoosePath::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get() != NULL);

  ui::SelectFileDialog::Type type = ui::SelectFileDialog::SELECT_FOLDER;
  ui::SelectFileDialog::FileTypeInfo info;
  if (params->select_type == developer::SELECT_TYPE_FILE) {
    type = ui::SelectFileDialog::SELECT_OPEN_FILE;
  }
  string16 select_title;

  int file_type_index = 0;
  if (params->file_type == developer::FILE_TYPE_LOAD)
    select_title = l10n_util::GetStringUTF16(IDS_EXTENSION_LOAD_FROM_DIRECTORY);
  else if (params->file_type== developer::FILE_TYPE_PEM) {
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
      DeveloperPrivateAPI::Get(profile())->GetLastUnpackedDirectory(),
      select_title,
      info,
      file_type_index);
  return result;
}

void DeveloperPrivateChoosePathFunction::FileSelected(
    const base::FilePath& path) {
  SetResult(new base::StringValue(UTF16ToUTF8(path.LossyDisplayName())));
  SendResponse(true);
  Release();
}

void DeveloperPrivateChoosePathFunction::FileSelectionCanceled() {
  SendResponse(false);
  Release();
}

DeveloperPrivateChoosePathFunction::~DeveloperPrivateChoosePathFunction() {}

bool DeveloperPrivateGetStringsFunction::RunImpl() {
  base::DictionaryValue* dict = new base::DictionaryValue();
  SetResult(dict);

  webui::SetFontAndTextDirection(dict);

  #define   SET_STRING(id, idr) \
    dict->SetString(id, l10n_util::GetStringUTF16(idr))
  SET_STRING("extensionSettings", IDS_MANAGE_EXTENSIONS_SETTING_WINDOWS_TITLE);

  SET_STRING("appsDevtoolSearch", IDS_APPS_DEVTOOL_SEARCH);
  SET_STRING("appsDevtoolApps", IDS_APPS_DEVTOOL_APPS_INSTALLED);
  SET_STRING("appsDevtoolExtensions", IDS_APPS_DEVTOOL_EXTENSIONS_INSTALLED);
  SET_STRING("appsDevtoolNoExtensions", IDS_EXTENSIONS_NONE_INSTALLED);
  SET_STRING("appsDevtoolUnpacked", IDS_APPS_DEVTOOL_UNPACKED_INSTALLED);
  SET_STRING("appsDevtoolInstalled", IDS_APPS_DEVTOOL_INSTALLED);
  SET_STRING("appsDevtoolNoPackedApps", IDS_APPS_DEVTOOL_NO_PACKED_APPS);
  SET_STRING("appsDevtoolNoUnpackedApps", IDS_APPS_DEVTOOL_NO_UNPACKED_APPS);
  SET_STRING("appsDevtoolNoPackedExtensions",
      IDS_APPS_DEVTOOL_NO_PACKED_EXTENSIONS);
  SET_STRING("appsDevtoolNoUnpackedExtensions",
      IDS_APPS_DEVTOOL_NO_UNPACKED_EXTENSIONS);
  SET_STRING("appsDevtoolUpdating", IDS_APPS_DEVTOOL_UPDATING);
  SET_STRING("extensionSettingsGetMoreExtensions", IDS_GET_MORE_EXTENSIONS);
  SET_STRING("extensionSettingsExtensionId", IDS_EXTENSIONS_ID);
  SET_STRING("extensionSettingsExtensionPath", IDS_EXTENSIONS_PATH);
  SET_STRING("extensionSettingsInspectViews", IDS_EXTENSIONS_INSPECT_VIEWS);
  SET_STRING("extensionSettingsInstallWarnings",
             IDS_EXTENSIONS_INSTALL_WARNINGS);
  SET_STRING("viewIncognito", IDS_EXTENSIONS_VIEW_INCOGNITO);
  SET_STRING("viewInactive", IDS_EXTENSIONS_VIEW_INACTIVE);
  SET_STRING("backgroundPage", IDS_EXTENSIONS_BACKGROUND_PAGE);
  SET_STRING("extensionSettingsEnable", IDS_EXTENSIONS_ENABLE);
  SET_STRING("extensionSettingsEnabled", IDS_EXTENSIONS_ENABLED);
  SET_STRING("extensionSettingsRemove", IDS_EXTENSIONS_REMOVE);
  SET_STRING("extensionSettingsEnableIncognito",
             IDS_EXTENSIONS_ENABLE_INCOGNITO);
  SET_STRING("extensionSettingsAllowFileAccess",
             IDS_EXTENSIONS_ALLOW_FILE_ACCESS);
  SET_STRING("extensionSettingsReloadTerminated",
             IDS_EXTENSIONS_RELOAD_TERMINATED);
  SET_STRING("extensionSettingsReloadUnpacked",
             IDS_APPS_DEV_TOOLS_RELOAD_UNPACKED);
  SET_STRING("extensionSettingsLaunch", IDS_EXTENSIONS_LAUNCH);
  SET_STRING("extensionSettingsOptions", IDS_EXTENSIONS_OPTIONS_LINK);
  SET_STRING("extensionSettingsPermissions", IDS_EXTENSIONS_PERMISSIONS_LINK);
  SET_STRING("extensionSettingsVisitWebsite", IDS_EXTENSIONS_VISIT_WEBSITE);
  SET_STRING("extensionSettingsVisitWebStore", IDS_EXTENSIONS_VISIT_WEBSTORE);
  SET_STRING("extensionSettingsPolicyControlled",
             IDS_EXTENSIONS_POLICY_CONTROLLED);
  SET_STRING("extensionSettingsManagedMode",
             IDS_EXTENSIONS_LOCKED_MANAGED_USER);
  SET_STRING("extensionSettingsShowButton", IDS_EXTENSIONS_SHOW_BUTTON);
  SET_STRING("appsDevtoolLoadUnpackedButton",
             IDS_APPS_DEVTOOL_LOAD_UNPACKED_BUTTON);
  SET_STRING("appsDevtoolPackButton", IDS_APPS_DEVTOOL_PACK_BUTTON);
  SET_STRING("extensionSettingsCommandsLink",
             IDS_EXTENSIONS_COMMANDS_CONFIGURE);
  SET_STRING("appsDevtoolUpdateButton", IDS_APPS_DEVTOOL_UPDATE_BUTTON);
  SET_STRING("extensionSettingsWarningsTitle", IDS_EXTENSION_WARNINGS_TITLE);
  SET_STRING("extensionSettingsShowDetails", IDS_EXTENSIONS_SHOW_DETAILS);
  SET_STRING("extensionSettingsHideDetails", IDS_EXTENSIONS_HIDE_DETAILS);
  SET_STRING("extensionUninstall", IDS_EXTENSIONS_UNINSTALL);
  SET_STRING("extensionsPermissionsHeading",
             IDS_EXTENSIONS_PERMISSIONS_HEADING);
  SET_STRING("extensionsPermissionsClose", IDS_EXTENSIONS_PERMISSIONS_CLOSE);
  SET_STRING("extensionDisabled", IDS_EXTENSIONS_DISABLED);
  SET_STRING("extensionSettingsShowLogsButton", IDS_EXTENSIONS_SHOW_LOGS);
  SET_STRING("extensionSettingsMoreDetailsButton", IDS_EXTENSIONS_MORE_DETAILS);
  SET_STRING("extensionSettingsVersion", IDS_EXTENSIONS_VERSION);
  SET_STRING("extensionSettingsDelete", IDS_EXTENSIONS_ADT_DELETE);
  SET_STRING("extensionSettingsPack", IDS_EXTENSIONS_PACK);

// Pack Extension strings
  SET_STRING("packExtensionOverlay", IDS_EXTENSION_PACK_DIALOG_TITLE);
  SET_STRING("packExtensionHeading", IDS_EXTENSION_ADT_PACK_DIALOG_HEADING);
  SET_STRING("packButton", IDS_EXTENSION_ADT_PACK_BUTTON);
  SET_STRING("ok", IDS_OK);
  SET_STRING("cancel", IDS_CANCEL);
  SET_STRING("packExtensionRootDir",
     IDS_EXTENSION_PACK_DIALOG_ROOT_DIRECTORY_LABEL);
  SET_STRING("packExtensionPrivateKey",
     IDS_EXTENSION_PACK_DIALOG_PRIVATE_KEY_LABEL);
  SET_STRING("packExtensionBrowseButton", IDS_EXTENSION_PACK_DIALOG_BROWSE);
  SET_STRING("packExtensionProceedAnyway", IDS_EXTENSION_PROCEED_ANYWAY);
  SET_STRING("packExtensionWarningTitle", IDS_EXTENSION_PACK_WARNING_TITLE);
  SET_STRING("packExtensionErrorTitle", IDS_EXTENSION_PACK_ERROR_TITLE);
  SET_STRING("packAppOverlay", IDS_EXTENSION_PACK_APP_DIALOG_TITLE);
  SET_STRING("packAppHeading", IDS_EXTENSION_ADT_PACK_APP_DIALOG_HEADING);

// Delete confirmation dialog.
  SET_STRING("deleteConfirmationDeleteButton",
      IDS_APPS_DEVTOOL_DELETE_CONFIRMATION_BUTTON);
  SET_STRING("deleteConfirmationTitle",
      IDS_APPS_DEVTOOL_DELETE_CONFIRMATION_TITLE);
  SET_STRING("deleteConfirmationMessageApp",
      IDS_APPS_DEVTOOL_DELETE_CONFIRMATION_MESSAGE_APP);
  SET_STRING("deleteConfirmationMessageExtension",
      IDS_APPS_DEVTOOL_DELETE_CONFIRMATION_MESSAGE_EXTENSION);

// Dialog when profile is managed.
  SET_STRING("managedProfileDialogCloseButton",
      IDS_APPS_DEVTOOL_MANAGED_PROFILE_DIALOG_CLOSE_BUTTON);
  SET_STRING("managedProfileDialogTitle",
      IDS_APPS_DEVTOOL_MANAGED_PROFILE_DIALOG_TITLE);
  SET_STRING("managedProfileDialogDescription",
      IDS_APPS_DEVTOOL_MANAGED_PROFILE_DIALOG_DESCRIPTION);

  #undef   SET_STRING
  return true;
}

DeveloperPrivateGetStringsFunction::~DeveloperPrivateGetStringsFunction() {}

bool DeveloperPrivateIsProfileManagedFunction::RunImpl() {
  SetResult(new base::FundamentalValue(profile_->IsManaged()));
  return true;
}

DeveloperPrivateIsProfileManagedFunction::
    ~DeveloperPrivateIsProfileManagedFunction() {
}

} // namespace api

} // namespace extensions
