// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_view/frame_utils.h"

#include "base/command_line.h"
#include "components/web_view/web_view_switches.h"
#include "mojo/shell/public/interfaces/shell.mojom.h"

namespace web_view {
namespace {

bool AlwaysCreateNewFrameTree() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      web_view::switches::kOOPIFAlwaysCreateNewFrameTree);
}

}  // namespace

bool AreAppIdsEqual(uint32_t app_id1, uint32_t app_id2) {
  return app_id1 == app_id2 &&
         app_id1 != mojo::shell::mojom::Shell::kInvalidApplicationID &&
         !AlwaysCreateNewFrameTree();
}

}  // namespace web_view
