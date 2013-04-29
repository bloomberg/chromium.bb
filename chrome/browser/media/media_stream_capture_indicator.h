// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_MEDIA_STREAM_CAPTURE_INDICATOR_H_
#define CHROME_BROWSER_MEDIA_MEDIA_STREAM_CAPTURE_INDICATOR_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "content/public/common/media_stream_request.h"
#include "ui/base/models/simple_menu_model.h"

namespace content {
class WebContents;
}  // namespace content

namespace gfx {
class ImageSkia;
}  // namespace gfx

class ScreenCaptureNotificationUI;
class StatusIcon;
class StatusTray;

// This indicator is owned by MediaCaptureDevicesDispatcher
// (MediaCaptureDevicesDispatcher is a singleton).
class MediaStreamCaptureIndicator
    : public base::RefCountedThreadSafe<MediaStreamCaptureIndicator>,
      public ui::SimpleMenuModel::Delegate {
 public:
  MediaStreamCaptureIndicator();

  // Registers a new media stream for |web_contents| and returns UI object
  // that's used by the content layer to notify about state of the stream.
  scoped_ptr<content::MediaStreamUI> RegisterMediaStream(
      content::WebContents* web_contents,
      const content::MediaStreamDevices& devices);

  // Overrides from SimpleMenuModel::Delegate implementation.
  virtual bool IsCommandIdChecked(int command_id) const OVERRIDE;
  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE;
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) OVERRIDE;
  virtual void ExecuteCommand(int command_id, int event_flags) OVERRIDE;

  // Returns true if the |web_contents| is capturing user media (e.g., webcam or
  // microphone input).
  bool IsCapturingUserMedia(content::WebContents* web_contents) const;

  // Returns true if the |web_contents| itself is being mirrored (e.g., a source
  // of media for remote broadcast).
  bool IsBeingMirrored(content::WebContents* web_contents) const;

 private:
  class UIDelegate;
  class WebContentsDeviceUsage;
  friend class WebContentsDeviceUsage;

  friend class base::RefCountedThreadSafe<MediaStreamCaptureIndicator>;
  virtual ~MediaStreamCaptureIndicator();

  // Following functions/variables are executed/accessed only on UI thread.

  // Called by WebContentsDeviceUsage when it's about to destroy itself, i.e.
  // when WebContents is being destroyed.
  void UnregisterWebContents(content::WebContents* web_contents);

  // Triggers a balloon in the corner telling capture devices are being used.
  // Called by CaptureDevicesOpened().
  void ShowBalloon(content::WebContents* web_contents,
                   int balloon_body_message_id);

  // ImageLoader callback.
  void OnImageLoaded(const string16& message, const gfx::Image& image);

  // Updates the status tray menu and the screen capture notification. Called by
  // WebContentsDeviceUsage.
  void UpdateNotificationUserInterface();

  // Helpers to create and destroy status tray icon. Called from
  // UpdateNotificationUserInterface().
  void EnsureStatusTrayIconResources();
  void MaybeCreateStatusTrayIcon();
  void MaybeDestroyStatusTrayIcon();
  void UpdateStatusTrayIconDisplay(bool audio, bool video);

  // Callback for ScreenCaptureNotificationUI.
  void OnStopScreenCapture(const base::Closure& stop);

  // Reference to our status icon - owned by the StatusTray. If null,
  // the platform doesn't support status icons.
  StatusIcon* status_icon_;

  // These images are owned by ResourceBundle and need not be destroyed.
  gfx::ImageSkia* mic_image_;
  gfx::ImageSkia* camera_image_;
  gfx::ImageSkia* balloon_image_;

  // A map that contains the usage counts of the opened capture devices for each
  // WebContents instance.
  typedef std::map<content::WebContents*, WebContentsDeviceUsage*> UsageMap;
  UsageMap usage_map_;

  // A vector which maps command IDs to their associated WebContents
  // instance. This is rebuilt each time the status tray icon context menu is
  // updated.
  typedef std::vector<content::WebContents*> CommandTargets;
  CommandTargets command_targets_;

  bool should_show_balloon_;

  scoped_ptr<ScreenCaptureNotificationUI> screen_capture_notification_;
};

#endif  // CHROME_BROWSER_MEDIA_MEDIA_STREAM_CAPTURE_INDICATOR_H_
