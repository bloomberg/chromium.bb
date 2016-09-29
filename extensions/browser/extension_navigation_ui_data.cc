// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_navigation_ui_data.h"

#include "content/public/browser/navigation_handle.h"

namespace extensions {

ExtensionNavigationUIData::ExtensionNavigationUIData() {}

ExtensionNavigationUIData::ExtensionNavigationUIData(
    content::NavigationHandle* navigation_handle,
    int tab_id,
    int window_id) {
  // TODO(clamy): See if it would be possible to have just one source for the
  // FrameData that works both for navigations and subresources loads.
  frame_data_.frame_id = ExtensionApiFrameIdMap::GetFrameId(navigation_handle);
  frame_data_.parent_frame_id =
      ExtensionApiFrameIdMap::GetParentFrameId(navigation_handle);
  frame_data_.tab_id = tab_id;
  frame_data_.window_id = window_id;
}

std::unique_ptr<ExtensionNavigationUIData> ExtensionNavigationUIData::DeepCopy()
    const {
  std::unique_ptr<ExtensionNavigationUIData> copy(
      new ExtensionNavigationUIData());
  copy->set_frame_data(frame_data_);
  return copy;
}

}  // namespace extensions
