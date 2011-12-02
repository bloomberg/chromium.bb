// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PLUGIN_DELEGATE_IMPL_H_
#define CONTENT_RENDERER_PEPPER_PLUGIN_DELEGATE_IMPL_H_
#pragma once

#include <map>
#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/id_map.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/renderer/pepper_parent_context_provider.h"
#include "ppapi/proxy/broker_dispatcher.h"
#include "ppapi/proxy/proxy_channel.h"
#include "ppapi/shared_impl/private/tcp_socket_private_impl.h"
#include "ppapi/shared_impl/private/udp_socket_private_impl.h"
#include "ui/base/ime/text_input_type.h"
#include "webkit/plugins/ppapi/plugin_delegate.h"
#include "webkit/plugins/ppapi/ppb_broker_impl.h"
#include "webkit/plugins/ppapi/ppb_flash_menu_impl.h"

class FilePath;
class PepperPluginDelegateImpl;
class RenderViewImpl;

namespace gfx {
class Point;
class Rect;
}

namespace IPC {
struct ChannelHandle;
}

namespace webkit {
struct WebPluginInfo;
namespace ppapi {
class PepperFilePath;
class PluginInstance;
class PluginModule;
}
}

namespace WebKit {
class WebFileChooserCompletion;
class WebMouseEvent;
struct WebCompositionUnderline;
struct WebFileChooserParams;
}

namespace webkit_glue {
struct CustomContextMenuContext;
}

class TransportDIB;

// This object is NOT thread-safe.
class CONTENT_EXPORT BrokerDispatcherWrapper {
 public:
  BrokerDispatcherWrapper();
  ~BrokerDispatcherWrapper();

  bool Init(base::ProcessHandle plugin_process_handle,
            const IPC::ChannelHandle& channel_handle);

  int32_t SendHandleToBroker(PP_Instance instance,
                             base::SyncSocket::Handle handle);

 private:
  scoped_ptr<ppapi::proxy::BrokerDispatcher> dispatcher_;
  scoped_ptr<ppapi::proxy::ProxyChannel::Delegate> dispatcher_delegate_;
};

// This object is NOT thread-safe.
class PpapiBrokerImpl : public webkit::ppapi::PluginDelegate::PpapiBroker,
                        public base::RefCountedThreadSafe<PpapiBrokerImpl>{
 public:
  PpapiBrokerImpl(webkit::ppapi::PluginModule* plugin_module,
                  PepperPluginDelegateImpl* delegate_);

  // PpapiBroker implementation.
  virtual void Connect(webkit::ppapi::PPB_Broker_Impl* client) OVERRIDE;
  virtual void Disconnect(webkit::ppapi::PPB_Broker_Impl* client) OVERRIDE;

  // Called when the channel to the broker has been established.
  void OnBrokerChannelConnected(base::ProcessHandle broker_process_handle,
                                const IPC::ChannelHandle& channel_handle);

  // Connects the plugin to the broker via a pipe.
  void ConnectPluginToBroker(webkit::ppapi::PPB_Broker_Impl* client);

  // Asynchronously sends a pipe to the broker.
  int32_t SendHandleToBroker(PP_Instance instance,
                             base::SyncSocket::Handle handle);

 protected:
  friend class base::RefCountedThreadSafe<PpapiBrokerImpl>;
  virtual ~PpapiBrokerImpl();
  scoped_ptr<BrokerDispatcherWrapper> dispatcher_;

  // A map of pointers to objects that have requested a connection to the weak
  // pointer we can use to reference them. The mapping is needed so we can clean
  // up entries for objects that may have been deleted.
  typedef std::map<webkit::ppapi::PPB_Broker_Impl*,
                   base::WeakPtr<webkit::ppapi::PPB_Broker_Impl> > ClientMap;
  ClientMap pending_connects_;

  // Pointer to the associated plugin module.
  // Always set and cleared at the same time as the module's pointer to this.
  webkit::ppapi::PluginModule* plugin_module_;

  base::WeakPtr<PepperPluginDelegateImpl> delegate_;

  DISALLOW_COPY_AND_ASSIGN(PpapiBrokerImpl);
};

