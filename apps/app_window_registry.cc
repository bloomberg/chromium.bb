// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/app_window_registry.h"

#include <string>
#include <vector>

#include "apps/app_window.h"
#include "apps/apps_client.h"
#include "apps/ui/native_app_window.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_manager.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/common/extension.h"

namespace {

// Create a key that identifies a AppWindow in a RenderViewHost across App
// reloads. If the window was given an id in CreateParams, the key is the
// extension id, a colon separator, and the AppWindow's |id|. If there is no
// |id|, the chrome-extension://extension-id/page.html URL will be used. If the
// RenderViewHost is not for a AppWindow, return an empty string.
std::string GetWindowKeyForRenderViewHost(
    const apps::AppWindowRegistry* registry,
    content::RenderViewHost* render_view_host) {
  apps::AppWindow* app_window =
      registry->GetAppWindowForRenderViewHost(render_view_host);
  if (!app_window)
    return std::string();  // Not a AppWindow.

  if (app_window->window_key().empty())
    return app_window->web_contents()->GetURL().possibly_invalid_spec();

  std::string key = app_window->extension_id();
  key += ':';
  key += app_window->window_key();
  return key;
}

}  // namespace

namespace apps {

void AppWindowRegistry::Observer::OnAppWindowAdded(AppWindow* app_window) {
}

void AppWindowRegistry::Observer::OnAppWindowIconChanged(
    AppWindow* app_window) {
}

void AppWindowRegistry::Observer::OnAppWindowRemoved(AppWindow* app_window) {
}

void AppWindowRegistry::Observer::OnAppWindowHidden(AppWindow* app_window) {
}

void AppWindowRegistry::Observer::OnAppWindowShown(AppWindow* app_window) {
}

AppWindowRegistry::Observer::~Observer() {
}

AppWindowRegistry::AppWindowRegistry(content::BrowserContext* context)
    : context_(context),
      devtools_callback_(base::Bind(&AppWindowRegistry::OnDevToolsStateChanged,
                                    base::Unretained(this))) {
  content::DevToolsManager::GetInstance()->AddAgentStateCallback(
      devtools_callback_);
}

AppWindowRegistry::~AppWindowRegistry() {
  content::DevToolsManager::GetInstance()->RemoveAgentStateCallback(
      devtools_callback_);
}

// static
AppWindowRegistry* AppWindowRegistry::Get(content::BrowserContext* context) {
  return Factory::GetForBrowserContext(context, true /* create */);
}

void AppWindowRegistry::AddAppWindow(AppWindow* app_window) {
  BringToFront(app_window);
  FOR_EACH_OBSERVER(Observer, observers_, OnAppWindowAdded(app_window));
}

void AppWindowRegistry::AppWindowIconChanged(AppWindow* app_window) {
  AddAppWindowToList(app_window);
  FOR_EACH_OBSERVER(Observer, observers_, OnAppWindowIconChanged(app_window));
}

void AppWindowRegistry::AppWindowActivated(AppWindow* app_window) {
  BringToFront(app_window);
}

void AppWindowRegistry::AppWindowHidden(AppWindow* app_window) {
  FOR_EACH_OBSERVER(Observer, observers_, OnAppWindowHidden(app_window));
}

void AppWindowRegistry::AppWindowShown(AppWindow* app_window) {
  FOR_EACH_OBSERVER(Observer, observers_, OnAppWindowShown(app_window));
}

void AppWindowRegistry::RemoveAppWindow(AppWindow* app_window) {
  const AppWindowList::iterator it =
      std::find(app_windows_.begin(), app_windows_.end(), app_window);
  if (it != app_windows_.end())
    app_windows_.erase(it);
  FOR_EACH_OBSERVER(Observer, observers_, OnAppWindowRemoved(app_window));
}

void AppWindowRegistry::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void AppWindowRegistry::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

AppWindowRegistry::AppWindowList AppWindowRegistry::GetAppWindowsForApp(
    const std::string& app_id) const {
  AppWindowList app_windows;
  for (AppWindowList::const_iterator i = app_windows_.begin();
       i != app_windows_.end();
       ++i) {
    if ((*i)->extension_id() == app_id)
      app_windows.push_back(*i);
  }
  return app_windows;
}

void AppWindowRegistry::CloseAllAppWindowsForApp(const std::string& app_id) {
  const AppWindowList windows = GetAppWindowsForApp(app_id);
  for (AppWindowRegistry::const_iterator it = windows.begin();
       it != windows.end();
       ++it) {
    (*it)->GetBaseWindow()->Close();
  }
}

AppWindow* AppWindowRegistry::GetAppWindowForRenderViewHost(
    content::RenderViewHost* render_view_host) const {
  for (AppWindowList::const_iterator i = app_windows_.begin();
       i != app_windows_.end();
       ++i) {
    if ((*i)->web_contents()->GetRenderViewHost() == render_view_host)
      return *i;
  }

  return NULL;
}

AppWindow* AppWindowRegistry::GetAppWindowForNativeWindow(
    gfx::NativeWindow window) const {
  for (AppWindowList::const_iterator i = app_windows_.begin();
       i != app_windows_.end();
       ++i) {
    if ((*i)->GetNativeWindow() == window)
      return *i;
  }

  return NULL;
}

AppWindow* AppWindowRegistry::GetCurrentAppWindowForApp(
    const std::string& app_id) const {
  AppWindow* result = NULL;
  for (AppWindowList::const_iterator i = app_windows_.begin();
       i != app_windows_.end();
       ++i) {
    if ((*i)->extension_id() == app_id) {
      result = *i;
      if (result->GetBaseWindow()->IsActive())
        return result;
    }
  }

  return result;
}

AppWindow* AppWindowRegistry::GetAppWindowForAppAndKey(
    const std::string& app_id,
    const std::string& window_key) const {
  AppWindow* result = NULL;
  for (AppWindowList::const_iterator i = app_windows_.begin();
       i != app_windows_.end();
       ++i) {
    if ((*i)->extension_id() == app_id && (*i)->window_key() == window_key) {
      result = *i;
      if (result->GetBaseWindow()->IsActive())
        return result;
    }
  }
  return result;
}

bool AppWindowRegistry::HadDevToolsAttached(
    content::RenderViewHost* render_view_host) const {
  std::string key = GetWindowKeyForRenderViewHost(this, render_view_host);
  return key.empty() ? false : inspected_windows_.count(key) != 0;
}

// static
AppWindow* AppWindowRegistry::GetAppWindowForNativeWindowAnyProfile(
    gfx::NativeWindow window) {
  std::vector<content::BrowserContext*> contexts =
      AppsClient::Get()->GetLoadedBrowserContexts();
  for (std::vector<content::BrowserContext*>::const_iterator i =
           contexts.begin();
       i != contexts.end();
       ++i) {
    AppWindowRegistry* registry =
        Factory::GetForBrowserContext(*i, false /* create */);
    if (!registry)
      continue;

    AppWindow* app_window = registry->GetAppWindowForNativeWindow(window);
    if (app_window)
      return app_window;
  }

  return NULL;
}

// static
bool AppWindowRegistry::IsAppWindowRegisteredInAnyProfile(
    int window_type_mask) {
  std::vector<content::BrowserContext*> contexts =
      AppsClient::Get()->GetLoadedBrowserContexts();
  for (std::vector<content::BrowserContext*>::const_iterator i =
           contexts.begin();
       i != contexts.end();
       ++i) {
    AppWindowRegistry* registry =
        Factory::GetForBrowserContext(*i, false /* create */);
    if (!registry)
      continue;

    const AppWindowList& app_windows = registry->app_windows();
    if (app_windows.empty())
      continue;

    if (window_type_mask == 0)
      return true;

    for (const_iterator j = app_windows.begin(); j != app_windows.end(); ++j) {
      if ((*j)->window_type() & window_type_mask)
        return true;
    }
  }

  return false;
}

// static
void AppWindowRegistry::CloseAllAppWindows() {
  std::vector<content::BrowserContext*> contexts =
      AppsClient::Get()->GetLoadedBrowserContexts();
  for (std::vector<content::BrowserContext*>::const_iterator i =
           contexts.begin();
       i != contexts.end();
       ++i) {
    AppWindowRegistry* registry =
        Factory::GetForBrowserContext(*i, false /* create */);
    if (!registry)
      continue;

    while (!registry->app_windows().empty())
      registry->app_windows().front()->GetBaseWindow()->Close();
  }
}

void AppWindowRegistry::OnDevToolsStateChanged(
    content::DevToolsAgentHost* agent_host,
    bool attached) {
  content::RenderViewHost* rvh = agent_host->GetRenderViewHost();
  // Ignore unrelated notifications.
  if (!rvh ||
      rvh->GetSiteInstance()->GetProcess()->GetBrowserContext() != context_)
    return;

  std::string key = GetWindowKeyForRenderViewHost(this, rvh);
  if (key.empty())
    return;

  if (attached)
    inspected_windows_.insert(key);
  else
    inspected_windows_.erase(key);
}

void AppWindowRegistry::AddAppWindowToList(AppWindow* app_window) {
  const AppWindowList::iterator it =
      std::find(app_windows_.begin(), app_windows_.end(), app_window);
  if (it != app_windows_.end())
    return;
  app_windows_.push_back(app_window);
}

void AppWindowRegistry::BringToFront(AppWindow* app_window) {
  const AppWindowList::iterator it =
      std::find(app_windows_.begin(), app_windows_.end(), app_window);
  if (it != app_windows_.end())
    app_windows_.erase(it);
  app_windows_.push_front(app_window);
}

///////////////////////////////////////////////////////////////////////////////
// Factory boilerplate

// static
AppWindowRegistry* AppWindowRegistry::Factory::GetForBrowserContext(
    content::BrowserContext* context,
    bool create) {
  return static_cast<AppWindowRegistry*>(
      GetInstance()->GetServiceForBrowserContext(context, create));
}

AppWindowRegistry::Factory* AppWindowRegistry::Factory::GetInstance() {
  return Singleton<AppWindowRegistry::Factory>::get();
}

AppWindowRegistry::Factory::Factory()
    : BrowserContextKeyedServiceFactory(
          "AppWindowRegistry",
          BrowserContextDependencyManager::GetInstance()) {}

AppWindowRegistry::Factory::~Factory() {}

KeyedService* AppWindowRegistry::Factory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new AppWindowRegistry(context);
}

bool AppWindowRegistry::Factory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool AppWindowRegistry::Factory::ServiceIsNULLWhileTesting() const {
  return false;
}

content::BrowserContext* AppWindowRegistry::Factory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return extensions::ExtensionsBrowserClient::Get()->GetOriginalContext(
      context);
}

}  // namespace apps
