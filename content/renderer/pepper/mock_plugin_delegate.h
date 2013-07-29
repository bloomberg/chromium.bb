// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_MOCK_PLUGIN_DELEGATE_H_
#define CONTENT_RENDERER_PEPPER_MOCK_PLUGIN_DELEGATE_H_

#include "content/renderer/pepper/plugin_delegate.h"

struct PP_NetAddress_Private;
namespace ppapi { class PPB_X509Certificate_Fields; }

namespace content {

class MockPluginDelegate : public PluginDelegate {
 public:
  MockPluginDelegate();
  virtual ~MockPluginDelegate();

  virtual void PluginFocusChanged(PepperPluginInstanceImpl* instance,
                                  bool focused) OVERRIDE;
  virtual void PluginTextInputTypeChanged(
      PepperPluginInstanceImpl* instance) OVERRIDE;
  virtual void PluginCaretPositionChanged(
      PepperPluginInstanceImpl* instance) OVERRIDE;
  virtual void PluginRequestedCancelComposition(
      PepperPluginInstanceImpl* instance) OVERRIDE;
  virtual void PluginSelectionChanged(
      PepperPluginInstanceImpl* instance) OVERRIDE;
  virtual void PluginCrashed(PepperPluginInstanceImpl* instance) OVERRIDE;
  virtual void InstanceCreated(PepperPluginInstanceImpl* instance) OVERRIDE;
  virtual void InstanceDeleted(PepperPluginInstanceImpl* instance) OVERRIDE;
  virtual bool AsyncOpenFile(const base::FilePath& path,
                             int flags,
                             const AsyncOpenFileCallback& callback) OVERRIDE;
  virtual scoped_refptr<base::MessageLoopProxy>
      GetFileThreadMessageLoopProxy() OVERRIDE;
  virtual uint32 TCPSocketCreate() OVERRIDE;
  virtual void TCPSocketConnect(PPB_TCPSocket_Private_Impl* socket,
                                uint32 socket_id,
                                const std::string& host,
                                uint16_t port) OVERRIDE;
  virtual void TCPSocketConnectWithNetAddress(
      PPB_TCPSocket_Private_Impl* socket,
      uint32 socket_id,
      const PP_NetAddress_Private& addr) OVERRIDE;
  virtual void TCPSocketSSLHandshake(
      uint32 socket_id,
      const std::string& server_name,
      uint16_t server_port,
      const std::vector<std::vector<char> >& trusted_certs,
      const std::vector<std::vector<char> >& untrusted_certs) OVERRIDE;
  virtual void TCPSocketRead(uint32 socket_id, int32_t bytes_to_read) OVERRIDE;
  virtual void TCPSocketWrite(uint32 socket_id,
                              const std::string& buffer) OVERRIDE;
  virtual void TCPSocketDisconnect(uint32 socket_id) OVERRIDE;
  virtual void TCPSocketSetOption(
      uint32 socket_id,
      PP_TCPSocket_Option name,
      const ::ppapi::SocketOptionData& value) OVERRIDE;
  virtual void RegisterTCPSocket(PPB_TCPSocket_Private_Impl* socket,
                                 uint32 socket_id) OVERRIDE;
  virtual void TCPServerSocketListen(PP_Resource socket_resource,
                                     const PP_NetAddress_Private& addr,
                                     int32_t backlog) OVERRIDE;
  virtual void TCPServerSocketAccept(uint32 server_socket_id) OVERRIDE;
  virtual void TCPServerSocketStopListening(PP_Resource socket_resource,
                                            uint32 socket_id) OVERRIDE;
  // Add/remove a network list observer.
  virtual bool LockMouse(PepperPluginInstanceImpl* instance) OVERRIDE;
  virtual void UnlockMouse(PepperPluginInstanceImpl* instance) OVERRIDE;
  virtual bool IsMouseLocked(PepperPluginInstanceImpl* instance) OVERRIDE;
  virtual void DidChangeCursor(PepperPluginInstanceImpl* instance,
                               const WebKit::WebCursorInfo& cursor) OVERRIDE;
  virtual void DidReceiveMouseEvent(
      PepperPluginInstanceImpl* instance) OVERRIDE;
  virtual void SampleGamepads(WebKit::WebGamepads* data) OVERRIDE;
  virtual void HandleDocumentLoad(
      PepperPluginInstanceImpl* instance,
      const WebKit::WebURLResponse& response) OVERRIDE;
  virtual RendererPpapiHost* CreateExternalPluginModule(
      scoped_refptr<PluginModule> module,
      const base::FilePath& path,
      ::ppapi::PpapiPermissions permissions,
      const IPC::ChannelHandle& channel_handle,
      base::ProcessId plugin_pid,
      int plugin_child_id) OVERRIDE;
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_MOCK_PLUGIN_DELEGATE_H_
