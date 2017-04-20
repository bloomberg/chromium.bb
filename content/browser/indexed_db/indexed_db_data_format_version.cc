// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_data_format_version.h"

#include "third_party/WebKit/public/web/WebSerializedScriptValueVersion.h"

namespace content {

// static
IndexedDBDataFormatVersion IndexedDBDataFormatVersion::current_(
    0,
    blink::kSerializedScriptValueVersion);

}  // namespace content
