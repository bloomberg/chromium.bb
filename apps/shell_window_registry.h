// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_SHELL_WINDOW_REGISTRY_H_
#define APPS_SHELL_WINDOW_REGISTRY_H_

#include <list>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/singleton.h"
#include "base/observer_list.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service_factory.h"
#include "ui/gfx/native_widget_types.h"

class Profile;

namespace content {
class DevToolsAgentHost;
class RenderViewHost;
}

namespace apps {

class ShellWindow;

// The ShellWindowRegistry tracks the ShellWindows for all platform apps for a
// particular profile.
// This class is planned to evolve into tracking all PlatformApps for a
// particular profile, with a PlatformApp encapsulating all views (background
// page, shell windows, tray view, panels etc.) and other app level behaviour
// (e.g. notifications the app is interested in, lifetime of the background
// page).
class ShellWindowRegistry : public BrowserContextKeyedService {
 public:
  class Observer {
   public:
    // Called just after a shell window was added.
    virtual void OnShellWindowAdded(apps::ShellWindow* shell_window) = 0;
    // Called when the window icon changes.
    virtual void OnShellWindowIconChanged(apps::ShellWindow* shell_window) = 0;
    // Called just after a shell window was removed.
    virtual void OnShellWindowRemoved(apps::ShellWindow* shell_window) = 0;

   protected:
    virtual ~Observer() {}
  };

  typedef std::list<apps::ShellWindow*> ShellWindowList;
  typedef ShellWindowList::const_iterator const_iterator;
  typedef std::set<std::string> InspectedWindowSet;

  explicit ShellWindowRegistry(Profile* profile);
  virtual ~ShellWindowRegistry();

  // Returns the instance for the given profile, or NULL if none. This is
  // a convenience wrapper around ShellWindowRegistry::Factory::GetForProfile.
  static ShellWindowRegistry* Get(Profile* profile);

  void AddShellWindow(apps::ShellWindow* shell_window);
  void ShellWindowIconChanged(apps::ShellWindow* shell_window);
  // Called by |shell_window| when it is activated.
  void ShellWindowActivated(apps::ShellWindow* shell_window);
  void RemoveShellWindow(apps::ShellWindow* shell_window);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns a set of windows owned by the application identified by app_id.
  ShellWindowList GetShellWindowsForApp(const std::string& app_id) const;
  const ShellWindowList& shell_windows() const { return shell_windows_; }

  // Close all shell windows associated with an app.
  void CloseAllShellWindowsForApp(const std::string& app_id);

  // Helper functions to find shell windows with particular attributes.
  apps::ShellWindow* GetShellWindowForRenderViewHost(
      content::RenderViewHost* render_view_host) const;
  apps::ShellWindow* GetShellWindowForNativeWindow(
      gfx::NativeWindow window) const;
  // Returns an app window for the given app, or NULL if no shell windows are
  // open. If there is a window for the given app that is active, that one will
  // be returned, otherwise an arbitrary window will be returned.
  apps::ShellWindow* GetCurrentShellWindowForApp(
      const std::string& app_id) const;
  // Returns an app window for the given app and window key, or NULL if no shell
  // window with the key are open. If there is a window for the given app and
  // key that is active, that one will be returned, otherwise an arbitrary
  // window will be returned.
  apps::ShellWindow* GetShellWindowForAppAndKey(
      const std::string& app_id,
      const std::string& window_key) const;

  // Returns whether a ShellWindow's ID was last known to have a DevToolsAgent
  // attached to it, which should be restored during a reload of a corresponding
  // newly created |render_view_host|.
  bool HadDevToolsAttached(content::RenderViewHost* render_view_host) const;

  // Returns the shell window for |window|, looking in all profiles.
  static apps::ShellWindow* GetShellWindowForNativeWindowAnyProfile(
      gfx::NativeWindow window);

  // Returns true if the number of shell windows registered across all profiles
  // is non-zero. |window_type_mask| is a bitwise OR filter of
  // ShellWindow::WindowType, or 0 for any window type.
  static bool IsShellWindowRegisteredInAnyProfile(int window_type_mask);

  class Factory : public BrowserContextKeyedServiceFactory {
   public:
    static ShellWindowRegistry* GetForProfile(Profile* profile, bool create);

    static Factory* GetInstance();
   private:
    friend struct DefaultSingletonTraits<Factory>;

    Factory();
    virtual ~Factory();

    // BrowserContextKeyedServiceFactory
    virtual BrowserContextKeyedService* BuildServiceInstanceFor(
        content::BrowserContext* profile) const OVERRIDE;
    virtual bool ServiceIsCreatedWithBrowserContext() const OVERRIDE;
    virtual bool ServiceIsNULLWhileTesting() const OVERRIDE;
    virtual content::BrowserContext* GetBrowserContextToUse(
        content::BrowserContext* context) const OVERRIDE;
  };

 protected:
  void OnDevToolsStateChanged(content::DevToolsAgentHost*, bool attached);

 private:
  // Ensures the specified |shell_window| is included in |shell_windows_|.
  // Otherwise adds |shell_window| to the back of |shell_windows_|.
  void AddShellWindowToList(apps::ShellWindow* shell_window);

  // Bring |shell_window| to the front of |shell_windows_|. If it is not in the
  // list, add it first.
  void BringToFront(apps::ShellWindow* shell_window);

  Profile* profile_;
  ShellWindowList shell_windows_;
  InspectedWindowSet inspected_windows_;
  ObserverList<Observer> observers_;
  base::Callback<void(content::DevToolsAgentHost*, bool)> devtools_callback_;
};

}  // namespace extensions

#endif  // APPS_SHELL_WINDOW_REGISTRY_H_
