// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_RENDER_FRAME_HOST_H_
#define CONTENT_PUBLIC_BROWSER_RENDER_FRAME_HOST_H_

#include <string>

#include "base/callback_forward.h"
#include "content/common/content_export.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sender.h"
#include "third_party/WebKit/public/platform/WebPageVisibilityState.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "url/gurl.h"

namespace base {
class Value;
}

namespace content {
class RenderProcessHost;
class RenderViewHost;
class ServiceRegistry;
class SiteInstance;

// The interface provides a communication conduit with a frame in the renderer.
class CONTENT_EXPORT RenderFrameHost : public IPC::Listener,
                                       public IPC::Sender {
 public:
  // Returns the RenderFrameHost given its ID and the ID of its render process.
  // Returns nullptr if the IDs do not correspond to a live RenderFrameHost.
  static RenderFrameHost* FromID(int render_process_id, int render_frame_id);

  ~RenderFrameHost() override {}

  // Returns the route id for this frame.
  virtual int GetRoutingID() = 0;

  // Returns the SiteInstance grouping all RenderFrameHosts that have script
  // access to this RenderFrameHost, and must therefore live in the same
  // process.
  virtual SiteInstance* GetSiteInstance() = 0;

  // Returns the process for this frame.
  virtual RenderProcessHost* GetProcess() = 0;

  // Returns the current RenderFrameHost of the parent frame, or nullptr if
  // there is no parent. The result may be in a different process than the
  // current RenderFrameHost.
  virtual RenderFrameHost* GetParent() = 0;

  // Returns the assigned name of the frame, the name of the iframe tag
  // declaring it. For example, <iframe name="framename">[...]</iframe>. It is
  // quite possible for a frame to have no name, in which case GetFrameName will
  // return an empty string.
  virtual const std::string& GetFrameName() = 0;

  // Returns true if the frame is out of process.
  virtual bool IsCrossProcessSubframe() = 0;

  // Returns the last committed URL of the frame.
  virtual GURL GetLastCommittedURL() = 0;

  // Returns the associated widget's native view.
  virtual gfx::NativeView GetNativeView() = 0;

  // Runs some JavaScript in this frame's context. If a callback is provided, it
  // will be used to return the result, when the result is available.
  typedef base::Callback<void(const base::Value*)> JavaScriptResultCallback;
  virtual void ExecuteJavaScript(const base::string16& javascript) = 0;
  virtual void ExecuteJavaScript(const base::string16& javascript,
                                 const JavaScriptResultCallback& callback) = 0;

  // ONLY FOR TESTS: Same as above but adds a fake UserGestureIndicator around
  // execution. (crbug.com/408426)
  virtual void ExecuteJavaScriptForTests(const base::string16& javascript) = 0;

  // Accessibility actions - these send a message to the RenderFrame
  // to trigger an action on an accessibility object.
  virtual void AccessibilitySetFocus(int acc_obj_id) = 0;
  virtual void AccessibilityDoDefaultAction(int acc_obj_id) = 0;
  virtual void AccessibilityScrollToMakeVisible(
      int acc_obj_id, const gfx::Rect& subfocus) = 0;
  virtual void AccessibilitySetTextSelection(
      int acc_obj_id, int start_offset, int end_offset) = 0;

  // This is called when the user has committed to the given find in page
  // request (e.g. by pressing enter or by clicking on the next / previous
  // result buttons). It triggers sending a native accessibility event on
  // the result object on the page, navigating assistive technology to that
  // result.
  virtual void ActivateFindInPageResultForAccessibility(int request_id) = 0;

  // Roundtrips through the renderer and compositor pipeline to ensure that any
  // changes to the contents resulting from operations executed prior to this
  // call are visible on screen. The call completes asynchronously by running
  // the supplied |callback| with a value of true upon successful completion and
  // false otherwise (when the frame is destroyed, detached, etc..).
  typedef base::Callback<void(bool)> VisualStateCallback;
  virtual void InsertVisualStateCallback(
      const VisualStateCallback& callback) = 0;

  // Temporary until we get rid of RenderViewHost.
  virtual RenderViewHost* GetRenderViewHost() = 0;

  // Returns the ServiceRegistry for this frame.
  virtual ServiceRegistry* GetServiceRegistry() = 0;

  // Returns the visibility state of the frame. The different visibility states
  // of a frame are defined in Blink.
  virtual blink::WebPageVisibilityState GetVisibilityState() = 0;

 private:
  // This interface should only be implemented inside content.
  friend class RenderFrameHostImpl;
  RenderFrameHost() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_RENDER_FRAME_HOST_H_
