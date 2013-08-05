// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_TCP_SERVER_SOCKET_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_TCP_SERVER_SOCKET_MESSAGE_FILTER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/renderer_host/pepper/pepper_message_filter.h"
#include "content/common/content_export.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/host/resource_message_filter.h"

struct PP_NetAddress_Private;

namespace net {
class ServerSocket;
class StreamSocket;
}

namespace content {

class BrowserPpapiHostImpl;

class CONTENT_EXPORT PepperTCPServerSocketMessageFilter
    : public ppapi::host::ResourceMessageFilter {
 public:
  PepperTCPServerSocketMessageFilter(
      BrowserPpapiHostImpl* host,
      PP_Instance instance,
      bool private_api,
      const scoped_refptr<PepperMessageFilter>& pepper_message_filter);

  static size_t GetNumInstances();

 protected:
  virtual ~PepperTCPServerSocketMessageFilter();

 private:
  enum State {
    STATE_BEFORE_LISTENING,
    STATE_LISTEN_IN_PROGRESS,
    STATE_LISTENING,
    STATE_ACCEPT_IN_PROGRESS,
    STATE_CLOSED
  };

  // ppapi::host::ResourceMessageFilter overrides.
  virtual scoped_refptr<base::TaskRunner> OverrideTaskRunnerForMessage(
      const IPC::Message& message) OVERRIDE;
  virtual int32_t OnResourceMessageReceived(
      const IPC::Message& msg,
      ppapi::host::HostMessageContext* context) OVERRIDE;

  int32_t OnMsgListen(const ppapi::host::HostMessageContext* context,
                      const PP_NetAddress_Private& addr,
                      int32_t backlog);
  int32_t OnMsgAccept(const ppapi::host::HostMessageContext* context,
                      uint32 plugin_dispatcher_id);
  int32_t OnMsgStopListening(const ppapi::host::HostMessageContext* context);

  void DoListen(const ppapi::host::ReplyMessageContext& context,
                const PP_NetAddress_Private& addr,
                int32_t backlog);

  void OnListenCompleted(const ppapi::host::ReplyMessageContext& context,
                         int net_result);
  void OnAcceptCompleted(const ppapi::host::ReplyMessageContext& context,
                         uint32 plugin_dispatcher_id,
                         int net_result);

  void SendListenReply(const ppapi::host::ReplyMessageContext& context,
                       int32_t pp_result,
                       const PP_NetAddress_Private& local_addr);
  void SendListenError(const ppapi::host::ReplyMessageContext& context,
                       int32_t pp_result);
  void SendAcceptReply(const ppapi::host::ReplyMessageContext& context,
                       int32_t pp_result,
                       uint32 accepted_socket_id,
                       const PP_NetAddress_Private& local_addr,
                       const PP_NetAddress_Private& remote_addr);
  void SendAcceptError(const ppapi::host::ReplyMessageContext& context,
                       int32_t pp_result);

  // Following fields are initialized and used only on the IO thread.
  State state_;
  scoped_ptr<net::ServerSocket> socket_;
  scoped_ptr<net::StreamSocket> socket_buffer_;
  scoped_refptr<PepperMessageFilter> pepper_message_filter_;

  // Following fields are initialized on the IO thread but used only
  // on the UI thread.
  const bool external_plugin_;
  const bool private_api_;
  int render_process_id_;
  int render_view_id_;

  DISALLOW_COPY_AND_ASSIGN(PepperTCPServerSocketMessageFilter);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_TCP_SERVER_SOCKET_MESSAGE_FILTER_H_
