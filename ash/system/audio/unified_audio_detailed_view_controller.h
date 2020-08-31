// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_AUDIO_UNIFIED_AUDIO_DETAILED_VIEW_CONTROLLER_H_
#define ASH_SYSTEM_AUDIO_UNIFIED_AUDIO_DETAILED_VIEW_CONTROLLER_H_

#include <memory>

#include "ash/system/unified/detailed_view_controller.h"
#include "base/macros.h"
#include "chromeos/audio/cras_audio_handler.h"

namespace ash {

namespace tray {
class AudioDetailedView;
}  // namespace tray

class DetailedViewDelegate;
class UnifiedSystemTrayController;

// Controller of Audio detailed view in UnifiedSystemTray.
class UnifiedAudioDetailedViewController
    : public DetailedViewController,
      public chromeos::CrasAudioHandler::AudioObserver {
 public:
  explicit UnifiedAudioDetailedViewController(
      UnifiedSystemTrayController* tray_controller);
  ~UnifiedAudioDetailedViewController() override;

  // DetailedViewControllerBase:
  views::View* CreateView() override;
  base::string16 GetAccessibleName() const override;

  // CrasAudioHandler::AudioObserver.
  void OnAudioNodesChanged() override;
  void OnActiveOutputNodeChanged() override;
  void OnActiveInputNodeChanged() override;

 private:
  const std::unique_ptr<DetailedViewDelegate> detailed_view_delegate_;

  tray::AudioDetailedView* view_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(UnifiedAudioDetailedViewController);
};

}  // namespace ash

#endif  // ASH_SYSTEM_AUDIO_UNIFIED_AUDIO_DETAILED_VIEW_CONTROLLER_H_
