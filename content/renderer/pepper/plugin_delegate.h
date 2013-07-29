// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PLUGIN_DELEGATE_H_
#define CONTENT_RENDERER_PEPPER_PLUGIN_DELEGATE_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/shared_memory.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/platform_file.h"
#include "base/process/process.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "ipc/ipc_platform_file.h"
#include "ppapi/c/dev/pp_video_dev.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_file_info.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/ppb_tcp_socket.h"
#include "ppapi/c/private/ppb_flash.h"
#include "ppapi/c/private/ppb_tcp_socket_private.h"
#include "ppapi/c/private/ppb_udp_socket_private.h"
#include "ppapi/shared_impl/dir_contents.h"
#include "ui/gfx/size.h"
#include "url/gurl.h"
#include "webkit/common/fileapi/file_system_types.h"
#include "webkit/common/quota/quota_types.h"

class GURL;
class SkBitmap;
class SkCanvas;
class TransportDIB;
struct PP_NetAddress_Private;

namespace IPC {
struct ChannelHandle;
}

namespace WebKit {
class WebGraphicsContext3D;
}

namespace base {
class MessageLoopProxy;
class Time;
}

namespace content {
class RendererPpapiHost;
}

namespace fileapi {
struct DirectoryEntry;
}

namespace gfx {
class Point;
}

namespace ppapi {
class PepperFilePath;
class PpapiPermissions;
class PPB_X509Certificate_Fields;
class SocketOptionData;
struct DeviceRefData;
struct HostPortPair;
struct Preferences;

namespace thunk {
class ResourceCreationAPI;
}

}  // namespace ppapi

namespace WebKit {
typedef SkCanvas WebCanvas;
class WebGamepads;
class WebPlugin;
struct WebCompositionUnderline;
struct WebCursorInfo;
struct WebURLError;
class WebURLLoaderClient;
class WebURLResponse;
}

namespace content {

class FileIO;
class FullscreenContainer;
class PepperPluginInstanceImpl;
class PluginModule;
class PPB_Flash_Menu_Impl;
class PPB_ImageData_Impl;
class PPB_TCPSocket_Private_Impl;

// Virtual interface that the browser implements to implement features for
// PPAPI plugins.
class PluginDelegate {
 public:
  // Notification that the given plugin is focused or unfocused.
  virtual void PluginFocusChanged(PepperPluginInstanceImpl* instance,
                                  bool focused) = 0;
  // Notification that the text input status of the given plugin is changed.
  virtual void PluginTextInputTypeChanged(
      PepperPluginInstanceImpl* instance) = 0;
  // Notification that the caret position in the given plugin is changed.
  virtual void PluginCaretPositionChanged(
      PepperPluginInstanceImpl* instance) = 0;
  // Notification that the plugin requested to cancel the current composition.
  virtual void PluginRequestedCancelComposition(
      PepperPluginInstanceImpl* instance) = 0;
  // Notification that the text selection in the given plugin is changed.
  virtual void PluginSelectionChanged(PepperPluginInstanceImpl* instance) = 0;
  // Requests simulating IME events for testing purpose.
  virtual void SimulateImeSetComposition(
      const base::string16& text,
      const std::vector<WebKit::WebCompositionUnderline>& underlines,
      int selection_start,
      int selection_end) = 0;
  virtual void SimulateImeConfirmComposition(const base::string16& text) = 0;

  // Notification that the given plugin has crashed. When a plugin crashes, all
  // instances associated with that plugin will notify that they've crashed via
  // this function.
  virtual void PluginCrashed(PepperPluginInstanceImpl* instance) = 0;

  // Indicates that the given instance has been created.
  virtual void InstanceCreated(PepperPluginInstanceImpl* instance) = 0;

  // Indicates that the given instance is being destroyed. This is called from
  // the destructor, so it's important that the instance is not dereferenced
  // from this call.
  virtual void InstanceDeleted(PepperPluginInstanceImpl* instance) = 0;

  // Creates the resource creation API for the given instance.
  virtual scoped_ptr< ::ppapi::thunk::ResourceCreationAPI>
      CreateResourceCreationAPI(PepperPluginInstanceImpl* instance) = 0;

  // Returns a pointer (ownership not transferred) to the bitmap to paint the
  // sad plugin screen with. Returns NULL on failure.
  virtual SkBitmap* GetSadPluginBitmap() = 0;

  // Creates a replacement plug-in that is shown when the plug-in at |file_path|
  // couldn't be loaded.
  virtual WebKit::WebPlugin* CreatePluginReplacement(
      const base::FilePath& file_path) = 0;

  // Get audio hardware output sample rate.
  virtual uint32_t GetAudioHardwareOutputSampleRate() = 0;

  // Get audio hardware output buffer size.
  virtual uint32_t GetAudioHardwareOutputBufferSize() = 0;

