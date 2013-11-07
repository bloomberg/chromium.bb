// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDER_FRAME_IMPL_H_
#define CONTENT_RENDERER_RENDER_FRAME_IMPL_H_

#include "base/basictypes.h"
#include "content/public/renderer/render_frame.h"
#include "ipc/ipc_message.h"
#include "third_party/WebKit/public/web/WebDataSource.h"
#include "third_party/WebKit/public/web/WebFrameClient.h"

namespace content {

class RenderViewImpl;

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

  // IPC::Sender
  virtual bool Send(IPC::Message* msg) OVERRIDE;

  // IPC::Listener
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;

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
  int GetRoutingID() const;

  RenderViewImpl* render_view_;
  int routing_id_;
  bool is_swapped_out_;
  bool is_detaching_;

  DISALLOW_COPY_AND_ASSIGN(RenderFrameImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_RENDER_FRAME_IMPL_H_
