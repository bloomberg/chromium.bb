// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SHELL_WINDOW_REGISTRY_H_
#define CHROME_BROWSER_EXTENSIONS_SHELL_WINDOW_REGISTRY_H_

#include <set>

#include "base/compiler_specific.h"
#include "base/memory/singleton.h"
#include "base/observer_list.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/gfx/native_widget_types.h"

class Profile;
class ShellWindow;

namespace content {
class RenderViewHost;
}

namespace extensions {

// The ShellWindowRegistry tracks the ShellWindows for all platform apps for a
// particular profile.
// This class is planned to evolve into tracking all PlatformApps for a
// particular profile, with a PlatformApp encapsulating all views (background
// page, shell windows, tray view, panels etc.) and other app level behaviour
// (e.g. notifications the app is interested in, lifetime of the background
// page).
class ShellWindowRegistry : public ProfileKeyedService,
                            public content::NotificationObserver {
 public:
  class Observer {
   public:
    // Called just after a shell window was added.
    virtual void OnShellWindowAdded(ShellWindow* shell_window) = 0;
    // Called when the window icon changes.
    virtual void OnShellWindowIconChanged(ShellWindow* shell_window) = 0;
    // Called just after a shell window was removed.
    virtual void OnShellWindowRemoved(ShellWindow* shell_window) = 0;

   protected:
    virtual ~Observer() {}
  };

  typedef std::set<ShellWindow*> ShellWindowSet;
  typedef ShellWindowSet::const_iterator const_iterator;
  typedef std::set<std::string> InspectedWindowSet;

  explicit ShellWindowRegistry(Profile* profile);
  virtual ~ShellWindowRegistry();

  // Returns the instance for the given profile, or NULL if none. This is
  // a convenience wrapper around ShellWindowRegistry::Factory::GetForProfile.
  static ShellWindowRegistry* Get(Profile* profile);

  void AddShellWindow(ShellWindow* shell_window);
  void ShellWindowIconChanged(ShellWindow* shell_window);
  void RemoveShellWindow(ShellWindow* shell_window);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns a set of windows owned by the application identified by app_id.
  ShellWindowSet GetShellWindowsForApp(const std::string& app_id) const;
  const ShellWindowSet& shell_windows() const { return shell_windows_; }

  // Helper functions to find shell windows with particular attributes.
  ShellWindow* GetShellWindowForRenderViewHost(
      content::RenderViewHost* render_view_host) const;
  ShellWindow* GetShellWindowForNativeWindow(gfx::NativeWindow window) const;
  // Returns an app window for the given app, or NULL if no shell windows are
  // open. If there is a window for the given app that is active, that one will
  // be returned, otherwise an arbitrary window will be returned.
  ShellWindow* GetCurrentShellWindowForApp(const std::string& app_id) const;
  // Returns an app window for the given app and window key, or NULL if no shell
  // window with the key are open. If there is a window for the given app and
  // key that is active, that one will be returned, otherwise an arbitrary
  // window will be returned.
  ShellWindow* GetShellWindowForAppAndKey(const std::string& app_id,
                                          const std::string& window_key) const;

  // Returns whether a ShellWindow's ID was last known to have a DevToolsAgent
  // attached to it, which should be restored during a reload of a corresponding
  // newly created |render_view_host|.
  bool HadDevToolsAttached(content::RenderViewHost* render_view_host) const;

  // Returns the shell window for |window|, looking in all profiles.
  static ShellWindow* GetShellWindowForNativeWindowAnyProfile(
      gfx::NativeWindow window);

  // Returns true if the number of shell windows registered across all profiles
  // is non-zero. |window_type_mask| is a bitwise OR filter of
  // ShellWindow::WindowType, or 0 for any window type.
  static bool IsShellWindowRegisteredInAnyProfile(int window_type_mask);

 protected:
  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  class Factory : public ProfileKeyedServiceFactory {
   public:
    static ShellWindowRegistry* GetForProfile(Profile* profile, bool create);

    static Factory* GetInstance();
   private:
    friend struct DefaultSingletonTraits<Factory>;

    Factory();
    virtual ~Factory();

    // ProfileKeyedServiceFactory
    virtual ProfileKeyedService* BuildServiceInstanceFor(
        Profile* profile) const OVERRIDE;
    virtual bool ServiceIsCreatedWithProfile() const OVERRIDE;
    virtual bool ServiceIsNULLWhileTesting() const OVERRIDE;
  };

  ShellWindowSet shell_windows_;
  InspectedWindowSet inspected_windows_;
  ObserverList<Observer> observers_;
  content::NotificationRegistrar registrar_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SHELL_WINDOW_REGISTRY_H_
