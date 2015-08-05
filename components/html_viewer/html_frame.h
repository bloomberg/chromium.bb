// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HTML_VIEWER_HTML_FRAME_H_
#define COMPONENTS_HTML_VIEWER_HTML_FRAME_H_

#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "components/html_viewer/html_frame_tree_manager.h"
#include "components/view_manager/public/cpp/view_observer.h"
#include "mandoline/tab/public/interfaces/frame_tree.mojom.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/web/WebFrameClient.h"
#include "third_party/WebKit/public/web/WebRemoteFrameClient.h"
#include "third_party/WebKit/public/web/WebSandboxFlags.h"
#include "third_party/WebKit/public/web/WebTextInputInfo.h"
#include "third_party/WebKit/public/web/WebViewClient.h"

namespace blink {
class WebFrame;
}

namespace mojo {
class Rect;
class View;
}

namespace html_viewer {

class GeolocationClientImpl;
class HTMLFrameDelegate;
class HTMLFrameTreeManager;
class TouchHandler;
class WebLayerImpl;
class WebLayerTreeViewImpl;

// Frame is used to represent a single frame in the frame tree of a page. The
// frame is either local or remote. Each Frame is associated with a single
// HTMLFrameTreeManager and can not be moved to another HTMLFrameTreeManager.
// Local frames have a mojo::View, remote frames do not.
//
// HTMLFrame serves as the FrameTreeClient. It implements it by forwarding
// the calls to HTMLFrameTreeManager so that HTMLFrameTreeManager can update
// the frame tree as appropriate.
//
// Local frames may share the connection (and client implementation) with an
// ancestor. This happens when a child frame is created. Once a navigate
// happens the frame is swapped to a remote frame.
//
// Remote frames may become local again if the embed happens in the same
// process. See HTMLFrameTreeManager for details.
class HTMLFrame : public blink::WebFrameClient,
                  public blink::WebViewClient,
                  public blink::WebRemoteFrameClient,
                  public mandoline::FrameTreeClient,
                  public mojo::ViewObserver {
 public:
  struct CreateParams {
    CreateParams(HTMLFrameTreeManager* manager, HTMLFrame* parent, uint32_t id)
        : manager(manager), parent(parent), id(id) {}
    ~CreateParams() {}

    HTMLFrameTreeManager* manager;
    HTMLFrame* parent;
    uint32_t id;
  };

  explicit HTMLFrame(const CreateParams& params);

  void Init(mojo::View* local_view,
            const mojo::Map<mojo::String, mojo::Array<uint8_t>>& properties);

  // Closes and deletes this Frame.
  void Close();

  uint32_t id() const { return id_; }

  // Returns the Frame whose id is |id|.
  HTMLFrame* FindFrame(uint32_t id) {
    return const_cast<HTMLFrame*>(
        const_cast<const HTMLFrame*>(this)->FindFrame(id));
  }
  const HTMLFrame* FindFrame(uint32_t id) const;

  HTMLFrame* parent() { return parent_; }

  const std::vector<HTMLFrame*>& children() { return children_; }

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

  HTMLFrameTreeManager* frame_tree_manager() { return frame_tree_manager_; }

  // Returns true if this or one of the frames descendants is local.
  bool HasLocalDescendant() const;

 private:
  friend class HTMLFrameTreeManager;

  virtual ~HTMLFrame();

  void set_delegate(HTMLFrameDelegate* delegate) { delegate_ = delegate; }

  // Binds this frame to the specified server. |this| serves as the
  // FrameTreeClient for the server.
  void Bind(mandoline::FrameTreeServerPtr frame_tree_server,
            mojo::InterfaceRequest<mandoline::FrameTreeClient>
                frame_tree_client_request);

  // Sets the appropriate value from the client property. |name| identifies
  // the property and |new_data| the new value.
  void SetValueFromClientProperty(const std::string& name,
                                  mojo::Array<uint8_t> new_data);

  // Returns true if the Frame is local, false if remote.
  bool IsLocal() const;

  // The local root is the first ancestor (starting at this) that has its own
  // connection.
  HTMLFrame* GetLocalRoot();

  // Returns the ApplicationImpl from the local root's delegate.
  mojo::ApplicationImpl* GetLocalRootApp();

  void SetView(mojo::View* view);

  // Creates the appropriate WebWidget implementation for the Frame.
  void CreateRootWebWidget();
  void CreateLocalRootWebWidget(blink::WebLocalFrame* local_frame);

  void InitializeWebWidget();

  void UpdateFocus();

  // Updates the size and scale factor of the webview and related classes from
  // |root_|.
  void UpdateWebViewSizeFromViewSize();

  // Swaps this frame from a local frame to remote frame. |request| is the url
  // to load in the frame.
  void SwapToRemote(const blink::WebURLRequest& request);

  // See comment in SwapToRemote() for details on this.
  void FinishSwapToRemote();

  // Swaps this frame from a remote frame to a local frame.
  void SwapToLocal(
      mojo::View* view,
      const mojo::Map<mojo::String, mojo::Array<uint8_t>>& properties);

  GlobalState* global_state() { return frame_tree_manager_->global_state(); }

  // Returns the Frame associated with the specified WebFrame.
  HTMLFrame* FindFrameWithWebFrame(blink::WebFrame* web_frame);

  // The various frameDetached() implementations call into this.
  void FrameDetachedImpl(blink::WebFrame* web_frame);

  // Update text input state from WebView to mojo::View. If the focused element
  // is editable and |show_ime| is True, the software keyboard will be shown.
  void UpdateTextInputState(bool show_ime);

  // mojo::ViewObserver methods:
  void OnViewBoundsChanged(mojo::View* view,
                           const mojo::Rect& old_bounds,
                           const mojo::Rect& new_bounds) override;
  void OnViewDestroyed(mojo::View* view) override;
  void OnViewInputEvent(mojo::View* view, const mojo::EventPtr& event) override;
  void OnViewFocusChanged(mojo::View* gained_focus,
                          mojo::View* lost_focus) override;

  // mandoline::FrameTreeClient:
  void OnConnect(mandoline::FrameTreeServerPtr server,
                 mojo::Array<mandoline::FrameDataPtr> frame_data) override;
  void OnFrameAdded(mandoline::FrameDataPtr frame_data) override;
  void OnFrameRemoved(uint32_t frame_id) override;
  void OnFrameClientPropertyChanged(uint32_t frame_id,
                                    const mojo::String& name,
                                    mojo::Array<uint8_t> new_value) override;

  // WebViewClient methods:
  virtual blink::WebStorageNamespace* createSessionStorageNamespace();
  virtual void didCancelCompositionOnSelectionChange();
  virtual void didChangeContents();

  // WebWidgetClient methods:
  virtual void initializeLayerTreeView();
  virtual blink::WebLayerTreeView* layerTreeView();
  virtual void resetInputMethod();
  virtual void didHandleGestureEvent(const blink::WebGestureEvent& event,
                                     bool eventCancelled);
  virtual void didUpdateTextOfFocusedElementByNonUserInput();
  virtual void showImeIfNeeded();

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
  virtual void didCommitProvisionalLoad(
      blink::WebLocalFrame* frame,
      const blink::WebHistoryItem& item,
      blink::WebHistoryCommitType commit_type);

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

  HTMLFrameTreeManager* frame_tree_manager_;
  HTMLFrame* parent_;
  // |view_| is non-null for local frames or remote frames that were once
  // local.
  mojo::View* view_;
  // The id for this frame. If there is a view, this is the same id as the
  // view has.
  const uint32_t id_;
  std::vector<HTMLFrame*> children_;
  blink::WebFrame* web_frame_;
  blink::WebWidget* web_widget_;
  scoped_ptr<GeolocationClientImpl> geolocation_client_impl_;
  scoped_ptr<WebLayerTreeViewImpl> web_layer_tree_view_impl_;
  scoped_ptr<TouchHandler> touch_handler_;

  blink::WebTreeScopeType scope_;

  scoped_ptr<WebLayerImpl> web_layer_;

  HTMLFrameDelegate* delegate_;
  scoped_ptr<mojo::Binding<mandoline::FrameTreeClient>>
      frame_tree_client_binding_;
  mandoline::FrameTreeServerPtr server_;

  blink::WebTextInputInfo text_input_info_;

  base::WeakPtrFactory<HTMLFrame> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(HTMLFrame);
};

}  // namespace html_viewer

#endif  // COMPONENTS_HTML_VIEWER_HTML_FRAME_H_
