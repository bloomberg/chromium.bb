// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/apps_client.h"
#include "apps/shell_window.h"
#include "apps/shell_window_registry.h"
#include "apps/ui/native_app_window.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_manager.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/extension.h"

namespace {

// Create a key that identifies a ShellWindow in a RenderViewHost across App
// reloads. If the window was given an id in CreateParams, the key is the
// extension id, a colon separator, and the ShellWindow's |id|. If there is no
// |id|, the chrome-extension://extension-id/page.html URL will be used. If the
// RenderViewHost is not for a ShellWindow, return an empty string.
std::string GetWindowKeyForRenderViewHost(
    const apps::ShellWindowRegistry* registry,
    content::RenderViewHost* render_view_host) {
  apps::ShellWindow* shell_window =
      registry->GetShellWindowForRenderViewHost(render_view_host);
  if (!shell_window)
    return std::string(); // Not a ShellWindow.

  if (shell_window->window_key().empty())
    return shell_window->web_contents()->GetURL().possibly_invalid_spec();

  std::string key = shell_window->extension()->id();
  key += ':';
  key += shell_window->window_key();
  return key;
}

}  // namespace

namespace apps {

ShellWindowRegistry::ShellWindowRegistry(content::BrowserContext* context)
    : context_(context),
      devtools_callback_(base::Bind(
          &ShellWindowRegistry::OnDevToolsStateChanged,
          base::Unretained(this))) {
  content::DevToolsManager::GetInstance()->AddAgentStateCallback(
      devtools_callback_);
}

ShellWindowRegistry::~ShellWindowRegistry() {
  content::DevToolsManager::GetInstance()->RemoveAgentStateCallback(
      devtools_callback_);
}

// static
ShellWindowRegistry* ShellWindowRegistry::Get(
    content::BrowserContext* context) {
  return Factory::GetForBrowserContext(context, true /* create */);
}

void ShellWindowRegistry::AddShellWindow(ShellWindow* shell_window) {
  BringToFront(shell_window);
  FOR_EACH_OBSERVER(Observer, observers_, OnShellWindowAdded(shell_window));
}

void ShellWindowRegistry::ShellWindowIconChanged(ShellWindow* shell_window) {
  AddShellWindowToList(shell_window);
  FOR_EACH_OBSERVER(Observer, observers_,
                    OnShellWindowIconChanged(shell_window));
}

void ShellWindowRegistry::ShellWindowActivated(ShellWindow* shell_window) {
  BringToFront(shell_window);
}

void ShellWindowRegistry::RemoveShellWindow(ShellWindow* shell_window) {
  const ShellWindowList::iterator it = std::find(shell_windows_.begin(),
                                                 shell_windows_.end(),
                                                 shell_window);
  if (it != shell_windows_.end())
    shell_windows_.erase(it);
  FOR_EACH_OBSERVER(Observer, observers_, OnShellWindowRemoved(shell_window));
}

void ShellWindowRegistry::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ShellWindowRegistry::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

ShellWindowRegistry::ShellWindowList ShellWindowRegistry::GetShellWindowsForApp(
    const std::string& app_id) const {
  ShellWindowList app_windows;
  for (ShellWindowList::const_iterator i = shell_windows_.begin();
       i != shell_windows_.end(); ++i) {
    if ((*i)->extension_id() == app_id)
      app_windows.push_back(*i);
  }
  return app_windows;
}

void ShellWindowRegistry::CloseAllShellWindowsForApp(
    const std::string& app_id) {
  for (ShellWindowList::const_iterator i = shell_windows_.begin();
       i != shell_windows_.end(); ) {
    ShellWindow* shell_window = *(i++);
    if (shell_window->extension_id() == app_id)
      shell_window->GetBaseWindow()->Close();
  }
}

ShellWindow* ShellWindowRegistry::GetShellWindowForRenderViewHost(
    content::RenderViewHost* render_view_host) const {
  for (ShellWindowList::const_iterator i = shell_windows_.begin();
       i != shell_windows_.end(); ++i) {
    if ((*i)->web_contents()->GetRenderViewHost() == render_view_host)
      return *i;
  }

  return NULL;
}

ShellWindow* ShellWindowRegistry::GetShellWindowForNativeWindow(
    gfx::NativeWindow window) const {
  for (ShellWindowList::const_iterator i = shell_windows_.begin();
       i != shell_windows_.end(); ++i) {
    if ((*i)->GetNativeWindow() == window)
      return *i;
  }

  return NULL;
}

ShellWindow* ShellWindowRegistry::GetCurrentShellWindowForApp(
    const std::string& app_id) const {
  ShellWindow* result = NULL;
  for (ShellWindowList::const_iterator i = shell_windows_.begin();
       i != shell_windows_.end(); ++i) {
    if ((*i)->extension()->id() == app_id) {
      result = *i;
      if (result->GetBaseWindow()->IsActive())
        return result;
    }
  }

  return result;
}

ShellWindow* ShellWindowRegistry::GetShellWindowForAppAndKey(
    const std::string& app_id,
    const std::string& window_key) const {
  ShellWindow* result = NULL;
  for (ShellWindowList::const_iterator i = shell_windows_.begin();
       i != shell_windows_.end(); ++i) {
    if ((*i)->extension()->id() == app_id && (*i)->window_key() == window_key) {
      result = *i;
      if (result->GetBaseWindow()->IsActive())
        return result;
    }
  }
  return result;
}

bool ShellWindowRegistry::HadDevToolsAttached(
    content::RenderViewHost* render_view_host) const {
  std::string key = GetWindowKeyForRenderViewHost(this, render_view_host);
  return key.empty() ? false : inspected_windows_.count(key) != 0;
}

// static
ShellWindow* ShellWindowRegistry::GetShellWindowForNativeWindowAnyProfile(
    gfx::NativeWindow window) {
  std::vector<content::BrowserContext*> contexts =
      AppsClient::Get()->GetLoadedBrowserContexts();
  for (std::vector<content::BrowserContext*>::const_iterator i =
           contexts.begin();
       i != contexts.end(); ++i) {
    ShellWindowRegistry* registry = Factory::GetForBrowserContext(
        *i, false /* create */);
    if (!registry)
      continue;

    ShellWindow* shell_window = registry->GetShellWindowForNativeWindow(window);
    if (shell_window)
      return shell_window;
  }

  return NULL;
}

// static
bool ShellWindowRegistry::IsShellWindowRegisteredInAnyProfile(
    int window_type_mask) {
  std::vector<content::BrowserContext*> contexts =
      AppsClient::Get()->GetLoadedBrowserContexts();
  for (std::vector<content::BrowserContext*>::const_iterator i =
           contexts.begin();
       i != contexts.end(); ++i) {
    ShellWindowRegistry* registry = Factory::GetForBrowserContext(
        *i, false /* create */);
    if (!registry)
      continue;

    const ShellWindowList& shell_windows = registry->shell_windows();
    if (shell_windows.empty())
      continue;

    if (window_type_mask == 0)
      return true;

    for (const_iterator j = shell_windows.begin(); j != shell_windows.end();
         ++j) {
      if ((*j)->window_type() & window_type_mask)
        return true;
    }
  }

  return false;
}

void ShellWindowRegistry::OnDevToolsStateChanged(
    content::DevToolsAgentHost* agent_host, bool attached) {
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

void ShellWindowRegistry::AddShellWindowToList(ShellWindow* shell_window) {
  const ShellWindowList::iterator it = std::find(shell_windows_.begin(),
                                                 shell_windows_.end(),
                                                 shell_window);
  if (it != shell_windows_.end())
    return;
  shell_windows_.push_back(shell_window);
}

void ShellWindowRegistry::BringToFront(ShellWindow* shell_window) {
  const ShellWindowList::iterator it = std::find(shell_windows_.begin(),
                                                 shell_windows_.end(),
                                                 shell_window);
  if (it != shell_windows_.end())
    shell_windows_.erase(it);
  shell_windows_.push_front(shell_window);
}

///////////////////////////////////////////////////////////////////////////////
// Factory boilerplate

// static
ShellWindowRegistry* ShellWindowRegistry::Factory::GetForBrowserContext(
    content::BrowserContext* context, bool create) {
  return static_cast<ShellWindowRegistry*>(
      GetInstance()->GetServiceForBrowserContext(context, create));
}

ShellWindowRegistry::Factory* ShellWindowRegistry::Factory::GetInstance() {
  return Singleton<ShellWindowRegistry::Factory>::get();
}

ShellWindowRegistry::Factory::Factory()
    : BrowserContextKeyedServiceFactory(
        "ShellWindowRegistry",
        BrowserContextDependencyManager::GetInstance()) {
}

ShellWindowRegistry::Factory::~Factory() {
}

BrowserContextKeyedService*
ShellWindowRegistry::Factory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new ShellWindowRegistry(context);
}

bool ShellWindowRegistry::Factory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool ShellWindowRegistry::Factory::ServiceIsNULLWhileTesting() const {
  return false;
}

content::BrowserContext* ShellWindowRegistry::Factory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

}  // namespace extensions
