// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_PLUGIN_DELEGATE_IMPL_H_
#define CONTENT_RENDERER_PEPPER_PEPPER_PLUGIN_DELEGATE_IMPL_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/id_map.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "content/public/renderer/render_view_observer.h"
#include "content/renderer/mouse_lock_dispatcher.h"
#include "content/renderer/pepper/pepper_browser_connection.h"
#include "content/renderer/pepper/plugin_delegate.h"
#include "content/renderer/render_view_pepper_helper.h"
#include "ppapi/c/pp_file_info.h"
#include "ppapi/shared_impl/private/ppb_tcp_server_socket_shared.h"
#include "ppapi/shared_impl/private/tcp_socket_private_impl.h"
#include "ui/base/ime/text_input_type.h"

namespace base {
class FilePath;
}

namespace ppapi {
class PepperFilePath;
class PPB_X509Certificate_Fields;
namespace host {
class ResourceHost;
}
}

namespace WebKit {
class WebGamepads;
struct WebCompositionUnderline;
}

namespace content {
class ContextProviderCommandBuffer;
class GamepadSharedMemoryReader;
class PepperBroker;
class PluginModule;
class PPB_Broker_Impl;
class RenderViewImpl;
struct WebPluginInfo;

class PepperPluginDelegateImpl
    : public PluginDelegate,
      public RenderViewPepperHelper,
      public base::SupportsWeakPtr<PepperPluginDelegateImpl>,
      public RenderViewObserver {
 public:
  explicit PepperPluginDelegateImpl(RenderViewImpl* render_view);
  virtual ~PepperPluginDelegateImpl();

  RenderViewImpl* render_view() { return render_view_; }

  PepperBrowserConnection* pepper_browser_connection() {
    return &pepper_browser_connection_;
  }

  // A pointer is returned immediately, but it is not ready to be used until
  // BrokerConnected has been called.
  // The caller is responsible for calling Disconnect() on the returned pointer
  // to clean up the corresponding resources allocated during this call.
  PepperBroker* ConnectToBroker(PPB_Broker_Impl* client);

  // Removes broker from pending_connect_broker_ if present. Returns true if so.
  bool StopWaitingForBrokerConnection(PepperBroker* broker);

  CONTENT_EXPORT int GetRoutingID() const;

 private:
  // RenderViewPepperHelper implementation.
  virtual WebKit::WebPlugin* CreatePepperWebPlugin(
    const WebPluginInfo& webplugin_info,
    const WebKit::WebPluginParams& params) OVERRIDE;
  virtual void ViewWillInitiatePaint() OVERRIDE;
  virtual void ViewInitiatedPaint() OVERRIDE;
  virtual void ViewFlushedPaint() OVERRIDE;
  virtual PepperPluginInstanceImpl* GetBitmapForOptimizedPluginPaint(
      const gfx::Rect& paint_bounds,
      TransportDIB** dib,
      gfx::Rect* location,
      gfx::Rect* clip,
      float* scale_factor) OVERRIDE;
  virtual void OnSetFocus(bool has_focus) OVERRIDE;
  virtual void PageVisibilityChanged(bool is_visible) OVERRIDE;
  virtual bool IsPluginFocused() const OVERRIDE;
  virtual gfx::Rect GetCaretBounds() const OVERRIDE;
  virtual ui::TextInputType GetTextInputType() const OVERRIDE;
  virtual bool IsPluginAcceptingCompositionEvents() const OVERRIDE;
  virtual bool CanComposeInline() const OVERRIDE;
  virtual void GetSurroundingText(string16* text,
                                  ui::Range* range) const OVERRIDE;
  virtual void OnImeSetComposition(
      const string16& text,
      const std::vector<WebKit::WebCompositionUnderline>& underlines,
      int selection_start,
      int selection_end) OVERRIDE;
  virtual void OnImeConfirmComposition(const string16& text) OVERRIDE;
  virtual void WillHandleMouseEvent() OVERRIDE;

  // PluginDelegate implementation.
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
      const ppapi::SocketOptionData& value) OVERRIDE;
  virtual void RegisterTCPSocket(PPB_TCPSocket_Private_Impl* socket,
                                 uint32 socket_id) OVERRIDE;
  virtual void TCPServerSocketListen(
      PP_Resource socket_resource,
      const PP_NetAddress_Private& addr,
      int32_t backlog) OVERRIDE;
  virtual void TCPServerSocketAccept(uint32 server_socket_id) OVERRIDE;
  virtual void TCPServerSocketStopListening(
      PP_Resource socket_resource,
      uint32 socket_id) OVERRIDE;
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

  // RenderViewObserver implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnDestruct() OVERRIDE;

