// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/api/webdata/web_data_results.h"

// TODO(caitkp): This should not live in chrome/browser/api.

WDTypedResult::WDTypedResult(WDResultType type)
    : type_(type),
    callback_(DestroyCallback()) {
}

WDTypedResult::WDTypedResult(WDResultType type, const DestroyCallback& callback)
    : type_(type),
    callback_(callback) {
}

WDTypedResult::~WDTypedResult() {}
