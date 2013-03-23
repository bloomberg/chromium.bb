// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/developer_private/developer_private_api.h"

#include "base/base64.h"
#include "base/file_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/extensions/api/developer_private/developer_private_api_factory.h"
#include "chrome/browser/extensions/api/developer_private/entry_picker.h"
#include "chrome/browser/extensions/extension_disabled_ui.h"
#include "chrome/browser/extensions/extension_error_reporter.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/management_policy.h"
#include "chrome/browser/extensions/shell_window_registry.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/extensions/updater/extension_updater.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/browser/ui/extensions/shell_window.h"
#include "chrome/browser/ui/webui/extensions/extension_icon_source.h"
#include "chrome/browser/view_type_utils.h"
#include "chrome/common/extensions/api/developer_private.h"
#include "chrome/common/extensions/api/icons/icons_handler.h"
#include "chrome/common/extensions/background_info.h"
#include "chrome/common/extensions/extension_icon_set.h"
#include "chrome/common/extensions/extension_resource.h"
#include "chrome/common/extensions/manifest_url_handler.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/constants.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "net/base/net_util.h"
#include "ui/base/l10n/l10n_util.h"

using content::RenderViewHost;
using extensions::DeveloperPrivateAPI;
using extensions::Extension;
using extensions::ExtensionSystem;
using extensions::ManagementPolicy;

namespace {

extensions::ExtensionUpdater* GetExtensionUpdater(Profile* profile) {
    return profile->GetExtensionService()->updater();
}

GURL ToDataURL(const base::FilePath& path) {
  std::string contents;
  if (!file_util::ReadFileToString(path, &contents))
    return GURL();

  std::string contents_base64;
  if (!base::Base64Encode(contents, &contents_base64))
    return GURL();

  const char kDataURLPrefix[] = "data:image;base64,";
  return GURL(kDataURLPrefix + contents_base64);
}


}  // namespace

namespace extensions {

namespace AllowFileAccess = api::developer_private::AllowFileAccess;
namespace AllowIncognito = api::developer_private::AllowIncognito;
namespace ChoosePath = api::developer_private::ChoosePath;
namespace Enable = api::developer_private::Enable;
namespace GetItemsInfo = api::developer_private::GetItemsInfo;
namespace Inspect = api::developer_private::Inspect;
namespace PackDirectory = api::developer_private::PackDirectory;
namespace Reload = api::developer_private::Reload;
namespace Restart = api::developer_private::Restart;

DeveloperPrivateAPI* DeveloperPrivateAPI::Get(Profile* profile) {
  return DeveloperPrivateAPIFactory::GetForProfile(profile);
}

DeveloperPrivateAPI::DeveloperPrivateAPI(Profile* profile) {

      RegisterNotifications();
}


void DeveloperPrivateAPI::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    // TODO(grv): Listen to other notifications and expose them
    // as events in API.
    case chrome::NOTIFICATION_BACKGROUND_CONTENTS_DELETED:
      break;
    default:
      NOTREACHED();
  }
}

void DeveloperPrivateAPI::SetLastUnpackedDirectory(const base::FilePath& path) {
  last_unpacked_directory_ = path;
}

void DeveloperPrivateAPI::RegisterNotifications() {
  registrar_.Add(this,
                 chrome::NOTIFICATION_BACKGROUND_CONTENTS_DELETED,
                 content::NotificationService::AllBrowserContextsAndSources());
}

DeveloperPrivateAPI::~DeveloperPrivateAPI() {}

void DeveloperPrivateAPI::Shutdown() {}

namespace api {

bool DeveloperPrivateAutoUpdateFunction::RunImpl() {
  extensions::ExtensionUpdater* updater = GetExtensionUpdater(profile());
  if (updater)
    updater->CheckNow(extensions::ExtensionUpdater::CheckParams());
  SetResult(Value::CreateBooleanValue(true));
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
  info->offline_enabled = item.offline_enabled();
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

  if (extensions::Manifest::IsUnpackedLocation(item.location())) {
    info->path.reset(
        new std::string(UTF16ToUTF8(item.path().LossyDisplayName())));
  }

  info->incognito_enabled = service->IsIncognitoEnabled(item.id());
  info->wants_file_access = item.wants_file_access();
  info->allow_file_access = service->AllowFileAccess(&item);
  info->allow_reload =
      extensions::Manifest::IsUnpackedLocation(item.location());
  info->is_unpacked = extensions::Manifest::IsUnpackedLocation(item.location());
  info->terminated = service->terminated_extensions()->Contains(item.id());
  info->allow_incognito = item.can_be_incognito_enabled();

  info->homepage_url.reset(new std::string(
      extensions::ManifestURL::GetHomepageURL(&item).spec()));
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
        item.GetFullLaunchURL().spec()));
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
      info->icon = ToDataURL(resource_ptr->second.GetFilePath()).spec();
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
        const std::set<content::RenderViewHost*>& views,
        ItemInspectViewList* result) {
  for (std::set<content::RenderViewHost*>::const_iterator iter = views.begin();
       iter != views.end(); ++iter) {
    content::RenderViewHost* host = *iter;
    content::WebContents* web_contents =
        content::WebContents::FromRenderViewHost(host);
    chrome::ViewType host_type = chrome::GetViewType(web_contents);
    if (chrome::VIEW_TYPE_EXTENSION_POPUP == host_type ||
        chrome::VIEW_TYPE_EXTENSION_DIALOG == host_type)
      continue;

    content::RenderProcessHost* process = host->GetProcess();
    result->push_back(
        constructInspectView(web_contents->GetURL(),
                             process->GetID(),
                             host->GetRoutingID(),
                             process->GetBrowserContext()->IsOffTheRecord()));
  }
}

