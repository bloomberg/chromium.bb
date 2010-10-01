// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/resource_response.h"

ResourceResponseHead::ResourceResponseHead()
    : replace_extension_localization_templates(false) {
}

ResourceResponseHead::~ResourceResponseHead() {}

SyncLoadResult::SyncLoadResult() {}

SyncLoadResult::~SyncLoadResult() {}

ResourceResponse::ResourceResponse() {}

ResourceResponse::~ResourceResponse() {}
