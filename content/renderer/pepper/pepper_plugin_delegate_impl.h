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
#include "content/renderer/pepper/pepper_parent_context_provider.h"
#include "content/renderer/render_view_pepper_helper.h"
#include "ppapi/shared_impl/private/ppb_host_resolver_shared.h"
#include "ppapi/shared_impl/private/ppb_tcp_server_socket_shared.h"
#include "ppapi/shared_impl/private/tcp_socket_private_impl.h"
#include "ppapi/shared_impl/private/udp_socket_private_impl.h"
#include "ui/base/ime/text_input_type.h"
#include "webkit/plugins/ppapi/plugin_delegate.h"

class FilePath;

namespace IPC {
struct ChannelHandle;
}

namespace ppapi {
class PepperFilePath;
class PPB_X509Certificate_Fields;
class PpapiPermissions;
}

namespace webkit {
struct WebPluginInfo;
namespace ppapi {
class PluginInstance;
class PluginModule;
}
}

namespace WebKit {
class WebGamepads;
struct WebCompositionUnderline;
}

namespace content {
class GamepadSharedMemoryReader;
class PepperBrokerImpl;
class PepperDeviceEnumerationEventHandler;
class RenderViewImpl;

class PepperPluginDelegateImpl
    : public webkit::ppapi::PluginDelegate,
      public RenderViewPepperHelper,
      public base::SupportsWeakPtr<PepperPluginDelegateImpl>,
      public PepperParentContextProvider,
      public RenderViewObserver {
 public:
  explicit PepperPluginDelegateImpl(RenderViewImpl* render_view);
  virtual ~PepperPluginDelegateImpl();

  RenderViewImpl* render_view() { return render_view_; }

  // Sets up the renderer host and out-of-process proxy for an external plugin
  // module. Returns the renderer host, or NULL if it couldn't be created.
  RendererPpapiHost* CreateExternalPluginModule(
      scoped_refptr<webkit::ppapi::PluginModule> module,
      const FilePath& path,
      ppapi::PpapiPermissions permissions,
      const IPC::ChannelHandle& channel_handle,
      base::ProcessId plugin_pid,
      int plugin_child_id);

  // Removes broker from pending_connect_broker_ if present. Returns true if so.
  bool StopWaitingForBrokerConnection(PepperBrokerImpl* broker);

  CONTENT_EXPORT int GetRoutingID() const;

  typedef base::Callback<void (int /* request_id */,
                               bool /* succeeded */,
                               const std::string& /* label */)>
      OpenDeviceCallback;

  // Opens the specified device. The request ID passed into the callback will be
  // the same as the return value. If successful, the label passed into the
  // callback identifies a audio/video steam, which can be used to call
  // CloseDevice() and GetSesssionID().
  int OpenDevice(PP_DeviceType_Dev type,
                 const std::string& device_id,
                 const OpenDeviceCallback& callback);
  void CloseDevice(const std::string& label);
  // Gets audio/video session ID given a label.
  int GetSessionID(PP_DeviceType_Dev type, const std::string& label);

 private:
  // RenderViewPepperHelper implementation.
  virtual WebKit::WebPlugin* CreatePepperWebPlugin(
    const webkit::WebPluginInfo& webplugin_info,
    const WebKit::WebPluginParams& params) OVERRIDE;
  virtual void ViewWillInitiatePaint() OVERRIDE;
  virtual void ViewInitiatedPaint() OVERRIDE;
  virtual void ViewFlushedPaint() OVERRIDE;
  virtual webkit::ppapi::PluginInstance* GetBitmapForOptimizedPluginPaint(
      const gfx::Rect& paint_bounds,
      TransportDIB** dib,
      gfx::Rect* location,
      gfx::Rect* clip,
      float* scale_factor) OVERRIDE;
  virtual void OnAsyncFileOpened(base::PlatformFileError error_code,
                                 base::PlatformFile file,
                                 int message_id) OVERRIDE;
  virtual void OnPpapiBrokerChannelCreated(
      int request_id,
      base::ProcessId broker_pid,
      const IPC::ChannelHandle& handle) OVERRIDE;
  virtual void OnPpapiBrokerPermissionResult(int request_id,
                                             bool result) OVERRIDE;
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
  virtual void PluginFocusChanged(webkit::ppapi::PluginInstance* instance,
                                  bool focused) OVERRIDE;
  virtual void PluginTextInputTypeChanged(
      webkit::ppapi::PluginInstance* instance) OVERRIDE;
  virtual void PluginCaretPositionChanged(
      webkit::ppapi::PluginInstance* instance) OVERRIDE;
  virtual void PluginRequestedCancelComposition(
      webkit::ppapi::PluginInstance* instance) OVERRIDE;
  virtual void PluginSelectionChanged(
      webkit::ppapi::PluginInstance* instance) OVERRIDE;
  virtual void SimulateImeSetComposition(
      const string16& text,
      const std::vector<WebKit::WebCompositionUnderline>& underlines,
      int selection_start,
      int selection_end) OVERRIDE;
  virtual void SimulateImeConfirmComposition(const string16& text) OVERRIDE;
  virtual void PluginCrashed(webkit::ppapi::PluginInstance* instance) OVERRIDE;
  virtual void InstanceCreated(
      webkit::ppapi::PluginInstance* instance) OVERRIDE;
  virtual void InstanceDeleted(
      webkit::ppapi::PluginInstance* instance) OVERRIDE;
  virtual scoped_ptr< ::ppapi::thunk::ResourceCreationAPI>
      CreateResourceCreationAPI(
          webkit::ppapi::PluginInstance* instance) OVERRIDE;
  virtual SkBitmap* GetSadPluginBitmap() OVERRIDE;
  virtual WebKit::WebPlugin* CreatePluginReplacement(
      const FilePath& file_path) OVERRIDE;
  virtual uint32_t GetAudioHardwareOutputSampleRate() OVERRIDE;
  virtual uint32_t GetAudioHardwareOutputBufferSize() OVERRIDE;
  virtual PlatformAudioOutput* CreateAudioOutput(
      uint32_t sample_rate,
      uint32_t sample_count,
      PlatformAudioOutputClient* client) OVERRIDE;
  virtual PlatformAudioInput* CreateAudioInput(
      const std::string& device_id,
      uint32_t sample_rate,
      uint32_t sample_count,
      PlatformAudioInputClient* client) OVERRIDE;
  virtual PlatformImage2D* CreateImage2D(int width, int height) OVERRIDE;
  virtual PlatformGraphics2D* GetGraphics2D(
      webkit::ppapi::PluginInstance* instance,
      PP_Resource resource) OVERRIDE;
  virtual PlatformContext3D* CreateContext3D() OVERRIDE;
  virtual void ReparentContext(PlatformContext3D*) OVERRIDE;
  virtual PlatformVideoCapture* CreateVideoCapture(
      const std::string& device_id,
      PlatformVideoCaptureEventHandler* handler) OVERRIDE;
  virtual PlatformVideoDecoder* CreateVideoDecoder(
      media::VideoDecodeAccelerator::Client* client,
      int32 command_buffer_route_id) OVERRIDE;
  virtual Broker* ConnectToBroker(
      webkit::ppapi::PPB_Broker_Impl* client) OVERRIDE;
  virtual void NumberOfFindResultsChanged(int identifier,
                                          int total,
                                          bool final_result) OVERRIDE;
  virtual void SelectedFindResultChanged(int identifier, int index) OVERRIDE;
  virtual bool AsyncOpenFile(const FilePath& path,
                             int flags,
                             const AsyncOpenFileCallback& callback) OVERRIDE;
  virtual bool AsyncOpenFileSystemURL(
      const GURL& path,
      int flags,
      const AsyncOpenFileSystemURLCallback& callback) OVERRIDE;
  virtual bool OpenFileSystem(
      const GURL& origin_url,
      fileapi::FileSystemType type,
      long long size,
      fileapi::FileSystemCallbackDispatcher* dispatcher) OVERRIDE;
  virtual bool MakeDirectory(
      const GURL& path,
      bool recursive,
      fileapi::FileSystemCallbackDispatcher* dispatcher) OVERRIDE;
  virtual bool Query(
      const GURL& path,
      fileapi::FileSystemCallbackDispatcher* dispatcher) OVERRIDE;
  virtual bool Touch(
      const GURL& path,
      const base::Time& last_access_time,
      const base::Time& last_modified_time,
      fileapi::FileSystemCallbackDispatcher* dispatcher) OVERRIDE;
  virtual bool SetLength(
      const GURL& path,
      int64_t length,
      fileapi::FileSystemCallbackDispatcher* dispatcher) OVERRIDE;
  virtual bool Delete(
      const GURL& path,
      fileapi::FileSystemCallbackDispatcher* dispatcher) OVERRIDE;
  virtual bool Rename(
      const GURL& file_path,
      const GURL& new_file_path,
      fileapi::FileSystemCallbackDispatcher* dispatcher) OVERRIDE;
  virtual bool ReadDirectory(
      const GURL& directory_path,
      fileapi::FileSystemCallbackDispatcher* dispatcher) OVERRIDE;
  virtual void QueryAvailableSpace(
      const GURL& origin,
      quota::StorageType type,
      const AvailableSpaceCallback& callback) OVERRIDE;
  virtual void WillUpdateFile(const GURL& file_path) OVERRIDE;
  virtual void DidUpdateFile(const GURL& file_path, int64_t delta) OVERRIDE;
  virtual void SyncGetFileSystemPlatformPath(
      const GURL& url,
      FilePath* platform_path) OVERRIDE;
  virtual scoped_refptr<base::MessageLoopProxy>
      GetFileThreadMessageLoopProxy() OVERRIDE;
  virtual uint32 TCPSocketCreate() OVERRIDE;
  virtual void TCPSocketConnect(
      webkit::ppapi::PPB_TCPSocket_Private_Impl* socket,
      uint32 socket_id,
      const std::string& host,
      uint16_t port) OVERRIDE;
  virtual void TCPSocketConnectWithNetAddress(
      webkit::ppapi::PPB_TCPSocket_Private_Impl* socket,
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
  virtual void RegisterTCPSocket(
      webkit::ppapi::PPB_TCPSocket_Private_Impl* socket,
      uint32 socket_id) OVERRIDE;
  virtual uint32 UDPSocketCreate() OVERRIDE;
  virtual void UDPSocketSetBoolSocketFeature(
      webkit::ppapi::PPB_UDPSocket_Private_Impl* socket,
      uint32 socket_id,
      int32_t name,
      bool value) OVERRIDE;
  virtual void UDPSocketBind(
      webkit::ppapi::PPB_UDPSocket_Private_Impl* socket,
      uint32 socket_id,
      const PP_NetAddress_Private& addr) OVERRIDE;
  virtual void UDPSocketRecvFrom(uint32 socket_id,
                                 int32_t num_bytes) OVERRIDE;
  virtual void UDPSocketSendTo(uint32 socket_id,
                               const std::string& buffer,
                               const PP_NetAddress_Private& addr) OVERRIDE;
  virtual void UDPSocketClose(uint32 socket_id) OVERRIDE;
  virtual void TCPServerSocketListen(
      PP_Resource socket_resource,
      const PP_NetAddress_Private& addr,
      int32_t backlog) OVERRIDE;
  virtual void TCPServerSocketAccept(uint32 server_socket_id) OVERRIDE;
  virtual void TCPServerSocketStopListening(
      PP_Resource socket_resource,
      uint32 socket_id) OVERRIDE;
  virtual void RegisterHostResolver(
      ppapi::PPB_HostResolver_Shared* host_resolver,
      uint32 host_resolver_id) OVERRIDE;
  virtual void HostResolverResolve(
      uint32 host_resolver_id,
      const ::ppapi::HostPortPair& host_port,
      const PP_HostResolver_Private_Hint* hint) OVERRIDE;
  virtual void UnregisterHostResolver(uint32 host_resolver_id) OVERRIDE;
  virtual bool AddNetworkListObserver(
      webkit_glue::NetworkListObserver* observer) OVERRIDE;
  virtual void RemoveNetworkListObserver(
      webkit_glue::NetworkListObserver* observer) OVERRIDE;
  virtual bool X509CertificateParseDER(
      const std::vector<char>& der,
      ppapi::PPB_X509Certificate_Fields* fields) OVERRIDE;
  virtual webkit::ppapi::FullscreenContainer*
      CreateFullscreenContainer(
          webkit::ppapi::PluginInstance* instance) OVERRIDE;
  virtual gfx::Size GetScreenSize() OVERRIDE;
  virtual std::string GetDefaultEncoding() OVERRIDE;
  virtual void ZoomLimitsChanged(double minimum_factor, double maximum_factor)
      OVERRIDE;
  virtual void DidStartLoading() OVERRIDE;
  virtual void DidStopLoading() OVERRIDE;
  virtual void SetContentRestriction(int restrictions) OVERRIDE;
  virtual void SaveURLAs(const GURL& url) OVERRIDE;
  virtual base::SharedMemory* CreateAnonymousSharedMemory(size_t size)
      OVERRIDE;
  virtual ::ppapi::Preferences GetPreferences() OVERRIDE;
  virtual bool LockMouse(webkit::ppapi::PluginInstance* instance) OVERRIDE;
  virtual void UnlockMouse(webkit::ppapi::PluginInstance* instance) OVERRIDE;
  virtual bool IsMouseLocked(webkit::ppapi::PluginInstance* instance) OVERRIDE;
  virtual void DidChangeCursor(webkit::ppapi::PluginInstance* instance,
                               const WebKit::WebCursorInfo& cursor) OVERRIDE;
  virtual void DidReceiveMouseEvent(
      webkit::ppapi::PluginInstance* instance) OVERRIDE;
  virtual bool IsInFullscreenMode() OVERRIDE;
  virtual void SampleGamepads(WebKit::WebGamepads* data) OVERRIDE;
  virtual bool IsPageVisible() const OVERRIDE;
  virtual int EnumerateDevices(
      PP_DeviceType_Dev type,
      const EnumerateDevicesCallback& callback) OVERRIDE;
  virtual void StopEnumerateDevices(int request_id) OVERRIDE;

  // RenderViewObserver implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnDestruct() OVERRIDE;

  void OnTCPSocketConnectACK(uint32 plugin_dispatcher_id,
                             uint32 socket_id,
                             bool succeeded,
                             const PP_NetAddress_Private& local_addr,
                             const PP_NetAddress_Private& remote_addr);
  void OnTCPSocketSSLHandshakeACK(
      uint32 plugin_dispatcher_id,
      uint32 socket_id,
      bool succeeded,
      const ppapi::PPB_X509Certificate_Fields& certificate_fields);
  void OnTCPSocketReadACK(uint32 plugin_dispatcher_id,
                          uint32 socket_id,
                          bool succeeded,
                          const std::string& data);
  void OnTCPSocketWriteACK(uint32 plugin_dispatcher_id,
                           uint32 socket_id,
                           bool succeeded,
                           int32_t bytes_written);
  void OnUDPSocketBindACK(uint32 plugin_dispatcher_id,
                          uint32 socket_id,
                          bool succeeded,
                          const PP_NetAddress_Private& addr);
  void OnUDPSocketSendToACK(uint32 plugin_dispatcher_id,
                            uint32 socket_id,
                            bool succeeded,
                            int32_t bytes_written);
  void OnUDPSocketRecvFromACK(uint32 plugin_dispatcher_id,
                              uint32 socket_id,
                              bool succeeded,
                              const std::string& data,
                              const PP_NetAddress_Private& addr);
  void OnTCPServerSocketListenACK(uint32 plugin_dispatcher_id,
                                  PP_Resource socket_resource,
                                  uint32 socket_id,
                                  int32_t status);
  void OnTCPServerSocketAcceptACK(uint32 plugin_dispatcher_id,
                                  uint32 socket_id,
                                  uint32 accepted_socket_id,
                                  const PP_NetAddress_Private& local_addr,
                                  const PP_NetAddress_Private& remote_addr);
  void OnHostResolverResolveACK(
      uint32 plugin_dispatcher_id,
      uint32 host_resolver_id,
      bool succeeded,
      const std::string& canonical_name,
      const std::vector<PP_NetAddress_Private>& net_address_list);

  // Attempts to create a PPAPI plugin for the given filepath. On success, it
  // will return the newly-created module.
  //
  // There are two reasons for failure. The first is that the plugin isn't
  // a PPAPI plugin. In this case, |*pepper_plugin_was_registered| will be set
  // to false and the caller may want to fall back on creating an NPAPI plugin.
  // the second is that the plugin failed to initialize. In this case,
  // |*pepper_plugin_was_registered| will be set to true and the caller should
  // not fall back on any other plugin types.
  scoped_refptr<webkit::ppapi::PluginModule>
  CreatePepperPluginModule(
      const webkit::WebPluginInfo& webplugin_info,
      bool* pepper_plugin_was_registered);

  // Asynchronously attempts to create a PPAPI broker for the given plugin.
  scoped_refptr<PepperBrokerImpl> CreateBroker(
      webkit::ppapi::PluginModule* plugin_module);

  // Create a new HostDispatcher for proxying, hook it to the PluginModule,
  // and perform other common initialization.
  RendererPpapiHost* CreateOutOfProcessModule(
      webkit::ppapi::PluginModule* module,
      const FilePath& path,
      ppapi::PpapiPermissions permissions,
      const IPC::ChannelHandle& channel_handle,
      base::ProcessId plugin_pid,
      int plugin_child_id,
      bool is_external);

  // Implementation of PepperParentContextProvider.
  virtual WebGraphicsContext3DCommandBufferImpl*
      GetParentContextForPlatformContext3D() OVERRIDE;

  MouseLockDispatcher::LockTarget* GetOrCreateLockTargetAdapter(
      webkit::ppapi::PluginInstance* instance);
  void UnSetAndDeleteLockTargetAdapter(webkit::ppapi::PluginInstance* instance);

  MouseLockDispatcher* GetMouseLockDispatcher(
      webkit::ppapi::PluginInstance* instance);

  // Pointer to the RenderView that owns us.
  RenderViewImpl* render_view_;

  std::set<webkit::ppapi::PluginInstance*> active_instances_;
  typedef std::map<webkit::ppapi::PluginInstance*,
                   MouseLockDispatcher::LockTarget*> LockTargetMap;
  LockTargetMap mouse_lock_instances_;

  IDMap<AsyncOpenFileCallback> pending_async_open_files_;

  IDMap<webkit::ppapi::PPB_TCPSocket_Private_Impl> tcp_sockets_;

  IDMap<webkit::ppapi::PPB_UDPSocket_Private_Impl> udp_sockets_;

  IDMap<ppapi::PPB_TCPServerSocket_Shared> tcp_server_sockets_;

  IDMap<ppapi::PPB_HostResolver_Shared> host_resolvers_;

  typedef IDMap<scoped_refptr<PepperBrokerImpl>, IDMapOwnPointer> BrokerMap;
  BrokerMap pending_connect_broker_;

  typedef IDMap<base::WeakPtr<webkit::ppapi::PPB_Broker_Impl> >
      PermissionRequestMap;
  PermissionRequestMap pending_permission_requests_;

  // Whether or not the focus is on a PPAPI plugin
  webkit::ppapi::PluginInstance* focused_plugin_;

  // Current text input composition text. Empty if no composition is in
  // progress.
  string16 composition_text_;

  // The plugin instance that received the last mouse event. It is set to NULL
  // if the last mouse event went to elements other than Pepper plugins.
  // |last_mouse_event_target_| is not owned by this class. We can know about
  // when it is destroyed via InstanceDeleted().
  webkit::ppapi::PluginInstance* last_mouse_event_target_;

  scoped_ptr<GamepadSharedMemoryReader> gamepad_shared_memory_reader_;

  scoped_ptr<PepperDeviceEnumerationEventHandler>
      device_enumeration_event_handler_;

  DISALLOW_COPY_AND_ASSIGN(PepperPluginDelegateImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_PLUGIN_DELEGATE_IMPL_H_
