// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_FUNCTION_DISPATCHER_H_
#define EXTENSIONS_BROWSER_EXTENSION_FUNCTION_DISPATCHER_H_

#include <map>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "extensions/browser/extension_function.h"
#include "ipc/ipc_sender.h"

struct ExtensionHostMsg_Request_Params;

namespace content {
class BrowserContext;
class RenderFrameHost;
class RenderViewHost;
class WebContents;
}

namespace extensions {

class Extension;
class ExtensionAPI;
class ExtensionMessageFilter;
class InfoMap;
class ProcessMap;
class WindowController;

// A factory function for creating new ExtensionFunction instances.
typedef ExtensionFunction* (*ExtensionFunctionFactory)();

// ExtensionFunctionDispatcher receives requests to execute functions from
// Chrome extensions running in a RenderViewHost and dispatches them to the
// appropriate handler. It lives entirely on the UI thread.
//
// ExtensionFunctionDispatcher should be a member of some class that hosts
// RenderViewHosts and wants them to be able to display extension content.
// This class should also implement ExtensionFunctionDispatcher::Delegate.
//
// Note that a single ExtensionFunctionDispatcher does *not* correspond to a
// single RVH, a single extension, or a single URL. This is by design so that
// we can gracefully handle cases like WebContents, where the RVH, extension,
// and URL can all change over the lifetime of the tab. Instead, these items
// are all passed into each request.
class ExtensionFunctionDispatcher
    : public base::SupportsWeakPtr<ExtensionFunctionDispatcher> {
 public:
  class Delegate {
   public:
    // Returns the WindowController associated with this delegate, or NULL if no
    // window is associated with the delegate.
    virtual WindowController* GetExtensionWindowController() const;

    // Asks the delegate for any relevant WebContents associated with this
    // context. For example, the WebContents in which an infobar or
    // chrome-extension://<id> URL are being shown. Callers must check for a
    // NULL return value (as in the case of a background page).
    virtual content::WebContents* GetAssociatedWebContents() const;

    // If the associated web contents is not null, returns that. Otherwise,
    // returns the next most relevant visible web contents. Callers must check
    // for a NULL return value (as in the case of a background page).
    virtual content::WebContents* GetVisibleWebContents() const;

   protected:
    virtual ~Delegate() {}
  };

  // Gets a list of all known extension function names.
  static void GetAllFunctionNames(std::vector<std::string>* names);

  // Override a previously registered function. Returns true if successful,
  // false if no such function was registered.
  static bool OverrideFunction(const std::string& name,
                               ExtensionFunctionFactory factory);

  // Dispatches an IO-thread extension function. Only used for specific
  // functions that must be handled on the IO-thread.
  static void DispatchOnIOThread(
      InfoMap* extension_info_map,
      void* profile_id,
      int render_process_id,
      base::WeakPtr<ExtensionMessageFilter> ipc_sender,
      int routing_id,
      const ExtensionHostMsg_Request_Params& params);

  // Public constructor. Callers must ensure that:
  // - |delegate| outlives this object.
  // - This object outlives any RenderViewHost's passed to created
  //   ExtensionFunctions.
  ExtensionFunctionDispatcher(content::BrowserContext* browser_context,
                              Delegate* delegate);

  ~ExtensionFunctionDispatcher();

  Delegate* delegate() { return delegate_; }

  // Message handlers.
  // The response is sent to the corresponding render view in an
  // ExtensionMsg_Response message.
  // TODO (jam): convert all callers to use RenderFrameHost.
  void Dispatch(const ExtensionHostMsg_Request_Params& params,
                content::RenderViewHost* render_view_host);
  // Dispatch an extension function and calls |callback| when the execution
  // completes.
  void DispatchWithCallback(
      const ExtensionHostMsg_Request_Params& params,
      content::RenderFrameHost* render_frame_host,
      const ExtensionFunction::ResponseCallback& callback);

  // Called when an ExtensionFunction is done executing, after it has sent
  // a response (if any) to the extension.
  void OnExtensionFunctionCompleted(const Extension* extension);

  // The BrowserContext that this dispatcher is associated with.
  content::BrowserContext* browser_context() { return browser_context_; }

 private:
  // For a given RenderViewHost instance, UIThreadResponseCallbackWrapper
  // creates ExtensionFunction::ResponseCallback instances which send responses
  // to the corresponding render view in ExtensionMsg_Response messages.
  // This class tracks the lifespan of the RenderViewHost instance, and will be
  // destroyed automatically when it goes away.
  class UIThreadResponseCallbackWrapper;

  // Helper to check whether an ExtensionFunction has the required permissions.
  // This should be called after the function is fully initialized.
  // If the check fails, |callback| is run with an access-denied error and false
  // is returned. |function| must not be run in that case.
  static bool CheckPermissions(
      ExtensionFunction* function,
      const Extension* extension,
      const ExtensionHostMsg_Request_Params& params,
      const ExtensionFunction::ResponseCallback& callback);

  // Helper to create an ExtensionFunction to handle the function given by
  // |params|. Can be called on any thread.
  // Does not set subclass properties, or include_incognito.
  static ExtensionFunction* CreateExtensionFunction(
      const ExtensionHostMsg_Request_Params& params,
      const Extension* extension,
      int requesting_process_id,
      const ProcessMap& process_map,
      ExtensionAPI* api,
      void* profile_id,
      const ExtensionFunction::ResponseCallback& callback);

  // Helper to run the response callback with an access denied error. Can be
  // called on any thread.
  static void SendAccessDenied(
      const ExtensionFunction::ResponseCallback& callback);

  void DispatchWithCallbackInternal(
      const ExtensionHostMsg_Request_Params& params,
      content::RenderViewHost* render_view_host,
      content::RenderFrameHost* render_frame_host,
      const ExtensionFunction::ResponseCallback& callback);

  content::BrowserContext* browser_context_;

  Delegate* delegate_;

  // This map doesn't own either the keys or the values. When a RenderViewHost
  // instance goes away, the corresponding entry in this map (if exists) will be
  // removed.
  typedef std::map<content::RenderViewHost*, UIThreadResponseCallbackWrapper*>
      UIThreadResponseCallbackWrapperMap;
  UIThreadResponseCallbackWrapperMap ui_thread_response_callback_wrappers_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_FUNCTION_DISPATCHER_H_
