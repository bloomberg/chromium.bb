// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PRESENTATION_PRESENTATION_CONNECTION_TEST_HELPER_H_
#define CONTENT_RENDERER_PRESENTATION_PRESENTATION_CONNECTION_TEST_HELPER_H_

#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/WebKit/public/platform/modules/presentation/WebPresentationConnection.h"

namespace content {

class TestPresentationConnection : public blink::WebPresentationConnection {
 public:
  TestPresentationConnection();
  ~TestPresentationConnection();

  MOCK_METHOD0(Init, void());
};

}  // namespace content

#endif  // CONTENT_RENDERER_PRESENTATION_PRESENTATION_CONNECTION_TEST_HELPER_H_
