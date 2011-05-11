// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_TAB_HELPER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_TAB_HELPER_H_
#pragma once

#include "content/browser/tab_contents/tab_contents_observer.h"
#include "chrome/browser/extensions/image_loading_tracker.h"
#include "chrome/common/web_apps.h"
#include "third_party/skia/include/core/SkBitmap.h"

class Extension;
class TabContentsWrapper;
struct WebApplicationInfo;

// Per-tab extension helper. Also handles non-extension apps.
class ExtensionTabHelper : public TabContentsObserver,
                           public ImageLoadingTracker::Observer {
 public:
  explicit ExtensionTabHelper(TabContentsWrapper* wrapper);
  virtual ~ExtensionTabHelper();

  // Copies the internal state from another ExtensionTabHelper.
  void CopyStateFrom(const ExtensionTabHelper& source);

  // Call this after updating a page action to notify clients about the changes.
  void PageActionStateChanged();

  // Requests application info for the specified page. This is an asynchronous
  // request. The delegate is notified by way of OnDidGetApplicationInfo when
  // the data is available.
  void GetApplicationInfo(int32 page_id);

  // App extensions ------------------------------------------------------------

  // Sets the extension denoting this as an app. If |extension| is non-null this
  // tab becomes an app-tab. TabContents does not listen for unload events for
  // the extension. It's up to consumers of TabContents to do that.
  //
  // NOTE: this should only be manipulated before the tab is added to a browser.
  // TODO(sky): resolve if this is the right way to identify an app tab. If it
  // is, than this should be passed in the constructor.
  void SetExtensionApp(const Extension* extension);

  // Convenience for setting the app extension by id. This does nothing if
  // |extension_app_id| is empty, or an extension can't be found given the
  // specified id.
  void SetExtensionAppById(const std::string& extension_app_id);

  const Extension* extension_app() const { return extension_app_; }
  bool is_app() const { return extension_app_ != NULL; }
  const WebApplicationInfo& web_app_info() const {
    return web_app_info_;
  }

  // If an app extension has been explicitly set for this TabContents its icon
  // is returned.
  //
  // NOTE: the returned icon is larger than 16x16 (its size is
  // Extension::EXTENSION_ICON_SMALLISH).
  SkBitmap* GetExtensionAppIcon();

  TabContents* tab_contents() const {
      return TabContentsObserver::tab_contents();
  }

  // Sets a non-extension app icon associated with TabContents and fires an
  // INVALIDATE_TITLE navigation state change to trigger repaint of title.
  void SetAppIcon(const SkBitmap& app_icon);

 private:
  // TabContentsObserver overrides.
  virtual void DidNavigateMainFramePostCommit(
      const NavigationController::LoadCommittedDetails& details,
      const ViewHostMsg_FrameNavigate_Params& params) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message);

  // Message handlers.
  void OnDidGetApplicationInfo(int32 page_id, const WebApplicationInfo& info);
  void OnInstallApplication(const WebApplicationInfo& info);

  // App extensions related methods:

  // Resets app_icon_ and if |extension| is non-null creates a new
  // ImageLoadingTracker to load the extension's image.
  void UpdateExtensionAppIcon(const Extension* extension);

  // ImageLoadingTracker::Observer.
  virtual void OnImageLoaded(SkBitmap* image, const ExtensionResource& resource,
                             int index);

  // Data for app extensions ---------------------------------------------------

  // If non-null this tab is an app tab and this is the extension the tab was
  // created for.
  const Extension* extension_app_;

  // Icon for extension_app_ (if non-null) or a manually-set icon for
  // non-extension apps.
  SkBitmap extension_app_icon_;

  // Used for loading extension_app_icon_.
  scoped_ptr<ImageLoadingTracker> extension_app_image_loader_;

  // Cached web app info data.
  WebApplicationInfo web_app_info_;

  TabContentsWrapper* wrapper_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionTabHelper);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_TAB_HELPER_H_
