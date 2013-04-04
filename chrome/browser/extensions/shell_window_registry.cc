// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/shell_window_registry.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/extensions/native_app_window.h"
#include "chrome/browser/ui/extensions/shell_window.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"

namespace {

// Create a key that identifies a ShellWindow in a RenderViewHost across App
// reloads. If the window was given an id in CreateParams, the key is the
// extension id, a colon separator, and the ShellWindow's |id|. If there is no
// |id|, the chrome-extension://extension-id/page.html URL will be used. If the
// RenderViewHost is not for a ShellWindow, return an empty string.
std::string GetWindowKeyForRenderViewHost(
    const extensions::ShellWindowRegistry* registry,
    content::RenderViewHost* render_view_host) {
  ShellWindow* shell_window =
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

}

namespace extensions {

ShellWindowRegistry::ShellWindowRegistry(Profile* profile) {
  registrar_.Add(this, content::NOTIFICATION_DEVTOOLS_AGENT_ATTACHED,
                 content::Source<content::BrowserContext>(profile));
  registrar_.Add(this, content::NOTIFICATION_DEVTOOLS_AGENT_DETACHED,
                 content::Source<content::BrowserContext>(profile));
}

ShellWindowRegistry::~ShellWindowRegistry() {}

// static
ShellWindowRegistry* ShellWindowRegistry::Get(Profile* profile) {
  return Factory::GetForProfile(profile, true /* create */);
}

void ShellWindowRegistry::AddShellWindow(ShellWindow* shell_window) {
  shell_windows_.insert(shell_window);
  FOR_EACH_OBSERVER(Observer, observers_, OnShellWindowAdded(shell_window));
}

void ShellWindowRegistry::ShellWindowIconChanged(ShellWindow* shell_window) {
  shell_windows_.insert(shell_window);
  FOR_EACH_OBSERVER(Observer, observers_,
                    OnShellWindowIconChanged(shell_window));
}

void ShellWindowRegistry::RemoveShellWindow(ShellWindow* shell_window) {
  shell_windows_.erase(shell_window);
  FOR_EACH_OBSERVER(Observer, observers_, OnShellWindowRemoved(shell_window));
}

void ShellWindowRegistry::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ShellWindowRegistry::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

ShellWindowRegistry::ShellWindowSet ShellWindowRegistry::GetShellWindowsForApp(
    const std::string& app_id) const {
  ShellWindowSet app_windows;
  for (ShellWindowSet::const_iterator i = shell_windows_.begin();
       i != shell_windows_.end(); ++i) {
    if ((*i)->extension()->id() == app_id)
      app_windows.insert(*i);
  }
  return app_windows;
}

ShellWindow* ShellWindowRegistry::GetShellWindowForRenderViewHost(
    content::RenderViewHost* render_view_host) const {
  for (ShellWindowSet::const_iterator i = shell_windows_.begin();
       i != shell_windows_.end(); ++i) {
    if ((*i)->web_contents()->GetRenderViewHost() == render_view_host)
      return *i;
  }

  return NULL;
}

ShellWindow* ShellWindowRegistry::GetShellWindowForNativeWindow(
    gfx::NativeWindow window) const {
  for (ShellWindowSet::const_iterator i = shell_windows_.begin();
       i != shell_windows_.end(); ++i) {
    if ((*i)->GetNativeWindow() == window)
      return *i;
  }

  return NULL;
}

ShellWindow* ShellWindowRegistry::GetCurrentShellWindowForApp(
    const std::string& app_id) const {
  ShellWindow* result = NULL;
  for (ShellWindowSet::const_iterator i = shell_windows_.begin();
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
  for (ShellWindowSet::const_iterator i = shell_windows_.begin();
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
  std::vector<Profile*> profiles =
      g_browser_process->profile_manager()->GetLoadedProfiles();
  for (std::vector<Profile*>::const_iterator i = profiles.begin();
       i != profiles.end(); ++i) {
    ShellWindowRegistry* registry = Factory::GetForProfile(*i,
                                                           false /* create */);
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
  std::vector<Profile*> profiles =
      g_browser_process->profile_manager()->GetLoadedProfiles();
  for (std::vector<Profile*>::const_iterator i = profiles.begin();
       i != profiles.end(); ++i) {
    ShellWindowRegistry* registry = Factory::GetForProfile(*i,
                                                           false /* create */);
    if (!registry)
      continue;

    const ShellWindowSet& shell_windows = registry->shell_windows();
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

void ShellWindowRegistry::Observe(int type,
                                  const content::NotificationSource& source,
                                  const content::NotificationDetails& details) {
  content::RenderViewHost* render_view_host =
      content::Details<content::RenderViewHost>(details).ptr();
  std::string key = GetWindowKeyForRenderViewHost(this, render_view_host);
  if (key.empty())
    return;

  switch (type) {
    case content::NOTIFICATION_DEVTOOLS_AGENT_ATTACHED:
      inspected_windows_.insert(key);
      break;
    case content::NOTIFICATION_DEVTOOLS_AGENT_DETACHED:
      inspected_windows_.erase(key);
      break;
    default:
      NOTREACHED();
    }
}

///////////////////////////////////////////////////////////////////////////////
// Factory boilerplate

// static
ShellWindowRegistry* ShellWindowRegistry::Factory::GetForProfile(
    Profile* profile, bool create) {
  return static_cast<ShellWindowRegistry*>(
      GetInstance()->GetServiceForProfile(profile, create));
}

ShellWindowRegistry::Factory* ShellWindowRegistry::Factory::GetInstance() {
  return Singleton<ShellWindowRegistry::Factory>::get();
}

ShellWindowRegistry::Factory::Factory()
    : ProfileKeyedServiceFactory("ShellWindowRegistry",
                                 ProfileDependencyManager::GetInstance()) {
}

ShellWindowRegistry::Factory::~Factory() {
}

ProfileKeyedService* ShellWindowRegistry::Factory::BuildServiceInstanceFor(
    Profile* profile) const {
  return new ShellWindowRegistry(profile);
}

bool ShellWindowRegistry::Factory::ServiceIsCreatedWithProfile() const {
  return true;
}

bool ShellWindowRegistry::Factory::ServiceIsNULLWhileTesting() const {
  return false;
}

}  // namespace extensions
