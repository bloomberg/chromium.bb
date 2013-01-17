// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/developer_private/developer_private_api.h"

#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/extensions/api/developer_private/developer_private_api_factory.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/management_policy.h"
#include "chrome/browser/extensions/updater/extension_updater.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/extensions/extension_icon_source.h"
#include "chrome/browser/view_type_utils.h"
#include "chrome/common/extensions/api/developer_private.h"
#include "chrome/common/extensions/manifest_url_handler.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"

using content::RenderViewHost;
using extensions::DeveloperPrivateAPI;
using extensions::Extension;
using extensions::ExtensionSystem;

namespace {

extensions::ExtensionUpdater* GetExtensionUpdater(Profile* profile) {
    return profile->GetExtensionService()->updater();
}

}  // namespace

namespace extensions {

DeveloperPrivateAPI* DeveloperPrivateAPI::Get(Profile* profile) {
  return DeveloperPrivateAPIFactory::GetForProfile(profile);
}

DeveloperPrivateAPI::DeveloperPrivateAPI(Profile* profile)
  : profile_(profile),
    deleting_render_view_host_(NULL) {
      RegisterNotifications();
}

scoped_ptr<developer::ItemInfo> DeveloperPrivateAPI::CreateItemInfo(
    const Extension& item,
    ExtensionSystem* system,
    bool item_is_enabled) {
  scoped_ptr<developer::ItemInfo> info(new developer::ItemInfo());
  ExtensionService* service = system->extension_service();

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

  info->enabled_incognito = service->IsIncognitoEnabled(item.id());
  info->wants_file_access = item.wants_file_access();
  info->allow_file_access = service->AllowFileAccess(&item);
  info->allow_reload = (item.location() == Extension::LOAD);
  info->is_unpacked = (item.location() == Extension::LOAD);

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

void DeveloperPrivateAPI::AddItemsInfo(const ExtensionSet& items,
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

void DeveloperPrivateAPI::GetInspectablePagesForExtensionProcess(
    const std::set<content::RenderViewHost*>& views,
    ItemInspectViewList* result) {
  for (std::set<content::RenderViewHost*>::const_iterator iter = views.begin();
       iter != views.end(); ++iter) {
    content::RenderViewHost* host = *iter;
    content::WebContents* web_contents =
        content::WebContents::FromRenderViewHost(host);
    chrome::ViewType host_type = chrome::GetViewType(web_contents);
    if (host == deleting_render_view_host_ ||
        chrome::VIEW_TYPE_EXTENSION_POPUP == host_type ||
        chrome::VIEW_TYPE_EXTENSION_DIALOG == host_type)
      continue;

    GURL url = web_contents->GetURL();
    content::RenderProcessHost* process = host->GetProcess();
    linked_ptr<developer::ItemInspectView>
        view(new developer::ItemInspectView());
    view->path = url.path().substr(1);
    view->render_process_id = process->GetID();
    view->render_view_id = host->GetRoutingID();
    view->incognito = process->GetBrowserContext()->IsOffTheRecord();

    result->push_back(view);
  }
}

ItemInspectViewList DeveloperPrivateAPI::GetInspectablePagesForExtension(
    const extensions::Extension* extension,
    bool extension_is_enabled) {

  ItemInspectViewList result;
  // Get the extension process's active views.
  ExtensionProcessManager* process_manager =
      extensions::ExtensionSystem::Get(profile_)->process_manager();
  GetInspectablePagesForExtensionProcess(
      process_manager->GetRenderViewHostsForExtension(extension->id()),
      &result);
  return result;
}

void DeveloperPrivateAPI::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    // TODO(grv): Listen to other notifications.
    case content::NOTIFICATION_RENDER_VIEW_HOST_DELETED:
    case chrome::NOTIFICATION_BACKGROUND_CONTENTS_DELETED:
      deleting_render_view_host_ =
          content::Source<RenderViewHost>(source).ptr();
      break;
    default:
      NOTREACHED();
  }
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
        *profile()->GetExtensionService()->disabled_extensions());
  }

  DeveloperPrivateAPI::Get(profile())->AddItemsInfo(
      extension_set, system, &items);

  results_ = developer::GetItemsInfo::Results::Create(items);
  return true;
}

DeveloperPrivateGetItemsInfoFunction::~DeveloperPrivateGetItemsInfoFunction() {}

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

}  // namespace api

}  // namespace extensions
