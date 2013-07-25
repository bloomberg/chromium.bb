// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_MOCK_PLUGIN_DELEGATE_H_
#define CONTENT_RENDERER_PEPPER_MOCK_PLUGIN_DELEGATE_H_

#include "content/renderer/pepper/plugin_delegate.h"

struct PP_NetAddress_Private;
namespace ppapi { class PPB_X509Certificate_Fields; }

namespace webkit {
namespace ppapi {

class MockPluginDelegate : public PluginDelegate {
 public:
  MockPluginDelegate();
  virtual ~MockPluginDelegate();

  virtual void PluginFocusChanged(PluginInstanceImpl* instance,
                                  bool focused) OVERRIDE;
  virtual void PluginTextInputTypeChanged(
      PluginInstanceImpl* instance) OVERRIDE;
  virtual void PluginCaretPositionChanged(
      PluginInstanceImpl* instance) OVERRIDE;
  virtual void PluginRequestedCancelComposition(
      PluginInstanceImpl* instance) OVERRIDE;
  virtual void PluginSelectionChanged(PluginInstanceImpl* instance) OVERRIDE;
  virtual void SimulateImeSetComposition(
      const base::string16& text,
      const std::vector<WebKit::WebCompositionUnderline>& underlines,
      int selection_start,
      int selection_end) OVERRIDE;
  virtual void SimulateImeConfirmComposition(
      const base::string16& text) OVERRIDE;
  virtual void PluginCrashed(PluginInstanceImpl* instance) OVERRIDE;
  virtual void InstanceCreated(PluginInstanceImpl* instance) OVERRIDE;
  virtual void InstanceDeleted(PluginInstanceImpl* instance) OVERRIDE;
  virtual scoped_ptr< ::ppapi::thunk::ResourceCreationAPI>
      CreateResourceCreationAPI(PluginInstanceImpl* instance) OVERRIDE;
  virtual SkBitmap* GetSadPluginBitmap() OVERRIDE;
  virtual WebKit::WebPlugin* CreatePluginReplacement(
      const base::FilePath& file_path) OVERRIDE;
  virtual PlatformImage2D* CreateImage2D(int width, int height) OVERRIDE;
  virtual PlatformGraphics2D* GetGraphics2D(PluginInstanceImpl* instance,
                                            PP_Resource graphics_2d) OVERRIDE;
  virtual PlatformContext3D* CreateContext3D() OVERRIDE;
  virtual PlatformVideoDecoder* CreateVideoDecoder(
      media::VideoDecodeAccelerator::Client* client,
      int32 command_buffer_route_id) OVERRIDE;
  virtual PlatformVideoCapture* CreateVideoCapture(
      const std::string& device_id,
      const GURL& document_url,
      PlatformVideoCaptureEventHandler* handler) OVERRIDE;
  virtual uint32_t GetAudioHardwareOutputSampleRate() OVERRIDE;
  virtual uint32_t GetAudioHardwareOutputBufferSize() OVERRIDE;
  virtual PlatformAudioOutput* CreateAudioOutput(
      uint32_t sample_rate,
      uint32_t sample_count,
      PlatformAudioOutputClient* client) OVERRIDE;
  virtual PlatformAudioInput* CreateAudioInput(
      const std::string& device_id,
      const GURL& document_url,
      uint32_t sample_rate,
      uint32_t sample_count,
      PlatformAudioInputClient* client) OVERRIDE;
  virtual Broker* ConnectToBroker(PPB_Broker_Impl* client) OVERRIDE;
  virtual void NumberOfFindResultsChanged(int identifier,
                                          int total,
                                          bool final_result) OVERRIDE;
  virtual void SelectedFindResultChanged(int identifier, int index) OVERRIDE;
  virtual bool AsyncOpenFile(const base::FilePath& path,
                             int flags,
                             const AsyncOpenFileCallback& callback) OVERRIDE;
  virtual void AsyncOpenFileSystemURL(
      const GURL& path,
      int flags,
      const AsyncOpenFileSystemURLCallback& callback) OVERRIDE;
  virtual bool IsFileSystemOpened(PP_Instance instance,
                                  PP_Resource resource) const OVERRIDE;
  virtual PP_FileSystemType GetFileSystemType(
      PP_Instance instance,
      PP_Resource resource) const OVERRIDE;
  virtual GURL GetFileSystemRootUrl(PP_Instance instance,
                                    PP_Resource resource) const OVERRIDE;
  virtual void MakeDirectory(
      const GURL& path,
      bool recursive,
      const StatusCallback& callback) OVERRIDE;
  virtual void Query(const GURL& path,
                     const MetadataCallback& success_callback,
                     const StatusCallback& error_callback) OVERRIDE;
  virtual void ReadDirectoryEntries(
      const GURL& path,
      const ReadDirectoryCallback& success_callback,
      const StatusCallback& error_callback) OVERRIDE;
  virtual void Touch(const GURL& path,
                     const base::Time& last_access_time,
                     const base::Time& last_modified_time,
                     const StatusCallback& callback) OVERRIDE;
  virtual void SetLength(const GURL& path,
                         int64_t length,
                         const StatusCallback& callback) OVERRIDE;
  virtual void Delete(const GURL& path,
                      const StatusCallback& callback) OVERRIDE;
  virtual void Rename(const GURL& file_path,
                      const GURL& new_file_path,
                      const StatusCallback& callback) OVERRIDE;
  virtual void ReadDirectory(
      const GURL& directory_path,
      const ReadDirectoryCallback& success_callback,
      const StatusCallback& error_callback) OVERRIDE;
  virtual void QueryAvailableSpace(
      const GURL& origin,
      quota::StorageType type,
      const AvailableSpaceCallback& callback) OVERRIDE;
  virtual void WillUpdateFile(const GURL& file_path) OVERRIDE;
  virtual void DidUpdateFile(const GURL& file_path, int64_t delta) OVERRIDE;
  virtual void SyncGetFileSystemPlatformPath(
      const GURL& url,
      base::FilePath* platform_path) OVERRIDE;
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
  virtual bool AddNetworkListObserver(
      webkit_glue::NetworkListObserver* observer) OVERRIDE;
  virtual void RemoveNetworkListObserver(
      webkit_glue::NetworkListObserver* observer) OVERRIDE;
  virtual bool X509CertificateParseDER(
      const std::vector<char>& der,
      ::ppapi::PPB_X509Certificate_Fields* fields) OVERRIDE;
  virtual FullscreenContainer* CreateFullscreenContainer(
      PluginInstanceImpl* instance) OVERRIDE;
  virtual gfx::Size GetScreenSize() OVERRIDE;
  virtual std::string GetDefaultEncoding() OVERRIDE;
  virtual void ZoomLimitsChanged(double minimum_factor,
                                 double maximum_factor) OVERRIDE;
  virtual base::SharedMemory* CreateAnonymousSharedMemory(size_t size) OVERRIDE;
  virtual ::ppapi::Preferences GetPreferences() OVERRIDE;
  virtual bool LockMouse(PluginInstanceImpl* instance) OVERRIDE;
  virtual void UnlockMouse(PluginInstanceImpl* instance) OVERRIDE;
  virtual bool IsMouseLocked(PluginInstanceImpl* instance) OVERRIDE;
  virtual void DidChangeCursor(PluginInstanceImpl* instance,
                               const WebKit::WebCursorInfo& cursor) OVERRIDE;
  virtual void DidReceiveMouseEvent(PluginInstanceImpl* instance) OVERRIDE;
  virtual void SampleGamepads(WebKit::WebGamepads* data) OVERRIDE;
  virtual bool IsInFullscreenMode() OVERRIDE;
  virtual bool IsPageVisible() const OVERRIDE;
  virtual int EnumerateDevices(
      PP_DeviceType_Dev type,
      const EnumerateDevicesCallback& callback) OVERRIDE;
  virtual void StopEnumerateDevices(int request_id) OVERRIDE;
  virtual IPC::PlatformFileForTransit ShareHandleWithRemote(
      base::PlatformFile handle,
      base::ProcessId target_process_id,
      bool should_close_source) const OVERRIDE;
  virtual bool IsRunningInProcess(PP_Instance instance) const OVERRIDE;
  virtual void HandleDocumentLoad(
      PluginInstanceImpl* instance,
      const WebKit::WebURLResponse& response) OVERRIDE;
  virtual content::RendererPpapiHost* CreateExternalPluginModule(
      scoped_refptr<PluginModule> module,
      const base::FilePath& path,
      ::ppapi::PpapiPermissions permissions,
      const IPC::ChannelHandle& channel_handle,
      base::ProcessId plugin_pid,
      int plugin_child_id) OVERRIDE;
};

}  // namespace ppapi
}  // namespace webkit

#endif  // CONTENT_RENDERER_PEPPER_MOCK_PLUGIN_DELEGATE_H_
