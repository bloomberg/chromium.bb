// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_HOST_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_HOST_H_
#pragma once

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/perftimer.h"
#include "chrome/browser/extensions/extension_function_dispatcher.h"
#include "content/browser/javascript_dialogs.h"
#include "content/browser/tab_contents/tab_contents_delegate.h"
#include "content/browser/tab_contents/tab_contents_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/common/view_types.h"

#if defined(TOOLKIT_VIEWS)
#include "chrome/browser/ui/views/extensions/extension_view.h"
#elif defined(OS_MACOSX)
#include "chrome/browser/ui/cocoa/extensions/extension_view_mac.h"
#elif defined(TOOLKIT_GTK)
#include "chrome/browser/ui/gtk/extensions/extension_view_gtk.h"
#endif

class Browser;
class Extension;
class RenderWidgetHostView;
class TabContents;
struct ViewHostMsg_RunFileChooser_Params;
struct WebPreferences;

namespace content {
class RenderProcessHost;
}

// This class is the browser component of an extension component's RenderView.
// It handles setting up the renderer process, if needed, with special
// privileges available to extensions.  It may have a view to be shown in the
// browser UI, or it may be hidden.
class ExtensionHost : public TabContentsDelegate,
                      public TabContentsObserver,
                      public ExtensionFunctionDispatcher::Delegate,
                      public content::NotificationObserver {
 public:
  class ProcessCreationQueue;

  ExtensionHost(const Extension* extension, SiteInstance* site_instance,
                const GURL& url, content::ViewType host_type);
  virtual ~ExtensionHost();

#if defined(TOOLKIT_VIEWS)
  void set_view(ExtensionView* view) { view_.reset(view); }
  const ExtensionView* view() const { return view_.get(); }
  ExtensionView* view() { return view_.get(); }
#elif defined(OS_MACOSX)
  const ExtensionViewMac* view() const { return view_.get(); }
  ExtensionViewMac* view() { return view_.get(); }
#elif defined(TOOLKIT_GTK)
  const ExtensionViewGtk* view() const { return view_.get(); }
  ExtensionViewGtk* view() { return view_.get(); }
#endif

  // Create an ExtensionView and tie it to this host and |browser|.  Note NULL
  // is a valid argument for |browser|.  Extension views may be bound to
  // tab-contents hosted in ExternalTabContainer objects, which do not
  // instantiate Browser objects.
  void CreateView(Browser* browser);

  const Extension* extension() const { return extension_; }
  const std::string& extension_id() const { return extension_id_; }
  TabContents* host_contents() const { return host_contents_.get(); }
  RenderViewHost* render_view_host() const;
  content::RenderProcessHost* render_process_host() const;
  bool did_stop_loading() const { return did_stop_loading_; }
  bool document_element_available() const {
    return document_element_available_;
  }

  Profile* profile() const { return profile_; }

  content::ViewType extension_host_type() const { return extension_host_type_; }
  const GURL& GetURL() const;

  // ExtensionFunctionDispatcher::Delegate
  virtual TabContents* GetAssociatedTabContents() const OVERRIDE;
  void set_associated_tab_contents(TabContents* associated_tab_contents) {
    associated_tab_contents_ = associated_tab_contents;
  }

  // Returns true if the render view is initialized and didn't crash.
  bool IsRenderViewLive() const;

  // Prepares to initializes our RenderViewHost by creating its RenderView and
  // navigating to this host's url. Uses host_view for the RenderViewHost's view
  // (can be NULL). This happens delayed to avoid locking the UI.
  void CreateRenderViewSoon();

  // Insert a default style sheet for Extension Infobars.
  void InsertInfobarCSS();

  // Tell the renderer not to draw scrollbars on windows smaller than
  // |size_limit| in both width and height.
  void DisableScrollbarsForSmallWindows(const gfx::Size& size_limit);

  // TabContentsObserver
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void RenderViewCreated(RenderViewHost* render_view_host) OVERRIDE;
  virtual void RenderViewReady() OVERRIDE;
  virtual void RenderViewGone(base::TerminationStatus status) OVERRIDE;
  virtual void DocumentAvailableInMainFrame() OVERRIDE;
  virtual void DidStopLoading() OVERRIDE;

  // TabContentsDelegate
  virtual bool PreHandleKeyboardEvent(const NativeWebKeyboardEvent& event,
                                      bool* is_keyboard_shortcut) OVERRIDE;
  virtual void HandleKeyboardEvent(const NativeWebKeyboardEvent& event)
      OVERRIDE;
  virtual void UpdatePreferredSize(TabContents* source,
                                   const gfx::Size& pref_size) OVERRIDE;
  virtual content::JavaScriptDialogCreator* GetJavaScriptDialogCreator()
      OVERRIDE;
  virtual void AddNewContents(TabContents* source,
                              TabContents* new_contents,
                              WindowOpenDisposition disposition,
                              const gfx::Rect& initial_pos,
                              bool user_gesture) OVERRIDE;
  virtual void CloseContents(TabContents* contents) OVERRIDE;

  // content::NotificationObserver
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 protected:
  // This should only be used by unit tests.
  ExtensionHost(const Extension* extension, content::ViewType host_type);

 private:
  friend class ProcessCreationQueue;

  // Actually create the RenderView for this host. See CreateRenderViewSoon.
  void CreateRenderViewNow();

  // Navigates to the initial page.
  void LoadInitialURL();

  // Const version of below function.
  const Browser* GetBrowser() const;

  // ExtensionFunctionDispatcher::Delegate
  virtual Browser* GetBrowser() OVERRIDE;
  virtual gfx::NativeView GetNativeViewOfHost() OVERRIDE;

  // Message handlers.
  void OnRequest(const ExtensionHostMsg_Request_Params& params);

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
#if defined(TOOLKIT_VIEWS)
  scoped_ptr<ExtensionView> view_;
#elif defined(OS_MACOSX)
  scoped_ptr<ExtensionViewMac> view_;
#elif defined(TOOLKIT_GTK)
  scoped_ptr<ExtensionViewGtk> view_;
#endif

  // The host for our HTML content.
  scoped_ptr<TabContents> host_contents_;

  // Whether the RenderWidget has reported that it has stopped loading.
  bool did_stop_loading_;

  // True if the main frame has finished parsing.
  bool document_element_available_;

  // The original URL of the page being hosted.
  GURL initial_url_;

  content::NotificationRegistrar registrar_;

  ExtensionFunctionDispatcher extension_function_dispatcher_;

  // Only EXTENSION_INFOBAR, EXTENSION_POPUP, and EXTENSION_BACKGROUND_PAGE
  // are used here, others are not hosted by ExtensionHost.
  content::ViewType extension_host_type_;

  // The relevant TabContents associated with this ExtensionHost, if any.
  TabContents* associated_tab_contents_;

  // Used to measure how long it's been since the host was created.
  PerfTimer since_created_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionHost);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_HOST_H_
