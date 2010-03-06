// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/status_icons/status_tray.h"

#include "base/stl_util-inl.h"
#include "chrome/browser/status_icons/status_icon.h"

StatusTray::StatusTray(StatusIconFactory* factory)
    : factory_(factory) {
}

StatusTray::~StatusTray() {
  // Walk any active status icons and delete them.
  STLDeleteContainerPairSecondPointers(status_icons_.begin(),
                                       status_icons_.end());
}

StatusIcon* StatusTray::GetStatusIcon(const string16& identifier) {
  StatusIconMap::const_iterator iter = status_icons_.find(identifier);
  if (iter != status_icons_.end())
    return iter->second;

  // No existing StatusIcon, create a new one.
  StatusIcon* icon = factory_->CreateIcon();
  if (icon)
    status_icons_[identifier] = icon;
  return icon;
}

void StatusTray::RemoveStatusIcon(const string16& identifier) {
  StatusIconMap::iterator iter = status_icons_.find(identifier);
  if (iter != status_icons_.end()) {
    // Free the StatusIcon from the map (can't put scoped_ptr in a map, so we
    // have to do it manually).
    delete iter->second;
    status_icons_.erase(iter);
  }
}
