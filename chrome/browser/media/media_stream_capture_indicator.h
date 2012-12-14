// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_MEDIA_STREAM_CAPTURE_INDICATOR_H_
#define CHROME_BROWSER_MEDIA_MEDIA_STREAM_CAPTURE_INDICATOR_H_

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/common/media_stream_request.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/gfx/image/image_skia.h"

namespace content {
class WebContents;
}  // namespace content

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
  virtual void ExecuteCommand(int command_id) OVERRIDE;

  // Called on IO thread when MediaStream opens new capture devices.
  void CaptureDevicesOpened(int render_process_id,
                            int render_view_id,
                            const content::MediaStreamDevices& devices);

  // Called on IO thread when MediaStream closes the opened devices.
  void CaptureDevicesClosed(int render_process_id,
                            int render_view_id,
                            const content::MediaStreamDevices& devices);

  // Returns true if the render process is capturing media.
  bool IsProcessCapturing(int render_process_id, int render_view_id) const;

  // Returns true if the render process is capturing tab media.
  bool IsProcessCapturingTab(int render_process_id, int render_view_id) const;

  // ImageLoader callback.
  void OnImageLoaded(const string16& message, const gfx::Image& image);

 private:
  // Struct to store the usage information of the capture devices for each tab.
  // Note: It is not safe to dereference WebContents pointer. This is used to
  // track the tab after navigations as render_view_id's can change.
  // TODO(estade): this should be called CaptureDeviceContents; not all the
  // render views it represents are tabs.
  struct CaptureDeviceTab {
    CaptureDeviceTab(content::WebContents* web_contents,
                     int render_process_id,
                     int render_view_id)
        : web_contents(web_contents),
          render_process_id(render_process_id),
          render_view_id(render_view_id),
          audio_ref_count(0),
          video_ref_count(0),
          tab_capture_ref_count(0) {}

    content::WebContents* web_contents;
    int render_process_id;
    int render_view_id;
    int audio_ref_count;
    int video_ref_count;
    int tab_capture_ref_count;
  };

  // A private predicate used in std::find_if to find a |CaptureDeviceTab|
  // which matches the information specified at construction. A tab will match
  // if either the web_contents pointer is the same or the render_process_id and
  // render_view_id's are the same. In the first case, a tab with a UI indicator
  // may have changed page so the id's are different. In the second case, the
  // web_contents may have already been destroyed before the indicator was
  // hidden.
  class TabEquals {
   public:
    TabEquals(content::WebContents* web_contents,
              int render_process_id,
              int render_view_id);

    bool operator() (
        const MediaStreamCaptureIndicator::CaptureDeviceTab& tab);

   private:
    content::WebContents* web_contents_;
    int render_process_id_;
    int render_view_id_;
  };

  friend class base::RefCountedThreadSafe<MediaStreamCaptureIndicator>;
  virtual ~MediaStreamCaptureIndicator();

  // Called by the public functions, executed on UI thread.
  void DoDevicesOpenedOnUIThread(int render_process_id,
                                 int render_view_id,
                                 const content::MediaStreamDevices& devices);
  void DoDevicesClosedOnUIThread(int render_process_id,
                                 int render_view_id,
                                 const content::MediaStreamDevices& devices);

  // Following functions/variables are executed/accessed only on UI thread.
  // Creates the status tray if it has not been created.
  void CreateStatusTray();

  // Makes sure we have done one-time initialization of the images.
  void EnsureStatusTrayIconResources();

  // Adds the new tab to the device usage list.
  void AddCaptureDeviceTab(int render_process_id,
                           int render_view_id,
                           const content::MediaStreamDevices& devices);

  // Removes the tab from the device usage list.
  void RemoveCaptureDeviceTab(int render_process_id,
                              int render_view_id,
                              const content::MediaStreamDevices& devices);

  // Triggers a balloon in the corner telling capture devices are being used.
  // This function is called by AddCaptureDeviceTab().
  void ShowBalloon(int render_process_id, int render_view_id,
                   bool audio, bool video);

  // Hides the status tray from the desktop. This function is called by
  // RemoveCaptureDeviceTab() when the device usage list becomes empty.
  void Hide();

  // Updates the status tray menu with the new device list. This call will be
  // triggered by both AddCaptureDeviceTab() and RemoveCaptureDeviceTab().
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

  // A list that contains the usage information of the opened capture devices.
  typedef std::vector<CaptureDeviceTab> CaptureDeviceTabs;
  CaptureDeviceTabs tabs_;

  bool should_show_balloon_;
};

#endif  // CHROME_BROWSER_MEDIA_MEDIA_STREAM_CAPTURE_INDICATOR_H_