void DeveloperPrivateGetItemsInfoFunction::
    GetShellWindowPagesForExtensionProfile(
        const extensions::Extension* extension,
        ItemInspectViewList* result) {
  extensions::ShellWindowRegistry* registry =
      extensions::ShellWindowRegistry::Get(profile());
  if (!registry) return;

  const extensions::ShellWindowRegistry::ShellWindowSet windows =
      registry->GetShellWindowsForApp(extension->id());

  for (extensions::ShellWindowRegistry::const_iterator it = windows.begin();
       it != windows.end(); ++it) {
    content::WebContents* web_contents = (*it)->web_contents();
    RenderViewHost* host = web_contents->GetRenderViewHost();
    content::RenderProcessHost* process = host->GetProcess();
    result->push_back(
        constructInspectView(web_contents->GetURL(),
                             process->GetID(),
                             host->GetRoutingID(),
                             process->GetBrowserContext()->IsOffTheRecord()));
  }
}

linked_ptr<developer::ItemInspectView> DeveloperPrivateGetItemsInfoFunction::
    constructInspectView(
        const GURL& url,
        int render_process_id,
        int render_view_id,
        bool incognito) {
  linked_ptr<developer::ItemInspectView> view(new developer::ItemInspectView());

  if (url.scheme() == extensions::kExtensionScheme) {
    // No leading slash.
    view->path = url.path().substr(1);
  } else {
    // For live pages, use the full URL.
    view->path = url.spec();
  }

  view->render_process_id = render_process_id;
  view->render_view_id = render_view_id;
  view->incognito = incognito;
  return view;
}

