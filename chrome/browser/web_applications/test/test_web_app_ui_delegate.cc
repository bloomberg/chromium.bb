// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/test_web_app_ui_delegate.h"

#include "base/stl_util.h"

namespace web_app {

TestWebAppUiDelegate::TestWebAppUiDelegate() = default;

TestWebAppUiDelegate::~TestWebAppUiDelegate() = default;

void TestWebAppUiDelegate::SetNumWindowsForApp(const AppId& app_id,
                                               size_t num_windows_for_app) {
  app_id_to_num_windows_map_[app_id] = num_windows_for_app;
}

size_t TestWebAppUiDelegate::GetNumWindowsForApp(const AppId& app_id) {
  DCHECK(base::ContainsKey(app_id_to_num_windows_map_, app_id));
  return app_id_to_num_windows_map_[app_id];
}

}  // namespace web_app
