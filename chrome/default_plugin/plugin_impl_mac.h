// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_DEFAULT_PLUGIN_PLUGIN_IMPL_MAC_H_
#define CHROME_DEFAULT_PLUGIN_PLUGIN_IMPL_MAC_H_
#pragma once

#include <string>

#include "third_party/npapi/bindings/npapi.h"
#include "ui/gfx/native_widget_types.h"

#ifdef __OBJC__
@class NSImage;
@class NSString;
#else
class NSImage;
class NSString;
#endif

// Possible plugin installer states.
enum PluginInstallerState {
  PluginInstallerStateUndefined,
  PluginListDownloadInitiated,
  PluginListDownloaded,
  PluginListDownloadedPluginNotFound,
  PluginListDownloadFailed,
  PluginDownloadInitiated,
  PluginDownloadCompleted,
  PluginDownloadFailed,
  PluginInstallerLaunchSuccess,
  PluginInstallerLaunchFailure
};

class PluginInstallDialog;
class PluginDatabaseHandler;

// Provides the plugin installation functionality. This class is
// instantiated with the information like the mime type of the
// target plugin, the display mode, etc.
class PluginInstallerImpl {
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

  // Used by the renderer to indicate plugin install through the infobar.
  int16 NPP_HandleEvent(void* event);

  const std::string& mime_type() const { return mime_type_; }

  // Replaces a resource string with the placeholder passed in as an argument
  //
  // Parameters:
  // message_id_with_placeholders
  //   The resource id of the string with placeholders. This is only used if
  //   the placeholder string (the replacement_string) parameter is valid.
  // message_id_without_placeholders
  //   The resource id of the string to be returned if the placeholder is
  //   empty.
  // replacement_string
  //   The placeholder which replaces tokens in the string identified by
  //   resource id message_id_with_placeholders.
  // Returns a string which has the placeholders replaced, or the string
  // without placeholders.
  static std::wstring ReplaceStringForPossibleEmptyReplacement(
      int message_id_with_placeholders, int message_id_without_placeholders,
      const std::wstring& replacement_string);

  // Setter/getter combination to set and retreieve the current
  // state of the plugin installer.
  void set_plugin_installer_state(PluginInstallerState new_state) {
    plugin_installer_state_ = new_state;
  }

  PluginInstallerState plugin_installer_state() const {
    return plugin_installer_state_;
  }

  // Getter for the NPP instance member.
  NPP instance() const {
    return instance_;
  }

  // Returns whether or not the UI layout is right-to-left (such as Hebrew or
  // Arabic).
  bool IsRTLLayout() const;

 protected:
  int16 OnDrawRect(CGContextRef context, CGRect dirty_rect);

  // Displays the plugin install confirmation dialog.
  void ShowInstallDialog();

  // Clears the current display state.
  void ClearDisplay();

  // Displays the status message identified by the message resource id
  // passed in.
  //
  // Parameters:
  // message_resource_id parameter
  //   The resource id of the message to be displayed.
  void DisplayStatus(int message_resource_id);

  // Displays status information for the third party plugin which is needed
  // by the page.
  void DisplayAvailablePluginStatus();

  // Displays information related to third party plugin download failure.
  void DisplayPluginDownloadFailedStatus();

  // Enables the plugin window if required and initiates an update of the
  // the plugin window.
  void RefreshDisplay();

  // Create tooltip window.
  bool CreateToolTip();

  // Update ToolTip text with the message shown inside the default plugin.
  void UpdateToolTip();

  // Resolves the relative URL (could be already an absolute URL too) to return
  // full URL based on current document's URL and base.
  //
  // Parameters:
  // instance
  //   The plugins opaque instance handle.
  // relative_url
  //   The URL to be resolved.
  // Returns the resolved URL.
  std::string ResolveURL(NPP instance, const std::string& relative_url);

  // Initializes resources like the icon, fonts, etc needed by the plugin
  // installer
  //
  // Parameters:
  // module_handle
  //   Handle to the dll in which this object is instantiated.
  // Returns true on success.
  bool InitializeResources(void *module_handle);

  // Parses the plugin instantiation arguments. This includes checking for
  // whether this is an activex install and reading the appropriate
  // arguments like codebase, etc. For plugin installs we download the
  // plugin finder URL and initalize the mime type and the plugin instance
  // info.
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
  // raw_activex_clsid
  //   Output parameter which contains the CLSID of the Activex plugin needed.
  //   This is only applicable if the webpage specifically requests an ActiveX
  //   control.
  // Returns true on success.
  bool ParseInstantiationArguments(NPMIMEType mime_type, NPP instance,
                                   int16 argc, char* argn[], char* argv[],
                                   std::string* raw_activex_clsid);

  // Paints user action messages to the plugin window. These include messages
  // like whether the user should click on the plugin window to download the
  // plugin, etc.
  //
  // Parameters:
  // paint_dc
  //   The device context returned in BeginPaint.
  // x_origin
  //   Horizontal reference point indicating where the text is to be displayed.
  // y_origin
  //   Vertical reference point indicating where the text is to be displayed.
  //
  void PaintUserActionInformation(gfx::NativeDrawingContext paint_dc,
                                  int x_origin, int y_origin);

 private:
  // Notify the renderer that plugin is available to download.
  void NotifyPluginStatus(int status);

  // The plugins opaque instance handle
  NPP instance_;
  // The current stream.
  NPStream* plugin_install_stream_;
  // The desired mime type.
  std::string mime_type_;
  // The current state of the plugin installer.
  PluginInstallerState plugin_installer_state_;
  // Dimensions of the plugin
  uint32_t width_;
  uint32_t height_;
  // Plugin icon, weak (owned by ResourceBundle).
  NSImage* image_;
  // Displayed text
  NSString* command_;

  DISALLOW_COPY_AND_ASSIGN(PluginInstallerImpl);
};


#endif  // CHROME_DEFAULT_PLUGIN_PLUGIN_IMPL_MAC_H_