class PepperPluginDelegateImpl
    : public webkit::ppapi::PluginDelegate,
      public base::SupportsWeakPtr<PepperPluginDelegateImpl>,
      public PepperParentContextProvider {
 public:
  explicit PepperPluginDelegateImpl(RenderViewImpl* render_view);
  virtual ~PepperPluginDelegateImpl();

  // Attempts to create a PPAPI plugin for the given filepath. On success, it
  // will return the newly-created module.
  //
  // There are two reasons for failure. The first is that the plugin isn't
  // a PPAPI plugin. In this case, |*pepper_plugin_was_registered| will be set
  // to false and the caller may want to fall back on creating an NPAPI plugin.
  // the second is that the plugin failed to initialize. In this case,
  // |*pepper_plugin_was_registered| will be set to true and the caller should
  // not fall back on any other plugin types.
  CONTENT_EXPORT scoped_refptr<webkit::ppapi::PluginModule>
  CreatePepperPluginModule(
      const webkit::WebPluginInfo& webplugin_info,
      bool* pepper_plugin_was_registered);

  // Called by RenderView to tell us about painting events, these two functions
  // just correspond to the DidInitiatePaint and DidFlushPaint in R.V..
  void ViewInitiatedPaint();
  void ViewFlushedPaint();

  // Called by RenderView to implement the corresponding function in its base
  // class RenderWidget (see that for more).
  webkit::ppapi::PluginInstance* GetBitmapForOptimizedPluginPaint(
      const gfx::Rect& paint_bounds,
      TransportDIB** dib,
      gfx::Rect* location,
      gfx::Rect* clip);

  // Called by RenderView when ViewMsg_AsyncOpenFile_ACK.
  void OnAsyncFileOpened(base::PlatformFileError error_code,
                         base::PlatformFile file,
                         int message_id);

  // Called by RenderView when ViewMsg_PpapiBrokerChannelCreated.
  void OnPpapiBrokerChannelCreated(int request_id,
                                   base::ProcessHandle broker_process_handle,
                                   const IPC::ChannelHandle& handle);

  // Removes broker from pending_connect_broker_ if present. Returns true if so.
  bool StopWaitingForPpapiBrokerConnection(PpapiBrokerImpl* broker);

  // Notification that the render view has been focused or defocused. This
  // notifies all of the plugins.
  void OnSetFocus(bool has_focus);

  // IME status.
  bool IsPluginFocused() const;
  gfx::Rect GetCaretBounds() const;
  ui::TextInputType GetTextInputType() const;
  bool IsPluginAcceptingCompositionEvents() const;
  bool CanComposeInline() const;

  // IME events.
  void OnImeSetComposition(
      const string16& text,
      const std::vector<WebKit::WebCompositionUnderline>& underlines,
      int selection_start,
      int selection_end);
  void OnImeConfirmComposition(const string16& text);

  // Notification that the request to lock the mouse has completed.
  void OnLockMouseACK(bool succeeded);
  // Notification that the plugin instance has lost the mouse lock.
  void OnMouseLockLost();
  // Notification that a mouse event has arrived at the render view.
  // Returns true if no further handling is needed. For example, if the mouse is
  // currently locked, this method directly dispatches the event to the owner of
  // the mouse lock and returns true.
  bool HandleMouseEvent(const WebKit::WebMouseEvent& event);

  // PluginDelegate implementation.
  virtual void PluginFocusChanged(webkit::ppapi::PluginInstance* instance,
                                  bool focused) OVERRIDE;
  virtual void PluginTextInputTypeChanged(
      webkit::ppapi::PluginInstance* instance) OVERRIDE;
  virtual void PluginCaretPositionChanged(
      webkit::ppapi::PluginInstance* instance) OVERRIDE;
  virtual void PluginRequestedCancelComposition(
      webkit::ppapi::PluginInstance* instance) OVERRIDE;
  virtual void PluginCrashed(webkit::ppapi::PluginInstance* instance) OVERRIDE;
  virtual void InstanceCreated(
      webkit::ppapi::PluginInstance* instance) OVERRIDE;
  virtual void InstanceDeleted(
      webkit::ppapi::PluginInstance* instance) OVERRIDE;
  virtual SkBitmap* GetSadPluginBitmap() OVERRIDE;
  virtual PlatformAudio* CreateAudio(
      uint32_t sample_rate,
      uint32_t sample_count,
      PlatformAudioCommonClient* client) OVERRIDE;
  virtual PlatformAudioInput* CreateAudioInput(
      uint32_t sample_rate,
      uint32_t sample_count,
      PlatformAudioCommonClient* client) OVERRIDE;
  virtual PlatformImage2D* CreateImage2D(int width, int height) OVERRIDE;
  virtual PlatformContext3D* CreateContext3D() OVERRIDE;
  virtual PlatformVideoCapture* CreateVideoCapture(
      media::VideoCapture::EventHandler* handler) OVERRIDE;
  virtual PlatformVideoDecoder* CreateVideoDecoder(
      media::VideoDecodeAccelerator::Client* client,
      int32 command_buffer_route_id) OVERRIDE;
  virtual PpapiBroker* ConnectToPpapiBroker(
      webkit::ppapi::PPB_Broker_Impl* client) OVERRIDE;
  virtual void NumberOfFindResultsChanged(int identifier,
                                          int total,
                                          bool final_result) OVERRIDE;
  virtual void SelectedFindResultChanged(int identifier, int index) OVERRIDE;
  virtual bool RunFileChooser(
      const WebKit::WebFileChooserParams& params,
      WebKit::WebFileChooserCompletion* chooser_completion) OVERRIDE;
  virtual bool AsyncOpenFile(const FilePath& path,
                             int flags,
                             const AsyncOpenFileCallback& callback) OVERRIDE;
  virtual bool AsyncOpenFileSystemURL(
      const GURL& path,
      int flags,
      const AsyncOpenFileCallback& callback) OVERRIDE;
  virtual bool OpenFileSystem(
      const GURL& url,
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
  virtual base::PlatformFileError OpenFile(
      const webkit::ppapi::PepperFilePath& path,
      int flags,
      base::PlatformFile* file) OVERRIDE;
  virtual base::PlatformFileError RenameFile(
      const webkit::ppapi::PepperFilePath& from_path,
      const webkit::ppapi::PepperFilePath& to_path) OVERRIDE;
  virtual base::PlatformFileError DeleteFileOrDir(
      const webkit::ppapi::PepperFilePath& path,
      bool recursive) OVERRIDE;
  virtual base::PlatformFileError CreateDir(
      const webkit::ppapi::PepperFilePath& path) OVERRIDE;
  virtual base::PlatformFileError QueryFile(
      const webkit::ppapi::PepperFilePath& path,
      base::PlatformFileInfo* info) OVERRIDE;
  virtual base::PlatformFileError GetDirContents(
      const webkit::ppapi::PepperFilePath& path,
      webkit::ppapi::DirContents* contents) OVERRIDE;
  virtual void SyncGetFileSystemPlatformPath(
      const GURL& url,
      FilePath* platform_path) OVERRIDE;
  virtual scoped_refptr<base::MessageLoopProxy>
      GetFileThreadMessageLoopProxy() OVERRIDE;
  virtual int32_t ConnectTcp(
      webkit::ppapi::PPB_Flash_NetConnector_Impl* connector,
      const char* host,
      uint16_t port) OVERRIDE;
  virtual int32_t ConnectTcpAddress(
      webkit::ppapi::PPB_Flash_NetConnector_Impl* connector,
      const struct PP_NetAddress_Private* addr) OVERRIDE;
  // This is the completion for both |ConnectTcp()| and |ConnectTcpAddress()|.
  void OnConnectTcpACK(
      int request_id,
      base::PlatformFile socket,
      const PP_NetAddress_Private& local_addr,
      const PP_NetAddress_Private& remote_addr);

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
  void OnTCPSocketConnectACK(uint32 socket_id,
                             bool succeeded,
                             const PP_NetAddress_Private& local_addr,
                             const PP_NetAddress_Private& remote_addr);

  virtual void TCPSocketSSLHandshake(uint32 socket_id,
                                     const std::string& server_name,
                                     uint16_t server_port) OVERRIDE;
  void OnTCPSocketSSLHandshakeACK(uint32 socket_id, bool succeeded);

  virtual void TCPSocketRead(uint32 socket_id, int32_t bytes_to_read) OVERRIDE;
  void OnTCPSocketReadACK(uint32 socket_id,
                          bool succeeded,
                          const std::string& data);

  virtual void TCPSocketWrite(uint32 socket_id,
                              const std::string& buffer) OVERRIDE;
  void OnTCPSocketWriteACK(uint32 socket_id,
                           bool succeeded,
                           int32_t bytes_written);

  virtual void TCPSocketDisconnect(uint32 socket_id) OVERRIDE;

  virtual uint32 UDPSocketCreate() OVERRIDE;

  virtual void UDPSocketBind(
      webkit::ppapi::PPB_UDPSocket_Private_Impl* socket,
      uint32 socket_id,
      const PP_NetAddress_Private& addr) OVERRIDE;
  void OnUDPSocketBindACK(uint32 socket_id, bool succeeded);

  virtual void UDPSocketRecvFrom(uint32 socket_id,
                                 int32_t num_bytes) OVERRIDE;
  void OnUDPSocketRecvFromACK(uint32 socket_id,
                              bool succeeded,
                              const std::string& data,
                              const PP_NetAddress_Private& remote_addr);

  virtual void UDPSocketSendTo(uint32 socket_id,
                               const std::string& buffer,
                               const PP_NetAddress_Private& addr) OVERRIDE;
  void OnUDPSocketSendToACK(uint32 socket_id,
                            bool succeeded,
                            int32_t bytes_written);

  virtual void UDPSocketClose(uint32 socket_id) OVERRIDE;

  virtual int32_t ShowContextMenu(
      webkit::ppapi::PluginInstance* instance,
      webkit::ppapi::PPB_Flash_Menu_Impl* menu,
      const gfx::Point& position) OVERRIDE;
  void OnContextMenuClosed(
      const webkit_glue::CustomContextMenuContext& custom_context);
  void OnCustomContextMenuAction(
      const webkit_glue::CustomContextMenuContext& custom_context,
      unsigned action);
  void CompleteShowContextMenu(int request_id,
                               bool did_select,
                               unsigned action);
  virtual webkit::ppapi::FullscreenContainer*
      CreateFullscreenContainer(
          webkit::ppapi::PluginInstance* instance) OVERRIDE;
  virtual gfx::Size GetScreenSize() OVERRIDE;
  virtual std::string GetDefaultEncoding() OVERRIDE;
  virtual void ZoomLimitsChanged(double minimum_factor, double maximum_factor)
      OVERRIDE;
  virtual std::string ResolveProxy(const GURL& url) OVERRIDE;
  virtual void DidStartLoading() OVERRIDE;
  virtual void DidStopLoading() OVERRIDE;
  virtual void SetContentRestriction(int restrictions) OVERRIDE;
  virtual void SaveURLAs(const GURL& url) OVERRIDE;
  virtual webkit_glue::P2PTransport* CreateP2PTransport() OVERRIDE;
  virtual double GetLocalTimeZoneOffset(base::Time t) OVERRIDE;
  virtual std::string GetFlashCommandLineArgs() OVERRIDE;
  virtual base::SharedMemory* CreateAnonymousSharedMemory(uint32_t size)
      OVERRIDE;
  virtual ::ppapi::Preferences GetPreferences() OVERRIDE;
  virtual void LockMouse(webkit::ppapi::PluginInstance* instance) OVERRIDE;
  virtual void UnlockMouse(webkit::ppapi::PluginInstance* instance) OVERRIDE;
  virtual void DidChangeCursor(webkit::ppapi::PluginInstance* instance,
                               const WebKit::WebCursorInfo& cursor) OVERRIDE;
  virtual void DidReceiveMouseEvent(
      webkit::ppapi::PluginInstance* instance) OVERRIDE;
  virtual bool IsInFullscreenMode() OVERRIDE;

  CONTENT_EXPORT int GetRoutingId() const;

 private:
  // Asynchronously attempts to create a PPAPI broker for the given plugin.
  scoped_refptr<PpapiBrokerImpl> CreatePpapiBroker(
      webkit::ppapi::PluginModule* plugin_module);

  bool MouseLockedOrPending() const {
    return mouse_locked_ || pending_lock_request_ || pending_unlock_request_;
  }

  // Implementation of PepperParentContextProvider.
  virtual RendererGLContext* GetParentContextForPlatformContext3D() OVERRIDE;

  // Pointer to the RenderView that owns us.
  RenderViewImpl* render_view_;

  std::set<webkit::ppapi::PluginInstance*> active_instances_;

  // Used to send a single context menu "completion" upon menu close.
  bool has_saved_context_menu_action_;
  unsigned saved_context_menu_action_;

  IDMap<AsyncOpenFileCallback> pending_async_open_files_;

  IDMap<scoped_refptr<webkit::ppapi::PPB_Flash_NetConnector_Impl>,
        IDMapOwnPointer> pending_connect_tcps_;

  IDMap<webkit::ppapi::PPB_TCPSocket_Private_Impl> tcp_sockets_;

  IDMap<webkit::ppapi::PPB_UDPSocket_Private_Impl> udp_sockets_;

  IDMap<scoped_refptr<webkit::ppapi::PPB_Flash_Menu_Impl>,
        IDMapOwnPointer> pending_context_menus_;

  typedef IDMap<scoped_refptr<PpapiBrokerImpl>, IDMapOwnPointer> BrokerMap;
  BrokerMap pending_connect_broker_;

  // Whether or not the focus is on a PPAPI plugin
  webkit::ppapi::PluginInstance* focused_plugin_;

  // Current text input composition text. Empty if no composition is in
  // progress.
  string16 composition_text_;

  // |mouse_lock_owner_| is not owned by this class. We can know about when it
  // is destroyed via InstanceDeleted().
  // |mouse_lock_owner_| being non-NULL doesn't indicate that currently the
  // mouse has been locked. It is possible that a request to lock the mouse has
  // been sent, but the response hasn't arrived yet.
  webkit::ppapi::PluginInstance* mouse_lock_owner_;
  bool mouse_locked_;
  // If both |pending_lock_request_| and |pending_unlock_request_| are true,
  // it means a lock request was sent before an unlock request and we haven't
  // received responses for them.
  // The logic in LockMouse() makes sure that a lock request won't be sent when
  // there is a pending unlock request.
  bool pending_lock_request_;
  bool pending_unlock_request_;

  // The plugin instance that received the last mouse event. It is set to NULL
  // if the last mouse event went to elements other than Pepper plugins.
  // |last_mouse_event_target_| is not owned by this class. We can know about
  // when it is destroyed via InstanceDeleted().
  webkit::ppapi::PluginInstance* last_mouse_event_target_;

  DISALLOW_COPY_AND_ASSIGN(PepperPluginDelegateImpl);
};

#endif  // CONTENT_RENDERER_PEPPER_PLUGIN_DELEGATE_IMPL_H_
