// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/bridge/wm_lookup_mus.h"

#include "ash/mus/bridge/wm_root_window_controller_mus.h"
#include "ash/mus/bridge/wm_shell_mus.h"
#include "ash/mus/bridge/wm_window_mus.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace mus {

WmLookupMus::WmLookupMus() {
  WmLookup::Set(this);
}

WmLookupMus::~WmLookupMus() {
  if (WmLookupMus::Get() == this)
    WmLookup::Set(nullptr);
}

WmRootWindowController* WmLookupMus::GetRootWindowControllerWithDisplayId(
    int64_t id) {
  return WmShellMus::Get()->GetRootWindowControllerWithDisplayId(id);
}

WmWindow* WmLookupMus::GetWindowForWidget(views::Widget* widget) {
  return WmWindowMus::Get(widget);
}

}  // namespace mus
}  // namespace ash
