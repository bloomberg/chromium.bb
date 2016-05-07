// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/wm/bridge/wm_lookup_mus.h"

#include "mash/wm/bridge/wm_globals_mus.h"
#include "mash/wm/bridge/wm_root_window_controller_mus.h"
#include "mash/wm/bridge/wm_window_mus.h"
#include "ui/views/widget/widget.h"

namespace mash {
namespace wm {

WmLookupMus::WmLookupMus() {}

WmLookupMus::~WmLookupMus() {}

ash::wm::WmRootWindowController*
WmLookupMus::GetRootWindowControllerWithDisplayId(int64_t id) {
  return WmGlobalsMus::Get()->GetRootWindowControllerWithDisplayId(id);
}

ash::wm::WmWindow* WmLookupMus::GetWindowForWidget(views::Widget* widget) {
  return WmWindowMus::Get(widget);
}

}  // namespace wm
}  // namespace mash
