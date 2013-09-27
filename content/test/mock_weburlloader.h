// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_MOCK_WEBURLLOADER_H_
#define CONTENT_TEST_MOCK_WEBURLLOADER_H_

#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/WebKit/public/platform/WebURLLoader.h"

namespace content {

class MockWebURLLoader : public WebKit::WebURLLoader {
 public:
  MockWebURLLoader();
  virtual ~MockWebURLLoader();

  MOCK_METHOD4(loadSynchronously, void(const WebKit::WebURLRequest& request,
                                       WebKit::WebURLResponse& response,
                                       WebKit::WebURLError& error,
                                       WebKit::WebData& data));
  MOCK_METHOD2(loadAsynchronously, void(const WebKit::WebURLRequest& request,
                                        WebKit::WebURLLoaderClient* client));
  MOCK_METHOD0(cancel, void());
  MOCK_METHOD1(setDefersLoading, void(bool value));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockWebURLLoader);
};

}  // namespace content

#endif  // CONTENT_TEST_MOCK_WEBURLLOADER_H_
