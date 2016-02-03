// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_NPAPI_WEBPLUGIN_H_
#define CONTENT_CHILD_NPAPI_WEBPLUGIN_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "build/build_config.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/gpu_preference.h"

// TODO(port): this typedef is obviously incorrect on non-Windows
// platforms, but now a lot of code now accidentally depends on them
// existing.  #ifdef out these declarations and fix all the users.
typedef void* HANDLE;

class GURL;
struct NPObject;

namespace content {

class WebPluginResourceClient;
#if defined(OS_MACOSX)
class WebPluginAcceleratedSurface;
#endif

// The WebKit side of a plugin implementation.  It provides wrappers around
// operations that need to interact with the frame and other WebCore objects.
class WebPlugin {
 public:
  virtual ~WebPlugin() {}

  // Called by the plugin delegate to let the WebPlugin know if the plugin is
  // windowed (i.e. handle is not NULL) or windowless (handle is NULL).  This
  // tells the WebPlugin to send mouse/keyboard events to the plugin delegate,
  // as well as the information about the HDC for paint operations.
  virtual void SetWindow(gfx::PluginWindowHandle window) = 0;

  // Whether input events should be sent to the delegate.
  virtual void SetAcceptsInputEvents(bool accepts) = 0;

  // Called by the plugin delegate to let it know that the window is being
  // destroyed.
  virtual void WillDestroyWindow(gfx::PluginWindowHandle window) = 0;

  virtual void Invalidate() = 0;
  virtual void InvalidateRect(const gfx::Rect& rect) = 0;

  // Returns the NPObject for the browser's window object. Does not
  // take a reference.
  virtual NPObject* GetWindowScriptNPObject() = 0;

  // Returns the DOM element that loaded the plugin. Does not take a
  // reference.
  virtual NPObject* GetPluginElement() = 0;

  // Resolves the proxies for the url, returns true on success.
  virtual bool FindProxyForUrl(const GURL& url, std::string* proxy_list) = 0;

  // Cookies
  virtual void SetCookie(const GURL& url,
                         const GURL& first_party_for_cookies,
                         const std::string& cookie) = 0;
  virtual std::string GetCookies(const GURL& url,
                                 const GURL& first_party_for_cookies) = 0;

  // Cancels document load.
  virtual void CancelDocumentLoad() = 0;

  virtual void DidStartLoading() = 0;
  virtual void DidStopLoading() = 0;

  // Returns true iff in incognito mode.
  virtual bool IsOffTheRecord() = 0;

#if defined(OS_WIN)
  // |pump_messages_event| is a event handle which is used in NPP_HandleEvent
  // calls to pump messages if the plugin enters a modal loop.
  // |dummy_activation_window} is used to ensure correct keyboard activation.
  // It needs to be a child of the parent window.
  virtual void SetWindowlessData(HANDLE pump_messages_event,
                                 gfx::NativeViewId dummy_activation_window) = 0;
#endif

#if defined(OS_MACOSX)
  // Called to inform the WebPlugin that the plugin has gained or lost focus.
  virtual void FocusChanged(bool focused) {}

  // Starts plugin IME.
  virtual void StartIme() {}

  // Returns the accelerated surface abstraction for accelerated plugins.
  virtual WebPluginAcceleratedSurface* GetAcceleratedSurface(
      gfx::GpuPreference gpu_preference) = 0;

  // Core Animation plugin support. CA plugins always render through
  // the compositor.
  virtual void AcceleratedPluginEnabledRendering() = 0;
  virtual void AcceleratedPluginAllocatedIOSurface(int32_t width,
                                                   int32_t height,
                                                   uint32_t surface_id) = 0;
  virtual void AcceleratedPluginSwappedIOSurface() = 0;
#endif
};

}  // namespace content

#endif  // CONTENT_CHILD_NPAPI_WEBPLUGIN_H_
