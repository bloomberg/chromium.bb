// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_HOST_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_HOST_H_
#pragma once

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/perftimer.h"
#include "chrome/browser/extensions/extension_function_dispatcher.h"
#include "content/public/browser/javascript_dialogs.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/view_type.h"

#if defined(TOOLKIT_VIEWS)
#include "chrome/browser/ui/views/extensions/extension_view.h"
#elif defined(OS_MACOSX)
#include "chrome/browser/ui/cocoa/extensions/extension_view_mac.h"
#elif defined(TOOLKIT_GTK)
#include "chrome/browser/ui/gtk/extensions/extension_view_gtk.h"
#elif defined(OS_ANDROID)
#include "chrome/browser/ui/android/extensions/extension_view_android.h"
#endif

class Browser;
class Extension;
class ExtensionWindowController;
class PrefsTabHelper;

namespace content {
class RenderProcessHost;
class RenderWidgetHostView;
class SiteInstance;
}

// This class is the browser component of an extension component's RenderView.
// It handles setting up the renderer process, if needed, with special
// privileges available to extensions.  It may have a view to be shown in the
// browser UI, or it may be hidden.
class ExtensionHost : public content::WebContentsDelegate,
                      public content::WebContentsObserver,
                      public ExtensionFunctionDispatcher::Delegate,
                      public content::NotificationObserver {
 public:
  class ProcessCreationQueue;

#if defined(TOOLKIT_VIEWS)
  typedef ExtensionView PlatformExtensionView;
#elif defined(OS_MACOSX)
  typedef ExtensionViewMac PlatformExtensionView;
#elif defined(TOOLKIT_GTK)
  typedef ExtensionViewGtk PlatformExtensionView;
#elif defined(OS_ANDROID)
  // Android does not support extensions.
  typedef ExtensionViewAndroid PlatformExtensionView;
#endif

  ExtensionHost(const Extension* extension,
                content::SiteInstance* site_instance,
                const GURL& url, content::ViewType host_type);
  virtual ~ExtensionHost();

#if defined(TOOLKIT_VIEWS)
  void set_view(PlatformExtensionView* view) { view_.reset(view); }
#endif

  const PlatformExtensionView* view() const {
#if defined(OS_ANDROID)
    NOTREACHED();
#endif
    return view_.get();
  }

  PlatformExtensionView* view() {
#if defined(OS_ANDROID)
    NOTREACHED();
#endif
    return view_.get();
  }

  // Create an ExtensionView and tie it to this host and |browser|.  Note NULL
  // is a valid argument for |browser|.  Extension views may be bound to
  // tab-contents hosted in ExternalTabContainer objects, which do not
  // instantiate Browser objects.
  void CreateView(Browser* browser);

  // Helper variant of the above for cases where no Browser is present.
  void CreateViewWithoutBrowser();

  const Extension* extension() const { return extension_; }
  const std::string& extension_id() const { return extension_id_; }
  content::WebContents* host_contents() const { return host_contents_.get(); }
  content::RenderViewHost* render_view_host() const;
  content::RenderProcessHost* render_process_host() const;
  bool did_stop_loading() const { return did_stop_loading_; }
  bool document_element_available() const {
    return document_element_available_;
  }

  Profile* profile() const { return profile_; }

  content::ViewType extension_host_type() const { return extension_host_type_; }
  const GURL& GetURL() const;

  // ExtensionFunctionDispatcher::Delegate
  virtual content::WebContents* GetAssociatedWebContents() const OVERRIDE;
  void SetAssociatedWebContents(content::WebContents* web_contents);

  // Returns true if the render view is initialized and didn't crash.
  bool IsRenderViewLive() const;

  // Prepares to initializes our RenderViewHost by creating its RenderView and
  // navigating to this host's url. Uses host_view for the RenderViewHost's view
  // (can be NULL). This happens delayed to avoid locking the UI.
  void CreateRenderViewSoon();

  // Insert a default style sheet for Extension Infobars.
  void InsertInfobarCSS();

  // Notifications from the JavaScriptDialogCreator when a dialog is being
  // opened/closed.
  void WillRunJavaScriptDialog();
  void DidCloseJavaScriptDialog();

  // content::WebContentsObserver
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void RenderViewCreated(
      content::RenderViewHost* render_view_host) OVERRIDE;
  virtual void RenderViewDeleted(
      content::RenderViewHost* render_view_host) OVERRIDE;
  virtual void RenderViewReady() OVERRIDE;
  virtual void RenderViewGone(base::TerminationStatus status) OVERRIDE;
  virtual void DocumentAvailableInMainFrame() OVERRIDE;
  virtual void DidStopLoading() OVERRIDE;

  // content::WebContentsDelegate
  virtual content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) OVERRIDE;
  virtual bool PreHandleKeyboardEvent(const NativeWebKeyboardEvent& event,
                                      bool* is_keyboard_shortcut) OVERRIDE;
  virtual void HandleKeyboardEvent(const NativeWebKeyboardEvent& event)
      OVERRIDE;
  virtual void ResizeDueToAutoResize(content::WebContents* source,
                                     const gfx::Size& new_size) OVERRIDE;
  virtual content::JavaScriptDialogCreator* GetJavaScriptDialogCreator()
      OVERRIDE;
  virtual void RunFileChooser(
      content::WebContents* tab,
      const content::FileChooserParams& params) OVERRIDE;
  virtual void AddNewContents(content::WebContents* source,
                              content::WebContents* new_contents,
                              WindowOpenDisposition disposition,
                              const gfx::Rect& initial_pos,
                              bool user_gesture) OVERRIDE;
  virtual void CloseContents(content::WebContents* contents) OVERRIDE;
  virtual bool ShouldSuppressDialogs() OVERRIDE;

  // content::NotificationObserver
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  friend class ProcessCreationQueue;

  // Actually create the RenderView for this host. See CreateRenderViewSoon.
  void CreateRenderViewNow();

  // Navigates to the initial page.
  void LoadInitialURL();

  // Closes this host (results in deletion).
  void Close();

  // ExtensionFunctionDispatcher::Delegate
  virtual ExtensionWindowController* GetExtensionWindowController()
      const OVERRIDE;

  // Message handlers.
  void OnRequest(const ExtensionHostMsg_Request_Params& params);
  void OnEventAck();
  void OnIncrementLazyKeepaliveCount();
  void OnDecrementLazyKeepaliveCount();

  // Handles keyboard events that were not handled by HandleKeyboardEvent().
  // Platform specific implementation may override this method to handle the
  // event in platform specific way.
  virtual void UnhandledKeyboardEvent(const NativeWebKeyboardEvent& event) {}

  // Returns true if we're hosting a background page.
  // This isn't valid until CreateRenderView is called.
  bool is_background_page() const { return !view(); }

  // The extension that we're hosting in this view.
  const Extension* extension_;

  // Id of extension that we're hosting in this view.
  const std::string extension_id_;

  // The profile that this host is tied to.
  Profile* profile_;

  // Optional view that shows the rendered content in the UI.
  scoped_ptr<PlatformExtensionView> view_;

  // Used to create dialog boxes.
  // It must outlive host_contents_ as host_contents_ will access it
  // during destruction.
  scoped_ptr<content::JavaScriptDialogCreator> dialog_creator_;

  // The host for our HTML content.
  scoped_ptr<content::WebContents> host_contents_;

  // Helpers that take care of extra functionality for our host contents.
  scoped_ptr<PrefsTabHelper> prefs_tab_helper_;

  // A weak pointer to the current or pending RenderViewHost. We don't access
  // this through the host_contents because we want to deal with the pending
  // host, so we can send messages to it before it finishes loading.
  content::RenderViewHost* render_view_host_;

  // Whether the RenderWidget has reported that it has stopped loading.
  bool did_stop_loading_;

  // True if the main frame has finished parsing.
  bool document_element_available_;

  // The original URL of the page being hosted.
  GURL initial_url_;

  content::NotificationRegistrar registrar_;

  ExtensionFunctionDispatcher extension_function_dispatcher_;

  // The type of view being hosted.
  content::ViewType extension_host_type_;

  // The relevant WebContents associated with this ExtensionHost, if any.
  content::WebContents* associated_web_contents_;

  // Used to measure how long it's been since the host was created.
  PerfTimer since_created_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionHost);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_HOST_H_
