// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "content/common/child_thread.h"
#include "content/common/socket_stream_dispatcher.h"
#include "content/common/webkitplatformsupport_impl.h"
#include "content/public/common/content_client.h"
#include "googleurl/src/gurl.h"

#if defined(OS_ANDROID)
#include "base/file_descriptor_posix.h"
#include "base/shared_memory.h"
#include "content/common/view_messages.h"

namespace {
void RunWebAudioMediaCodec(
    base::SharedMemoryHandle encoded_data_handle,
    base::FileDescriptor pcm_output,
    size_t data_size) {
  content::ChildThread::current()->Send(
      new ViewHostMsg_RunWebAudioMediaCodec(encoded_data_handle,
                                            pcm_output,
                                            data_size));
}

} // anonymous namespace
#endif

namespace content {

WebKitPlatformSupportImpl::WebKitPlatformSupportImpl() {
}

WebKitPlatformSupportImpl::~WebKitPlatformSupportImpl() {
}

string16 WebKitPlatformSupportImpl::GetLocalizedString(int message_id) {
  return GetContentClient()->GetLocalizedString(message_id);
}

base::StringPiece WebKitPlatformSupportImpl::GetDataResource(
    int resource_id,
    ui::ScaleFactor scale_factor) {
  return GetContentClient()->GetDataResource(resource_id, scale_factor);
}

void WebKitPlatformSupportImpl::GetPlugins(
    bool refresh, std::vector<webkit::WebPluginInfo>* plugins) {
  // This should not be called except in the renderer.
  // RendererWebKitPlatformSupportImpl overrides this.
  NOTREACHED();
}

webkit_glue::ResourceLoaderBridge*
WebKitPlatformSupportImpl::CreateResourceLoader(
    const webkit_glue::ResourceLoaderBridge::RequestInfo& request_info) {
  return ChildThread::current()->CreateBridge(request_info);
}

webkit_glue::WebSocketStreamHandleBridge*
WebKitPlatformSupportImpl::CreateWebSocketBridge(
    WebKit::WebSocketStreamHandle* handle,
    webkit_glue::WebSocketStreamHandleDelegate* delegate) {
  SocketStreamDispatcher* dispatcher =
      ChildThread::current()->socket_stream_dispatcher();
  return dispatcher->CreateBridge(handle, delegate);
}

#if defined(OS_ANDROID)
webkit_media::WebAudioMediaCodecRunner
WebKitPlatformSupportImpl::GetWebAudioMediaCodecRunner() {
  return base::Bind(&RunWebAudioMediaCodec);
}
#endif

}  // namespace content
