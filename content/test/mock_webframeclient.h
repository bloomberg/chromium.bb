// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_MOCK_WEBFRAMECLIENT_H_
#define CONTENT_TEST_MOCK_WEBFRAMECLIENT_H_

#include "third_party/WebKit/public/web/WebFrameClient.h"

namespace content {

class MockWebFrameClient : public blink::WebFrameClient {};

}  // namespace content

#endif  // CONTENT_TEST_MOCK_WEBFRAMECLIENT_H_
