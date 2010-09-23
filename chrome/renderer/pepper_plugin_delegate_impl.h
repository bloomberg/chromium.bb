// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PEPPER_PLUGIN_DELEGATE_IMPL_H_
#define CHROME_RENDERER_PEPPER_PLUGIN_DELEGATE_IMPL_H_
#pragma once

#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/id_map.h"
#include "base/weak_ptr.h"
#include "third_party/ppapi/c/pp_errors.h"
#include "webkit/glue/plugins/pepper_plugin_delegate.h"
#include "webkit/glue/plugins/pepper_plugin_instance.h"

class FilePath;
class RenderView;

namespace pepper {
class FileIO;
class PluginInstance;
}

namespace WebKit {
class WebFileChooserCompletion;
struct WebFileChooserParams;
}

class PepperPluginDelegateImpl
    : public pepper::PluginDelegate,
      public base::SupportsWeakPtr<PepperPluginDelegateImpl> {
 public:
  explicit PepperPluginDelegateImpl(RenderView* render_view);
  virtual ~PepperPluginDelegateImpl() {}

  // Called by RenderView to tell us about painting events, these two functions
  // just correspond to the DidInitiatePaint and DidFlushPaint in R.V..
  void ViewInitiatedPaint();
  void ViewFlushedPaint();

  // Called by RenderView when ViewMsg_AsyncOpenFile_ACK.
  void OnAsyncFileOpened(base::PlatformFileError error_code,
                         base::PlatformFile file,
                         int message_id);

  // pepper::PluginDelegate implementation.
  virtual void InstanceCreated(pepper::PluginInstance* instance);
  virtual void InstanceDeleted(pepper::PluginInstance* instance);
  virtual PlatformAudio* CreateAudio(
      uint32_t sample_rate,
      uint32_t sample_count,
      pepper::PluginDelegate::PlatformAudio::Client* client);
  virtual PlatformImage2D* CreateImage2D(int width, int height);
  virtual PlatformContext3D* CreateContext3D();
  virtual PlatformVideoDecoder* CreateVideoDecoder(
      const PP_VideoDecoderConfig_Dev& decoder_config);
  virtual void DidChangeNumberOfFindResults(int identifier,
                                            int total,
                                            bool final_result);
  virtual void DidChangeSelectedFindResult(int identifier, int index);
  virtual bool RunFileChooser(
      const WebKit::WebFileChooserParams& params,
      WebKit::WebFileChooserCompletion* chooser_completion);
  virtual bool AsyncOpenFile(const FilePath& path,
                             int flags,
                             AsyncOpenFileCallback* callback);
  virtual scoped_refptr<base::MessageLoopProxy> GetFileThreadMessageLoopProxy();
  virtual pepper::FullscreenContainer* CreateFullscreenContainer(
      pepper::PluginInstance* instance);
  virtual std::string GetDefaultEncoding();

 private:
  // Pointer to the RenderView that owns us.
  RenderView* render_view_;

  std::set<pepper::PluginInstance*> active_instances_;

  int id_generator_;
  IDMap<AsyncOpenFileCallback> messages_waiting_replies_;

  DISALLOW_COPY_AND_ASSIGN(PepperPluginDelegateImpl);
};

#endif  // CHROME_RENDERER_PEPPER_PLUGIN_DELEGATE_IMPL_H_
