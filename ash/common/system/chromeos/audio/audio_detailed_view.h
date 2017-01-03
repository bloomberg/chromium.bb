// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_CHROMEOS_AUDIO_AUDIO_DETAILED_VIEW_H_
#define ASH_COMMON_SYSTEM_CHROMEOS_AUDIO_AUDIO_DETAILED_VIEW_H_

#include <map>

#include "ash/common/system/tray/tray_details_view.h"
#include "base/macros.h"
#include "chromeos/audio/audio_device.h"
#include "ui/gfx/font.h"

namespace gfx {
struct VectorIcon;
}

namespace views {
class View;
}

namespace ash {
class HoverHighlightView;

namespace tray {

class AudioDetailedView : public TrayDetailsView {
 public:
  explicit AudioDetailedView(SystemTrayItem* owner);

  ~AudioDetailedView() override;

  void Update();

 private:
  // Helper functions to add non-clickable header rows within the scrollable
  // list.
  void AddInputHeader();
  void AddOutputHeader();
  void AddScrollListInfoItem(int text_id, const gfx::VectorIcon& icon);

  HoverHighlightView* AddScrollListItem(const base::string16& text,
                                        bool highlight,
                                        bool checked);

  void CreateItems();

  void UpdateScrollableList();
  void UpdateAudioDevices();

  // TrayDetailsView:
  void HandleViewClicked(views::View* view) override;

  typedef std::map<views::View*, chromeos::AudioDevice> AudioDeviceMap;

  chromeos::AudioDeviceList output_devices_;
  chromeos::AudioDeviceList input_devices_;
  AudioDeviceMap device_map_;

  DISALLOW_COPY_AND_ASSIGN(AudioDetailedView);
};

}  // namespace tray
}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_CHROMEOS_AUDIO_AUDIO_DETAILED_VIEW_H_
