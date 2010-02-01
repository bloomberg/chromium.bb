// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_PLUGIN_WEBPLUGIN_DELEGATE_STUB_H_
#define CHROME_PLUGIN_WEBPLUGIN_DELEGATE_STUB_H_

#include <queue>
#include <string>
#include <vector>

#include "base/gfx/rect.h"
#include "base/ref_counted.h"
#include "base/shared_memory.h"
#include "base/task.h"
#include "chrome/common/transport_dib.h"
#include "chrome/plugin/command_buffer_stub.h"
#include "googleurl/src/gurl.h"
#include "ipc/ipc_channel.h"
#include "third_party/npapi/bindings/npapi.h"

class PluginChannel;
class WebPluginProxy;
struct PluginMsg_Init_Params;
struct PluginMsg_DidReceiveResponseParams;
struct PluginMsg_UpdateGeometry_Param;
class WebCursor;

namespace WebKit {
class WebInputEvent;
}

class WebPluginDelegateImpl;

// Converts the IPC messages from WebPluginDelegateProxy into calls to the
// actual WebPluginDelegate object.
class WebPluginDelegateStub : public IPC::Channel::Listener,
                              public IPC::Message::Sender,
                              public base::RefCounted<WebPluginDelegateStub> {
 public:
  WebPluginDelegateStub(const std::string& mime_type, int instance_id,
                        PluginChannel* channel);

  // IPC::Channel::Listener implementation:
  virtual void OnMessageReceived(const IPC::Message& msg);

  // IPC::Message::Sender implementation:
  virtual bool Send(IPC::Message* msg);

  int instance_id() { return instance_id_; }
  WebPluginProxy* webplugin() { return webplugin_; }

 private:
  friend class base::RefCounted<WebPluginDelegateStub>;

  ~WebPluginDelegateStub();

  // Message handlers for the WebPluginDelegate calls that are proxied from the
  // renderer over the IPC channel.
  void OnInit(const PluginMsg_Init_Params& params, bool* result);
  void OnWillSendRequest(int id, const GURL& url);
  void OnDidReceiveResponse(const PluginMsg_DidReceiveResponseParams& params);
  void OnDidReceiveData(int id, const std::vector<char>& buffer,
                        int data_offset);
  void OnDidFinishLoading(int id);
  void OnDidFail(int id);
  void OnDidFinishLoadWithReason(const GURL& url, int reason, int notify_id);
  void OnSetFocus();
  void OnHandleInputEvent(const WebKit::WebInputEvent* event,
                          bool* handled, WebCursor* cursor);
  void OnPaint(const gfx::Rect& damaged_rect);
  void OnDidPaint();
  void OnPrint(base::SharedMemoryHandle* shared_memory, size_t* size);
  void OnUpdateGeometry(const PluginMsg_UpdateGeometry_Param& param);
  void OnGetPluginScriptableObject(int* route_id);
  void OnSendJavaScriptStream(const GURL& url,
                              const std::string& result,
                              bool success,
                              int notify_id);

#if defined(OS_MACOSX)
  void OnSetWindowFocus(bool has_focus);
#endif

  void OnDidReceiveManualResponse(
      const GURL& url,
      const PluginMsg_DidReceiveResponseParams& params);
  void OnDidReceiveManualData(const std::vector<char>& buffer);
  void OnDidFinishManualLoading();
  void OnDidManualLoadFail();
  void OnInstallMissingPlugin();
  void OnHandleURLRequestReply(unsigned long resource_id,
                               const GURL& url,
                               int notify_id);
  void OnHTTPRangeRequestReply(unsigned long resource_id, int range_request_id);
  void OnCreateCommandBuffer(int* route_id);
  void CreateSharedBuffer(size_t size,
                          base::SharedMemory* shared_buf,
                          base::SharedMemoryHandle* remote_handle);

  std::string mime_type_;
  int instance_id_;

  scoped_refptr<PluginChannel> channel_;

  WebPluginDelegateImpl* delegate_;
  WebPluginProxy* webplugin_;
  bool in_destructor_;

  // The url of the main frame hosting the plugin.
  GURL page_url_;

#if defined(ENABLE_GPU)
  // If this is the GPU plugin, the stub object that forwards to the
  // command buffer service.
  scoped_ptr<CommandBufferStub> command_buffer_stub_;
#endif

  DISALLOW_IMPLICIT_CONSTRUCTORS(WebPluginDelegateStub);
};

#endif  // CHROME_PLUGIN_WEBPLUGIN_DELEGATE_STUB_H_
