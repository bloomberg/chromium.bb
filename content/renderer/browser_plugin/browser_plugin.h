// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_BROWSER_PLUGIN_BROWSER_PLUGIN_H_
#define CONTENT_RENDERER_BROWSER_PLUGIN_BROWSER_PLUGIN_H_

#include "third_party/WebKit/public/web/WebPlugin.h"

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequenced_task_runner_helpers.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "components/viz/common/surfaces/local_surface_id_allocator.h"
#include "content/public/common/screen_info.h"
#include "content/renderer/mouse_lock_dispatcher.h"
#include "content/renderer/render_view_impl.h"
#include "third_party/WebKit/public/web/WebDragStatus.h"
#include "third_party/WebKit/public/web/WebImeTextSpan.h"
#include "third_party/WebKit/public/web/WebInputMethodController.h"
#include "third_party/WebKit/public/web/WebNode.h"

#if defined(USE_AURA)
#include "content/renderer/mus/mus_embedded_frame_delegate.h"
#endif

namespace base {
class UnguessableToken;
}

namespace viz {
class SurfaceInfo;
struct SurfaceSequence;
}

namespace content {

class BrowserPluginDelegate;
class BrowserPluginManager;
class ChildFrameCompositingHelper;

#if defined(USE_AURA)
class MusEmbeddedFrame;
#endif

class CONTENT_EXPORT BrowserPlugin : public blink::WebPlugin,
#if defined(USE_AURA)
                                     public MusEmbeddedFrameDelegate,
#endif
                                     public MouseLockDispatcher::LockTarget {
 public:
  static BrowserPlugin* GetFromNode(blink::WebNode& node);

  int render_frame_routing_id() const { return render_frame_routing_id_; }
  int browser_plugin_instance_id() const { return browser_plugin_instance_id_; }
  bool attached() const { return attached_; }

  bool OnMessageReceived(const IPC::Message& msg);

  // Update Browser Plugin's DOM Node attribute |attribute_name| with the value
  // |attribute_value|.
  void UpdateDOMAttribute(const std::string& attribute_name,
                          const base::string16& attribute_value);

  // Returns whether the guest process has crashed.
  bool guest_crashed() const { return guest_crashed_; }

  // Informs the guest of an updated focus state.
  void UpdateGuestFocusState(blink::WebFocusType focus_type);

  void ScreenInfoChanged(const ScreenInfo& screen_info);

  // Indicates whether the guest should be focused.
  bool ShouldGuestBeFocused() const;

  // Called by CompositingHelper to send current SurfaceSequence to browser.
  void SendSatisfySequence(const viz::SurfaceSequence& sequence);

  // Provided that a guest instance ID has been allocated, this method attaches
  // this BrowserPlugin instance to that guest.
  void Attach();

  // This method detaches this BrowserPlugin instance from the guest that it's
  // currently attached to, if any.
  void Detach();

  // Notify the plugin about a compositor commit so that frame ACKs could be
  // sent, if needed.
  void DidCommitCompositorFrame();

  void WasResized();

  // Returns whether a message should be forwarded to BrowserPlugin.
  static bool ShouldForwardToBrowserPlugin(const IPC::Message& message);

  // blink::WebPlugin implementation.
  blink::WebPluginContainer* Container() const override;
  bool Initialize(blink::WebPluginContainer* container) override;
  void Destroy() override;
  v8::Local<v8::Object> V8ScriptableObject(v8::Isolate* isolate) override;
  bool SupportsKeyboardFocus() const override;
  bool SupportsEditCommands() const override;
  bool SupportsInputMethod() const override;
  bool CanProcessDrag() const override;
  void UpdateAllLifecyclePhases() override {}
  void Paint(blink::WebCanvas* canvas, const blink::WebRect& rect) override {}
  void UpdateGeometry(const blink::WebRect& window_rect,
                      const blink::WebRect& clip_rect,
                      const blink::WebRect& unobscured_rect,
                      bool is_visible) override;
  void UpdateFocus(bool focused, blink::WebFocusType focus_type) override;
  void UpdateVisibility(bool visible) override;
  blink::WebInputEventResult HandleInputEvent(
      const blink::WebCoalescedInputEvent& event,
      blink::WebCursorInfo& cursor_info) override;
  bool HandleDragStatusUpdate(blink::WebDragStatus drag_status,
                              const blink::WebDragData& drag_data,
                              blink::WebDragOperationsMask mask,
                              const blink::WebFloatPoint& position,
                              const blink::WebFloatPoint& screen) override;
  void DidReceiveResponse(const blink::WebURLResponse& response) override;
  void DidReceiveData(const char* data, int data_length) override;
  void DidFinishLoading() override;
  void DidFailLoading(const blink::WebURLError& error) override;
  bool ExecuteEditCommand(const blink::WebString& name) override;
  bool ExecuteEditCommand(const blink::WebString& name,
                          const blink::WebString& value) override;
  bool SetComposition(
      const blink::WebString& text,
      const blink::WebVector<blink::WebImeTextSpan>& ime_text_spans,
      const blink::WebRange& replacementRange,
      int selectionStart,
      int selectionEnd) override;
  bool CommitText(const blink::WebString& text,
                  const blink::WebVector<blink::WebImeTextSpan>& ime_text_spans,
                  const blink::WebRange& replacementRange,
                  int relative_cursor_pos) override;
  bool FinishComposingText(
      blink::WebInputMethodController::ConfirmCompositionBehavior
          selection_behavior) override;

  void ExtendSelectionAndDelete(int before, int after) override;

  // MouseLockDispatcher::LockTarget implementation.
  void OnLockMouseACK(bool succeeded) override;
  void OnMouseLockLost() override;
  bool HandleMouseLockedInputEvent(const blink::WebMouseEvent& event) override;

 private:
  friend class base::DeleteHelper<BrowserPlugin>;
  // Only the manager is allowed to create a BrowserPlugin.
  friend class BrowserPluginManager;

  // A BrowserPlugin object is a controller that represents an instance of a
  // browser plugin within the embedder renderer process. Once a BrowserPlugin
  // does an initial navigation or is attached to a newly created guest, it
  // acquires a browser_plugin_instance_id as well. The guest instance ID
  // uniquely identifies a guest WebContents that's hosted by this
  // BrowserPlugin.
  BrowserPlugin(RenderFrame* render_frame,
                const base::WeakPtr<BrowserPluginDelegate>& delegate);

  ~BrowserPlugin() override;

  const gfx::Rect& frame_rect() const {
    return pending_resize_params_.frame_rect;
  }
  gfx::Rect FrameRectInPixels() const;
  float GetDeviceScaleFactor() const;

  const ScreenInfo& screen_info() const {
    return pending_resize_params_.screen_info;
  }

  uint64_t auto_size_sequence_number() const {
    return pending_resize_params_.sequence_number;
  }

  void UpdateInternalInstanceId();

#if defined(USE_AURA)
  void CreateMusWindowAndEmbed(const base::UnguessableToken& embed_token);
#endif

  // IPC message handlers.
  // Please keep in alphabetical order.
  void OnAdvanceFocus(int instance_id, bool reverse);
  void OnGuestGone(int instance_id);
  void OnGuestReady(int instance_id, const viz::FrameSinkId& frame_sink_id);
  void OnResizeDueToAutoResize(int browser_plugin_instance_id,
                               uint64_t sequence_number);
  void OnSetChildFrameSurface(int instance_id,
                              const viz::SurfaceInfo& surface_info,
                              const viz::SurfaceSequence& sequence);
  void OnSetContentsOpaque(int instance_id, bool opaque);
  void OnSetCursor(int instance_id, const WebCursor& cursor);
  void OnSetMouseLock(int instance_id, bool enable);
#if defined(USE_AURA)
  void OnSetMusEmbedToken(int instance_id,
                          const base::UnguessableToken& embed_token);
#endif
  void OnSetTooltipText(int browser_plugin_instance_id,
                        const base::string16& tooltip_text);
  void OnShouldAcceptTouchEvents(int instance_id, bool accept);

#if defined(USE_AURA)
  // MusEmbeddedFrameDelegate
  void OnMusEmbeddedFrameSurfaceChanged(
      const viz::SurfaceInfo& surface_info) override;
  void OnMusEmbeddedFrameSinkIdAllocated(
      const viz::FrameSinkId& frame_sink_id) override;
#endif

  // This indicates whether this BrowserPlugin has been attached to a
  // WebContents and is ready to receive IPCs.
  bool attached_;
  // We cache the |render_frame_routing_id| because we need it on destruction.
  // If the RenderFrame is destroyed before the BrowserPlugin is destroyed
  // then we will attempt to access a nullptr.
  const int render_frame_routing_id_;
  blink::WebPluginContainer* container_;
  // The plugin's rect in css pixels.
  bool guest_crashed_;
  bool plugin_focused_;
  // Tracks the visibility of the browser plugin regardless of the whole
  // embedder RenderView's visibility.
  bool visible_;

  WebCursor cursor_;

  bool mouse_locked_;

  // This indicates that the BrowserPlugin has a geometry.
  bool ready_;

  // Used for HW compositing.
  std::unique_ptr<ChildFrameCompositingHelper> compositing_helper_;

  // URL for the embedder frame.
  int browser_plugin_instance_id_;

  std::vector<EditCommand> edit_commands_;

  viz::FrameSinkId frame_sink_id_;
  viz::LocalSurfaceId local_surface_id_;
  viz::LocalSurfaceIdAllocator local_surface_id_allocator_;

  bool enable_surface_synchronization_ = false;

  // TODO(fsamuel): We might want to unify this with content::ResizeParams.
  struct ResizeParams {
    gfx::Rect frame_rect;
    ScreenInfo screen_info;
    uint64_t sequence_number = 0lu;
  };

  // The last ResizeParams sent to the browser process, if any.
  base::Optional<ResizeParams> sent_resize_params_;

  // The current set of ResizeParams. This may or may not match
  // |sent_resize_params_|.
  ResizeParams pending_resize_params_;

  // We call lifetime managing methods on |delegate_|, but we do not directly
  // own this. The delegate destroys itself.
  base::WeakPtr<BrowserPluginDelegate> delegate_;

#if defined(USE_AURA)
  // Set if OnSetMusEmbedToken() is called before attached.
  base::Optional<base::UnguessableToken> pending_embed_token_;

  std::unique_ptr<MusEmbeddedFrame> mus_embedded_frame_;
#endif

  // Weak factory used in v8 |MakeWeak| callback, since the v8 callback might
  // get called after BrowserPlugin has been destroyed.
  base::WeakPtrFactory<BrowserPlugin> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BrowserPlugin);
};

}  // namespace content

#endif  // CONTENT_RENDERER_BROWSER_PLUGIN_BROWSER_PLUGIN_H_
