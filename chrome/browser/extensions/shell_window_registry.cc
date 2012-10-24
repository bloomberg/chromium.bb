// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/shell_window_registry.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/browser/ui/extensions/shell_window.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"

namespace {

// Create a key that identifies a ShellWindow in a RenderViewHost across App
// reloads. Current format concatenates the extension ID (fixed length string)
// a colon separator, and the ShellWindow's |id| (passed to create). If the
// RenderViewHost is not for a ShellWindow, or if the window was not created
// with an |id|, return an empty string.
std::string GetWindowKeyForRenderViewHost(
    const extensions::ShellWindowRegistry* registry,
    content::RenderViewHost* render_view_host) {
  ShellWindow* shell_window =
      registry->GetShellWindowForRenderViewHost(render_view_host);
  if (!shell_window)
    return std::string(); // Not a ShellWindow.

  if (shell_window->window_key().empty())
    return std::string(); // Not created with an |id| in CreateParams.

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
  return Factory::GetForProfile(profile);
}

void ShellWindowRegistry::AddShellWindow(ShellWindow* shell_window) {
  shell_windows_.insert(shell_window);
  FOR_EACH_OBSERVER(Observer, observers_, OnShellWindowAdded(shell_window));
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
    if ((*i)->extension()->id() == app_id)
      result = *i;
    if (result->GetBaseWindow()->IsActive())
      return result;
  }

  return result;
}

bool ShellWindowRegistry::HadDevToolsAttached(
    content::RenderViewHost* render_view_host) const {
  std::string key = GetWindowKeyForRenderViewHost(this, render_view_host);
  return key.empty() ? false : inspected_windows_.count(key) != 0;
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
    Profile* profile) {
  return static_cast<ShellWindowRegistry*>(
      GetInstance()->GetServiceForProfile(profile, true));
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

bool ShellWindowRegistry::Factory::ServiceHasOwnInstanceInIncognito() const {
  return true;
}

bool ShellWindowRegistry::Factory::ServiceIsCreatedWithProfile() const {
  return true;
}

bool ShellWindowRegistry::Factory::ServiceIsNULLWhileTesting() const {
  return false;
}

}  // namespace extensions