  // Notifies that the number of find results has changed.
  virtual void NumberOfFindResultsChanged(int identifier,
                                          int total,
                                          bool final_result) = 0;

  // Notifies that the index of the currently selected item has been updated.
  virtual void SelectedFindResultChanged(int identifier, int index) = 0;

  // Sends an async IPC to open a local file.
  typedef base::Callback<void (base::PlatformFileError, base::PassPlatformFile)>
      AsyncOpenFileCallback;
  virtual bool AsyncOpenFile(const base::FilePath& path,
                             int flags,
                             const AsyncOpenFileCallback& callback) = 0;

  // These functions expose some of PepperFileSystemHost methods for
  // PPB_FileRef_Impl (which is in webkit) to access.  Once we migrate FileRef
  // to the new design in content/, we won't need this delegation.
  // TODO(victorhsieh): remove these delegation.
  virtual bool IsFileSystemOpened(PP_Instance instance,
                                  PP_Resource resource) const = 0;
  virtual PP_FileSystemType GetFileSystemType(PP_Instance instance,
                                              PP_Resource resource) const = 0;
  virtual GURL GetFileSystemRootUrl(PP_Instance instance,
                                    PP_Resource resource) const = 0;

  // Sends an async IPC to open a file through filesystem API.
  // When a file is successfully opened, |callback| is invoked with
  // PLATFORM_FILE_OK, the opened file handle, and a callback function for
  // notifying that the file is closed. When the users of this function
  // finished using the file, they must close the file handle and then must call
  // the supplied callback function.
  typedef base::Callback<void (base::PlatformFileError)>
      NotifyCloseFileCallback;
  typedef base::Callback<
      void (base::PlatformFileError error,
            base::PassPlatformFile file,
            quota::QuotaLimitType quota_policy,
            const NotifyCloseFileCallback& close_file_callback)>
      AsyncOpenFileSystemURLCallback;
  virtual void AsyncOpenFileSystemURL(
      const GURL& path,
      int flags,
      const AsyncOpenFileSystemURLCallback& callback) = 0;

  // Callback typedefs for FileSystem related methods.
  typedef base::Callback<void (base::PlatformFileError)> StatusCallback;
  typedef base::Callback<void(
      const std::vector<fileapi::DirectoryEntry>& entries,
      bool has_more)> ReadDirectoryCallback;
  typedef base::Callback<void(
      const base::PlatformFileInfo& file_info)> MetadataCallback;

  virtual void MakeDirectory(
      const GURL& path,
      bool recursive,
      const StatusCallback& callback) = 0;
  virtual void Query(const GURL& path,
                     const MetadataCallback& success_callback,
                     const StatusCallback& error_callback) = 0;
  virtual void ReadDirectoryEntries(
      const GURL& path,
      const ReadDirectoryCallback& success_callback,
      const StatusCallback& error_callback) = 0;
  virtual void Touch(const GURL& path,
                     const base::Time& last_access_time,
                     const base::Time& last_modified_time,
                     const StatusCallback& callback) = 0;
  virtual void SetLength(const GURL& path,
                         int64_t length,
                         const StatusCallback& callback) = 0;
  virtual void Delete(const GURL& path,
                      const StatusCallback& callback) = 0;
  virtual void Rename(const GURL& file_path,
                      const GURL& new_file_path,
                      const StatusCallback& callback) = 0;
  virtual void ReadDirectory(
      const GURL& directory_path,
      const ReadDirectoryCallback& success_callback,
      const StatusCallback& error_callback) = 0;

  // Synchronously returns the platform file path for a filesystem URL.
  virtual void SyncGetFileSystemPlatformPath(const GURL& url,
                                             base::FilePath* platform_path) = 0;

  // Returns a MessageLoopProxy instance associated with the message loop
  // of the file thread in this renderer.
  virtual scoped_refptr<base::MessageLoopProxy>
      GetFileThreadMessageLoopProxy() = 0;

  // For PPB_TCPSocket_Private.
  virtual uint32 TCPSocketCreate() = 0;
  virtual void TCPSocketConnect(PPB_TCPSocket_Private_Impl* socket,
                                uint32 socket_id,
                                const std::string& host,
                                uint16_t port) = 0;
  virtual void TCPSocketConnectWithNetAddress(
      PPB_TCPSocket_Private_Impl* socket,
      uint32 socket_id,
      const PP_NetAddress_Private& addr) = 0;
  virtual void TCPSocketSSLHandshake(
      uint32 socket_id,
      const std::string& server_name,
      uint16_t server_port,
      const std::vector<std::vector<char> >& trusted_certs,
      const std::vector<std::vector<char> >& untrusted_certs) = 0;
  virtual void TCPSocketRead(uint32 socket_id, int32_t bytes_to_read) = 0;
  virtual void TCPSocketWrite(uint32 socket_id, const std::string& buffer) = 0;
  virtual void TCPSocketDisconnect(uint32 socket_id) = 0;
  virtual void TCPSocketSetOption(uint32 socket_id,
                                  PP_TCPSocket_Option name,
                                  const ::ppapi::SocketOptionData& value) = 0;
  virtual void RegisterTCPSocket(PPB_TCPSocket_Private_Impl* socket,
                                 uint32 socket_id) = 0;

