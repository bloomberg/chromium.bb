// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/app/web_main_parts.h"

namespace web {

int WebMainParts::PreCreateThreads() {
  return 0;
}

}  // namespace web
