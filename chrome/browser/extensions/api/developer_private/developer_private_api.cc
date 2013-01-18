// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/developer_private/developer_private_api.h"

#include "base/base64.h"
#include "base/file_util.h"
#include "base/string_number_conversions.h"
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

}  // namespace

namespace extensions {

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
    case content::NOTIFICATION_RENDER_VIEW_HOST_DELETED:
    case chrome::NOTIFICATION_BACKGROUND_CONTENTS_DELETED:
      break;
    default:
      NOTREACHED();
  }
}

void DeveloperPrivateAPI::SetLastUnpackedDirectory(const FilePath& path) {
  last_unpacked_directory_ = path;
}

void DeveloperPrivateAPI::RegisterNotifications() {
  registrar_.Add(this,
                 content::NOTIFICATION_RENDER_VIEW_HOST_DELETED,
                 content::NotificationService::AllBrowserContextsAndSources());
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
      ExtensionSystem* system,
      bool item_is_enabled) {
  scoped_ptr<developer::ItemInfo> info(new developer::ItemInfo());

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

  if (item.location() == Extension::LOAD) {
    info->path.reset(
        new std::string(UTF16ToUTF8(item.path().LossyDisplayName())));
  }

  info->incognito_enabled = service->IsIncognitoEnabled(item.id());
  info->wants_file_access = item.wants_file_access();
  info->allow_file_access = service->AllowFileAccess(&item);
  info->allow_reload = (item.location() == Extension::LOAD);
  info->is_unpacked = (item.location() == Extension::LOAD);
  info->terminated = service->terminated_extensions()->Contains(item.id());
  info->allow_incognito = item.can_be_incognito_enabled();

  GURL icon =
      ExtensionIconSource::GetIconURL(&item,
                                      extension_misc::EXTENSION_ICON_MEDIUM,
                                      ExtensionIconSet::MATCH_BIGGER,
                                      !info->enabled,
                                      NULL);
  info->icon = icon.spec();

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

void DeveloperPrivateGetItemsInfoFunction::AddItemsInfo(
    const ExtensionSet& items,
    ExtensionSystem* system,
    ItemInfoList* item_list) {
  for (ExtensionSet::const_iterator iter = items.begin();
       iter != items.end(); ++iter) {
    const Extension& item = **iter;
    if (item.location() == Extension::COMPONENT)
      continue;  // Skip built-in extensions / apps;
    item_list->push_back(make_linked_ptr<developer::ItemInfo>(
        CreateItemInfo(item, system, false).release()));
  }
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

linked_ptr<developer::ItemInspectView>
  DeveloperPrivateGetItemsInfoFunction::constructInspectView(
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
  if (extension->has_lazy_background_page() && extension_is_enabled &&
      !process_manager->GetBackgroundHostForExtension(extension->id())) {
    result.push_back(
        constructInspectView(extension->GetBackgroundURL(), -1, -1, false));
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

    if (extension->has_lazy_background_page() && extension_is_enabled &&
        !process_manager->GetBackgroundHostForExtension(extension->id())) {
    result.push_back(
        constructInspectView(extension->GetBackgroundURL(), -1, -1, false));
    }
  }

  return result;
}

bool DeveloperPrivateGetItemsInfoFunction::RunImpl() {
  ItemInfoList items;
  ExtensionSystem* system = ExtensionSystem::Get(profile());
  scoped_ptr<developer::GetItemsInfo::Params> params(
      developer::GetItemsInfo::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get() != NULL);

  bool include_disabled = params->include_disabled;
  bool include_terminated = params->include_terminated;
  ExtensionSet extension_set;
  extension_set.InsertAll(
      *profile()->GetExtensionService()->extensions());

  if (include_disabled) {
    extension_set.InsertAll(
        *profile()->GetExtensionService()->disabled_extensions());
  }

  if (include_terminated) {
    extension_set.InsertAll(
        *profile()->GetExtensionService()->terminated_extensions());
  }

  AddItemsInfo(extension_set, system, &items);

  results_ = developer::GetItemsInfo::Results::Create(items);
  return true;
}

DeveloperPrivateGetItemsInfoFunction::~DeveloperPrivateGetItemsInfoFunction() {}

bool DeveloperPrivateAllowFileAccessFunction::RunImpl() {
  std::string extension_id;
  bool allow = false;
  EXTENSION_FUNCTION_VALIDATE(user_gesture_);
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &extension_id));
  EXTENSION_FUNCTION_VALIDATE(args_->GetBoolean(1, &allow));

  ExtensionSystem* system = ExtensionSystem::Get(profile());
  extensions::ManagementPolicy* management_policy =
      system->management_policy();
  ExtensionService* service = profile()->GetExtensionService();
  const Extension* extension = service->GetInstalledExtension(extension_id);
  bool result = true;

  if (!extension) {
    result = false;
  } else if (!management_policy->UserMayModifySettings(extension, NULL)) {
    LOG(ERROR) << "Attempt to change allow file access of an extension that "
               << "non-usermanagable was made. Extension id : "
               << extension->id();
    result = false;
  } else {
    service->SetAllowFileAccess(extension, allow);
    result = true;
  }

  SetResult(Value::CreateBooleanValue(result));
  return true;
}