  // For PPB_TCPServerSocket_Private.
  virtual void TCPServerSocketListen(PP_Resource socket_resource,
                                     const PP_NetAddress_Private& addr,
                                     int32_t backlog) = 0;
  virtual void TCPServerSocketAccept(uint32 server_socket_id) = 0;
  virtual void TCPServerSocketStopListening(
      PP_Resource socket_resource,
      uint32 socket_id) = 0;

  // For PPB_X509Certificate_Private.
  virtual bool X509CertificateParseDER(
      const std::vector<char>& der,
      ::ppapi::PPB_X509Certificate_Fields* fields) = 0;

  // Create a fullscreen container for a plugin instance. This effectively
  // switches the plugin to fullscreen.
  virtual FullscreenContainer* CreateFullscreenContainer(
      PepperPluginInstanceImpl* instance) = 0;

  // Gets the size of the screen. The fullscreen window will be created at that
  // size.
  virtual gfx::Size GetScreenSize() = 0;

  // Returns a string with the name of the default 8-bit char encoding.
  virtual std::string GetDefaultEncoding() = 0;

  // Sets the minimum and maximum zoom factors.
  virtual void ZoomLimitsChanged(double minimum_factor,
                                 double maximum_factor) = 0;

  // Create an anonymous shared memory segment of size |size| bytes, and return
  // a pointer to it, or NULL on error.  Caller owns the returned pointer.
  virtual base::SharedMemory* CreateAnonymousSharedMemory(size_t size) = 0;

  // Returns the current preferences.
  virtual ::ppapi::Preferences GetPreferences() = 0;

  // Locks the mouse for |instance|. If false is returned, the lock is not
  // possible. If true is returned then the lock is pending. Success or
  // failure will be delivered asynchronously via
  // PluginInstance::OnLockMouseACK().
  virtual bool LockMouse(PepperPluginInstanceImpl* instance) = 0;

  // Unlocks the mouse if |instance| currently owns the mouse lock. Whenever an
  // plugin instance has lost the mouse lock, it will be notified by
  // PluginInstance::OnMouseLockLost(). Please note that UnlockMouse() is not
  // the only cause of losing mouse lock. For example, a user may press the Esc
  // key to quit the mouse lock mode, which also results in an OnMouseLockLost()
  // call to the current mouse lock owner.
  virtual void UnlockMouse(PepperPluginInstanceImpl* instance) = 0;

  // Returns true iff |instance| currently owns the mouse lock.
  virtual bool IsMouseLocked(PepperPluginInstanceImpl* instance) = 0;

  // Notifies that |instance| has changed the cursor.
  // This will update the cursor appearance if it is currently over the plugin
  // instance.
  virtual void DidChangeCursor(PepperPluginInstanceImpl* instance,
                               const WebKit::WebCursorInfo& cursor) = 0;

  // Notifies that |instance| has received a mouse event.
  virtual void DidReceiveMouseEvent(PepperPluginInstanceImpl* instance) = 0;

  // Determines if the browser entered fullscreen mode.
  virtual bool IsInFullscreenMode() = 0;

  // Retrieve current gamepad data.
  virtual void SampleGamepads(WebKit::WebGamepads* data) = 0;

  // Returns true if the containing page is visible.
  virtual bool IsPageVisible() const = 0;

  // Share a given handle with the target process.
  virtual IPC::PlatformFileForTransit ShareHandleWithRemote(
      base::PlatformFile handle,
      base::ProcessId target_process_id,
      bool should_close_source) const = 0;

  // Returns true if running in process.
  virtual bool IsRunningInProcess(PP_Instance instance) const = 0;

  // Notifies the plugin of the document load. This should initiate the call to
  // PPP_Instance.HandleDocumentLoad.
  //
  // The loader object should set itself on the PluginInstance as the document
  // loader using set_document_loader.
  virtual void HandleDocumentLoad(PepperPluginInstanceImpl* instance,
                                  const WebKit::WebURLResponse& response) = 0;

  // Sets up the renderer host and out-of-process proxy for an external plugin
  // module. Returns the renderer host, or NULL if it couldn't be created.
  virtual RendererPpapiHost* CreateExternalPluginModule(
      scoped_refptr<PluginModule> module,
      const base::FilePath& path,
      ::ppapi::PpapiPermissions permissions,
      const IPC::ChannelHandle& channel_handle,
      base::ProcessId plugin_pid,
      int plugin_child_id) = 0;
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PLUGIN_DELEGATE_H_
