// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HTML_VIEWER_HTML_FRAME_H_
#define COMPONENTS_HTML_VIEWER_HTML_FRAME_H_

#include <stdint.h>

#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "cc/layers/surface_layer.h"
#include "components/html_viewer/html_frame_tree_manager.h"
#include "components/html_viewer/replicated_frame_state.h"
#include "components/mus/public/cpp/input_event_handler.h"
#include "components/mus/public/cpp/window_observer.h"
#include "components/web_view/public/interfaces/frame.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/services/tracing/public/interfaces/tracing.mojom.h"
#include "third_party/WebKit/public/platform/WebMediaPlayer.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/web/WebFrameClient.h"
#include "third_party/WebKit/public/web/WebRemoteFrameClient.h"
#include "third_party/WebKit/public/web/WebSandboxFlags.h"
#include "third_party/WebKit/public/web/WebTextInputInfo.h"

namespace cc_blink {
class WebLayerImpl;
}

namespace blink {
class WebFrame;
class WebWidget;
}

namespace mojo {
class ApplicationImpl;
class Rect;
}

namespace mus {
class ScopedWindowPtr;
class Window;
}

namespace html_viewer {

class DevToolsAgentImpl;
class GeolocationClientImpl;
class HTMLFrameDelegate;
class HTMLFrameTreeManager;
class HTMLWidget;
class TouchHandler;
class WebLayerTreeViewImpl;

// Frame is used to represent a single frame in the frame tree of a page. The
// frame is either local or remote. Each Frame is associated with a single
// HTMLFrameTreeManager and can not be moved to another HTMLFrameTreeManager.
// Local frames have a mus::Window, remote frames do not.
//
// HTMLFrame serves as the FrameClient. It implements it by forwarding the
// calls to HTMLFrameTreeManager so that HTMLFrameTreeManager can update the
// frame tree as appropriate.
//
// Local frames may share the connection (and client implementation) with an
// ancestor. This happens when a child frame is created. Once a navigate
// happens the frame is swapped to a remote frame.
//
// Remote frames may become local again if the embed happens in the same
// process. See HTMLFrameTreeManager for details.
class HTMLFrame : public blink::WebFrameClient,
                  public blink::WebRemoteFrameClient,
                  public web_view::mojom::FrameClient,
                  public mus::WindowObserver,
                  public mus::InputEventHandler {
 public:
  struct CreateParams {
    CreateParams(
        HTMLFrameTreeManager* manager,
        HTMLFrame* parent,
        uint32_t id,
        mus::Window* window,
        const mojo::Map<mojo::String, mojo::Array<uint8_t>>& properties,
        HTMLFrameDelegate* delegate)
        : manager(manager),
          parent(parent),
          id(id),
          window(window),
          properties(properties),
          delegate(delegate),
          is_local_create_child(false) {}
    ~CreateParams() {}

    HTMLFrameTreeManager* manager;
    HTMLFrame* parent;
    uint32_t id;
    mus::Window* window;
    const mojo::Map<mojo::String, mojo::Array<uint8_t>>& properties;
    HTMLFrameDelegate* delegate;

   private:
    friend class HTMLFrame;

    // True if the creation is the result of createChildFrame(). That is, a
    // local parent is creating a new child frame.
    bool is_local_create_child;
  };

  explicit HTMLFrame(CreateParams* params);

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
  blink::WebWidget* GetWebWidget();

  // The mus::Window this frame renders to. This is non-null for the local frame
  // the frame tree was created with as well as non-null for any frames created
  // locally.
  mus::Window* window() { return window_; }

  HTMLFrameTreeManager* frame_tree_manager() { return frame_tree_manager_; }

  // Returns null if the browser side didn't request to setup an agent in this
  // frame.
  DevToolsAgentImpl* devtools_agent() { return devtools_agent_.get(); }

  // Returns true if the Frame is local, false if remote.
  bool IsLocal() const;

  // Returns true if this or one of the frames descendants is local.
  bool HasLocalDescendant() const;

  void LoadRequest(const blink::WebURLRequest& request,
                   base::TimeTicks navigation_start_time);

 protected:
  ~HTMLFrame() override;

  // WebFrameClient methods:
  blink::WebMediaPlayer* createMediaPlayer(
      blink::WebLocalFrame* frame,
      blink::WebMediaPlayer::LoadType load_type,
      const blink::WebURL& url,
      blink::WebMediaPlayerClient* client,
      blink::WebMediaPlayerEncryptedMediaClient* encrypted_client,
      blink::WebContentDecryptionModule* initial_cdm,
      const blink::WebString& sink_id,
      blink::WebMediaSession* media_session) override;
  blink::WebFrame* createChildFrame(
      blink::WebLocalFrame* parent,
      blink::WebTreeScopeType scope,
      const blink::WebString& frame_ame,
      blink::WebSandboxFlags sandbox_flags,
      const blink::WebFrameOwnerProperties& frame_owner_properties) override;
  void frameDetached(blink::WebFrame* frame,
                     blink::WebFrameClient::DetachType type) override;
  blink::WebCookieJar* cookieJar(blink::WebLocalFrame* frame) override;
  blink::WebNavigationPolicy decidePolicyForNavigation(
      const NavigationPolicyInfo& info) override;
  bool hasPendingNavigation(blink::WebLocalFrame* frame) override;
  void didHandleOnloadEvents(blink::WebLocalFrame* frame) override;
  void didAddMessageToConsole(const blink::WebConsoleMessage& message,
                              const blink::WebString& source_name,
                              unsigned source_line,
                              const blink::WebString& stack_trace) override;
  void didFinishLoad(blink::WebLocalFrame* frame) override;
  void didNavigateWithinPage(blink::WebLocalFrame* frame,
                             const blink::WebHistoryItem& history_item,
                             blink::WebHistoryCommitType commit_type) override;
  blink::WebGeolocationClient* geolocationClient() override;
  blink::WebEncryptedMediaClient* encryptedMediaClient() override;
  void didStartLoading(bool to_different_document) override;
  void didStopLoading() override;
  void didChangeLoadProgress(double load_progress) override;
  void dispatchLoad() override;
  void didChangeName(blink::WebLocalFrame* frame,
                     const blink::WebString& name) override;
  void didCommitProvisionalLoad(
      blink::WebLocalFrame* frame,
      const blink::WebHistoryItem& item,
      blink::WebHistoryCommitType commit_type) override;
  void didReceiveTitle(blink::WebLocalFrame* frame,
                       const blink::WebString& title,
                       blink::WebTextDirection direction) override;
  void reportFindInFrameMatchCount(int identifier,
                                   int count,
                                   bool finalUpdate) override;
  void reportFindInPageSelection(int identifier,
                                 int activeMatchOrdinal,
                                 const blink::WebRect& selection) override;
  bool shouldSearchSingleFrame() override;

 private:
  friend class HTMLFrameTreeManager;

  // Binds this frame to the specified server. |this| serves as the
  // FrameClient for the server.
  void Bind(web_view::mojom::FramePtr frame,
            mojo::InterfaceRequest<web_view::mojom::FrameClient>
                frame_client_request);

  // Sets the appropriate value from the client property. |name| identifies
  // the property and |new_data| the new value.
  void SetValueFromClientProperty(const std::string& name,
                                  mojo::Array<uint8_t> new_data);

  // The local root is the first ancestor (starting at this) that has its own
  // delegate.
  HTMLFrame* GetFirstAncestorWithDelegate();

  // Returns the ApplicationImpl from the first ancestor with a delegate.
  mojo::ApplicationImpl* GetApp();

  // Gets the server Frame to use for this frame.
  web_view::mojom::Frame* GetServerFrame();

  void SetWindow(mus::Window* window);

  // Creates the appropriate WebWidget implementation for the Frame.
  void CreateRootWebWidget();
  void CreateLocalRootWebWidget(blink::WebLocalFrame* local_frame);

  void UpdateFocus();

  // Swaps this frame from a local frame to remote frame. |request| is the url
  // to load in the frame.
  void SwapToRemote();

  // Swaps this frame from a remote frame to a local frame.
  void SwapToLocal(
      HTMLFrameDelegate* delegate,
      mus::Window* window,
      const mojo::Map<mojo::String, mojo::Array<uint8_t>>& properties);

  // Invoked when changing the delegate. This informs the new delegate to take
  // over. This is used when a different connection is going to take over
  // responsibility for the frame.
  void SwapDelegate(HTMLFrameDelegate* delegate);

  GlobalState* global_state() { return frame_tree_manager_->global_state(); }

  // Returns the focused element if the focused element is in this
  // frame. Returns an empty one otherwise.
  blink::WebElement GetFocusedElement();

  // Returns the Frame associated with the specified WebFrame.
  HTMLFrame* FindFrameWithWebFrame(blink::WebFrame* web_frame);

  // The various frameDetached() implementations call into this.
  void FrameDetachedImpl(blink::WebFrame* web_frame);

  // mus::WindowObserver methods:
  void OnWindowBoundsChanged(mus::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds) override;
  void OnWindowDestroyed(mus::Window* window) override;
  void OnWindowFocusChanged(mus::Window* gained_focus,
                            mus::Window* lost_focus) override;

  // mus::InputEventHandler:
  void OnWindowInputEvent(mus::Window* window,
                          mus::mojom::EventPtr event,
                          scoped_ptr<base::Closure>* ack_callback) override;

  // web_view::mojom::FrameClient:
  void OnConnect(web_view::mojom::FramePtr server,
                 uint32_t change_id,
                 uint32_t window_id,
                 web_view::mojom::WindowConnectType window_connect_type,
                 mojo::Array<web_view::mojom::FrameDataPtr> frame_data,
                 int64_t navigation_start_time_ticks,
                 const OnConnectCallback& callback) override;
  void OnFrameAdded(uint32_t change_id,
                    web_view::mojom::FrameDataPtr frame_data) override;
  void OnFrameRemoved(uint32_t change_id, uint32_t frame_id) override;
  void OnFrameClientPropertyChanged(uint32_t frame_id,
                                    const mojo::String& name,
                                    mojo::Array<uint8_t> new_value) override;
  void OnPostMessageEvent(
      uint32_t source_frame_id,
      uint32_t target_frame_id,
      web_view::mojom::HTMLMessageEventPtr serialized_event) override;
  void OnWillNavigate(const mojo::String& origin,
                      const OnWillNavigateCallback& callback) override;
  void OnFrameLoadingStateChanged(uint32_t frame_id, bool loading) override;
  void OnDispatchFrameLoadEvent(uint32_t frame_id) override;
  void Find(int32_t request_id,
            const mojo::String& search_text,
            web_view::mojom::FindOptionsPtr options,
            bool wrap_within_frame,
            const FindCallback& callback) override;
  void StopFinding(bool clear_selection) override;
  void HighlightFindResults(int32_t request_id,
                            const mojo::String& search_test,
                            web_view::mojom::FindOptionsPtr options,
                            bool reset) override;
  void StopHighlightingFindResults() override;

  // blink::WebRemoteFrameClient:
  void frameDetached(blink::WebRemoteFrameClient::DetachType type) override;
  void postMessageEvent(blink::WebLocalFrame* source_web_frame,
                        blink::WebRemoteFrame* target_web_frame,
                        blink::WebSecurityOrigin target_origin,
                        blink::WebDOMMessageEvent event) override;
  void initializeChildFrame(const blink::WebRect& frame_rect,
                            float scale_factor) override;
  void navigate(const blink::WebURLRequest& request,
                bool should_replace_current_entry) override;
  void reload(bool ignore_cache, bool is_client_redirect) override;
  void frameRectsChanged(const blink::WebRect& frame_rect) override;

  HTMLFrameTreeManager* frame_tree_manager_;
  HTMLFrame* parent_;
  // |window_| is non-null for local frames or remote frames that were once
  // local.
  mus::Window* window_;
  // The id for this frame. If there is a window, this is the same id as the
  // window has.
  const uint32_t id_;
  std::vector<HTMLFrame*> children_;
  blink::WebFrame* web_frame_;
  scoped_ptr<HTMLWidget> html_widget_;
  scoped_ptr<GeolocationClientImpl> geolocation_client_impl_;
  scoped_ptr<TouchHandler> touch_handler_;

  scoped_ptr<cc_blink::WebLayerImpl> web_layer_;
  scoped_refptr<cc::SurfaceLayer> surface_layer_;

  HTMLFrameDelegate* delegate_;
  scoped_ptr<mojo::Binding<web_view::mojom::FrameClient>> frame_client_binding_;
  web_view::mojom::FramePtr server_;

  ReplicatedFrameState state_;

  // If this frame is the result of creating a local frame
  // (createChildFrame()), then |owned_window_| is the Window initially created
  // for the frame. While the frame is local |owned_window_| is the same as
  // |window_|. If this frame becomes remote |window_| is set to null and
  // |owned_window_| remains as the Window initially created for the frame.
  //
  // This is done to ensure the Window isn't prematurely deleted (it must exist
  // as long as the frame is valid). If the Window was deleted as soon as the
  // frame was swapped to remote then the process rendering to the window would
  // be severed.
  scoped_ptr<mus::ScopedWindowPtr> owned_window_;

  // This object is only valid in the context of performance tests.
  tracing::StartupPerformanceDataCollectorPtr
      startup_performance_data_collector_;

  scoped_ptr<DevToolsAgentImpl> devtools_agent_;

  // A navigation request has been sent to the frame server side, and we haven't
  // received response to it.
  bool pending_navigation_;

  base::TimeTicks navigation_start_time_;

  base::WeakPtrFactory<HTMLFrame> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(HTMLFrame);
};

}  // namespace html_viewer

#endif  // COMPONENTS_HTML_VIEWER_HTML_FRAME_H_
