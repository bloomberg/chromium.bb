// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/drive/gdata_wapi_parser.h"

namespace google_apis {

////////////////////////////////////////////////////////////////////////////////
// ResourceEntry implementation

ResourceEntry::ResourceEntry()
    : kind_(ENTRY_KIND_UNKNOWN),
      deleted_(false) {
}

ResourceEntry::~ResourceEntry() {
}

}  // namespace google_apis
