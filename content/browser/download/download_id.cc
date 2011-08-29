// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/download_id.h"

std::ostream& operator<<(std::ostream& out, const DownloadId& global_id) {
  return out << global_id.manager_ << ":" << global_id.local();
}
