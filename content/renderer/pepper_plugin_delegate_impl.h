// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PLUGIN_DELEGATE_IMPL_H_
#define CONTENT_RENDERER_PEPPER_PLUGIN_DELEGATE_IMPL_H_
#pragma once

#include <set>
#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/id_map.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "ppapi/proxy/broker_dispatcher.h"
#include "ppapi/proxy/proxy_channel.h"
#include "webkit/plugins/ppapi/plugin_delegate.h"
#include "webkit/plugins/ppapi/ppb_broker_impl.h"
#include "webkit/plugins/ppapi/ppb_flash_menu_impl.h"

class FilePath;
class PepperPluginDelegateImpl;
class RenderView;

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
struct WebFileChooserParams;
}

namespace webkit_glue {
struct CustomContextMenuContext;
}

class TransportDIB;

// This object is NOT thread-safe.
class BrokerDispatcherWrapper {
 public:
  BrokerDispatcherWrapper();
  ~BrokerDispatcherWrapper();

  bool Init(base::ProcessHandle plugin_process_handle,
            const IPC::ChannelHandle& channel_handle);

  int32_t SendHandleToBroker(PP_Instance instance,
                             base::SyncSocket::Handle handle);

 private:
  scoped_ptr<ppapi::proxy::BrokerDispatcher> dispatcher_;
};

// This object is NOT thread-safe.
class PpapiBrokerImpl : public webkit::ppapi::PluginDelegate::PpapiBroker,
                        public base::RefCountedThreadSafe<PpapiBrokerImpl>{
 public:
  PpapiBrokerImpl(webkit::ppapi::PluginModule* plugin_module,
                  PepperPluginDelegateImpl* delegate_);

  // PpapiBroker implementation.
  virtual void Connect(webkit::ppapi::PPB_Broker_Impl* client);
  virtual void Disconnect(webkit::ppapi::PPB_Broker_Impl* client);

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
      public base::SupportsWeakPtr<PepperPluginDelegateImpl> {
 public:
  explicit PepperPluginDelegateImpl(RenderView* render_view);
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
  scoped_refptr<webkit::ppapi::PluginModule> CreatePepperPluginModule(
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

  // Returns whether or not a Pepper plugin is focused.
  bool IsPluginFocused() const;

  // PluginDelegate implementation.
  virtual void PluginFocusChanged(bool focused) OVERRIDE;
  virtual void PluginCrashed(webkit::ppapi::PluginInstance* instance);
  virtual void InstanceCreated(
      webkit::ppapi::PluginInstance* instance);
  virtual void InstanceDeleted(
      webkit::ppapi::PluginInstance* instance);
  virtual SkBitmap* GetSadPluginBitmap();
  virtual PlatformAudio* CreateAudio(
      uint32_t sample_rate,
      uint32_t sample_count,
      PlatformAudio::Client* client);
  virtual PlatformImage2D* CreateImage2D(int width, int height);
  virtual PlatformContext3D* CreateContext3D();
  virtual PlatformVideoCapture* CreateVideoCapture(
      media::VideoCapture::EventHandler* handler) OVERRIDE;
  virtual PlatformVideoDecoder* CreateVideoDecoder(
      media::VideoDecodeAccelerator::Client* client,
      int32 command_buffer_route_id);
  virtual PpapiBroker* ConnectToPpapiBroker(
      webkit::ppapi::PPB_Broker_Impl* client);
  virtual void NumberOfFindResultsChanged(int identifier,
                                          int total,
                                          bool final_result);
  virtual void SelectedFindResultChanged(int identifier, int index);
  virtual bool RunFileChooser(
      const WebKit::WebFileChooserParams& params,
      WebKit::WebFileChooserCompletion* chooser_completion);
  virtual bool AsyncOpenFile(const FilePath& path,
                             int flags,
                             AsyncOpenFileCallback* callback);
  virtual bool AsyncOpenFileSystemURL(
      const GURL& path,
      int flags,
      AsyncOpenFileCallback* callback) OVERRIDE;
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
  virtual void PublishPolicy(const std::string& policy_json) OVERRIDE;
  virtual void QueryAvailableSpace(const GURL& origin,
                                   quota::StorageType type,
                                   AvailableSpaceCallback* callback) OVERRIDE;
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
      const struct PP_Flash_NetAddress* addr) OVERRIDE;
  // This is the completion for both |ConnectTcp()| and |ConnectTcpAddress()|.
  void OnConnectTcpACK(
      int request_id,
      base::PlatformFile socket,
      const PP_Flash_NetAddress& local_addr,
      const PP_Flash_NetAddress& remote_addr);
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
  virtual void SubscribeToPolicyUpdates(
      webkit::ppapi::PluginInstance* instance) OVERRIDE;
  virtual std::string ResolveProxy(const GURL& url) OVERRIDE;
  virtual void DidStartLoading() OVERRIDE;
  virtual void DidStopLoading() OVERRIDE;
  virtual void SetContentRestriction(int restrictions) OVERRIDE;
  virtual void SaveURLAs(const GURL& url) OVERRIDE;
  virtual P2PSocketDispatcher* GetP2PSocketDispatcher() OVERRIDE;
  virtual webkit_glue::P2PTransport* CreateP2PTransport() OVERRIDE;
  virtual double GetLocalTimeZoneOffset(base::Time t) OVERRIDE;
  virtual std::string GetFlashCommandLineArgs() OVERRIDE;
  virtual base::SharedMemory* CreateAnonymousSharedMemory(uint32_t size)
      OVERRIDE;
  virtual ::ppapi::Preferences GetPreferences() OVERRIDE;

  int GetRoutingId() const;

 private:
  void PublishInitialPolicy(
      scoped_refptr<webkit::ppapi::PluginInstance> instance,
      const std::string& policy);

  // Asynchronously attempts to create a PPAPI broker for the given plugin.
  scoped_refptr<PpapiBrokerImpl> CreatePpapiBroker(
      webkit::ppapi::PluginModule* plugin_module);

  // Pointer to the RenderView that owns us.
  RenderView* render_view_;

  std::set<webkit::ppapi::PluginInstance*> active_instances_;

  // Used to send a single context menu "completion" upon menu close.
  bool has_saved_context_menu_action_;
  unsigned saved_context_menu_action_;

  // TODO(viettrungluu): Get rid of |id_generator_| -- just use |IDMap::Add()|.
  // Rename |messages_waiting_replies_| (to specify async open file).
  int id_generator_;
  IDMap<AsyncOpenFileCallback> messages_waiting_replies_;

  IDMap<scoped_refptr<webkit::ppapi::PPB_Flash_NetConnector_Impl>,
        IDMapOwnPointer> pending_connect_tcps_;

  IDMap<scoped_refptr<webkit::ppapi::PPB_Flash_Menu_Impl>,
        IDMapOwnPointer> pending_context_menus_;

  typedef IDMap<scoped_refptr<PpapiBrokerImpl>, IDMapOwnPointer> BrokerMap;
  BrokerMap pending_connect_broker_;

  // Whether or not the focus is on a PPAPI plugin
  bool is_pepper_plugin_focused_;

  // Set of instances to receive a notification when the enterprise policy has
  // been updated.
  std::set<webkit::ppapi::PluginInstance*> subscribed_to_policy_updates_;

  DISALLOW_COPY_AND_ASSIGN(PepperPluginDelegateImpl);
};

#endif  // CONTENT_RENDERER_PEPPER_PLUGIN_DELEGATE_IMPL_H_
