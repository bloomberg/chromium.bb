// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_HOST_H_
#define EXTENSIONS_BROWSER_EXTENSION_HOST_H_

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/timer/elapsed_timer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/common/stack_frame.h"
#include "extensions/common/view_type.h"

class PrefsTabHelper;

namespace content {
class BrowserContext;
class RenderProcessHost;
class RenderWidgetHostView;
class SiteInstance;
}

namespace extensions {
class Extension;
class ExtensionHostDelegate;
class WindowController;

// This class is the browser component of an extension component's RenderView.
// It handles setting up the renderer process, if needed, with special
// privileges available to extensions.  It may have a view to be shown in the
// browser UI, or it may be hidden.
//
// If you are adding code that only affects visible extension views (and not
// invisible background pages) you should add it to ExtensionViewHost.
class ExtensionHost : public content::WebContentsDelegate,
                      public content::WebContentsObserver,
                      public ExtensionFunctionDispatcher::Delegate,
                      public content::NotificationObserver {
 public:
  class ProcessCreationQueue;

  ExtensionHost(const Extension* extension,
                content::SiteInstance* site_instance,
                const GURL& url, ViewType host_type);
  virtual ~ExtensionHost();

  const Extension* extension() const { return extension_; }
  const std::string& extension_id() const { return extension_id_; }
  content::WebContents* host_contents() const { return host_contents_.get(); }
  content::RenderViewHost* render_view_host() const;
  content::RenderProcessHost* render_process_host() const;
  bool did_stop_loading() const { return did_stop_loading_; }
  bool document_element_available() const {
    return document_element_available_;
  }

  content::BrowserContext* browser_context() { return browser_context_; }

  ViewType extension_host_type() const { return extension_host_type_; }
  const GURL& GetURL() const;

  // Returns true if the render view is initialized and didn't crash.
  bool IsRenderViewLive() const;

  // Prepares to initializes our RenderViewHost by creating its RenderView and
  // navigating to this host's url. Uses host_view for the RenderViewHost's view
  // (can be NULL). This happens delayed to avoid locking the UI.
  void CreateRenderViewSoon();

  // content::WebContentsObserver
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void RenderViewCreated(
      content::RenderViewHost* render_view_host) OVERRIDE;
  virtual void RenderViewDeleted(
      content::RenderViewHost* render_view_host) OVERRIDE;
  virtual void RenderViewReady() OVERRIDE;
  virtual void RenderProcessGone(base::TerminationStatus status) OVERRIDE;
  virtual void DocumentAvailableInMainFrame() OVERRIDE;
  virtual void DidStopLoading(
      content::RenderViewHost* render_view_host) OVERRIDE;

  // content::WebContentsDelegate
  virtual content::JavaScriptDialogManager*
      GetJavaScriptDialogManager() OVERRIDE;
  virtual void AddNewContents(content::WebContents* source,
                              content::WebContents* new_contents,
                              WindowOpenDisposition disposition,
                              const gfx::Rect& initial_pos,
                              bool user_gesture,
                              bool* was_blocked) OVERRIDE;
  virtual void CloseContents(content::WebContents* contents) OVERRIDE;
  virtual void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      const content::MediaResponseCallback& callback) OVERRIDE;
  virtual bool IsNeverVisible(content::WebContents* web_contents) OVERRIDE;

  // content::NotificationObserver
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 protected:
  content::NotificationRegistrar* registrar() { return &registrar_; }

  // Called after the extension page finishes loading but before the
  // EXTENSION_HOST_DID_STOP_LOADING notification is sent.
  virtual void OnDidStopLoading();

  // Called once when the document first becomes available.
  virtual void OnDocumentAvailable();

  // Navigates to the initial page.
  virtual void LoadInitialURL();

  // Returns true if we're hosting a background page.
  virtual bool IsBackgroundPage() const;

  // Closes this host (results in deletion).
  void Close();

 private:
  friend class ProcessCreationQueue;

  // Actually create the RenderView for this host. See CreateRenderViewSoon.
  void CreateRenderViewNow();

  // Message handlers.
  void OnRequest(const ExtensionHostMsg_Request_Params& params);
  void OnEventAck();
  void OnIncrementLazyKeepaliveCount();
  void OnDecrementLazyKeepaliveCount();

  // Delegate for functionality that cannot exist in the extensions module.
  scoped_ptr<ExtensionHostDelegate> delegate_;

  // The extension that we're hosting in this view.
  const Extension* extension_;

  // Id of extension that we're hosting in this view.
  const std::string extension_id_;

  // The browser context that this host is tied to.
  content::BrowserContext* browser_context_;

  // The host for our HTML content.
  scoped_ptr<content::WebContents> host_contents_;

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
  ViewType extension_host_type_;

  // Used to measure how long it's been since the host was created.
  base::ElapsedTimer since_created_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionHost);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_HOST_H_
