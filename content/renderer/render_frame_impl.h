// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDER_FRAME_IMPL_H_
#define CONTENT_RENDERER_RENDER_FRAME_IMPL_H_

#include <set>
#include <vector>

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/observer_list.h"
#include "base/process/process_handle.h"
#include "base/strings/string16.h"
#include "content/public/renderer/render_frame.h"
#include "ipc/ipc_message.h"
#include "third_party/WebKit/public/web/WebDataSource.h"
#include "third_party/WebKit/public/web/WebFrameClient.h"

class TransportDIB;

namespace blink {
class WebMouseEvent;
struct WebCompositionUnderline;
struct WebCursorInfo;
}

namespace gfx {
class Range;
class Rect;
}

namespace content {

class PepperPluginInstanceImpl;
class RendererPpapiHost;
class RenderFrameObserver;
class RenderViewImpl;
class RenderWidget;
class RenderWidgetFullscreenPepper;

class CONTENT_EXPORT RenderFrameImpl
    : public RenderFrame,
      NON_EXPORTED_BASE(public blink::WebFrameClient) {
 public:
  // Creates a new RenderFrame. |render_view| is the RenderView object that this
  // frame belongs to.
  static RenderFrameImpl* Create(RenderViewImpl* render_view, int32 routing_id);

  // Used by content_layouttest_support to hook into the creation of
  // RenderFrameImpls.
  static void InstallCreateHook(
      RenderFrameImpl* (*create_render_frame_impl)(RenderViewImpl*, int32));

  virtual ~RenderFrameImpl();

  // TODO(jam): this is a temporary getter until all the code is transitioned
  // to using RenderFrame instead of RenderView.
  RenderViewImpl* render_view() { return render_view_; }

  // Returns the RenderWidget associated with this frame.
  RenderWidget* GetRenderWidget();

#if defined(ENABLE_PLUGINS)
  // Notification that a PPAPI plugin has been created.
  void PepperPluginCreated(RendererPpapiHost* host);

  // Indicates that the given instance has been created.
  void PepperInstanceCreated(PepperPluginInstanceImpl* instance);

  // Indicates that the given instance is being destroyed. This is called from
  // the destructor, so it's important that the instance is not dereferenced
  // from this call.
  void PepperInstanceDeleted(PepperPluginInstanceImpl* instance);

  // Notifies that |instance| has changed the cursor.
  // This will update the cursor appearance if it is currently over the plugin
  // instance.
  void PepperDidChangeCursor(PepperPluginInstanceImpl* instance,
                             const blink::WebCursorInfo& cursor);

  // Notifies that |instance| has received a mouse event.
  void PepperDidReceiveMouseEvent(PepperPluginInstanceImpl* instance);

  // Notification that the given plugin is focused or unfocused.
  void PepperFocusChanged(PepperPluginInstanceImpl* instance, bool focused);

  // Informs the render view that a PPAPI plugin has changed text input status.
  void PepperTextInputTypeChanged(PepperPluginInstanceImpl* instance);
  void PepperCaretPositionChanged(PepperPluginInstanceImpl* instance);

  // Cancels current composition.
  void PepperCancelComposition(PepperPluginInstanceImpl* instance);

  // Informs the render view that a PPAPI plugin has changed selection.
  void PepperSelectionChanged(PepperPluginInstanceImpl* instance);

  // Creates a fullscreen container for a pepper plugin instance.
  RenderWidgetFullscreenPepper* CreatePepperFullscreenContainer(
      PepperPluginInstanceImpl* plugin);

  bool IsPepperAcceptingCompositionEvents() const;

  // Notification that the given plugin has crashed.
  void PluginCrashed(const base::FilePath& plugin_path,
                     base::ProcessId plugin_pid);

  // These map to virtual methods on RenderWidget that are used to call out to
  // RenderView.
  // TODO(jam): once we get rid of RenderView, RenderFrame will own RenderWidget
  // and methods would be on a delegate interface.
  void DidInitiatePaint();
  void DidFlushPaint();
  PepperPluginInstanceImpl* GetBitmapForOptimizedPluginPaint(
      const gfx::Rect& paint_bounds,
      TransportDIB** dib,
      gfx::Rect* location,
      gfx::Rect* clip,
      float* scale_factor);
  void PageVisibilityChanged(bool shown);
  void OnSetFocus(bool enable);
  void WillHandleMouseEvent(const blink::WebMouseEvent& event);

  // Simulates IME events for testing purpose.
  void SimulateImeSetComposition(
      const base::string16& text,
      const std::vector<blink::WebCompositionUnderline>& underlines,
      int selection_start,
      int selection_end);
  void SimulateImeConfirmComposition(const base::string16& text,
                                     const gfx::Range& replacement_range);

  // TODO(jam): remove these once the IPC handler moves from RenderView to
  // RenderFrame.
  void OnImeSetComposition(
    const base::string16& text,
    const std::vector<blink::WebCompositionUnderline>& underlines,
    int selection_start,
    int selection_end);
 void OnImeConfirmComposition(
    const base::string16& text,
    const gfx::Range& replacement_range,
    bool keep_selection);
#endif  // ENABLE_PLUGINS

  // IPC::Sender
  virtual bool Send(IPC::Message* msg) OVERRIDE;

  // IPC::Listener
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;

  // RenderFrame implementation:
  virtual RenderView* GetRenderView() OVERRIDE;
  virtual int GetRoutingID() OVERRIDE;
  virtual WebPreferences& GetWebkitPreferences() OVERRIDE;
  virtual int ShowContextMenu(ContextMenuClient* client,
                              const ContextMenuParams& params) OVERRIDE;
  virtual void CancelContextMenu(int request_id) OVERRIDE;
  virtual blink::WebPlugin* CreatePlugin(
      blink::WebFrame* frame,
      const WebPluginInfo& info,
      const blink::WebPluginParams& params) OVERRIDE;
  virtual void LoadURLExternally(
      blink::WebFrame* frame,
      const blink::WebURLRequest& request,
      blink::WebNavigationPolicy policy) OVERRIDE;

  // blink::WebFrameClient implementation -------------------------------------
  virtual blink::WebPlugin* createPlugin(
      blink::WebFrame* frame,
      const blink::WebPluginParams& params);
  virtual blink::WebMediaPlayer* createMediaPlayer(
      blink::WebFrame* frame,
      const blink::WebURL& url,
      blink::WebMediaPlayerClient* client);
  virtual blink::WebApplicationCacheHost* createApplicationCacheHost(
      blink::WebFrame* frame,
      blink::WebApplicationCacheHostClient* client);
  virtual blink::WebWorkerPermissionClientProxy*
      createWorkerPermissionClientProxy(blink::WebFrame* frame);
  virtual blink::WebCookieJar* cookieJar(blink::WebFrame* frame);
  virtual blink::WebServiceWorkerProvider* createServiceWorkerProvider(
      blink::WebFrame* frame,
      blink::WebServiceWorkerProviderClient*);
  virtual void didAccessInitialDocument(blink::WebFrame* frame);
  virtual blink::WebFrame* createChildFrame(blink::WebFrame* parent,
                                             const blink::WebString& name);
  virtual void didDisownOpener(blink::WebFrame* frame);
  virtual void frameDetached(blink::WebFrame* frame);
  virtual void willClose(blink::WebFrame* frame);
  virtual void didChangeName(blink::WebFrame* frame,
                             const blink::WebString& name);
  virtual void didMatchCSS(
      blink::WebFrame* frame,
      const blink::WebVector<blink::WebString>& newly_matching_selectors,
      const blink::WebVector<blink::WebString>& stopped_matching_selectors);
  virtual void loadURLExternally(blink::WebFrame* frame,
                                 const blink::WebURLRequest& request,
                                 blink::WebNavigationPolicy policy);
  virtual void loadURLExternally(
      blink::WebFrame* frame,
      const blink::WebURLRequest& request,
      blink::WebNavigationPolicy policy,
      const blink::WebString& suggested_name);
  virtual blink::WebNavigationPolicy decidePolicyForNavigation(
      blink::WebFrame* frame,
      blink::WebDataSource::ExtraData* extra_data,
      const blink::WebURLRequest& request,
      blink::WebNavigationType type,
      blink::WebNavigationPolicy default_policy,
      bool is_redirect);
  // DEPRECATED
  virtual blink::WebNavigationPolicy decidePolicyForNavigation(
      blink::WebFrame* frame,
      const blink::WebURLRequest& request,
      blink::WebNavigationType type,
      blink::WebNavigationPolicy default_policy,
      bool is_redirect);
  virtual void willSendSubmitEvent(blink::WebFrame* frame,
                                   const blink::WebFormElement& form);
  virtual void willSubmitForm(blink::WebFrame* frame,
                              const blink::WebFormElement& form);
  virtual void didCreateDataSource(blink::WebFrame* frame,
                                   blink::WebDataSource* datasource);
  virtual void didStartProvisionalLoad(blink::WebFrame* frame);
  virtual void didReceiveServerRedirectForProvisionalLoad(
      blink::WebFrame* frame);
  virtual void didFailProvisionalLoad(
      blink::WebFrame* frame,
      const blink::WebURLError& error);
  virtual void didCommitProvisionalLoad(blink::WebFrame* frame,
                                        bool is_new_navigation);
  virtual void didClearWindowObject(blink::WebFrame* frame);
  virtual void didCreateDocumentElement(blink::WebFrame* frame);
  virtual void didReceiveTitle(blink::WebFrame* frame,
                               const blink::WebString& title,
                               blink::WebTextDirection direction);
  virtual void didChangeIcon(blink::WebFrame* frame,
                             blink::WebIconURL::Type icon_type);
  virtual void didFinishDocumentLoad(blink::WebFrame* frame);
  virtual void didHandleOnloadEvents(blink::WebFrame* frame);
  virtual void didFailLoad(blink::WebFrame* frame,
                           const blink::WebURLError& error);
  virtual void didFinishLoad(blink::WebFrame* frame);
  virtual void didNavigateWithinPage(blink::WebFrame* frame,
                                     bool is_new_navigation);
  virtual void didUpdateCurrentHistoryItem(blink::WebFrame* frame);
  virtual void willRequestAfterPreconnect(blink::WebFrame* frame,
                                          blink::WebURLRequest& request);
  virtual void willSendRequest(
      blink::WebFrame* frame,
      unsigned identifier,
      blink::WebURLRequest& request,
      const blink::WebURLResponse& redirect_response);
  virtual void didReceiveResponse(
      blink::WebFrame* frame,
      unsigned identifier,
      const blink::WebURLResponse& response);
  virtual void didFinishResourceLoad(blink::WebFrame* frame,
                                     unsigned identifier);
  virtual void didLoadResourceFromMemoryCache(
      blink::WebFrame* frame,
      const blink::WebURLRequest& request,
      const blink::WebURLResponse& response);
  virtual void didDisplayInsecureContent(blink::WebFrame* frame);
  virtual void didRunInsecureContent(blink::WebFrame* frame,
                                     const blink::WebSecurityOrigin& origin,
                                     const blink::WebURL& target);
  virtual void didAbortLoading(blink::WebFrame* frame);
  virtual void didExhaustMemoryAvailableForScript(
      blink::WebFrame* frame);
  virtual void didCreateScriptContext(blink::WebFrame* frame,
                                      v8::Handle<v8::Context> context,
                                      int extension_group,
                                      int world_id);
  virtual void willReleaseScriptContext(blink::WebFrame* frame,
                                        v8::Handle<v8::Context> context,
                                        int world_id);
  virtual void didFirstVisuallyNonEmptyLayout(blink::WebFrame* frame);
  virtual void didChangeContentsSize(blink::WebFrame* frame,
                                     const blink::WebSize& size);
  virtual void didChangeScrollOffset(blink::WebFrame* frame);
  virtual void willInsertBody(blink::WebFrame* frame);
  virtual void reportFindInPageMatchCount(int request_id,
                                          int count,
                                          bool final_update);
  virtual void reportFindInPageSelection(int request_id,
                                         int active_match_ordinal,
                                         const blink::WebRect& sel);
  virtual void requestStorageQuota(
      blink::WebFrame* frame,
      blink::WebStorageQuotaType type,
      unsigned long long requested_size,
      blink::WebStorageQuotaCallbacks* callbacks);
  virtual void willOpenSocketStream(
      blink::WebSocketStreamHandle* handle);
  virtual void willStartUsingPeerConnectionHandler(
      blink::WebFrame* frame,
      blink::WebRTCPeerConnectionHandler* handler);
  virtual bool willCheckAndDispatchMessageEvent(
      blink::WebFrame* sourceFrame,
      blink::WebFrame* targetFrame,
      blink::WebSecurityOrigin targetOrigin,
      blink::WebDOMMessageEvent event);
  virtual blink::WebString userAgentOverride(
      blink::WebFrame* frame,
      const blink::WebURL& url);
  virtual blink::WebString doNotTrackValue(blink::WebFrame* frame);
  virtual bool allowWebGL(blink::WebFrame* frame, bool default_value);
  virtual void didLoseWebGLContext(blink::WebFrame* frame,
                                   int arb_robustness_status_code);

 protected:
  RenderFrameImpl(RenderViewImpl* render_view, int32 routing_id);

 private:
  friend class RenderFrameObserver;

  // Functions to add and remove observers for this object.
  void AddObserver(RenderFrameObserver* observer);
  void RemoveObserver(RenderFrameObserver* observer);

  RenderViewImpl* render_view_;
  int routing_id_;
  bool is_swapped_out_;
  bool is_detaching_;

#if defined(ENABLE_PLUGINS)
  typedef std::set<PepperPluginInstanceImpl*> PepperPluginSet;
  PepperPluginSet active_pepper_instances_;

  // Current text input composition text. Empty if no composition is in
  // progress.
  base::string16 pepper_composition_text_;
#endif

  // All the registered observers.
  ObserverList<RenderFrameObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(RenderFrameImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_RENDER_FRAME_IMPL_H_
