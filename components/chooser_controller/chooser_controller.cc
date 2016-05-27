// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/chooser_controller/chooser_controller.h"

#include "content/public/browser/render_frame_host.h"

ChooserController::ChooserController(content::RenderFrameHost* owner)
    : owning_frame_(owner) {}

ChooserController::~ChooserController() {}

url::Origin ChooserController::GetOrigin() const {
  return const_cast<content::RenderFrameHost*>(owning_frame_)
      ->GetLastCommittedOrigin();
}
