// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/mock_webassociatedurlloader.h"

#include "third_party/WebKit/public/platform/WebData.h"
#include "third_party/WebKit/public/platform/WebURLError.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/platform/WebURLResponse.h"

namespace content {

MockWebAssociatedURLLoader::MockWebAssociatedURLLoader() {}

MockWebAssociatedURLLoader::~MockWebAssociatedURLLoader() {}

}  // namespace content
