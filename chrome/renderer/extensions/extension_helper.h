// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_EXTENSION_HELPER_H_
#define CHROME_RENDERER_EXTENSIONS_EXTENSION_HELPER_H_

#include <map>
#include <vector>

#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/common/view_type.h"
#include "content/public/common/console_message_level.h"
#include "content/public/renderer/render_view_observer.h"
#include "content/public/renderer/render_view_observer_tracker.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebURLResponse.h"

class GURL;
class SkBitmap;
struct ExtensionMsg_ExecuteCode_Params;
struct WebApplicationInfo;

namespace base {
class ListValue;
}

namespace webkit_glue {
class ResourceFetcher;
class ImageResourceFetcher;
}

namespace extensions {
class Dispatcher;

// RenderView-level plumbing for extension features.
class ExtensionHelper
    : public content::RenderViewObserver,
      public content::RenderViewObserverTracker<ExtensionHelper> {
 public:
  // Returns a list of extension RenderViews that match the given filter
  // criteria. If |browser_window_id| is not extension_misc::kUnknownWindowId,
  // the list is restricted to views in that browser window.
  static std::vector<content::RenderView*> GetExtensionViews(
      const std::string& extension_id,
      int browser_window_id,
      chrome::ViewType view_type);

  // Returns the given extension's background page, or NULL if none.
  static content::RenderView* GetBackgroundPage(
      const std::string& extension_id);

  ExtensionHelper(content::RenderView* render_view, Dispatcher* dispatcher);
  virtual ~ExtensionHelper();

  // Starts installation of the page in the specified frame as a web app. The
  // page must link to an external 'definition file'. This is different from
  // the 'application shortcuts' feature where we pull the application
  // definition out of optional meta tags in the page.
  bool InstallWebApplicationUsingDefinitionFile(WebKit::WebFrame* frame,
                                                string16* error);

  int tab_id() const { return tab_id_; }
  int browser_window_id() const { return browser_window_id_; }
  chrome::ViewType view_type() const { return view_type_; }

  // Helper to add a logging message to the root frame's console.
  void AddMessageToRootConsole(content::ConsoleMessageLevel level,
                               const std::string& message);

 private:
  // RenderViewObserver implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void DidFinishDocumentLoad(WebKit::WebFrame* frame) OVERRIDE;
  virtual void DidFinishLoad(WebKit::WebFrame* frame) OVERRIDE;
  virtual void DidCreateDocumentElement(WebKit::WebFrame* frame) OVERRIDE;
  virtual void DidStartProvisionalLoad(WebKit::WebFrame* frame) OVERRIDE;
  virtual void FrameDetached(WebKit::WebFrame* frame) OVERRIDE;
  virtual void DidCreateDataSource(WebKit::WebFrame* frame,
                                   WebKit::WebDataSource* ds) OVERRIDE;
  virtual void DraggableRegionsChanged(WebKit::WebFrame* frame) OVERRIDE;

  void OnExtensionResponse(int request_id, bool success,
                           const base::ListValue& response,
                           const std::string& error);
  void OnExtensionMessageInvoke(const std::string& extension_id,
                                const std::string& function_name,
                                const base::ListValue& args,
                                const GURL& event_url,
                                bool user_gesture);
  void OnExtensionDispatchOnConnect(int target_port_id,
                                    const std::string& channel_name,
                                    const std::string& tab_json,
                                    const std::string& source_extension_id,
                                    const std::string& target_extension_id);
  void OnExtensionDeliverMessage(int target_port_id,
                                 const std::string& message);
  void OnExtensionDispatchOnDisconnect(int port_id, bool connection_error);
  void OnExecuteCode(const ExtensionMsg_ExecuteCode_Params& params);
  void OnGetApplicationInfo(int page_id);
  void OnNotifyRendererViewType(chrome::ViewType view_type);
  void OnSetTabId(int tab_id);
  void OnUpdateBrowserWindowId(int window_id);
  void OnAddMessageToConsole(content::ConsoleMessageLevel level,
                             const std::string& message);
  void OnAppWindowClosed();

  // Callback triggered when we finish downloading the application definition
  // file.
  void DidDownloadApplicationDefinition(const WebKit::WebURLResponse& response,
                                        const std::string& data);

  // Callback triggered after each icon referenced by the application definition
  // is downloaded.
  void DidDownloadApplicationIcon(webkit_glue::ImageResourceFetcher* fetcher,
                                  const SkBitmap& image);

  // Helper to add a logging message to the root frame's console.
  void AddMessageToRootConsole(content::ConsoleMessageLevel level,
                               const string16& message);

  Dispatcher* dispatcher_;

  // The app info that we are processing. This is used when installing an app
  // via application definition. The in-progress web app is stored here while
  // its manifest and icons are downloaded.
  scoped_ptr<WebApplicationInfo> pending_app_info_;

  // Used to download the application definition file.
  scoped_ptr<webkit_glue::ResourceFetcher> app_definition_fetcher_;

  // Used to download the icons for an application.
  typedef std::vector<linked_ptr<webkit_glue::ImageResourceFetcher> >
      ImageResourceFetcherList;
  ImageResourceFetcherList app_icon_fetchers_;

  // The number of app icon requests outstanding. When this reaches zero, we're
  // done processing an app definition file.
  int pending_app_icon_requests_;

  // Type of view attached with RenderView.
  chrome::ViewType view_type_;

  // Id of the tab which the RenderView is attached to.
  int tab_id_;

  // Id number of browser window which RenderView is attached to.
  int browser_window_id_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionHelper);
};

}  // namespace extensions

#endif  // CHROME_RENDERER_EXTENSIONS_EXTENSION_HELPER_H_
