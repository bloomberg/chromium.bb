// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_NAVIGATION_UI_DATA_H_
#define EXTENSIONS_BROWSER_EXTENSION_NAVIGATION_UI_DATA_H_

#include <memory>

#include "base/macros.h"
#include "extensions/browser/extension_api_frame_id_map.h"

namespace content {
class NavigationHandle;
}

namespace extensions {

// PlzNavigate: initialized on the UI thread for all navigations. A copy is used
// on the IO thread by the WebRequest API to access to the FrameData.
class ExtensionNavigationUIData {
 public:
  ExtensionNavigationUIData();
  ExtensionNavigationUIData(content::NavigationHandle* navigation_handle,
                            int tab_id,
                            int window_id);

  std::unique_ptr<ExtensionNavigationUIData> DeepCopy() const;

  const ExtensionApiFrameIdMap::FrameData& frame_data() const {
    return frame_data_;
  }

 private:
  void set_frame_data(const ExtensionApiFrameIdMap::FrameData& frame_data) {
    frame_data_ = frame_data;
  }

  ExtensionApiFrameIdMap::FrameData frame_data_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionNavigationUIData);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_NAVIGATION_UI_DATA_H_
