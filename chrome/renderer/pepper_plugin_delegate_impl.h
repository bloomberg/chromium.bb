// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PEPPER_PLUGIN_DELEGATE_IMPL_H_
#define CHROME_RENDERER_PEPPER_PLUGIN_DELEGATE_IMPL_H_
#pragma once

#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/id_map.h"
#include "base/ref_counted.h"
#include "base/weak_ptr.h"
#include "ppapi/c/pp_errors.h"
#include "webkit/plugins/ppapi/plugin_delegate.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/ppb_flash_menu_impl.h"

class FilePath;
class RenderView;

namespace gfx {
class Point;
class Rect;
}

namespace webkit {
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

class PepperPluginDelegateImpl
    : public webkit::ppapi::PluginDelegate,
      public base::SupportsWeakPtr<PepperPluginDelegateImpl> {
 public:
  explicit PepperPluginDelegateImpl(RenderView* render_view);
  virtual ~PepperPluginDelegateImpl();

  scoped_refptr<webkit::ppapi::PluginModule> CreatePepperPlugin(
      const FilePath& path);

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

  // Notification that the render view has been focused or defocused. This
  // notifies all of the plugins.
  void OnSetFocus(bool has_focus);

  // PluginDelegate implementation.
  virtual void InstanceCreated(
      webkit::ppapi::PluginInstance* instance);
  virtual void InstanceDeleted(
      webkit::ppapi::PluginInstance* instance);
  virtual PlatformAudio* CreateAudio(
      uint32_t sample_rate,
      uint32_t sample_count,
      PlatformAudio::Client* client);
  virtual PlatformImage2D* CreateImage2D(int width, int height);
  virtual PlatformContext3D* CreateContext3D();
  virtual PlatformVideoDecoder* CreateVideoDecoder(
      const PP_VideoDecoderConfig_Dev& decoder_config);
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
  virtual bool OpenFileSystem(
      const GURL& url,
      fileapi::FileSystemType type,
      long long size,
      fileapi::FileSystemCallbackDispatcher* dispatcher);
  virtual bool MakeDirectory(const FilePath& path,
                             bool recursive,
                             fileapi::FileSystemCallbackDispatcher* dispatcher);
  virtual bool Query(const FilePath& path,
                     fileapi::FileSystemCallbackDispatcher* dispatcher);
  virtual bool Touch(const FilePath& path,
                     const base::Time& last_access_time,
                     const base::Time& last_modified_time,
                     fileapi::FileSystemCallbackDispatcher* dispatcher);
  virtual bool Delete(const FilePath& path,
                      fileapi::FileSystemCallbackDispatcher* dispatcher);
  virtual bool Rename(const FilePath& file_path,
                      const FilePath& new_file_path,
                      fileapi::FileSystemCallbackDispatcher* dispatcher);
  virtual bool ReadDirectory(const FilePath& directory_path,
                             fileapi::FileSystemCallbackDispatcher* dispatcher);
  virtual base::PlatformFileError OpenFile(
      const webkit::ppapi::PepperFilePath& path,
      int flags,
      base::PlatformFile* file);
  virtual base::PlatformFileError RenameFile(
      const webkit::ppapi::PepperFilePath& from_path,
      const webkit::ppapi::PepperFilePath& to_path);
  virtual base::PlatformFileError DeleteFileOrDir(
      const webkit::ppapi::PepperFilePath& path,
      bool recursive);
  virtual base::PlatformFileError CreateDir(
      const webkit::ppapi::PepperFilePath& path);
  virtual base::PlatformFileError QueryFile(
      const webkit::ppapi::PepperFilePath& path,
      base::PlatformFileInfo* info);
  virtual base::PlatformFileError GetDirContents(
      const webkit::ppapi::PepperFilePath& path,
      webkit::ppapi::DirContents* contents);
  virtual scoped_refptr<base::MessageLoopProxy> GetFileThreadMessageLoopProxy();
  virtual int32_t ConnectTcp(
      webkit::ppapi::PPB_Flash_NetConnector_Impl* connector,
      const char* host,
      uint16_t port);
  virtual int32_t ConnectTcpAddress(
      webkit::ppapi::PPB_Flash_NetConnector_Impl* connector,
      const struct PP_Flash_NetAddress* addr);
  // This is the completion for both |ConnectTcp()| and |ConnectTcpAddress()|.
  void OnConnectTcpACK(
      int request_id,
      base::PlatformFile socket,
      const PP_Flash_NetAddress& local_addr,
      const PP_Flash_NetAddress& remote_addr);
  virtual int32_t ShowContextMenu(
      webkit::ppapi::PPB_Flash_Menu_Impl* menu,
      const gfx::Point& position);
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
          webkit::ppapi::PluginInstance* instance);
  virtual std::string GetDefaultEncoding();
  virtual void ZoomLimitsChanged(double minimum_factor, double maximum_factor);
  virtual std::string ResolveProxy(const GURL& url);
  virtual void DidStartLoading();
  virtual void DidStopLoading();
  virtual void SetContentRestriction(int restrictions);
  virtual void HasUnsupportedFeature();

 private:
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

  DISALLOW_COPY_AND_ASSIGN(PepperPluginDelegateImpl);
};

#endif  // CHROME_RENDERER_PEPPER_PLUGIN_DELEGATE_IMPL_H_