  void OnTCPSocketConnectACK(uint32 plugin_dispatcher_id,
                             uint32 socket_id,
                             int32_t result,
                             const PP_NetAddress_Private& local_addr,
                             const PP_NetAddress_Private& remote_addr);
  void OnTCPSocketSSLHandshakeACK(
      uint32 plugin_dispatcher_id,
      uint32 socket_id,
      bool succeeded,
      const ppapi::PPB_X509Certificate_Fields& certificate_fields);
  void OnTCPSocketReadACK(uint32 plugin_dispatcher_id,
                          uint32 socket_id,
                          int32_t result,
                          const std::string& data);
  void OnTCPSocketWriteACK(uint32 plugin_dispatcher_id,
                           uint32 socket_id,
                           int32_t result);
  void OnTCPSocketSetOptionACK(uint32 plugin_dispatcher_id,
                               uint32 socket_id,
                               int32_t result);
  void OnTCPServerSocketListenACK(uint32 plugin_dispatcher_id,
                                  PP_Resource socket_resource,
                                  uint32 socket_id,
                                  const PP_NetAddress_Private& local_addr,
                                  int32_t status);
  void OnTCPServerSocketAcceptACK(uint32 plugin_dispatcher_id,
                                  uint32 socket_id,
                                  uint32 accepted_socket_id,
                                  const PP_NetAddress_Private& local_addr,
                                  const PP_NetAddress_Private& remote_addr);
  void OnPpapiBrokerChannelCreated(int request_id,
                                   base::ProcessId broker_pid,
                                   const IPC::ChannelHandle& handle);
  void OnAsyncFileOpened(base::PlatformFileError error_code,
                         IPC::PlatformFileForTransit file_for_transit,
                         int message_id);
  void OnPpapiBrokerPermissionResult(int request_id, bool result);

  // Attempts to create a PPAPI plugin for the given filepath. On success, it
  // will return the newly-created module.
  //
  // There are two reasons for failure. The first is that the plugin isn't
  // a PPAPI plugin. In this case, |*pepper_plugin_was_registered| will be set
  // to false and the caller may want to fall back on creating an NPAPI plugin.
  // the second is that the plugin failed to initialize. In this case,
  // |*pepper_plugin_was_registered| will be set to true and the caller should
  // not fall back on any other plugin types.
  scoped_refptr<PluginModule> CreatePepperPluginModule(
      const WebPluginInfo& webplugin_info,
      bool* pepper_plugin_was_registered);

  // Asynchronously attempts to create a PPAPI broker for the given plugin.
  scoped_refptr<PepperBroker> CreateBroker(PluginModule* plugin_module);

  // Create a new HostDispatcher for proxying, hook it to the PluginModule,
  // and perform other common initialization.
  RendererPpapiHost* CreateOutOfProcessModule(
      PluginModule* module,
      const base::FilePath& path,
      ppapi::PpapiPermissions permissions,
      const IPC::ChannelHandle& channel_handle,
      base::ProcessId plugin_pid,
      int plugin_child_id,
      bool is_external);

  MouseLockDispatcher::LockTarget* GetOrCreateLockTargetAdapter(
      PepperPluginInstanceImpl* instance);
  void UnSetAndDeleteLockTargetAdapter(PepperPluginInstanceImpl* instance);

  MouseLockDispatcher* GetMouseLockDispatcher(
      PepperPluginInstanceImpl* instance);

  // Pointer to the RenderView that owns us.
  RenderViewImpl* render_view_;

  // Connection for sending and receiving pepper host-related messages to/from
  // the browser.
  PepperBrowserConnection pepper_browser_connection_;

  std::set<PepperPluginInstanceImpl*> active_instances_;
  typedef std::map<PepperPluginInstanceImpl*,
                   MouseLockDispatcher::LockTarget*> LockTargetMap;
  LockTargetMap mouse_lock_instances_;

  IDMap<AsyncOpenFileCallback> pending_async_open_files_;

  IDMap<PPB_TCPSocket_Private_Impl> tcp_sockets_;

  IDMap<ppapi::PPB_TCPServerSocket_Shared> tcp_server_sockets_;

  typedef IDMap<scoped_refptr<PepperBroker>, IDMapOwnPointer> BrokerMap;
  BrokerMap pending_connect_broker_;

  typedef IDMap<base::WeakPtr<PPB_Broker_Impl> > PermissionRequestMap;
  PermissionRequestMap pending_permission_requests_;

  // Whether or not the focus is on a PPAPI plugin
  PepperPluginInstanceImpl* focused_plugin_;

  // Current text input composition text. Empty if no composition is in
  // progress.
  string16 composition_text_;

  // The plugin instance that received the last mouse event. It is set to NULL
  // if the last mouse event went to elements other than Pepper plugins.
  // |last_mouse_event_target_| is not owned by this class. We can know about
  // when it is destroyed via InstanceDeleted().
  PepperPluginInstanceImpl* last_mouse_event_target_;

  scoped_ptr<GamepadSharedMemoryReader> gamepad_shared_memory_reader_;

  scoped_refptr<ContextProviderCommandBuffer> offscreen_context3d_;

  DISALLOW_COPY_AND_ASSIGN(PepperPluginDelegateImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_PLUGIN_DELEGATE_IMPL_H_
