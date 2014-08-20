// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_CUSTOM_LAUNCHER_PAGE_CONTENTS_H_
#define APPS_CUSTOM_LAUNCHER_PAGE_CONTENTS_H_

#include "base/memory/scoped_ptr.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/browser/extension_function_dispatcher.h"

class GURL;

namespace content {
class BrowserContext;
}

namespace extensions {
class AppDelegate;
class AppWebContentsHelper;
}

namespace apps {

// Manages the web contents for extension-hosted launcher pages. The
// implementation for this class should create and maintain the WebContents for
// the page, and handle any message passing between the web contents and the
// extension system.
class CustomLauncherPageContents
    : public content::WebContentsDelegate,
      public content::WebContentsObserver,
      public extensions::ExtensionFunctionDispatcher::Delegate {
 public:
  CustomLauncherPageContents(scoped_ptr<extensions::AppDelegate> app_delegate,
                             const std::string& extension_id);
  virtual ~CustomLauncherPageContents();

  // Called to initialize and load the WebContents.
  void Initialize(content::BrowserContext* context, const GURL& url);

  content::WebContents* web_contents() const { return web_contents_.get(); }

  // content::WebContentsDelegate overrides:
  virtual content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) OVERRIDE;
  virtual void AddNewContents(content::WebContents* source,
                              content::WebContents* new_contents,
                              WindowOpenDisposition disposition,
                              const gfx::Rect& initial_pos,
                              bool user_gesture,
                              bool* was_blocked) OVERRIDE;
  virtual bool IsPopupOrPanel(
      const content::WebContents* source) const OVERRIDE;
  virtual bool ShouldSuppressDialogs() OVERRIDE;
  virtual bool PreHandleGestureEvent(
      content::WebContents* source,
      const blink::WebGestureEvent& event) OVERRIDE;
  virtual content::ColorChooser* OpenColorChooser(
      content::WebContents* web_contents,
      SkColor color,
      const std::vector<content::ColorSuggestion>& suggestions) OVERRIDE;
  virtual void RunFileChooser(
      content::WebContents* tab,
      const content::FileChooserParams& params) OVERRIDE;
  virtual void RequestToLockMouse(content::WebContents* web_contents,
                                  bool user_gesture,
                                  bool last_unlocked_by_target) OVERRIDE;
  virtual void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      const content::MediaResponseCallback& callback) OVERRIDE;

 private:
  // content::WebContentsObserver overrides:
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // extensions::ExtensionFunctionDispatcher::Delegate overrides:
  virtual extensions::WindowController* GetExtensionWindowController()
      const OVERRIDE;
  virtual content::WebContents* GetAssociatedWebContents() const OVERRIDE;

  void OnRequest(const ExtensionHostMsg_Request_Params& params);

  scoped_ptr<content::WebContents> web_contents_;
  scoped_ptr<extensions::ExtensionFunctionDispatcher>
      extension_function_dispatcher_;
  scoped_ptr<extensions::AppDelegate> app_delegate_;
  scoped_ptr<extensions::AppWebContentsHelper> helper_;

  std::string extension_id_;

  DISALLOW_COPY_AND_ASSIGN(CustomLauncherPageContents);
};

}  // namespace apps

#endif  // APPS_CUSTOM_LAUNCHER_PAGE_CONTENTS_H_
