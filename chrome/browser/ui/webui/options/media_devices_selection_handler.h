// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_MEDIA_DEVICES_SELECTION_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_MEDIA_DEVICES_SELECTION_HANDLER_H_

#include "base/macros.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/ui/webui/options/options_ui.h"
#include "content/public/browser/web_contents.h"

namespace options {

// Handler for media devices selection in content settings.
class MediaDevicesSelectionHandler
    : public MediaCaptureDevicesDispatcher::Observer,
      public OptionsPageUIHandler {
 public:
  MediaDevicesSelectionHandler();
  ~MediaDevicesSelectionHandler() override;

  // OptionsPageUIHandler implementation.
  void GetLocalizedValues(base::DictionaryValue* values) override;
  void InitializePage() override;
  void RegisterMessages() override;

  // MediaCaptureDevicesDispatcher::Observer implementation.
  void OnUpdateAudioDevices(
      const content::MediaStreamDevices& devices) override;
  void OnUpdateVideoDevices(
      const content::MediaStreamDevices& devices) override;

 private:
  enum DeviceType {
    AUDIO,
    VIDEO,
  };

  // Sets the default audio/video capture device for media. |args| includes the
  // media type (kAuudio/kVideo) and the unique id of the new default device
  // that the user has chosen.
  void SetDefaultCaptureDevice(const base::ListValue* args);

  // Helpers methods to update the device menus.
  void UpdateDevicesMenuForType(DeviceType type);
  void UpdateDevicesMenu(DeviceType type,
                         const content::MediaStreamDevices& devices);

  DISALLOW_COPY_AND_ASSIGN(MediaDevicesSelectionHandler);
};

}  // namespace options

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_MEDIA_DEVICES_SELECTION_HANDLER_H_
