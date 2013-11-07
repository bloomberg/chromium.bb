// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/drop_data.h"

namespace content {

DropData::FileInfo::FileInfo() {
}

DropData::FileInfo::FileInfo(const base::string16& path,
                             const base::string16& display_name)
    : path(path),
      display_name(display_name) {
}

DropData::DropData()
    : referrer_policy(blink::WebReferrerPolicyDefault) {
}

DropData::~DropData() {
}

}  // namespace content
