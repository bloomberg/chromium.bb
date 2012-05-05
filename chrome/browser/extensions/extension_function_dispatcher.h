// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_FUNCTION_DISPATCHER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_FUNCTION_DISPATCHER_H_
#pragma once

#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "ipc/ipc_message.h"
#include "googleurl/src/gurl.h"

class ChromeRenderMessageFilter;
class Extension;
class ExtensionFunction;
class ExtensionWindowController;
class ExtensionInfoMap;
class Profile;
struct ExtensionHostMsg_Request_Params;

namespace content {
class RenderViewHost;
class WebContents;
}

namespace extensions {
class ExtensionAPI;
class ProcessMap;
}

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
    // Returns the ExtensionWindowController associated with this delegate,
    // or NULL if no window is associated with the delegate.
    virtual ExtensionWindowController* GetExtensionWindowController() const = 0;

    // Asks the delegate for any relevant WebContents associated with this
    // context. For example, the WebbContents in which an infobar or
    // chrome-extension://<id> URL are being shown. Callers must check for a
    // NULL return value (as in the case of a background page).
    virtual content::WebContents* GetAssociatedWebContents() const = 0;

   protected:
    virtual ~Delegate() {}
  };

  // Gets a list of all known extension function names.
  static void GetAllFunctionNames(std::vector<std::string>* names);

  // Override a previously registered function. Returns true if successful,
  // false if no such function was registered.
  static bool OverrideFunction(const std::string& name,
                               ExtensionFunctionFactory factory);

  // Resets all functions to their initial implementation.
  static void ResetFunctions();

  // Dispatches an IO-thread extension function. Only used for specific
  // functions that must be handled on the IO-thread.
  static void DispatchOnIOThread(
      ExtensionInfoMap* extension_info_map,
      void* profile,
      int render_process_id,
      base::WeakPtr<ChromeRenderMessageFilter> ipc_sender,
      int routing_id,
      const ExtensionHostMsg_Request_Params& params);

  // Public constructor. Callers must ensure that:
  // - |delegate| outlives this object.
  // - This object outlives any RenderViewHost's passed to created
  //   ExtensionFunctions.
  ExtensionFunctionDispatcher(Profile* profile, Delegate* delegate);

  ~ExtensionFunctionDispatcher();

  Delegate* delegate() { return delegate_; }

  // Message handlers.
  void Dispatch(const ExtensionHostMsg_Request_Params& params,
                content::RenderViewHost* sender);

  // Called when an ExtensionFunction is done executing, after it has sent
  // a response (if any) to the extension.
  void OnExtensionFunctionCompleted(const Extension* extension);

  // The profile that this dispatcher is associated with.
  Profile* profile() { return profile_; }

 private:
  // Helper to create an ExtensionFunction to handle the function given by
  // |params|. Can be called on any thread.
  // Does not set subclass properties, or include_incognito.
  static ExtensionFunction* CreateExtensionFunction(
      const ExtensionHostMsg_Request_Params& params,
      const Extension* extension,
      int requesting_process_id,
      const extensions::ProcessMap& process_map,
      extensions::ExtensionAPI* api,
      void* profile,
      IPC::Message::Sender* ipc_sender,
      int routing_id);

  // Helper to send an access denied error to the requesting renderer. Can be
  // called on any thread.
  static void SendAccessDenied(IPC::Message::Sender* ipc_sender,
                               int routing_id,
                               int request_id);

  Profile* profile_;

  Delegate* delegate_;
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_FUNCTION_DISPATCHER_H_
