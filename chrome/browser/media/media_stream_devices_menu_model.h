// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_MEDIA_STREAM_DEVICES_MENU_MODEL_H_
#define CHROME_BROWSER_MEDIA_MEDIA_STREAM_DEVICES_MENU_MODEL_H_

#include <map>
#include <string>

#include "content/public/common/media_stream_request.h"
#include "ui/base/models/simple_menu_model.h"

class MediaStreamInfoBarDelegate;

// A menu model that builds the contents of the devices menu in the media stream
// infobar. This menu has only one level (no submenus).
class MediaStreamDevicesMenuModel : public ui::SimpleMenuModel,
                                    public ui::SimpleMenuModel::Delegate {
 public:
  explicit MediaStreamDevicesMenuModel(MediaStreamInfoBarDelegate* delegate);
  virtual ~MediaStreamDevicesMenuModel();

  // ui::SimpleMenuModel::Delegate implementation:
  virtual bool IsCommandIdChecked(int command_id) const OVERRIDE;
  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE;
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) OVERRIDE;
  virtual void ExecuteCommand(int command_id) OVERRIDE;

 private:
  typedef std::map<int, content::MediaStreamDevice> CommandMap;

  // Internal method to add the |devices| to the current menu.
  void AddDevices(const content::MediaStreamDevices& devices);

  // Internal method to add "always allow" option to the current menu.
  void AddAlwaysAllowOption(bool audio, bool video);

  // Map of command IDs to devices.
  CommandMap commands_;

  MediaStreamInfoBarDelegate* media_stream_delegate_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamDevicesMenuModel);
};

#endif  // CHROME_BROWSER_MEDIA_MEDIA_STREAM_DEVICES_MENU_MODEL_H_
