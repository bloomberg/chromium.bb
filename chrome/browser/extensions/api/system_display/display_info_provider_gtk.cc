// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/system_display/display_info_provider.h"

#include <gdk/gdk.h>

#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"

namespace extensions {

void DisplayInfoProvider::SetInfo(
    const std::string& display_id,
    const api::system_display::DisplayProperties& info,
    const SetInfoCallback& callback) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, false, "Not implemented"));
}

void DisplayInfoProvider::UpdateDisplayUnitInfoForPlatform(
    const gfx::Display& display,
    extensions::api::system_display::DisplayUnitInfo* unit) {
  // TODO(Haojian): determine the DPI of the display
  GdkScreen* screen = gdk_screen_get_default();
  // The |id| in Display for GTK is the monitor index.
  gint monitor_num = static_cast<gint>(display.id());
  char* monitor_name = reinterpret_cast<char*>(gdk_screen_get_monitor_plug_name(
       screen, monitor_num));
  unit->name = std::string(monitor_name);
}

}  // namespace extensions
