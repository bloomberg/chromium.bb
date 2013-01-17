// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_TAB_HELPER_H_
#define CHROME_BROWSER_EXTENSIONS_TAB_HELPER_H_

#include <map>
#include <string>
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/extensions/active_tab_permission_granter.h"
#include "chrome/browser/extensions/app_notify_channel_setup.h"
#include "chrome/browser/extensions/extension_function_dispatcher.h"
#include "chrome/browser/extensions/image_loading_tracker.h"
#include "chrome/common/web_apps.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "third_party/skia/include/core/SkBitmap.h"

struct WebApplicationInfo;

namespace content {
struct LoadCommittedDetails;
}

namespace extensions {
class Extension;
class LocationBarController;
class ScriptBadgeController;
class ScriptBubbleController;
class ScriptExecutor;

// Per-tab extension helper. Also handles non-extension apps.
class TabHelper : public content::WebContentsObserver,
                  public ExtensionFunctionDispatcher::Delegate,
                  public ImageLoadingTracker::Observer,
                  public AppNotifyChannelSetup::Delegate,
                  public base::SupportsWeakPtr<TabHelper>,
                  public content::NotificationObserver,
                  public content::WebContentsUserData<TabHelper> {
 public:
  // Different types of action when web app info is available.
  // OnDidGetApplicationInfo uses this to dispatch calls.
  enum WebAppAction {
    NONE,             // No action at all.
    CREATE_SHORTCUT,  // Bring up create application shortcut dialog.
    UPDATE_SHORTCUT   // Update icon for app shortcut.
  };

  // Observer base class for classes that need to be notified when content
  // scripts and/or tabs.executeScript calls run on a page.
  class ScriptExecutionObserver {
   public:
    // Map of extensions IDs to the executing script paths.
    typedef std::map<std::string, std::set<std::string> > ExecutingScriptsMap;

    // Automatically observes and unobserves |tab_helper| on construction
    // and destruction. |tab_helper| must outlive |this|.
    explicit ScriptExecutionObserver(TabHelper* tab_helper);
    ScriptExecutionObserver();

    // Called when script(s) have executed on a page.
    //
    // |executing_scripts_map| contains all extensions that are executing
    // scripts, mapped to the paths for those scripts. This may be an empty set
    // if the script has no path associated with it (e.g. in the case of
    // tabs.executeScript).
    virtual void OnScriptsExecuted(
        const content::WebContents* web_contents,
        const ExecutingScriptsMap& executing_scripts_map,
        int32 on_page_id,
        const GURL& on_url) = 0;

   protected:
    virtual ~ScriptExecutionObserver();

    TabHelper* tab_helper_;
  };

  virtual ~TabHelper();

  void AddScriptExecutionObserver(ScriptExecutionObserver* observer) {
    script_execution_observers_.AddObserver(observer);
  }

  void RemoveScriptExecutionObserver(ScriptExecutionObserver* observer) {
    script_execution_observers_.RemoveObserver(observer);
  }

  void CreateApplicationShortcuts();
  bool CanCreateApplicationShortcuts() const;

  void set_pending_web_app_action(WebAppAction action) {
    pending_web_app_action_ = action;
  }

  // App extensions ------------------------------------------------------------

  // Sets the extension denoting this as an app. If |extension| is non-null this
  // tab becomes an app-tab. WebContents does not listen for unload events for
  // the extension. It's up to consumers of WebContents to do that.
  //
  // NOTE: this should only be manipulated before the tab is added to a browser.
  // TODO(sky): resolve if this is the right way to identify an app tab. If it
  // is, than this should be passed in the constructor.
  void SetExtensionApp(const Extension* extension);

  // Convenience for setting the app extension by id. This does nothing if
  // |extension_app_id| is empty, or an extension can't be found given the
  // specified id.
  void SetExtensionAppById(const std::string& extension_app_id);

  // Set just the app icon, used by panels created by an extension.
  void SetExtensionAppIconById(const std::string& extension_app_id);

  const Extension* extension_app() const { return extension_app_; }
  bool is_app() const { return extension_app_ != NULL; }
  const WebApplicationInfo& web_app_info() const {
    return web_app_info_;
  }

  // If an app extension has been explicitly set for this WebContents its icon
  // is returned.
  //
  // NOTE: the returned icon is larger than 16x16 (its size is
  // Extension::EXTENSION_ICON_SMALLISH).
  SkBitmap* GetExtensionAppIcon();

  content::WebContents* web_contents() const {
    return content::WebContentsObserver::web_contents();
  }

  ScriptExecutor* script_executor() {
    return script_executor_.get();
  }

  LocationBarController* location_bar_controller() {
    return location_bar_controller_.get();
  }

  ActiveTabPermissionGranter* active_tab_permission_granter() {
    return active_tab_permission_granter_.get();
  }

  ScriptBubbleController* script_bubble_controller() {
    return script_bubble_controller_.get();
  }

  // Sets a non-extension app icon associated with WebContents and fires an
  // INVALIDATE_TYPE_TITLE navigation state change to trigger repaint of title.
  void SetAppIcon(const SkBitmap& app_icon);

 private:
  explicit TabHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<TabHelper>;

  // content::WebContentsObserver overrides.
  virtual void RenderViewCreated(
      content::RenderViewHost* render_view_host) OVERRIDE;
  virtual void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void DidCloneToNewWebContents(
      content::WebContents* old_web_contents,
      content::WebContents* new_web_contents) OVERRIDE;

  // ExtensionFunctionDispatcher::Delegate overrides.
  virtual extensions::WindowController* GetExtensionWindowController()
      const OVERRIDE;
  virtual content::WebContents* GetAssociatedWebContents() const OVERRIDE;

  // Message handlers.
  void OnDidGetApplicationInfo(int32 page_id, const WebApplicationInfo& info);
  void OnInstallApplication(const WebApplicationInfo& info);
  void OnInlineWebstoreInstall(int install_id,
                               int return_route_id,
                               const std::string& webstore_item_id,
                               const GURL& requestor_url);
  void OnGetAppNotifyChannel(const GURL& requestor_url,
                             const std::string& client_id,
                             int return_route_id,
                             int callback_id);
  void OnGetAppInstallState(const GURL& requestor_url,
                            int return_route_id,
                            int callback_id);
  void OnRequest(const ExtensionHostMsg_Request_Params& params);
  void OnContentScriptsExecuting(
      const ScriptExecutionObserver::ExecutingScriptsMap& extension_ids,
      int32 page_id,
      const GURL& on_url);

  // App extensions related methods:

  // Resets app_icon_ and if |extension| is non-null creates a new
  // ImageLoadingTracker to load the extension's image.
  void UpdateExtensionAppIcon(const Extension* extension);

  const Extension* GetExtension(
      const std::string& extension_app_id);

  // ImageLoadingTracker::Observer.
  virtual void OnImageLoaded(const gfx::Image& image,
                             const std::string& extension_id,
                             int index) OVERRIDE;

  // WebstoreStandaloneInstaller::Callback.
  virtual void OnInlineInstallComplete(int install_id,
                                       int return_route_id,
                                       bool success,
                                       const std::string& error);

  // AppNotifyChannelSetup::Delegate.
  virtual void AppNotifyChannelSetupComplete(
      const std::string& channel_id,
      const std::string& error,
      const AppNotifyChannelSetup* setup) OVERRIDE;

  // content::NotificationObserver.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Requests application info for the specified page. This is an asynchronous
  // request. The delegate is notified by way of OnDidGetApplicationInfo when
  // the data is available.
  void GetApplicationInfo(int32 page_id);

  // Data for app extensions ---------------------------------------------------

  // Our content script observers. Declare at top so that it will outlive all
  // other members, since they might add themselves as observers.
  ObserverList<ScriptExecutionObserver> script_execution_observers_;

  // If non-null this tab is an app tab and this is the extension the tab was
  // created for.
  const Extension* extension_app_;

  // Icon for extension_app_ (if non-null) or a manually-set icon for
  // non-extension apps.
  SkBitmap extension_app_icon_;

  // Process any extension messages coming from the tab.
  ExtensionFunctionDispatcher extension_function_dispatcher_;

  // Used for loading extension_app_icon_.
  scoped_ptr<ImageLoadingTracker> extension_app_image_loader_;

  // Cached web app info data.
  WebApplicationInfo web_app_info_;

  // Which deferred action to perform when OnDidGetApplicationInfo is notified
  // from a WebContents.
  WebAppAction pending_web_app_action_;

  content::NotificationRegistrar registrar_;

  scoped_ptr<ScriptExecutor> script_executor_;

  scoped_ptr<LocationBarController> location_bar_controller_;

  scoped_ptr<ActiveTabPermissionGranter> active_tab_permission_granter_;

  scoped_ptr<ScriptBubbleController> script_bubble_controller_;

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(TabHelper);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_TAB_HELPER_H_
