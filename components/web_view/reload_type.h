// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_VIEW_RELOAD_TYPE_H_
#define COMPONENTS_WEB_VIEW_RELOAD_TYPE_H_

namespace web_view {

enum class ReloadType {
  NO_RELOAD,                   // Normal load.
  RELOAD,                      // Normal (cache-validating) reload.
  RELOAD_IGNORING_CACHE,       // Reload bypassing the cache (shift-reload).
  RELOAD_ORIGINAL_REQUEST_URL  // Reload using the original request URL.
};

}  // namespace web_view

#endif  // COMPONENTS_WEB_VIEW_RELOAD_TYPE_H_
