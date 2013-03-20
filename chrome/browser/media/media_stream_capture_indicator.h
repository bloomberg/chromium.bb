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

class StatusIcon;
class StatusTray;

// This indicator is owned by MediaInternals and deleted when MediaInternals
// is deleted.
class MediaStreamCaptureIndicator
    : public base::RefCountedThreadSafe<MediaStreamCaptureIndicator>,
      public ui::SimpleMenuModel::Delegate {
 public:
  MediaStreamCaptureIndicator();

  // Overrides from SimpleMenuModel::Delegate implementation.
  virtual bool IsCommandIdChecked(int command_id) const OVERRIDE;
  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE;
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) OVERRIDE;
  virtual void ExecuteCommand(int command_id, int event_flags) OVERRIDE;

  // Called on IO thread when MediaStream opens new capture devices.
  void CaptureDevicesOpened(int render_process_id,
                            int render_view_id,
                            const content::MediaStreamDevices& devices,
                            const base::Closure& close_callback);

  // Called on IO thread when MediaStream closes the opened devices.
  void CaptureDevicesClosed(int render_process_id,
                            int render_view_id,
                            const content::MediaStreamDevices& devices);

  // Returns true if the render view is capturing user media (e.g., webcam
  // or microphone input).
  bool IsCapturingUserMedia(int render_process_id, int render_view_id) const;

  // Returns true if the render view itself is being mirrored (e.g., a source of
  // media for remote broadcast).
  bool IsBeingMirrored(int render_process_id, int render_view_id) const;

  // ImageLoader callback.
  void OnImageLoaded(const string16& message, const gfx::Image& image);

 private:
  class WebContentsDeviceUsage;

  friend class base::RefCountedThreadSafe<MediaStreamCaptureIndicator>;
  virtual ~MediaStreamCaptureIndicator();

  // Called by the public functions, executed on UI thread.
  void DoDevicesOpenedOnUIThread(int render_process_id,
                                 int render_view_id,
                                 const content::MediaStreamDevices& devices,
                                 const base::Closure& close_callback);
  void DoDevicesClosedOnUIThread(int render_process_id,
                                 int render_view_id,
                                 const content::MediaStreamDevices& devices);

  // Following functions/variables are executed/accessed only on UI thread.

  // Creates and shows the status tray icon if it has not been created and is
  // supported on the current platform.
  void MaybeCreateStatusTrayIcon();

  // Makes sure we have done one-time initialization of the images.
  void EnsureStatusTrayIconResources();

  // Finds a WebContents instance by its RenderView's current or
  // previously-known IDs. This is necessary since clients that called
  // CaptureDevicesOpened() may later call CaptureDevicesClosed() with stale
  // IDs. Do not attempt to dereference the pointer without first consulting
  // WebContentsDeviceUsage::IsWebContentsDestroyed().
  content::WebContents* LookUpByKnownAlias(int render_process_id,
                                           int render_view_id) const;

  // Adds devices to usage map and triggers necessary UI updates.
  void AddCaptureDevices(int render_process_id,
                         int render_view_id,
                         const content::MediaStreamDevices& devices,
                         const base::Closure& close_callback);

  // Removes devices from the usage map and triggers necessary UI updates.
  void RemoveCaptureDevices(int render_process_id,
                            int render_view_id,
                            const content::MediaStreamDevices& devices);

  // Triggers a balloon in the corner telling capture devices are being used.
  // This function is called by AddCaptureDevices().
  void ShowBalloon(content::WebContents* web_contents,
                   int balloon_body_message_id);

  // Removes the status tray icon from the desktop. This function is called by
  // RemoveCaptureDevices() when the device usage map becomes empty.
  void MaybeDestroyStatusTrayIcon();

  // Updates the status tray menu with the new device list. This call will be
  // triggered by both AddCaptureDevices() and RemoveCaptureDevices().
  void UpdateStatusTrayIconContextMenu();

  // Updates the status tray tooltip and image according to which kind of
  // devices are being used. This function is called by
  // UpdateStatusTrayIconContextMenu().
  void UpdateStatusTrayIconDisplay(bool audio, bool video);

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

  // A map of previously-known render_process_id/render_view_id pairs that have
  // been associated with a WebContents instance.
  typedef std::pair<int, int> RenderViewIDs;
  typedef std::map<RenderViewIDs, content::WebContents*> AliasMap;
  AliasMap aliases_;

  // A vector which maps command IDs to their associated WebContents
  // instance. This is rebuilt each time the status tray icon context menu is
  // updated.
  typedef std::vector<content::WebContents*> CommandTargets;
  CommandTargets command_targets_;

  bool should_show_balloon_;
};

#endif  // CHROME_BROWSER_MEDIA_MEDIA_STREAM_CAPTURE_INDICATOR_H_
