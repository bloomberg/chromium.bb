// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_VOICE_INTERACTION_HIGHLIGHTER_CONTROLLER_CLIENT_H_
#define CHROME_BROWSER_CHROMEOS_ARC_VOICE_INTERACTION_HIGHLIGHTER_CONTROLLER_CLIENT_H_

#include <memory>

#include "ash/highlighter/highlighter_selection_observer.h"
#include "base/macros.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace base {
class Timer;
}  // namespace base

namespace arc {

class ArcVoiceInteractionFrameworkService;

// TODO(kaznacheev) Convert observer to a mojo connection (crbug/769996)
class HighlighterControllerClient : public ash::HighlighterSelectionObserver {
 public:
  explicit HighlighterControllerClient(
      ArcVoiceInteractionFrameworkService* service);
  ~HighlighterControllerClient() override;

 private:
  // ash::HighlighterSelectionObserver:
  void HandleSelection(const gfx::Rect& rect) override;
  void HandleFailedSelection() override;
  void HandleEnabledStateChange(bool enabled) override;

  void ReportSelection(const gfx::Rect& rect);

  bool start_session_pending() const { return delay_timer_.get(); }

  ArcVoiceInteractionFrameworkService* service_;

  std::unique_ptr<base::Timer> delay_timer_;

  DISALLOW_COPY_AND_ASSIGN(HighlighterControllerClient);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_VOICE_INTERACTION_HIGHLIGHTER_CONTROLLER_CLIENT_H_
