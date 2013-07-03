// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/importer/importer_data_types.h"

namespace importer {

SourceProfile::SourceProfile()
    : importer_type(TYPE_UNKNOWN),
      services_supported(0) {
}

SourceProfile::~SourceProfile() {
}

}  // namespace importer
