// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_TAB_HELPER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_TAB_HELPER_H_
#pragma once

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/extensions/app_notify_channel_setup.h"
#include "chrome/browser/extensions/extension_function_dispatcher.h"
#include "chrome/browser/extensions/image_loading_tracker.h"
#include "chrome/browser/extensions/webstore_inline_installer.h"
#include "chrome/common/web_apps.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/skia/include/core/SkBitmap.h"

class Extension;
class ExtensionTabHelperDelegate;
class TabContentsWrapper;
struct WebApplicationInfo;

namespace content {
struct LoadCommittedDetails;
}

namespace extensions {
class ActionBoxController;
class ScriptExecutor;
}

// Per-tab extension helper. Also handles non-extension apps.
class ExtensionTabHelper
    : public content::WebContentsObserver,
      public ExtensionFunctionDispatcher::Delegate,
      public ImageLoadingTracker::Observer,
      public WebstoreInlineInstaller::Delegate,
      public AppNotifyChannelSetup::Delegate,
      public base::SupportsWeakPtr<ExtensionTabHelper> {
 public:
  class Observer {
   public:
    // Called when the page action state (such as visibility, title) changes.
    virtual void OnPageActionStateChanged() = 0;

   protected:
    virtual ~Observer() {}
  };

  explicit ExtensionTabHelper(TabContentsWrapper* wrapper);
  virtual ~ExtensionTabHelper();

  // Copies the internal state from another ExtensionTabHelper.
  void CopyStateFrom(const ExtensionTabHelper& source);

  ExtensionTabHelperDelegate* delegate() const { return delegate_; }
  void set_delegate(ExtensionTabHelperDelegate* d) { delegate_ = d; }

  // Call this after updating a page action to notify clients about the changes.
  void PageActionStateChanged();

  // Requests application info for the specified page. This is an asynchronous
  // request. The delegate is notified by way of OnDidGetApplicationInfo when
  // the data is available.
  void GetApplicationInfo(int32 page_id);

  // Observer management.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

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

  TabContentsWrapper* tab_contents_wrapper() {
    return wrapper_;
  }

  content::WebContents* web_contents() const {
    return content::WebContentsObserver::web_contents();
  }

  extensions::ScriptExecutor* script_executor() {
    return script_executor_.get();
  }

  extensions::ActionBoxController* action_box_controller() {
    return action_box_controller_.get();
  }

  // Sets a non-extension app icon associated with WebContents and fires an
  // INVALIDATE_TYPE_TITLE navigation state change to trigger repaint of title.
  void SetAppIcon(const SkBitmap& app_icon);

 private:
  // content::WebContentsObserver overrides.
  virtual void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // ExtensionFunctionDispatcher::Delegate overrides.
  virtual ExtensionWindowController* GetExtensionWindowController()
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

  // App extensions related methods:

  // Resets app_icon_ and if |extension| is non-null creates a new
  // ImageLoadingTracker to load the extension's image.
  void UpdateExtensionAppIcon(const Extension* extension);

  const Extension* GetExtension(const std::string& extension_app_id);

  // ImageLoadingTracker::Observer.
  virtual void OnImageLoaded(const gfx::Image& image,
                             const std::string& extension_id,
                             int index) OVERRIDE;

  // WebstoreInlineInstaller::Delegate.
  virtual void OnInlineInstallSuccess(int install_id,
                                      int return_route_id) OVERRIDE;
  virtual void OnInlineInstallFailure(int install_id,
                                      int return_route_id,
                                      const std::string& error) OVERRIDE;

  // AppNotifyChannelSetup::Delegate.
  virtual void AppNotifyChannelSetupComplete(
      const std::string& channel_id,
      const std::string& error,
      const AppNotifyChannelSetup* setup) OVERRIDE;

  // Data for app extensions ---------------------------------------------------

  // Delegate for notifying our owner about stuff. Not owned by us.
  ExtensionTabHelperDelegate* delegate_;

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

  TabContentsWrapper* wrapper_;

  scoped_ptr<extensions::ScriptExecutor> script_executor_;

  scoped_ptr<extensions::ActionBoxController> action_box_controller_;

  ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionTabHelper);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_TAB_HELPER_H_
