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
  RenderFrameImpl(RenderViewImpl* render_view, int routing_id);
  virtual ~RenderFrameImpl();

  // IPC::Sender
  virtual bool Send(IPC::Message* msg) OVERRIDE;

  // IPC::Listener
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;

  // WebKit::WebFrameClient implementation -------------------------------------
  virtual WebKit::WebPlugin* createPlugin(
      WebKit::WebFrame* frame,
      const WebKit::WebPluginParams& params) OVERRIDE;
  virtual WebKit::WebSharedWorker* createSharedWorker(
      WebKit::WebFrame* frame,
      const WebKit::WebURL& url,
      const WebKit::WebString& name,
      unsigned long long document_id) OVERRIDE;
  virtual WebKit::WebMediaPlayer* createMediaPlayer(
      WebKit::WebFrame* frame,
      const WebKit::WebURL& url,
      WebKit::WebMediaPlayerClient* client) OVERRIDE;
  virtual WebKit::WebApplicationCacheHost* createApplicationCacheHost(
      WebKit::WebFrame* frame,
      WebKit::WebApplicationCacheHostClient* client) OVERRIDE;
  virtual WebKit::WebCookieJar* cookieJar(WebKit::WebFrame* frame) OVERRIDE;
  virtual void didAccessInitialDocument(WebKit::WebFrame* frame) OVERRIDE;
  virtual void didCreateFrame(WebKit::WebFrame* parent,
                              WebKit::WebFrame* child) OVERRIDE;
  virtual void didDisownOpener(WebKit::WebFrame* frame) OVERRIDE;
  virtual void frameDetached(WebKit::WebFrame* frame) OVERRIDE;
  virtual void willClose(WebKit::WebFrame* frame) OVERRIDE;
  virtual void didChangeName(WebKit::WebFrame* frame,
                             const WebKit::WebString& name) OVERRIDE;
  virtual void loadURLExternally(WebKit::WebFrame* frame,
                                 const WebKit::WebURLRequest& request,
                                 WebKit::WebNavigationPolicy policy) OVERRIDE;
  virtual void loadURLExternally(
      WebKit::WebFrame* frame,
      const WebKit::WebURLRequest& request,
      WebKit::WebNavigationPolicy policy,
      const WebKit::WebString& suggested_name) OVERRIDE;
  // The WebDataSource::ExtraData* is assumed to be a DocumentState* subclass.
  virtual WebKit::WebNavigationPolicy decidePolicyForNavigation(
      WebKit::WebFrame* frame,
      WebKit::WebDataSource::ExtraData* extraData,
      const WebKit::WebURLRequest& request,
      WebKit::WebNavigationType type,
      WebKit::WebNavigationPolicy default_policy,
      bool is_redirect);
  // DEPRECATED.
  virtual WebKit::WebNavigationPolicy decidePolicyForNavigation(
      WebKit::WebFrame* frame,
      const WebKit::WebURLRequest& request,
      WebKit::WebNavigationType type,
      WebKit::WebNavigationPolicy default_policy,
      bool is_redirect) OVERRIDE;
  virtual WebKit::WebURLError cannotHandleRequestError(
      WebKit::WebFrame* frame,
      const WebKit::WebURLRequest& request) OVERRIDE;
  virtual WebKit::WebURLError cancelledError(
      WebKit::WebFrame* frame,
      const WebKit::WebURLRequest& request) OVERRIDE;
  virtual void unableToImplementPolicyWithError(
      WebKit::WebFrame* frame,
      const WebKit::WebURLError& error) OVERRIDE;
  virtual void willSendSubmitEvent(WebKit::WebFrame* frame,
                                   const WebKit::WebFormElement& form) OVERRIDE;
  virtual void willSubmitForm(WebKit::WebFrame* frame,
                              const WebKit::WebFormElement& form) OVERRIDE;
  virtual void willPerformClientRedirect(WebKit::WebFrame* frame,
                                         const WebKit::WebURL& from,
                                         const WebKit::WebURL& to,
                                         double interval,
                                         double fire_time) OVERRIDE;
  virtual void didCancelClientRedirect(WebKit::WebFrame* frame) OVERRIDE;
  virtual void didCompleteClientRedirect(WebKit::WebFrame* frame,
                                         const WebKit::WebURL& from) OVERRIDE;
  virtual void didCreateDataSource(WebKit::WebFrame* frame,
                                   WebKit::WebDataSource* datasource) OVERRIDE;
  virtual void didStartProvisionalLoad(WebKit::WebFrame* frame) OVERRIDE;
  virtual void didReceiveServerRedirectForProvisionalLoad(
      WebKit::WebFrame* frame) OVERRIDE;
  virtual void didFailProvisionalLoad(
      WebKit::WebFrame* frame,
      const WebKit::WebURLError& error) OVERRIDE;
  virtual void didReceiveDocumentData(WebKit::WebFrame* frame,
                                      const char* data,
                                      size_t length,
                                      bool& prevent_default) OVERRIDE;
  virtual void didCommitProvisionalLoad(WebKit::WebFrame* frame,
                                        bool is_new_navigation) OVERRIDE;
  virtual void didClearWindowObject(WebKit::WebFrame* frame) OVERRIDE;
  virtual void didCreateDocumentElement(WebKit::WebFrame* frame) OVERRIDE;
  virtual void didReceiveTitle(WebKit::WebFrame* frame,
                               const WebKit::WebString& title,
                               WebKit::WebTextDirection direction) OVERRIDE;
  virtual void didChangeIcon(WebKit::WebFrame* frame,
                             WebKit::WebIconURL::Type icon_type) OVERRIDE;
  virtual void didFinishDocumentLoad(WebKit::WebFrame* frame) OVERRIDE;
  virtual void didHandleOnloadEvents(WebKit::WebFrame* frame) OVERRIDE;
  virtual void didFailLoad(WebKit::WebFrame* frame,
                           const WebKit::WebURLError& error) OVERRIDE;
  virtual void didFinishLoad(WebKit::WebFrame* frame) OVERRIDE;
  virtual void didNavigateWithinPage(WebKit::WebFrame* frame,
                                     bool is_new_navigation) OVERRIDE;
  virtual void didUpdateCurrentHistoryItem(WebKit::WebFrame* frame) OVERRIDE;
  virtual void willSendRequest(
      WebKit::WebFrame* frame,
      unsigned identifier,
      WebKit::WebURLRequest& request,
      const WebKit::WebURLResponse& redirect_response) OVERRIDE;
  virtual void didReceiveResponse(
      WebKit::WebFrame* frame,
      unsigned identifier,
      const WebKit::WebURLResponse& response) OVERRIDE;
  virtual void didFinishResourceLoad(WebKit::WebFrame* frame,
                                     unsigned identifier) OVERRIDE;
  virtual void didFailResourceLoad(WebKit::WebFrame* frame,
                                   unsigned identifier,
                                   const WebKit::WebURLError& error) OVERRIDE;
  virtual void didLoadResourceFromMemoryCache(
      WebKit::WebFrame* frame,
      const WebKit::WebURLRequest& request,
      const WebKit::WebURLResponse& response) OVERRIDE;
  virtual void didDisplayInsecureContent(WebKit::WebFrame* frame) OVERRIDE;
  virtual void didRunInsecureContent(WebKit::WebFrame* frame,
                                     const WebKit::WebSecurityOrigin& origin,
                                     const WebKit::WebURL& target) OVERRIDE;
  virtual void didExhaustMemoryAvailableForScript(
      WebKit::WebFrame* frame) OVERRIDE;
  virtual void didCreateScriptContext(WebKit::WebFrame* frame,
                                      v8::Handle<v8::Context> context,
                                      int extension_group,
                                      int world_id) OVERRIDE;
  virtual void willReleaseScriptContext(WebKit::WebFrame* frame,
                                        v8::Handle<v8::Context> context,
                                        int world_id) OVERRIDE;
  virtual void didFirstVisuallyNonEmptyLayout(WebKit::WebFrame* frame) OVERRIDE;
  virtual void didChangeContentsSize(WebKit::WebFrame* frame,
                                     const WebKit::WebSize& size) OVERRIDE;
  virtual void didChangeScrollOffset(WebKit::WebFrame* frame) OVERRIDE;
  virtual void willInsertBody(WebKit::WebFrame* frame) OVERRIDE;
  virtual void reportFindInPageMatchCount(int request_id,
                                          int count,
                                          bool final_update) OVERRIDE;
  virtual void reportFindInPageSelection(int request_id,
                                         int active_match_ordinal,
                                         const WebKit::WebRect& sel) OVERRIDE;
  virtual void openFileSystem(
      WebKit::WebFrame* frame,
      WebKit::WebFileSystemType type,
      long long size,
      bool create,
      WebKit::WebFileSystemCallbacks* callbacks) OVERRIDE;
  virtual void deleteFileSystem(
      WebKit::WebFrame* frame,
      WebKit::WebFileSystemType type,
      WebKit::WebFileSystemCallbacks* callbacks) OVERRIDE;
  virtual void queryStorageUsageAndQuota(
      WebKit::WebFrame* frame,
      WebKit::WebStorageQuotaType type,
      WebKit::WebStorageQuotaCallbacks* callbacks) OVERRIDE;
  virtual void requestStorageQuota(
      WebKit::WebFrame* frame,
      WebKit::WebStorageQuotaType type,
      unsigned long long requested_size,
      WebKit::WebStorageQuotaCallbacks* callbacks) OVERRIDE;
  virtual void willOpenSocketStream(
      WebKit::WebSocketStreamHandle* handle) OVERRIDE;
  virtual void willStartUsingPeerConnectionHandler(
      WebKit::WebFrame* frame,
      WebKit::WebRTCPeerConnectionHandler* handler) OVERRIDE;
  virtual bool willCheckAndDispatchMessageEvent(
      WebKit::WebFrame* sourceFrame,
      WebKit::WebFrame* targetFrame,
      WebKit::WebSecurityOrigin targetOrigin,
      WebKit::WebDOMMessageEvent event) OVERRIDE;
  virtual WebKit::WebString userAgentOverride(
      WebKit::WebFrame* frame,
      const WebKit::WebURL& url) OVERRIDE;
  virtual WebKit::WebString doNotTrackValue(WebKit::WebFrame* frame) OVERRIDE;
  virtual bool allowWebGL(WebKit::WebFrame* frame, bool default_value) OVERRIDE;
  virtual void didLoseWebGLContext(WebKit::WebFrame* frame,
                                   int arb_robustness_status_code) OVERRIDE;

  // RenderFrameImpl methods
  int routing_id() { return routing_id_; }

 private:
  RenderViewImpl* render_view_;
  int routing_id_;

  DISALLOW_COPY_AND_ASSIGN(RenderFrameImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_RENDER_FRAME_IMPL_H_
