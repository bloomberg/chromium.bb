// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_APP_WINDOW_REGISTRY_H_
#define APPS_APP_WINDOW_REGISTRY_H_

#include <list>
#include <string>
#include <set>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/singleton.h"
#include "base/observer_list.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "ui/gfx/native_widget_types.h"

namespace content {
class BrowserContext;
class DevToolsAgentHost;
class RenderViewHost;
}

namespace apps {

class AppWindow;

// The AppWindowRegistry tracks the AppWindows for all platform apps for a
// particular browser context.
class AppWindowRegistry : public KeyedService {
 public:
  class Observer {
   public:
    // Called just after a app window was added.
    virtual void OnAppWindowAdded(apps::AppWindow* app_window);
    // Called when the window icon changes.
    virtual void OnAppWindowIconChanged(apps::AppWindow* app_window);
    // Called just after a app window was removed.
    virtual void OnAppWindowRemoved(apps::AppWindow* app_window);
    // Called just after a app window was hidden. This is different from
    // window visibility as a minimize does not hide a window, but does make
    // it not visible.
    virtual void OnAppWindowHidden(apps::AppWindow* app_window);
    // Called just after a app window was shown.
    virtual void OnAppWindowShown(apps::AppWindow* app_window);

   protected:
    virtual ~Observer();
  };

  typedef std::list<apps::AppWindow*> AppWindowList;
  typedef AppWindowList::const_iterator const_iterator;
  typedef std::set<std::string> InspectedWindowSet;

  explicit AppWindowRegistry(content::BrowserContext* context);
  virtual ~AppWindowRegistry();

  // Returns the instance for the given browser context, or NULL if none. This
  // is a convenience wrapper around
  // AppWindowRegistry::Factory::GetForBrowserContext().
  static AppWindowRegistry* Get(content::BrowserContext* context);

  void AddAppWindow(apps::AppWindow* app_window);
  void AppWindowIconChanged(apps::AppWindow* app_window);
  // Called by |app_window| when it is activated.
  void AppWindowActivated(apps::AppWindow* app_window);
  void AppWindowHidden(apps::AppWindow* app_window);
  void AppWindowShown(apps::AppWindow* app_window);
  void RemoveAppWindow(apps::AppWindow* app_window);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns a set of windows owned by the application identified by app_id.
  AppWindowList GetAppWindowsForApp(const std::string& app_id) const;
  const AppWindowList& app_windows() const { return app_windows_; }

  // Close all app windows associated with an app.
  void CloseAllAppWindowsForApp(const std::string& app_id);

  // Helper functions to find app windows with particular attributes.
  apps::AppWindow* GetAppWindowForRenderViewHost(
      content::RenderViewHost* render_view_host) const;
  apps::AppWindow* GetAppWindowForNativeWindow(gfx::NativeWindow window) const;
  // Returns an app window for the given app, or NULL if no app windows are
  // open. If there is a window for the given app that is active, that one will
  // be returned, otherwise an arbitrary window will be returned.
  apps::AppWindow* GetCurrentAppWindowForApp(const std::string& app_id) const;
  // Returns an app window for the given app and window key, or NULL if no app
  // window with the key are open. If there is a window for the given app and
  // key that is active, that one will be returned, otherwise an arbitrary
  // window will be returned.
  apps::AppWindow* GetAppWindowForAppAndKey(const std::string& app_id,
                                            const std::string& window_key)
      const;

  // Returns whether a AppWindow's ID was last known to have a DevToolsAgent
  // attached to it, which should be restored during a reload of a corresponding
  // newly created |render_view_host|.
  bool HadDevToolsAttached(content::RenderViewHost* render_view_host) const;

  // Returns the app window for |window|, looking in all browser contexts.
  static apps::AppWindow* GetAppWindowForNativeWindowAnyProfile(
      gfx::NativeWindow window);

  // Returns true if the number of app windows registered across all browser
  // contexts is non-zero. |window_type_mask| is a bitwise OR filter of
  // AppWindow::WindowType, or 0 for any window type.
  static bool IsAppWindowRegisteredInAnyProfile(int window_type_mask);

  // Close all app windows in all profiles.
  static void CloseAllAppWindows();

  class Factory : public BrowserContextKeyedServiceFactory {
   public:
    static AppWindowRegistry* GetForBrowserContext(
        content::BrowserContext* context,
        bool create);

    static Factory* GetInstance();

   private:
    friend struct DefaultSingletonTraits<Factory>;

    Factory();
    virtual ~Factory();

    // BrowserContextKeyedServiceFactory
    virtual KeyedService* BuildServiceInstanceFor(
        content::BrowserContext* context) const OVERRIDE;
    virtual bool ServiceIsCreatedWithBrowserContext() const OVERRIDE;
    virtual bool ServiceIsNULLWhileTesting() const OVERRIDE;
    virtual content::BrowserContext* GetBrowserContextToUse(
        content::BrowserContext* context) const OVERRIDE;
  };

 protected:
  void OnDevToolsStateChanged(content::DevToolsAgentHost*, bool attached);

 private:
  // Ensures the specified |app_window| is included in |app_windows_|.
  // Otherwise adds |app_window| to the back of |app_windows_|.
  void AddAppWindowToList(apps::AppWindow* app_window);

  // Bring |app_window| to the front of |app_windows_|. If it is not in the
  // list, add it first.
  void BringToFront(apps::AppWindow* app_window);

  content::BrowserContext* context_;
  AppWindowList app_windows_;
  InspectedWindowSet inspected_windows_;
  ObserverList<Observer> observers_;
  base::Callback<void(content::DevToolsAgentHost*, bool)> devtools_callback_;
};

}  // namespace apps

#endif  // APPS_APP_WINDOW_REGISTRY_H_
