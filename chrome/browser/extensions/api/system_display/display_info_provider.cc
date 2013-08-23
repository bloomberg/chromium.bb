// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/system_display/display_info_provider.h"

#include "base/strings/string_number_conversions.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"

namespace extensions {

namespace {

// Converts Rotation enum to integer.
int RotationToDegrees(gfx::Display::Rotation rotation) {
  switch (rotation) {
    case gfx::Display::ROTATE_0:
      return 0;
    case gfx::Display::ROTATE_90:
      return 90;
    case gfx::Display::ROTATE_180:
      return 180;
    case gfx::Display::ROTATE_270:
      return 270;
  }
  return 0;
}

// Creates new DisplayUnitInfo struct for |display| and adds it at the end of
// |list|.
extensions::api::system_display::DisplayUnitInfo*
CreateDisplayUnitInfo(const gfx::Display& display,
               int64 primary_display_id) {
  extensions::api::system_display::DisplayUnitInfo* unit =
      new extensions::api::system_display::DisplayUnitInfo();
  const gfx::Rect& bounds = display.bounds();
  const gfx::Rect& work_area = display.work_area();
  unit->id = base::Int64ToString(display.id());
  unit->is_primary = (display.id() == primary_display_id);
  unit->is_internal = display.IsInternal();
  unit->is_enabled = true;
  unit->rotation = RotationToDegrees(display.rotation());
  unit->bounds.left = bounds.x();
  unit->bounds.top = bounds.y();
  unit->bounds.width = bounds.width();
  unit->bounds.height = bounds.height();
  unit->work_area.left = work_area.x();
  unit->work_area.top = work_area.y();
  unit->work_area.width = work_area.width();
  unit->work_area.height = work_area.height();
  return unit;
}

}  // namespace

DisplayInfoProvider::DisplayInfoProvider() {
}

DisplayInfoProvider::~DisplayInfoProvider() {
}

// Static member intialization.
base::LazyInstance<scoped_refptr<DisplayInfoProvider > >
    DisplayInfoProvider::provider_ = LAZY_INSTANCE_INITIALIZER;

const DisplayInfo& DisplayInfoProvider::display_info() const {
  return info_;
}

void DisplayInfoProvider::InitializeForTesting(
    scoped_refptr<DisplayInfoProvider> provider) {
  DCHECK(provider.get() != NULL);
  provider_.Get() = provider;
}

void DisplayInfoProvider::RequestInfo(const RequestInfoCallback& callback) {
  bool success = QueryInfo();

  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, success));
}

#if !defined(OS_WIN)
bool DisplayInfoProvider::QueryInfo() {
  info_.clear();

  // TODO(scottmg): Native is wrong http://crbug.com/133312
  gfx::Screen* screen = gfx::Screen::GetNativeScreen();
  int64 primary_id = screen->GetPrimaryDisplay().id();
  std::vector<gfx::Display> displays = screen->GetAllDisplays();
  for (int i = 0; i < screen->GetNumDisplays(); ++i) {
    linked_ptr<extensions::api::system_display::DisplayUnitInfo> unit(
        CreateDisplayUnitInfo(displays[i], primary_id));
    UpdateDisplayUnitInfoForPlatform(displays[i], unit.get());
    info_.push_back(unit);
  }
  return true;
}
#endif

// static
DisplayInfoProvider* DisplayInfoProvider::Get() {
  if (provider_.Get().get() == NULL)
    provider_.Get() = new DisplayInfoProvider();
  return provider_.Get();
}

}  // namespace extensions
