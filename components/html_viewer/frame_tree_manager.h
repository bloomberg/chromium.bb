// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HTML_VIEWER_FRAME_TREE_MANAGER_H_
#define COMPONENTS_HTML_VIEWER_FRAME_TREE_MANAGER_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "mandoline/services/navigation/public/interfaces/navigation.mojom.h"
#include "mandoline/tab/public/interfaces/frame_tree.mojom.h"
#include "mojo/application/public/cpp/lazy_interface_ptr.h"
#include "third_party/WebKit/public/web/WebFrameClient.h"
#include "third_party/WebKit/public/web/WebNavigationPolicy.h"

namespace blink {
class WebFrame;
class WebFrameClient;
class WebLocalFrame;
class WebRemoteFrameClient;
class WebString;
class WebView;
}

namespace gfx {
class Size;
}

namespace mojo {
class ApplicationConnection;
class ApplicationImpl;
class View;
}

namespace html_viewer {

class Frame;
class FrameTreeManagerDelegate;
class GlobalState;

// FrameTreeManager is responsible for managing the frames that comprise a
// document. Some of the frames may be remote. FrameTreeManager updates its
// state in response to changes from the FrameTreeServer, as well as changes
// from the underlying frames. The frame tree has at least one local frame
// that is backed by a mojo::View.
class FrameTreeManager : public mandoline::FrameTreeClient {
 public:
  FrameTreeManager(GlobalState* global_state,
                   mojo::ApplicationImpl* app,
                   mojo::ApplicationConnection* app_connection,
                   uint32_t local_frame_id,
                   mandoline::FrameTreeServerPtr server);
  ~FrameTreeManager() override;

  void Init(mojo::View* local_view,
            mojo::Array<mandoline::FrameDataPtr> frame_data);

  void set_delegate(FrameTreeManagerDelegate* delegate) {
    delegate_ = delegate;
  }

  GlobalState* global_state() { return global_state_; }
  mojo::ApplicationImpl* app() { return app_; }

  // Returns the Frame/WebFrame that is rendering to the supplied view.
  // TODO(sky): we need to support more than one local frame.
  Frame* GetLocalFrame();
  blink::WebLocalFrame* GetLocalWebFrame();

  blink::WebView* GetWebView();

  void LoadingStarted();
  void LoadingStopped();
  void ProgressChanged(double progress);

 private:
  friend class Frame;

  // Returns the navigation policy for the specified frame.
  blink::WebNavigationPolicy DecidePolicyForNavigation(
      Frame* frame,
      const blink::WebFrameClient::NavigationPolicyInfo& info);

  // Invoked when a Frame finishes loading.
  void OnFrameDidFinishLoad(Frame* frame);

  // Invoked when a Frame navigates.
  void OnFrameDidNavigateLocally(Frame* frame, const std::string& url);

  // Invoked when a Frame is destroye.
  void OnFrameDestroyed(Frame* frame);

  // Invoked when the name of a frame changes.
  void OnFrameDidChangeName(Frame* frame, const blink::WebString& name);

  // mandoline::FrameTreeClient:
  void OnConnect(mandoline::FrameTreeServerPtr server,
                 mojo::Array<mandoline::FrameDataPtr> frame_data) override;
  void OnFrameAdded(mandoline::FrameDataPtr frame_data) override;
  void OnFrameRemoved(uint32_t frame_id) override;
  void OnFrameNameChanged(uint32_t frame_id, const mojo::String& name) override;

  GlobalState* global_state_;

  mojo::ApplicationImpl* app_;

  FrameTreeManagerDelegate* delegate_;

  // Frame id of the frame we're rendering to.
  const uint32_t local_frame_id_;

  mandoline::FrameTreeServerPtr server_;

  mojo::LazyInterfacePtr<mojo::NavigatorHost> navigator_host_;

  Frame* root_;

  DISALLOW_COPY_AND_ASSIGN(FrameTreeManager);
};

}  // namespace html_viewer

#endif  // COMPONENTS_HTML_VIEWER_FRAME_TREE_MANAGER_H_