ItemInspectViewList DeveloperPrivateGetItemsInfoFunction::
    GetInspectablePagesForExtension(
        const extensions::Extension* extension,
        bool extension_is_enabled) {

  ItemInspectViewList result;
  // Get the extension process's active views.
  ExtensionProcessManager* process_manager =
      ExtensionSystem::Get(profile())->process_manager();
  GetInspectablePagesForExtensionProcess(
      process_manager->GetRenderViewHostsForExtension(extension->id()),
      &result);

  // Get shell window views
  GetShellWindowPagesForExtensionProfile(extension, &result);

  // Include a link to start the lazy background page, if applicable.
  if (BackgroundInfo::HasLazyBackgroundPage(extension) &&
      extension_is_enabled &&
      !process_manager->GetBackgroundHostForExtension(extension->id())) {
    result.push_back(constructInspectView(
        BackgroundInfo::GetBackgroundURL(extension), -1, -1, false));
  }

  ExtensionService* service = profile()->GetExtensionService();
  // Repeat for the incognito process, if applicable. Don't try to get
  // shell windows for incognito process.
  if (service->profile()->HasOffTheRecordProfile() &&
      extension->incognito_split_mode()) {
    process_manager = ExtensionSystem::Get(
        service->profile()->GetOffTheRecordProfile())->process_manager();
    GetInspectablePagesForExtensionProcess(
        process_manager->GetRenderViewHostsForExtension(extension->id()),
        &result);

    if (BackgroundInfo::HasLazyBackgroundPage(extension) &&
        extension_is_enabled &&
        !process_manager->GetBackgroundHostForExtension(extension->id())) {
    result.push_back(constructInspectView(
        BackgroundInfo::GetBackgroundURL(extension), -1, -1, false));
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
    const Extension& item = **iter;

    ExtensionResource item_resource =
        extensions::IconsInfo::GetIconResource(&item,
                                               48,
                                               ExtensionIconSet::MATCH_BIGGER);
    id_to_icon[item.id()] = item_resource;

    if (item.location() == Manifest::COMPONENT)
      continue;  // Skip built-in extensions / apps;
    item_list.push_back(make_linked_ptr<developer::ItemInfo>(
        CreateItemInfo(item, false).release()));
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
  extensions::ManagementPolicy* management_policy =
      system->management_policy();
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

DeveloperPrivateReloadFunction::~DeveloperPrivateReloadFunction() {}

bool DeveloperPrivateRestartFunction::RunImpl() {
  scoped_ptr<Restart::Params> params(Restart::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  ExtensionService* service = profile()->GetExtensionService();
  EXTENSION_FUNCTION_VALIDATE(!params->item_id.empty());
  service->RestartExtension(params->item_id);
  return true;
}

DeveloperPrivateRestartFunction::~DeveloperPrivateRestartFunction() {}

DeveloperPrivateEnableFunction::DeveloperPrivateEnableFunction() {}

bool DeveloperPrivateEnableFunction::RunImpl() {
  scoped_ptr<Enable::Params> params(Enable::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::string extension_id = params->item_id;

  ExtensionSystem* system = ExtensionSystem::Get(profile());
  extensions::ManagementPolicy* management_policy =
      system->management_policy();
  ExtensionService* service = profile()->GetExtensionService();

  const Extension* extension = service->GetInstalledExtension(extension_id);
  if (!extension ||
      !management_policy->UserMayModifySettings(extension, NULL)) {
    LOG(ERROR) << "Attempt to enable an extension that is non-usermanagable "
               "was made. Extension id: " << extension->id();
    return false;
  }

  if (params->enable) {
    extensions::ExtensionPrefs* prefs = service->extension_prefs();
    if (prefs->DidExtensionEscalatePermissions(extension_id)) {
      ShellWindowRegistry* registry = ShellWindowRegistry::Get(profile());
      CHECK(registry);
      ShellWindow* shell_window = registry->GetShellWindowForRenderViewHost(
          render_view_host());
      if (!shell_window) {
        return false;
      }

      extensions::ShowExtensionDisabledDialog(
      service, shell_window->web_contents(), extension);
    } else if ((prefs->GetDisableReasons(extension_id) &
                  Extension::DISABLE_UNSUPPORTED_REQUIREMENT) &&
               !requirements_checker_.get()) {
      // Recheck the requirements.
      scoped_refptr<const Extension> extension =
          service->GetExtensionById(extension_id,
                                     true );// include_disabled
      requirements_checker_.reset(new extensions::RequirementsChecker());
      // Released by OnRequirementsChecked.
      AddRef();
      requirements_checker_->Check(
          extension,
          base::Bind(&DeveloperPrivateEnableFunction::OnRequirementsChecked,
                     this, extension_id));
    } else {
      service->EnableExtension(extension_id);

      // Make sure any browser action contained within it is not hidden.
      prefs->SetBrowserActionVisibility(extension, true);
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
      service = extensions::ExtensionSystem::Get(
          service->profile()->GetOffTheRecordProfile())->extension_service();
    const Extension* extension = service->extensions()->GetByID(
        options.extension_id);
    DCHECK(extension);
    // Wakes up the background page and  opens the inspect window.
    service->InspectBackgroundPage(extension);
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
  extensions::UnpackedInstaller::Create(service)->Load(path);
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
      UTF16ToUTF8(extensions::PackExtensionJob::StandardSuccessMessage(
          crx_file, pem_file));
  response.status = developer::PACK_STATUS_SUCCESS;
  results_ = developer::PackDirectory::Results::Create(response);
  SendResponse(true);
  Release();
}

void DeveloperPrivatePackDirectoryFunction::OnPackFailure(
    const std::string& error,
    extensions::ExtensionCreator::ErrorType error_type) {
  developer::PackDirectoryResponse response;
  response.message = error;
  if (error_type == extensions::ExtensionCreator::kCRXExists) {
    response.item_path = item_path_str_;
    response.pem_path = key_path_str_;
    response.override_flags = extensions::ExtensionCreator::kOverwriteCRX;
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
      base::FilePath::FromWStringHack(UTF8ToWide(item_path_str_));

  base::FilePath key_file =
      base::FilePath::FromWStringHack(UTF8ToWide(key_path_str_));

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

  pack_job_ = new extensions::PackExtensionJob(
      this, root_directory, key_file, flags);
  pack_job_->Start();
  return true;
}

DeveloperPrivatePackDirectoryFunction::DeveloperPrivatePackDirectoryFunction()
{}

DeveloperPrivatePackDirectoryFunction::~DeveloperPrivatePackDirectoryFunction()
{}

DeveloperPrivateLoadUnpackedFunction::~DeveloperPrivateLoadUnpackedFunction() {}

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
  SetResult(base::Value::CreateStringValue(
      UTF16ToUTF8(path.LossyDisplayName())));
  SendResponse(true);
  Release();
}

void DeveloperPrivateChoosePathFunction::FileSelectionCanceled() {
  SendResponse(false);
  Release();
}

DeveloperPrivateChoosePathFunction::~DeveloperPrivateChoosePathFunction() {}

bool DeveloperPrivateGetStringsFunction::RunImpl() {
  DictionaryValue* dict = new DictionaryValue();
  SetResult(dict);

  #define   SET_STRING(id, idr) \
    dict->SetString(id, l10n_util::GetStringUTF16(idr))
  SET_STRING("extensionSettings", IDS_MANAGE_EXTENSIONS_SETTING_WINDOWS_TITLE);

  SET_STRING("appsDevtoolNoItems",
             IDS_APPS_DEVTOOL_NONE_INSTALLED);
  SET_STRING("appsDevtoolTitle", IDS_APPS_DEVTOOL_TITLE);
  SET_STRING("extensionSettingsGetMoreExtensions", IDS_GET_MORE_EXTENSIONS);
  SET_STRING("extensionSettingsExtensionId", IDS_EXTENSIONS_ID);
  SET_STRING("extensionSettingsExtensionPath", IDS_EXTENSIONS_PATH);
  SET_STRING("extensionSettingsInspectViews", IDS_EXTENSIONS_INSPECT_VIEWS);
  SET_STRING("extensionSettingsInstallWarnings",
             IDS_EXTENSIONS_INSTALL_WARNINGS);
  SET_STRING("viewIncognito", IDS_EXTENSIONS_VIEW_INCOGNITO);
  SET_STRING("viewInactive", IDS_EXTENSIONS_VIEW_INACTIVE);
  SET_STRING("extensionSettingsEnable", IDS_EXTENSIONS_ENABLE);
  SET_STRING("extensionSettingsEnabled", IDS_EXTENSIONS_ENABLED);
  SET_STRING("extensionSettingsRemove", IDS_EXTENSIONS_REMOVE);
  SET_STRING("extensionSettingsEnableIncognito",
             IDS_EXTENSIONS_ENABLE_INCOGNITO);
  SET_STRING("extensionSettingsAllowFileAccess",
             IDS_EXTENSIONS_ALLOW_FILE_ACCESS);
  SET_STRING("extensionSettingsReloadTerminated",
             IDS_EXTENSIONS_RELOAD_TERMINATED);
  SET_STRING("extensionSettingsReloadUnpacked", IDS_EXTENSIONS_RELOAD_UNPACKED);
  SET_STRING("extensionSettingsLaunch", IDS_EXTENSIONS_LAUNCH);
  SET_STRING("extensionSettingsRestart", IDS_EXTENSIONS_RESTART);
  SET_STRING("extensionSettingsOptions", IDS_EXTENSIONS_OPTIONS_LINK);
  SET_STRING("extensionSettingsActivity", IDS_EXTENSIONS_ACTIVITY_LINK);
  SET_STRING("extensionSettingsPermissions", IDS_EXTENSIONS_PERMISSIONS_LINK);
  SET_STRING("extensionSettingsVisitWebsite", IDS_EXTENSIONS_VISIT_WEBSITE);
  SET_STRING("extensionSettingsVisitWebStore", IDS_EXTENSIONS_VISIT_WEBSTORE);
  SET_STRING("extensionSettingsPolicyControlled",
             IDS_EXTENSIONS_POLICY_CONTROLLED);
  SET_STRING("extensionSettingsManagedMode",
             IDS_EXTENSIONS_LOCKED_MANAGED_MODE);
  SET_STRING("extensionSettingsSideloadWipeout",
             IDS_OPTIONS_SIDELOAD_WIPEOUT_BANNER);
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

// Pack Extension strings
  SET_STRING("packExtensionOverlay", IDS_EXTENSION_PACK_DIALOG_TITLE);
  SET_STRING("packExtensionHeading", IDS_EXTENSION_PACK_DIALOG_HEADING);
  SET_STRING("packExtensionCommit", IDS_EXTENSION_PACK_BUTTON);
  SET_STRING("ok",IDS_OK);
  SET_STRING("cancel",IDS_CANCEL);
  SET_STRING("packExtensionRootDir",
     IDS_EXTENSION_PACK_DIALOG_ROOT_DIRECTORY_LABEL);
  SET_STRING("packExtensionPrivateKey",
     IDS_EXTENSION_PACK_DIALOG_PRIVATE_KEY_LABEL);
  SET_STRING("packExtensionBrowseButton", IDS_EXTENSION_PACK_DIALOG_BROWSE);
  SET_STRING("packExtensionProceedAnyway", IDS_EXTENSION_PROCEED_ANYWAY);
  SET_STRING("packExtensionWarningTitle", IDS_EXTENSION_PACK_WARNING_TITLE);
  SET_STRING("packExtensionErrorTitle", IDS_EXTENSION_PACK_ERROR_TITLE);

  #undef   SET_STRING
  return true;
}

DeveloperPrivateGetStringsFunction::~DeveloperPrivateGetStringsFunction() {}

} // namespace api

} // namespace extensions
