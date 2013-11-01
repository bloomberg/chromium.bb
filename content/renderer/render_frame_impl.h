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
      NON_EXPORTED_BASE(public WebKit::WebFrameClient) {
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

  // WebKit::WebFrameClient implementation -------------------------------------
  virtual WebKit::WebPlugin* createPlugin(
      WebKit::WebFrame* frame,
      const WebKit::WebPluginParams& params);
  virtual WebKit::WebMediaPlayer* createMediaPlayer(
      WebKit::WebFrame* frame,
      const WebKit::WebURL& url,
      WebKit::WebMediaPlayerClient* client);
  virtual WebKit::WebApplicationCacheHost* createApplicationCacheHost(
      WebKit::WebFrame* frame,
      WebKit::WebApplicationCacheHostClient* client);
  virtual WebKit::WebWorkerPermissionClientProxy*
      createWorkerPermissionClientProxy(WebKit::WebFrame* frame);
  virtual WebKit::WebCookieJar* cookieJar(WebKit::WebFrame* frame);
  virtual WebKit::WebServiceWorkerProvider* createServiceWorkerProvider(
      WebKit::WebFrame* frame,
      WebKit::WebServiceWorkerProviderClient*);
  virtual void didAccessInitialDocument(WebKit::WebFrame* frame);
  virtual WebKit::WebFrame* createChildFrame(WebKit::WebFrame* parent,
                                             const WebKit::WebString& name);
  virtual void didDisownOpener(WebKit::WebFrame* frame);
  virtual void frameDetached(WebKit::WebFrame* frame);
  virtual void willClose(WebKit::WebFrame* frame);
  virtual void didChangeName(WebKit::WebFrame* frame,
                             const WebKit::WebString& name);
  virtual void didMatchCSS(
      WebKit::WebFrame* frame,
      const WebKit::WebVector<WebKit::WebString>& newly_matching_selectors,
      const WebKit::WebVector<WebKit::WebString>& stopped_matching_selectors);
  virtual void loadURLExternally(WebKit::WebFrame* frame,
                                 const WebKit::WebURLRequest& request,
                                 WebKit::WebNavigationPolicy policy);
  virtual void loadURLExternally(
      WebKit::WebFrame* frame,
      const WebKit::WebURLRequest& request,
      WebKit::WebNavigationPolicy policy,
      const WebKit::WebString& suggested_name);
  virtual WebKit::WebNavigationPolicy decidePolicyForNavigation(
      WebKit::WebFrame* frame,
      WebKit::WebDataSource::ExtraData* extra_data,
      const WebKit::WebURLRequest& request,
      WebKit::WebNavigationType type,
      WebKit::WebNavigationPolicy default_policy,
      bool is_redirect);
  // DEPRECATED
  virtual WebKit::WebNavigationPolicy decidePolicyForNavigation(
      WebKit::WebFrame* frame,
      const WebKit::WebURLRequest& request,
      WebKit::WebNavigationType type,
      WebKit::WebNavigationPolicy default_policy,
      bool is_redirect);
  virtual void willSendSubmitEvent(WebKit::WebFrame* frame,
                                   const WebKit::WebFormElement& form);
  virtual void willSubmitForm(WebKit::WebFrame* frame,
                              const WebKit::WebFormElement& form);
  virtual void didCreateDataSource(WebKit::WebFrame* frame,
                                   WebKit::WebDataSource* datasource);
  virtual void didStartProvisionalLoad(WebKit::WebFrame* frame);
  virtual void didReceiveServerRedirectForProvisionalLoad(
      WebKit::WebFrame* frame);
  virtual void didFailProvisionalLoad(
      WebKit::WebFrame* frame,
      const WebKit::WebURLError& error);
  virtual void didCommitProvisionalLoad(WebKit::WebFrame* frame,
                                        bool is_new_navigation);
  virtual void didClearWindowObject(WebKit::WebFrame* frame);
  virtual void didCreateDocumentElement(WebKit::WebFrame* frame);
  virtual void didReceiveTitle(WebKit::WebFrame* frame,
                               const WebKit::WebString& title,
                               WebKit::WebTextDirection direction);
  virtual void didChangeIcon(WebKit::WebFrame* frame,
                             WebKit::WebIconURL::Type icon_type);
  virtual void didFinishDocumentLoad(WebKit::WebFrame* frame);
  virtual void didHandleOnloadEvents(WebKit::WebFrame* frame);
  virtual void didFailLoad(WebKit::WebFrame* frame,
                           const WebKit::WebURLError& error);
  virtual void didFinishLoad(WebKit::WebFrame* frame);
  virtual void didNavigateWithinPage(WebKit::WebFrame* frame,
                                     bool is_new_navigation);
  virtual void didUpdateCurrentHistoryItem(WebKit::WebFrame* frame);
  virtual void willRequestAfterPreconnect(WebKit::WebFrame* frame,
                                          WebKit::WebURLRequest& request);
  virtual void willSendRequest(
      WebKit::WebFrame* frame,
      unsigned identifier,
      WebKit::WebURLRequest& request,
      const WebKit::WebURLResponse& redirect_response);
  virtual void didReceiveResponse(
      WebKit::WebFrame* frame,
      unsigned identifier,
      const WebKit::WebURLResponse& response);
  virtual void didFinishResourceLoad(WebKit::WebFrame* frame,
                                     unsigned identifier);
  virtual void didLoadResourceFromMemoryCache(
      WebKit::WebFrame* frame,
      const WebKit::WebURLRequest& request,
      const WebKit::WebURLResponse& response);
  virtual void didDisplayInsecureContent(WebKit::WebFrame* frame);
  virtual void didRunInsecureContent(WebKit::WebFrame* frame,
                                     const WebKit::WebSecurityOrigin& origin,
                                     const WebKit::WebURL& target);
  virtual void didAbortLoading(WebKit::WebFrame* frame);
  virtual void didExhaustMemoryAvailableForScript(
      WebKit::WebFrame* frame);
  virtual void didCreateScriptContext(WebKit::WebFrame* frame,
                                      v8::Handle<v8::Context> context,
                                      int extension_group,
                                      int world_id);
  virtual void willReleaseScriptContext(WebKit::WebFrame* frame,
                                        v8::Handle<v8::Context> context,
                                        int world_id);
  virtual void didFirstVisuallyNonEmptyLayout(WebKit::WebFrame* frame);
  virtual void didChangeContentsSize(WebKit::WebFrame* frame,
                                     const WebKit::WebSize& size);
  virtual void didChangeScrollOffset(WebKit::WebFrame* frame);
  virtual void willInsertBody(WebKit::WebFrame* frame);
  virtual void reportFindInPageMatchCount(int request_id,
                                          int count,
                                          bool final_update);
  virtual void reportFindInPageSelection(int request_id,
                                         int active_match_ordinal,
                                         const WebKit::WebRect& sel);
  virtual void requestStorageQuota(
      WebKit::WebFrame* frame,
      WebKit::WebStorageQuotaType type,
      unsigned long long requested_size,
      WebKit::WebStorageQuotaCallbacks* callbacks);
  virtual void willOpenSocketStream(
      WebKit::WebSocketStreamHandle* handle);
  virtual void willStartUsingPeerConnectionHandler(
      WebKit::WebFrame* frame,
      WebKit::WebRTCPeerConnectionHandler* handler);
  virtual bool willCheckAndDispatchMessageEvent(
      WebKit::WebFrame* sourceFrame,
      WebKit::WebFrame* targetFrame,
      WebKit::WebSecurityOrigin targetOrigin,
      WebKit::WebDOMMessageEvent event);
  virtual WebKit::WebString userAgentOverride(
      WebKit::WebFrame* frame,
      const WebKit::WebURL& url);
  virtual WebKit::WebString doNotTrackValue(WebKit::WebFrame* frame);
  virtual bool allowWebGL(WebKit::WebFrame* frame, bool default_value);
  virtual void didLoseWebGLContext(WebKit::WebFrame* frame,
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