DeveloperPrivateAllowFileAccessFunction::
    ~DeveloperPrivateAllowFileAccessFunction() {}

bool DeveloperPrivateReloadFunction::RunImpl() {
  std::string extension_id;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &extension_id));
  ExtensionService* service = profile()->GetExtensionService();
  CHECK(!extension_id.empty());
  service->ReloadExtension(extension_id);
  SetResult(Value::CreateBooleanValue(true));
  return true;
}

DeveloperPrivateReloadFunction::~DeveloperPrivateReloadFunction() {}

DeveloperPrivateEnableFunction::DeveloperPrivateEnableFunction() {}

bool DeveloperPrivateEnableFunction::RunImpl() {
  std::string extension_id;
  bool enable = false;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &extension_id));
  EXTENSION_FUNCTION_VALIDATE(args_->GetBoolean(1, &enable));

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

  if (enable) {
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
  SetResult(Value::CreateBooleanValue(true));
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
  SetResult(Value::CreateBooleanValue(false));

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

  SetResult(Value::CreateBooleanValue(true));
  DevToolsWindow::OpenDevToolsWindow(host);
  return true;
}

DeveloperPrivateInspectFunction::~DeveloperPrivateInspectFunction() {}

bool DeveloperPrivateLoadUnpackedFunction::RunImpl() {
  string16 select_title =
      l10n_util::GetStringUTF16(IDS_EXTENSION_LOAD_FROM_DIRECTORY);

  const ui::SelectFileDialog::Type kSelectType =
      ui::SelectFileDialog::SELECT_FOLDER;
  const FilePath& last_unpacked_directory =
      DeveloperPrivateAPI::Get(profile())->getLastUnpackedDirectory();
  SetResult(Value::CreateBooleanValue(true));
  // Balanced in FileSelected / FileSelectionCanceled.
  AddRef();
  bool result = ShowPicker(kSelectType, last_unpacked_directory, select_title);
  return result;
}

bool DeveloperPrivateChooseEntryFunction::ShowPicker(
    ui::SelectFileDialog::Type picker_type,
    const FilePath& last_directory,
    const string16& select_title) {
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
  last_directory, select_title);
  return true;
}

bool DeveloperPrivateChooseEntryFunction::RunImpl() { return false; }

DeveloperPrivateChooseEntryFunction::~DeveloperPrivateChooseEntryFunction() {}

void DeveloperPrivateLoadUnpackedFunction::FileSelected(const FilePath& path) {

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

DeveloperPrivateLoadUnpackedFunction::~DeveloperPrivateLoadUnpackedFunction() {}

bool DeveloperPrivateGetStringsFunction::RunImpl() {
  DictionaryValue* dict = new DictionaryValue();
  SetResult(dict);

  #define   SET_STRING(id, idr) \
    dict->SetString(id, l10n_util::GetStringUTF16(idr))
  SET_STRING("extensionSettings", IDS_MANAGE_EXTENSIONS_SETTING_WINDOWS_TITLE);

  SET_STRING("extensionSettingsNoExtensions", IDS_EXTENSIONS_NONE_INSTALLED);
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
  SET_STRING("extensionSettingsOptions", IDS_EXTENSIONS_OPTIONS_LINK);
  SET_STRING("extensionSettingsActivity", IDS_EXTENSIONS_ACTIVITY_LINK);
  SET_STRING("extensionSettingsVisitWebsite", IDS_EXTENSIONS_VISIT_WEBSITE);
  SET_STRING("extensionSettingsVisitWebStore", IDS_EXTENSIONS_VISIT_WEBSTORE);
  SET_STRING("extensionSettingsPolicyControlled",
             IDS_EXTENSIONS_POLICY_CONTROLLED);
  SET_STRING("extensionSettingsManagedMode",
             IDS_EXTENSIONS_LOCKED_MANAGED_MODE);
  SET_STRING("extensionSettingsSideloadWipeout",
             IDS_OPTIONS_SIDELOAD_WIPEOUT_BANNER);
  SET_STRING("extensionSettingsShowButton", IDS_EXTENSIONS_SHOW_BUTTON);
  SET_STRING("extensionSettingsLoadUnpackedButton",
             IDS_EXTENSIONS_LOAD_UNPACKED_BUTTON);
  SET_STRING("extensionSettingsPackButton", IDS_EXTENSIONS_PACK_BUTTON);
  SET_STRING("extensionSettingsCommandsLink",
             IDS_EXTENSIONS_COMMANDS_CONFIGURE);
  SET_STRING("extensionSettingsUpdateButton", IDS_EXTENSIONS_UPDATE_BUTTON);
  SET_STRING("extensionSettingsWarningsTitle", IDS_EXTENSION_WARNINGS_TITLE);
  SET_STRING("extensionSettingsShowDetails", IDS_EXTENSIONS_SHOW_DETAILS);
  SET_STRING("extensionSettingsHideDetails", IDS_EXTENSIONS_HIDE_DETAILS);
  SET_STRING("extensionUninstall", IDS_EXTENSIONS_UNINSTALL);

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
