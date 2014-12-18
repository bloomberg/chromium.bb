// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_GUEST_VIEW_MIME_HANDLER_VIEW_CONTAINER_H_
#define EXTENSIONS_RENDERER_GUEST_VIEW_MIME_HANDLER_VIEW_CONTAINER_H_

#include "extensions/renderer/guest_view/guest_view_container.h"
#include "third_party/WebKit/public/platform/WebURLLoader.h"
#include "third_party/WebKit/public/platform/WebURLLoaderClient.h"
#include "url/gurl.h"

namespace extensions {

// A container for loading up an extension inside a BrowserPlugin to handle a
// MIME type. A request for the URL of the data to load inside the container is
// made and a url is sent back in response which points to the URL which the
// container should be navigated to. There are two cases for making this URL
// request, the case where the plugin is embedded and the case where it is top
// level:
// 1) In the top level case a URL request for the data to load has already been
//    made by the renderer on behalf of the plugin. The |DidReceiveData| and
//    |DidFinishLoading| callbacks (from BrowserPluginDelegate) will be called
//    when data is received and when it has finished being received,
//    respectively.
// 2) In the embedded case, no URL request is automatically made by the
//    renderer. We make a URL request for the data inside the container using
//    a WebURLLoader. In this case, the |didReceiveData| and |didFinishLoading|
//    (from WebURLLoaderClient) when data is received and when it has finished
//    being received.
class MimeHandlerViewContainer : public GuestViewContainer,
                                 public blink::WebURLLoaderClient {
 public:
  MimeHandlerViewContainer(content::RenderFrame* render_frame,
                           const std::string& mime_type,
                           const GURL& original_url);
  ~MimeHandlerViewContainer() override;

  // BrowserPluginDelegate implementation.
  void DidFinishLoading() override;
  void DidReceiveData(const char* data, int data_length) override;
  bool OnMessageReceived(const IPC::Message& message) override;
  void Ready() override;

  // WebURLLoaderClient overrides.
  void didReceiveData(blink::WebURLLoader* loader,
                      const char* data,
                      int data_length,
                      int encoded_data_length) override;
  void didFinishLoading(blink::WebURLLoader* loader,
                        double finish_time,
                        int64_t total_encoded_data_length) override;

 private:
  // Message handlers.
  void OnCreateMimeHandlerViewGuestACK(int element_instance_id);

  void CreateMimeHandlerViewGuest();

  // The MIME type of the plugin.
  const std::string mime_type_;

  // The URL of the extension to navigate to.
  std::string html_string_;

  // Whether the plugin is embedded or not.
  bool is_embedded_;

  // The original URL of the plugin.
  GURL original_url_;

  // A URL loader to load the |original_url_| when the plugin is embedded. In
  // the embedded case, no URL request is made automatically.
  scoped_ptr<blink::WebURLLoader> loader_;

  DISALLOW_COPY_AND_ASSIGN(MimeHandlerViewContainer);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_GUEST_VIEW_MIME_HANDLER_VIEW_CONTAINER_H_
