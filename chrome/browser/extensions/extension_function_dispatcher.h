// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_FUNCTION_DISPATCHER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_FUNCTION_DISPATCHER_H_
#pragma once

#include <string>
#include <vector>

#include "base/ref_counted.h"
#include "googleurl/src/gurl.h"
#include "ui/gfx/native_widget_types.h"

class Browser;
class Extension;
class ExtensionFunction;
class ListValue;
class Profile;
class RenderViewHost;
class TabContents;
struct ViewHostMsg_DomMessage_Params;

// A factory function for creating new ExtensionFunction instances.
typedef ExtensionFunction* (*ExtensionFunctionFactory)();

// ExtensionFunctionDispatcher receives requests to execute functions from
// Chromium extensions running in a RenderViewHost and dispatches them to the
// appropriate handler. It lives entirely on the UI thread.
class ExtensionFunctionDispatcher {
 public:
  class Delegate {
   public:
    // Returns the browser that this delegate is associated with, if any.
    // Returns NULL otherwise.
    virtual Browser* GetBrowser() = 0;

    // Returns the native view for this extension view, if any. This may be NULL
    // if the view is not visible.
    virtual gfx::NativeView GetNativeViewOfHost() = 0;

    // Asks the delegate for any relevant TabContents associated with this
    // context. For example, the TabContents in which an infobar or
    // chrome-extension://<id> URL are being shown. Callers must check for a
    // NULL return value (as in the case of a background page).
    virtual TabContents* associated_tab_contents() const = 0;

   protected:
    virtual ~Delegate() {}
  };

  // The peer object allows us to notify ExtensionFunctions when we are
  // destroyed.
  // TODO: this should use WeakPtr
  struct Peer : public base::RefCounted<Peer> {
    explicit Peer(ExtensionFunctionDispatcher* dispatcher)
        : dispatcher_(dispatcher) {}
    ExtensionFunctionDispatcher* dispatcher_;

   private:
    friend class base::RefCounted<Peer>;

    ~Peer() {}
  };

  // Gets a list of all known extension function names.
  static void GetAllFunctionNames(std::vector<std::string>* names);

  // Override a previously registered function. Returns true if successful,
  // false if no such function was registered.
  static bool OverrideFunction(const std::string& name,
                               ExtensionFunctionFactory factory);

  // Resets all functions to their initial implementation.
  static void ResetFunctions();

  // Creates an instance for the specified RenderViewHost and URL. If the URL
  // does not contain a valid extension, returns NULL.
  static ExtensionFunctionDispatcher* Create(RenderViewHost* render_view_host,
                                             Delegate* delegate,
                                             const GURL& url);

  ~ExtensionFunctionDispatcher();

  Delegate* delegate() { return delegate_; }

  // Handle a request to execute an extension function.
  void HandleRequest(const ViewHostMsg_DomMessage_Params& params);

  // Send a response to a function.
  void SendResponse(ExtensionFunction* api, bool success);

  // Returns the current browser. Callers should generally prefer
  // ExtensionFunction::GetCurrentBrowser() over this method, as that one
  // provides the correct value for |include_incognito|.
  //
  // See the comments for ExtensionFunction::GetCurrentBrowser() for more
  // details.
  Browser* GetCurrentBrowser(bool include_incognito);

  // Handle a malformed message.  Possibly the result of an attack, so kill
  // the renderer.
  void HandleBadMessage(ExtensionFunction* api);

  // Gets the URL for the view we're displaying.
  const GURL& url() { return url_; }

  // Gets the ID for this extension.
  const std::string extension_id() { return extension_id_; }

  // The profile that this dispatcher is associated with.
  Profile* profile();

  // The RenderViewHost this dispatcher is associated with.
  RenderViewHost* render_view_host() { return render_view_host_; }

 private:
  ExtensionFunctionDispatcher(RenderViewHost* render_view_host,
                              Delegate* delegate,
                              const Extension* extension,
                              const GURL& url);

  // We need to keep a pointer to the profile because we use it in the dtor
  // in sending EXTENSION_FUNCTION_DISPATCHER_DESTROYED, but by that point
  // the render_view_host_ has been deleted.
  Profile* profile_;

  RenderViewHost* render_view_host_;

  Delegate* delegate_;

  GURL url_;

  std::string extension_id_;

  scoped_refptr<Peer> peer_;

  // AutomationExtensionFunction requires access to the RenderViewHost
  // associated with us.  We make it a friend rather than exposing the
  // RenderViewHost as a public method as we wouldn't want everyone to
  // start assuming a 1:1 relationship between us and RenderViewHost,
  // whereas AutomationExtensionFunction is by necessity "tight" with us.
  friend class AutomationExtensionFunction;
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_FUNCTION_DISPATCHER_H_
