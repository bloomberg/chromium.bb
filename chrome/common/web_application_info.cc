// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/web_application_info.h"

WebApplicationInfo::WebApplicationInfo() {
  is_bookmark_app = false;
  is_offline_enabled = false;
}

WebApplicationInfo::~WebApplicationInfo() {
}
