// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_TAB_HELPER_H_
#define CHROME_BROWSER_EXTENSIONS_TAB_HELPER_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/extensions/active_tab_permission_granter.h"
#include "chrome/common/extensions/webstore_install_result.h"
#include "chrome/common/web_application_info.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/common/stack_frame.h"
#include "third_party/skia/include/core/SkBitmap.h"

class FaviconDownloader;

namespace content {
struct LoadCommittedDetails;
class RenderFrameHost;
}

namespace gfx {
class Image;
}

namespace extensions {
class BookmarkAppHelper;
class Extension;
class LocationBarController;
class ScriptExecutor;
class WebstoreInlineInstallerFactory;

// Per-tab extension helper. Also handles non-extension apps.
class TabHelper : public content::WebContentsObserver,
                  public extensions::ExtensionFunctionDispatcher::Delegate,
                  public base::SupportsWeakPtr<TabHelper>,
                  public content::NotificationObserver,
                  public content::WebContentsUserData<TabHelper> {
 public:
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
  void CreateHostedAppFromWebContents();
  bool CanCreateApplicationShortcuts() const;
  bool CanCreateBookmarkApp() const;

  void UpdateShortcutOnLoadComplete() {
    update_shortcut_on_load_complete_ = true;
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
  // extension_misc::EXTENSION_ICON_SMALLISH).
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

  // Sets a non-extension app icon associated with WebContents and fires an
  // INVALIDATE_TYPE_TITLE navigation state change to trigger repaint of title.
  void SetAppIcon(const SkBitmap& app_icon);

  // Sets the factory used to create inline webstore item installers.
  // Used for testing. Takes ownership of the factory instance.
  void SetWebstoreInlineInstallerFactoryForTests(
      WebstoreInlineInstallerFactory* factory);

 private:
  // Different types of action when web app info is available.
  // OnDidGetApplicationInfo uses this to dispatch calls.
  enum WebAppAction {
    NONE,               // No action at all.
    CREATE_SHORTCUT,    // Bring up create application shortcut dialog.
    CREATE_HOSTED_APP,  // Create and install a hosted app.
    UPDATE_SHORTCUT     // Update icon for app shortcut.
  };

  explicit TabHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<TabHelper>;

  // Displays UI for completion of creating a bookmark hosted app.
  void FinishCreateBookmarkApp(const extensions::Extension* extension,
                               const WebApplicationInfo& web_app_info);

  // content::WebContentsObserver overrides.
  virtual void RenderViewCreated(
      content::RenderViewHost* render_view_host) OVERRIDE;
  virtual void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual bool OnMessageReceived(
      const IPC::Message& message,
      content::RenderFrameHost* render_frame_host) OVERRIDE;
  virtual void DidCloneToNewWebContents(
      content::WebContents* old_web_contents,
      content::WebContents* new_web_contents) OVERRIDE;

  // extensions::ExtensionFunctionDispatcher::Delegate overrides.
  virtual extensions::WindowController* GetExtensionWindowController()
      const OVERRIDE;
  virtual content::WebContents* GetAssociatedWebContents() const OVERRIDE;

  // Message handlers.
  void OnDidGetApplicationInfo(const WebApplicationInfo& info);
  void OnInlineWebstoreInstall(int install_id,
                               int return_route_id,
                               const std::string& webstore_item_id,
                               const GURL& requestor_url,
                               int listeners_mask);
  void OnGetAppInstallState(const GURL& requestor_url,
                            int return_route_id,
                            int callback_id);
  void OnRequest(const ExtensionHostMsg_Request_Params& params);
  void OnContentScriptsExecuting(
      const ScriptExecutionObserver::ExecutingScriptsMap& extension_ids,
      const GURL& on_url);
  void OnWatchedPageChange(const std::vector<std::string>& css_selectors);
  void OnDetailedConsoleMessageAdded(const base::string16& message,
                                     const base::string16& source,
                                     const StackTrace& stack_trace,
                                     int32 severity_level);

  // App extensions related methods:

  // Resets app_icon_ and if |extension| is non-null uses ImageLoader to load
  // the extension's image asynchronously.
  void UpdateExtensionAppIcon(const Extension* extension);

  const Extension* GetExtension(const std::string& extension_app_id);

  void OnImageLoaded(const gfx::Image& image);

  // WebstoreStandaloneInstaller::Callback.
  virtual void OnInlineInstallComplete(int install_id,
                                       int return_route_id,
                                       bool success,
                                       const std::string& error,
                                       webstore_install::Result result);

  // content::NotificationObserver.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Requests application info for the specified page. This is an asynchronous
  // request. The delegate is notified by way of OnDidGetApplicationInfo when
  // the data is available.
  void GetApplicationInfo(WebAppAction action);

  // Sends our tab ID to |render_view_host|.
  void SetTabId(content::RenderViewHost* render_view_host);

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
  extensions::ExtensionFunctionDispatcher extension_function_dispatcher_;

  // Cached web app info data.
  WebApplicationInfo web_app_info_;

  // Which deferred action to perform when OnDidGetApplicationInfo is notified
  // from a WebContents.
  WebAppAction pending_web_app_action_;

  // Which page id was active when the GetApplicationInfo request was sent, for
  // verification when the reply returns.
  int32 last_committed_page_id_;

  // Whether to trigger an update when the page load completes.
  bool update_shortcut_on_load_complete_;

  content::NotificationRegistrar registrar_;

  scoped_ptr<ScriptExecutor> script_executor_;

  scoped_ptr<LocationBarController> location_bar_controller_;

  scoped_ptr<ActiveTabPermissionGranter> active_tab_permission_granter_;

  scoped_ptr<BookmarkAppHelper> bookmark_app_helper_;

  Profile* profile_;

  // Vend weak pointers that can be invalidated to stop in-progress loads.
  base::WeakPtrFactory<TabHelper> image_loader_ptr_factory_;

  // Creates WebstoreInlineInstaller instances for inline install triggers.
  scoped_ptr<WebstoreInlineInstallerFactory> webstore_inline_installer_factory_;

  DISALLOW_COPY_AND_ASSIGN(TabHelper);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_TAB_HELPER_H_
