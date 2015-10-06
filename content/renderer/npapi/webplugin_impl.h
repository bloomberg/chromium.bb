// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_NPAPI_WEBPLUGIN_IMPL_H_
#define CONTENT_RENDERER_NPAPI_WEBPLUGIN_IMPL_H_

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/child/npapi/webplugin.h"
#include "content/common/content_export.h"
#include "content/common/webplugin_geometry.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURLLoaderClient.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/web/WebPlugin.h"
#include "ui/gfx/native_widget_types.h"
#include "url/gurl.h"

namespace cc {
class IOSurfaceLayer;
}

namespace blink {
class WebFrame;
class WebLayer;
class WebPluginContainer;
class WebURLResponse;
class WebURLLoader;
class WebURLRequest;
struct WebPluginParams;
}

namespace content {
class MultipartResponseDelegate;
class RenderFrameImpl;
class RenderViewImpl;
class WebPluginDelegateProxy;

// This is the WebKit side of the plugin implementation that forwards calls,
// after changing out of WebCore types, to a delegate.  The delegate may
// be in a different process.
class WebPluginImpl : public WebPlugin,
                      public blink::WebPlugin {
 public:
  WebPluginImpl(
      blink::WebFrame* frame,
      const blink::WebPluginParams& params,
      const base::FilePath& file_path,
      const base::WeakPtr<RenderViewImpl>& render_view,
      RenderFrameImpl* render_frame);
  ~WebPluginImpl() override;

  // Helper function for sorting post data.
  CONTENT_EXPORT static bool SetPostData(blink::WebURLRequest* request,
                                         const char* buf,
                                         uint32 length);

  blink::WebFrame* webframe() { return webframe_; }

  // blink::WebPlugin methods:
  bool initialize(blink::WebPluginContainer* container) override;
  void destroy() override;
  NPObject* scriptableObject() override;
  struct _NPP* pluginNPP() override;
  bool getFormValue(blink::WebString& value) override;
  void layoutIfNeeded() override;
  void paint(blink::WebCanvas* canvas,
             const blink::WebRect& paint_rect) override;
  void updateGeometry(const blink::WebRect& window_rect,
                      const blink::WebRect& clip_rect,
                      const blink::WebRect& unobscured_rect,
                      const blink::WebVector<blink::WebRect>& cut_outs_rects,
                      bool is_visible) override;
  void updateFocus(bool focused, blink::WebFocusType focus_type) override;
  void updateVisibility(bool visible) override;
  bool acceptsInputEvents() override;
  bool handleInputEvent(const blink::WebInputEvent& event,
                        blink::WebCursorInfo& cursor_info) override;
  void didReceiveResponse(const blink::WebURLResponse& response) override;
  void didReceiveData(const char* data, int data_length) override;
  void didFinishLoading() override;
  void didFailLoading(const blink::WebURLError& error) override;
  void didFinishLoadingFrameRequest(const blink::WebURL& url,
                                    void* notify_data) override;
  void didFailLoadingFrameRequest(const blink::WebURL& url,
                                  void* notify_data,
                                  const blink::WebURLError& error) override;
  bool isPlaceholder() override;

  // WebPlugin implementation:
  void SetWindow(gfx::PluginWindowHandle window) override;
  void SetAcceptsInputEvents(bool accepts) override;
  void WillDestroyWindow(gfx::PluginWindowHandle window) override;
  void CancelResource(unsigned long id) override;
  void Invalidate() override;
  void InvalidateRect(const gfx::Rect& rect) override;
  NPObject* GetWindowScriptNPObject() override;
  NPObject* GetPluginElement() override;
  bool FindProxyForUrl(const GURL& url, std::string* proxy_list) override;
  void SetCookie(const GURL& url,
                 const GURL& first_party_for_cookies,
                 const std::string& cookie) override;
  std::string GetCookies(const GURL& url,
                         const GURL& first_party_for_cookies) override;
  void HandleURLRequest(const char* url,
                        const char* method,
                        const char* target,
                        const char* buf,
                        unsigned int len,
                        int notify_id,
                        bool popups_allowed,
                        bool notify_redirects) override;
  void CancelDocumentLoad() override;
  void InitiateHTTPRangeRequest(const char* url,
                                const char* range_info,
                                int pending_request_id) override;
  void DidStartLoading() override;
  void DidStopLoading() override;
  bool IsOffTheRecord() override;
  void SetDeferResourceLoading(unsigned long resource_id, bool defer) override;
  void URLRedirectResponse(bool allow, int resource_id) override;
  bool CheckIfRunInsecureContent(const GURL& url) override;
#if defined(OS_WIN)
  void SetWindowlessData(HANDLE pump_messages_event,
                         gfx::NativeViewId dummy_activation_window) override {}
  void ReparentPluginWindow(HWND window, HWND parent) { }
  void ReportExecutableMemory(size_t size) { }
#endif
#if defined(OS_MACOSX)
  WebPluginAcceleratedSurface* GetAcceleratedSurface(
      gfx::GpuPreference gpu_preference) override;
  void AcceleratedPluginEnabledRendering() override;
  void AcceleratedPluginAllocatedIOSurface(int32 width,
                                           int32 height,
                                           uint32 surface_id) override;
  void AcceleratedPluginSwappedIOSurface() override;
#endif

 private:
  // Given a (maybe partial) url, completes using the base url.
  GURL CompleteURL(const char* url);

  enum RoutingStatus {
    ROUTED,
    NOT_ROUTED,
    INVALID_URL,
    GENERAL_FAILURE
  };

  // Determines the referrer value sent along with outgoing HTTP requests
  // issued by plugins.
  enum ReferrerValue {
    PLUGIN_SRC,
    DOCUMENT_URL,
    NO_REFERRER
  };

  // Given a download request, check if we need to route the output to a frame.
  // Returns ROUTED if the load is done and routed to a frame, NOT_ROUTED or
  // corresponding error codes otherwise.
  RoutingStatus RouteToFrame(const char* url,
                             bool is_javascript_url,
                             bool popups_allowed,
                             const char* method,
                             const char* target,
                             const char* buf,
                             unsigned int len,
                             int notify_id,
                             ReferrerValue referrer_flag);

  // Returns the next avaiable resource id. Returns 0 if the operation fails.
  // It may fail if the page has already been closed.
  unsigned long GetNextResourceId();

  // Initiates HTTP GET/POST requests.
  // Returns true on success.
  bool InitiateHTTPRequest(unsigned long resource_id,
                           WebPluginResourceClient* client,
                           const GURL& url,
                           const char* method,
                           const char* buf,
                           int len,
                           const char* range_info,
                           ReferrerValue referrer_flag,
                           bool notify_redirects,
                           bool check_mixed_scripting);

  gfx::Rect GetWindowClipRect(const gfx::Rect& rect);

  // Sets the actual Widget for the plugin.
  void SetContainer(blink::WebPluginContainer* container);

  // Destroys the plugin instance.
  // The response_handle_to_ignore parameter if not NULL indicates the
  // resource handle to be left valid during plugin shutdown.
  void TearDownPluginInstance(blink::WebURLLoader* loader_to_ignore);

  // WebURLLoaderClient implementation.  We implement this interface in the
  // renderer process, and then use the simple WebPluginResourceClient interface
  // to relay the callbacks to the plugin.
  void willFollowRedirect(blink::WebURLLoader* loader,
                          blink::WebURLRequest& new_request,
                          const blink::WebURLResponse& response);
  void didSendData(blink::WebURLLoader* loader,
                   unsigned long long bytes_sent,
                   unsigned long long total_bytes_to_be_sent);
  void didReceiveResponse(blink::WebURLLoader* loader,
                                  const blink::WebURLResponse& response);

  void didReceiveData(blink::WebURLLoader* loader, const char *buffer,
                      int data_length, int encoded_data_length);
  void didFinishLoading(blink::WebURLLoader* loader,
                        double finishTime);
  void didFail(blink::WebURLLoader* loader,
               const blink::WebURLError& error);

  // Helper function to remove the stored information about a resource
  // request given its index in m_clients.
  void RemoveClient(size_t i);

  // Helper function to remove the stored information about a resource
  // request given a handle.
  void RemoveClient(blink::WebURLLoader* loader);

  // Handles HTTP multipart responses, i.e. responses received with a HTTP
  // status code of 206.
  // Returns false if response is not multipart (may be if we requested
  // single range).
  bool HandleHttpMultipartResponse(const blink::WebURLResponse& response,
                                   WebPluginResourceClient* client);

  void HandleURLRequestInternal(const char* url,
                                const char* method,
                                const char* target,
                                const char* buf,
                                unsigned int len,
                                int notify_id,
                                bool popups_allowed,
                                ReferrerValue referrer_flag,
                                bool notify_redirects,
                                bool check_mixed_scripting);

  // Tears down the existing plugin instance and creates a new plugin instance
  // to handle the response identified by the loader parameter.
  bool ReinitializePluginForResponse(blink::WebURLLoader* loader);

  // Delayed task for downloading the plugin source URL.
  void OnDownloadPluginSrcUrl();

  struct ClientInfo;

  // Helper functions
  WebPluginResourceClient* GetClientFromLoader(blink::WebURLLoader* loader);
  ClientInfo* GetClientInfoFromLoader(blink::WebURLLoader* loader);

  // Helper function to set the referrer on the request passed in.
  void SetReferrer(blink::WebURLRequest* request, ReferrerValue referrer_flag);

  // Check for invalid chars like @, ;, \ before the first / (in path).
  bool IsValidUrl(const GURL& url, ReferrerValue referrer_flag);

  std::vector<ClientInfo> clients_;

  bool windowless_;
  gfx::PluginWindowHandle window_;
#if defined(OS_MACOSX)
  bool next_io_surface_allocated_;
  int32 next_io_surface_width_;
  int32 next_io_surface_height_;
  uint32 next_io_surface_id_;
  scoped_refptr<cc::IOSurfaceLayer> io_surface_layer_;
  scoped_ptr<blink::WebLayer> web_layer_;
#endif
  bool accepts_input_events_;
  RenderFrameImpl* render_frame_;
  base::WeakPtr<RenderViewImpl> render_view_;
  blink::WebFrame* webframe_;

  WebPluginDelegateProxy* delegate_;

  // This is just a weak reference.
  blink::WebPluginContainer* container_;

  // Unique identifier for this plugin, used to track script objects.
  struct _NPP* npp_;

  typedef std::map<WebPluginResourceClient*, MultipartResponseDelegate*>
      MultiPartResponseHandlerMap;
  // Tracks HTTP multipart response handlers instantiated for
  // a WebPluginResourceClient instance.
  MultiPartResponseHandlerMap multi_part_response_map_;

  // The plugin source URL.
  GURL plugin_url_;

  // Indicates if the download would be initiated by the plugin or us.
  bool load_manually_;

  // Indicates if this is the first geometry update received by the plugin.
  bool first_geometry_update_;

  // Set to true if the next response error should be ignored.
  bool ignore_response_error_;

  // The current plugin geometry and clip rectangle.
  WebPluginGeometry geometry_;

  // The location of the plugin on disk.
  base::FilePath file_path_;

  // The mime type of the plugin.
  std::string mime_type_;

  // Holds the list of argument names and values passed to the plugin.  We keep
  // these so that we can re-initialize the plugin if we need to.
  std::vector<std::string> arg_names_;
  std::vector<std::string> arg_values_;

  class LoaderClient : public blink::WebURLLoaderClient {
   public:
    LoaderClient(WebPluginImpl*);

    void willFollowRedirect(blink::WebURLLoader*,
                            blink::WebURLRequest&,
                            const blink::WebURLResponse&) override;
    void didSendData(blink::WebURLLoader*,
                     unsigned long long bytesSent,
                     unsigned long long totalBytesToBeSent) override;
    void didReceiveResponse(blink::WebURLLoader*,
                            const blink::WebURLResponse&) override;
    void didDownloadData(blink::WebURLLoader*,
                         int dataLength,
                         int encodedDataLength) override;
    void didReceiveData(blink::WebURLLoader*,
                        const char* data,
                        int dataLength,
                        int encodedDataLength) override;
    void didReceiveCachedMetadata(blink::WebURLLoader*,
                                  const char* data,
                                  int dataLength) override;
    void didFinishLoading(blink::WebURLLoader*,
                          double finishTime,
                          int64_t total_encoded_data_length) override;
    void didFail(blink::WebURLLoader*, const blink::WebURLError&) override;

   private:
    WebPluginImpl* parent_;
  };

  LoaderClient loader_client_;

  base::WeakPtrFactory<WebPluginImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebPluginImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_NPAPI_WEBPLUGIN_IMPL_H_
