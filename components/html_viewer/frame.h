// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HTML_VIEWER_FRAME_H_
#define COMPONENTS_HTML_VIEWER_FRAME_H_

#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "components/html_viewer/frame_tree_manager.h"
#include "components/view_manager/public/cpp/view_observer.h"
#include "mandoline/tab/public/interfaces/frame_tree.mojom.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/web/WebFrameClient.h"
#include "third_party/WebKit/public/web/WebRemoteFrameClient.h"
#include "third_party/WebKit/public/web/WebSandboxFlags.h"
#include "third_party/WebKit/public/web/WebViewClient.h"

namespace blink {
class WebFrame;
}

namespace mojo {
class Rect;
class View;
}

namespace html_viewer {

class FrameTreeManager;
class GeolocationClientImpl;
class TouchHandler;
class WebLayerImpl;
class WebLayerTreeViewImpl;

// Frame is used to represent a single frame in the frame tree of a page. The
// frame is either local or remote. Each Frame is associated with a single
// FrameTreeManager and can not be moved to another FrameTreeManager. Local
// frames have a mojo::View, remote frames do not.
class Frame : public blink::WebFrameClient,
              public blink::WebViewClient,
              public blink::WebRemoteFrameClient,
              public mojo::ViewObserver {
 public:
  struct CreateParams {
    CreateParams(FrameTreeManager* manager, Frame* parent, uint32_t id)
        : manager(manager), parent(parent), id(id) {}
    ~CreateParams() {}

    FrameTreeManager* manager;
    Frame* parent;
    uint32_t id;
  };

  explicit Frame(const CreateParams& params);

  void Init(mojo::View* local_view,
            const blink::WebString& remote_frame_name,
            const blink::WebString& remote_origin);

  // Closes and deletes this Frame.
  void Close();

  uint32_t id() const { return id_; }

  // Returns the Frame whose id is |id|.
  Frame* FindFrame(uint32_t id) {
    return const_cast<Frame*>(const_cast<const Frame*>(this)->FindFrame(id));
  }
  const Frame* FindFrame(uint32_t id) const;

  Frame* parent() { return parent_; }

  const std::vector<Frame*>& children() { return children_; }

  // Returns the WebFrame for this Frame. This is either a WebLocalFrame or
  // WebRemoteFrame.
  blink::WebFrame* web_frame() { return web_frame_; }

  // Returns the WebView for this frame, or null if there isn't one. The root
  // has a WebView, the children WebFrameWidgets.
  blink::WebView* web_view();

  // The mojo::View this frame renders to. This is non-null for the local frame
  // the frame tree was created with as well as non-null for any frames created
  // locally.
  mojo::View* view() { return view_; }

 private:
  friend class FrameTreeManager;

  virtual ~Frame();

  // Sets the name of the remote frame. Does nothing if this is a local frame.
  void SetRemoteFrameName(const mojo::String& name);

  // Returns true if the Frame is local, false if remote.
  bool IsLocal() const;

  void SetView(mojo::View* view);

  // Creates the appropriate WebWidget implementation for the Frame.
  void CreateWebWidget();

  void UpdateFocus();

  // Updates the size and scale factor of the webview and related classes from
  // |root_|.
  void UpdateWebViewSizeFromViewSize();

  // Swaps this frame from a local frame to remote frame. |request| is the url
  // to load in the frame.
  void SwapToRemote(const blink::WebURLRequest& request);

  // See comment in SwapToRemote() for details on this.
  void FinishSwapToRemote();

  GlobalState* global_state() { return frame_tree_manager_->global_state(); }

  // Returns the Frame associated with the specified WebFrame.
  Frame* FindFrameWithWebFrame(blink::WebFrame* web_frame);

  // The various frameDetached() implementations call into this.
  void FrameDetachedImpl(blink::WebFrame* web_frame);

  // mojo::ViewObserver methods:
  void OnViewBoundsChanged(mojo::View* view,
                           const mojo::Rect& old_bounds,
                           const mojo::Rect& new_bounds) override;
  void OnViewDestroyed(mojo::View* view) override;
  void OnViewInputEvent(mojo::View* view, const mojo::EventPtr& event) override;
  void OnViewFocusChanged(mojo::View* gained_focus,
                          mojo::View* lost_focus) override;

  // WebViewClient methods:
  virtual void initializeLayerTreeView() override;
  virtual blink::WebLayerTreeView* layerTreeView() override;
  virtual blink::WebStorageNamespace* createSessionStorageNamespace();

  // WebFrameClient methods:
  virtual blink::WebMediaPlayer* createMediaPlayer(
      blink::WebLocalFrame* frame,
      const blink::WebURL& url,
      blink::WebMediaPlayerClient* client,
      blink::WebMediaPlayerEncryptedMediaClient* encrypted_client,
      blink::WebContentDecryptionModule* initial_cdm);
  virtual blink::WebFrame* createChildFrame(
      blink::WebLocalFrame* parent,
      blink::WebTreeScopeType scope,
      const blink::WebString& frame_ame,
      blink::WebSandboxFlags sandbox_flags);
  virtual void frameDetached(blink::WebFrame* frame,
                             blink::WebFrameClient::DetachType type);
  virtual blink::WebCookieJar* cookieJar(blink::WebLocalFrame* frame);
  virtual blink::WebNavigationPolicy decidePolicyForNavigation(
      const NavigationPolicyInfo& info);
  virtual void didAddMessageToConsole(const blink::WebConsoleMessage& message,
                                      const blink::WebString& source_name,
                                      unsigned source_line,
                                      const blink::WebString& stack_trace);
  virtual void didFinishLoad(blink::WebLocalFrame* frame);
  virtual void didNavigateWithinPage(blink::WebLocalFrame* frame,
                                     const blink::WebHistoryItem& history_item,
                                     blink::WebHistoryCommitType commit_type);
  virtual blink::WebGeolocationClient* geolocationClient();
  virtual blink::WebEncryptedMediaClient* encryptedMediaClient();
  virtual void didStartLoading(bool to_different_document);
  virtual void didStopLoading();
  virtual void didChangeLoadProgress(double load_progress);
  virtual void didChangeName(blink::WebLocalFrame* frame,
                             const blink::WebString& name);

  // blink::WebRemoteFrameClient:
  virtual void frameDetached(blink::WebRemoteFrameClient::DetachType type);
  virtual void postMessageEvent(blink::WebLocalFrame* source_frame,
                                blink::WebRemoteFrame* target_frame,
                                blink::WebSecurityOrigin target_origin,
                                blink::WebDOMMessageEvent event);
  virtual void initializeChildFrame(const blink::WebRect& frame_rect,
                                    float scale_factor);
  virtual void navigate(const blink::WebURLRequest& request,
                        bool should_replace_current_entry);
  virtual void reload(bool ignore_cache, bool is_client_redirect);
  virtual void forwardInputEvent(const blink::WebInputEvent* event);

  FrameTreeManager* frame_tree_manager_;
  Frame* parent_;
  mojo::View* view_;
  // The id for this frame. If there is a view, this is the same id as the
  // view has.
  const uint32_t id_;
  std::vector<Frame*> children_;
  blink::WebFrame* web_frame_;
  blink::WebWidget* web_widget_;
  scoped_ptr<GeolocationClientImpl> geolocation_client_impl_;
  scoped_ptr<WebLayerTreeViewImpl> web_layer_tree_view_impl_;
  scoped_ptr<TouchHandler> touch_handler_;

  // TODO(sky): better factor this, maybe push to View.
  blink::WebTreeScopeType scope_;

  scoped_ptr<WebLayerImpl> web_layer_;

  base::WeakPtrFactory<Frame> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(Frame);
};

}  // namespace html_viewer

#endif  // COMPONENTS_HTML_VIEWER_FRAME_H_
