// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_DEFAULT_PLUGIN_PLUGIN_IMPL_AURA_H_
#define CHROME_DEFAULT_PLUGIN_PLUGIN_IMPL_AURA_H_
#pragma once

#include <string>

#include "chrome/default_plugin/plugin_installer_base.h"
#include "third_party/npapi/bindings/npapi.h"
#include "ui/gfx/native_widget_types.h"

// Provides the plugin installation functionality. This class is
// instantiated with the information like the mime type of the
// target plugin, the display mode, etc.
class PluginInstallerImpl : public PluginInstallerBase {
 public:
  // mode is the plugin instantiation mode, i.e. whether it is a full
  // page plugin (NP_FULL) or an embedded plugin (NP_EMBED)
  explicit PluginInstallerImpl(int16 mode);
  virtual ~PluginInstallerImpl();

  // Initializes the plugin with the instance information, mime type
  // and the list of parameters passed down to the plugin from the webpage.
  //
  // Parameters:
  // module_handle
  //   The handle to the dll in which this object is instantiated.
  // instance
  //   The plugins opaque instance handle.
  // mime_type
  //   Identifies the third party plugin which would be eventually installed.
  // argc
  //   Indicates the count of arguments passed in from the webpage.
  // argv
  //   Pointer to the arguments.
  // Returns true on success.
  bool Initialize(void* module_handle, NPP instance, NPMIMEType mime_type,
                  int16 argc, char* argn[], char* argv[]);

  // Informs the plugin of its window information.
  //
  // Parameters:
  // window_info
  //   The window info passed to npapi.
  bool NPP_SetWindow(NPWindow* window_info);

  // Destroys the install dialog.
  void Shutdown();

  // Starts plugin download. Spawns the plugin installer after it is
  // downloaded.
  void DownloadPlugin();

  // Indicates that the plugin download was cancelled.
  void DownloadCancelled();

  // Initializes the plugin download stream.
  //
  // Parameters:
  // stream
  //   Pointer to the new stream being created.
  void NewStream(NPStream* stream);

  // Uninitializes the plugin download stream.
  //
  // Parameters:
  // stream
  //   Pointer to the stream being destroyed.
  // reason
  //   Indicates why the stream is being destroyed.
  //
  void DestroyStream(NPStream* stream, NPError reason);

  // Determines whether the plugin is ready to accept data.
  // We only accept data when we have initiated a download for the plugin
  // database.
  //
  // Parameters:
  // stream
  //   Pointer to the stream being destroyed.
  // Returns true if the plugin is ready to accept data.
  bool WriteReady(NPStream* stream);

  // Delivers data to the plugin instance.
  //
  // Parameters:
  // stream
  //   Pointer to the stream being destroyed.
  // offset
  //   Indicates the data offset.
  // buffer_length
  //   Indicates the length of the data buffer.
  // buffer
  //   Pointer to the actual buffer.
  // Returns the number of bytes actually written, 0 on error.
  int32 Write(NPStream* stream, int32 offset, int32 buffer_length,
              void* buffer);

  // Handles notifications received in response to GetURLNotify calls issued
  // by the plugin.
  //
  // Parameters:
  // url
  //   Pointer to the URL.
  // reason
  //   Describes why the notification was sent.
  void URLNotify(const char* url, NPReason reason);

  // Used by the renderer to pass events (for e.g. input events) to the plugin.
  int16 NPP_HandleEvent(void* event);

 private:
  DISALLOW_COPY_AND_ASSIGN(PluginInstallerImpl);
};

#endif  // CHROME_DEFAULT_PLUGIN_PLUGIN_IMPL_AURA_H_
