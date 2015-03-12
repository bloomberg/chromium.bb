// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/developer_private/inspectable_views_finder.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/developer_private.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "url/gurl.h"

namespace extensions {

InspectableViewsFinder::InspectableViewsFinder(
    Profile* profile,
    content::RenderViewHost* deleting_rvh)
    : profile_(profile),
      deleting_rvh_(deleting_rvh) {
}

InspectableViewsFinder::~InspectableViewsFinder() {
}

// static
InspectableViewsFinder::View InspectableViewsFinder::ConstructView(
    const GURL& url,
    int render_process_id,
    int render_view_id,
    bool incognito,
    ViewType type) {
  linked_ptr<api::developer_private::ExtensionView> view(
      new api::developer_private::ExtensionView());
  view->url = url.spec();
  view->render_process_id = render_process_id;
  view->render_view_id = render_view_id;
  view->incognito = incognito;
  switch (type) {
    case VIEW_TYPE_APP_WINDOW:
      view->type = api::developer_private::VIEW_TYPE_APP_WINDOW;
      break;
    case VIEW_TYPE_BACKGROUND_CONTENTS:
      view->type = api::developer_private::VIEW_TYPE_BACKGROUND_CONTENTS;
      break;
    case VIEW_TYPE_EXTENSION_BACKGROUND_PAGE:
      view->type = api::developer_private::VIEW_TYPE_EXTENSION_BACKGROUND_PAGE;
      break;
    case VIEW_TYPE_EXTENSION_DIALOG:
      view->type = api::developer_private::VIEW_TYPE_EXTENSION_DIALOG;
      break;
    case VIEW_TYPE_EXTENSION_POPUP:
      view->type = api::developer_private::VIEW_TYPE_EXTENSION_POPUP;
      break;
    case VIEW_TYPE_LAUNCHER_PAGE:
      view->type = api::developer_private::VIEW_TYPE_LAUNCHER_PAGE;
      break;
    case VIEW_TYPE_PANEL:
      view->type = api::developer_private::VIEW_TYPE_PANEL;
      break;
    case VIEW_TYPE_TAB_CONTENTS:
      view->type = api::developer_private::VIEW_TYPE_TAB_CONTENTS;
      break;
    case VIEW_TYPE_VIRTUAL_KEYBOARD:
      view->type = api::developer_private::VIEW_TYPE_VIRTUAL_KEYBOARD;
      break;
    default:
      NOTREACHED();
  }
  return view;
}

InspectableViewsFinder::ViewList InspectableViewsFinder::GetViewsForExtension(
    const Extension& extension,
    bool is_enabled) {
  ViewList result;
  GetViewsForExtensionForProfile(
      extension, profile_, is_enabled, false, &result);
  if (profile_->HasOffTheRecordProfile()) {
    GetViewsForExtensionForProfile(
        extension,
        profile_->GetOffTheRecordProfile(),
        is_enabled,
        true,
        &result);
  }

  return result;
}

void InspectableViewsFinder::GetViewsForExtensionForProfile(
    const Extension& extension,
    Profile* profile,
    bool is_enabled,
    bool is_incognito,
    ViewList* result) {
  ProcessManager* process_manager = ProcessManager::Get(profile);
  // Get the extension process's active views.
  GetViewsForExtensionProcess(
      extension,
      process_manager->GetRenderViewHostsForExtension(extension.id()),
      is_incognito,
      result);
  // Get app window views, if not incognito.
  if (!is_incognito)
    GetAppWindowViewsForExtension(extension, result);
  // Include a link to start the lazy background page, if applicable.
  if (BackgroundInfo::HasLazyBackgroundPage(&extension) &&
      is_enabled &&
      !process_manager->GetBackgroundHostForExtension(extension.id())) {
    result->push_back(ConstructView(
        BackgroundInfo::GetBackgroundURL(&extension),
        -1,
        -1,
        is_incognito,
        VIEW_TYPE_EXTENSION_BACKGROUND_PAGE));
  }
}

void InspectableViewsFinder::GetViewsForExtensionProcess(
    const Extension& extension,
    const std::set<content::RenderViewHost*>& views,
    bool is_incognito,
    ViewList* result) {
  for (const content::RenderViewHost* host : views) {
    content::WebContents* web_contents =
        content::WebContents::FromRenderViewHost(host);
    ViewType host_type = GetViewType(web_contents);
    if (host == deleting_rvh_ ||
        host_type == VIEW_TYPE_EXTENSION_POPUP ||
        host_type == VIEW_TYPE_EXTENSION_DIALOG) {
      continue;
    }

    const GURL& url = web_contents->GetURL();
    content::RenderProcessHost* process = host->GetProcess();
    result->push_back(ConstructView(
        url,
        process->GetID(),
        host->GetRoutingID(),
        is_incognito,
        host_type));
  }
}

void InspectableViewsFinder::GetAppWindowViewsForExtension(
    const Extension& extension,
    ViewList* result) {
  AppWindowRegistry* registry = AppWindowRegistry::Get(profile_);
  if (!registry)
    return;

  AppWindowRegistry::AppWindowList windows =
      registry->GetAppWindowsForApp(extension.id());

  for (const AppWindow* window : windows) {
    content::WebContents* web_contents = window->web_contents();
    content::RenderViewHost* host = web_contents->GetRenderViewHost();
    content::RenderProcessHost* process = host->GetProcess();

    result->push_back(ConstructView(
        web_contents->GetURL(),
        process->GetID(),
        host->GetRoutingID(),
        false,
        GetViewType(web_contents)));
  }
}

}  // namespace extensions
