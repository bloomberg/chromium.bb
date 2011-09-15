// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_EXTENSION_HELPER_H_
#define CHROME_RENDERER_EXTENSIONS_EXTENSION_HELPER_H_
#pragma once

#include <map>

#include "content/common/view_types.h"
#include "content/renderer/render_view.h"
#include "content/renderer/render_view_observer.h"
#include "content/renderer/render_view_observer_tracker.h"

class ExtensionDispatcher;
class GURL;
struct ExtensionMsg_ExecuteCode_Params;
struct WebApplicationInfo;

namespace base {
class ListValue;
}

namespace webkit_glue {
class ResourceFetcher;
}

// RenderView-level plumbing for extension features.
class ExtensionHelper : public RenderViewObserver,
                        public RenderViewObserverTracker<ExtensionHelper> {
 public:
  ExtensionHelper(RenderView* render_view,
                  ExtensionDispatcher* extension_dispatcher);
  virtual ~ExtensionHelper();

  // Starts installation of the page in the specified frame as a web app. The
  // page must link to an external 'definition file'. This is different from
  // the 'application shortcuts' feature where we pull the application
  // definition out of optional meta tags in the page.
  bool InstallWebApplicationUsingDefinitionFile(WebKit::WebFrame* frame,
                                                string16* error);

  void InlineWebstoreInstall(int install_id,
                             std::string webstore_item_id,
                             GURL requestor_url);

  int browser_window_id() const { return browser_window_id_; }
  ViewType::Type view_type() const { return view_type_; }

 private:
  // RenderViewObserver implementation.
  virtual bool OnMessageReceived(const IPC::Message& message);
  virtual void DidFinishDocumentLoad(WebKit::WebFrame* frame);
  virtual void DidFinishLoad(WebKit::WebFrame* frame);
  virtual void DidCreateDocumentElement(WebKit::WebFrame* frame);
  virtual void DidStartProvisionalLoad(WebKit::WebFrame* frame);
  virtual void FrameDetached(WebKit::WebFrame* frame);
  virtual void DidCreateDataSource(WebKit::WebFrame* frame,
                                   WebKit::WebDataSource* ds);

  void OnExtensionResponse(int request_id, bool success,
                           const std::string& response,
                           const std::string& error);
  void OnExtensionMessageInvoke(const std::string& extension_id,
                                const std::string& function_name,
                                const base::ListValue& args,
                                const GURL& event_url);
  void OnExtensionDeliverMessage(int target_port_id,
                                 const std::string& message);
  void OnExecuteCode(const ExtensionMsg_ExecuteCode_Params& params);
  void OnGetApplicationInfo(int page_id);
  void OnNotifyRendererViewType(ViewType::Type view_type);
  void OnUpdateBrowserWindowId(int window_id);
  void OnInlineWebstoreInstallResponse(
      int install_id, bool success, const std::string& error);

  // Callback triggered when we finish downloading the application definition
  // file.
  void DidDownloadApplicationDefinition(const WebKit::WebURLResponse& response,
                                        const std::string& data);

  // Callback triggered after each icon referenced by the application definition
  // is downloaded.
  void DidDownloadApplicationIcon(webkit_glue::ImageResourceFetcher* fetcher,
                                  const SkBitmap& image);

  // Helper to add an error message to the root frame's console.
  void AddErrorToRootConsole(const string16& message);

  ExtensionDispatcher* extension_dispatcher_;

  // The app info that we are processing. This is used when installing an app
  // via application definition. The in-progress web app is stored here while
  // its manifest and icons are downloaded.
  scoped_ptr<WebApplicationInfo> pending_app_info_;

  // Used to download the application definition file.
  scoped_ptr<webkit_glue::ResourceFetcher> app_definition_fetcher_;

  // Used to download the icons for an application.
  RenderView::ImageResourceFetcherList app_icon_fetchers_;

  // The number of app icon requests outstanding. When this reaches zero, we're
  // done processing an app definition file.
  int pending_app_icon_requests_;

  // Type of view attached with RenderView.
  ViewType::Type view_type_;

  // Id number of browser window which RenderView is attached to.
  int browser_window_id_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionHelper);
};

#endif  // CHROME_RENDERER_EXTENSIONS_EXTENSION_HELPER_H_
