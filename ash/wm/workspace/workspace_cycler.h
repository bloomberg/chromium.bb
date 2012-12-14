// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WORKSPACE_WORKSPACE_CYCLER_H_
#define ASH_WM_WORKSPACE_WORKSPACE_CYCLER_H_

#include "ash/ash_export.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/base/events/event_handler.h"

namespace ash {
namespace internal {

class WorkspaceManager;

// Class to enable quick workspace switching (scrubbing) via 3 finger vertical
// scroll.
class ASH_EXPORT WorkspaceCycler : public ui::EventHandler {
 public:
  explicit WorkspaceCycler(WorkspaceManager* workspace_manager);
  virtual ~WorkspaceCycler();

 private:
  // ui::EventHandler overrides:
  virtual void OnScrollEvent(ui::ScrollEvent* event) OVERRIDE;

  WorkspaceManager* workspace_manager_;

  // Whether the user is scrubbing through workspaces.
  bool scrubbing_;

  // The amount of scrolling which has occurred since the last time the
  // workspace was switched. If scrubbing has just begun, |scroll_x_| and
  // |scroll_y_| are set to 0.
  int scroll_x_;
  int scroll_y_;

  DISALLOW_COPY_AND_ASSIGN(WorkspaceCycler);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_WORKSPACE_WORKSPACE_CYCLER_H_
