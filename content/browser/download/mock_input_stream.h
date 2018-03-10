// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_MOCK_INPUT_STREAM_H_
#define CONTENT_BROWSER_DOWNLOAD_MOCK_INPUT_STREAM_H_

#include "components/download/public/common/input_stream.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {

class MockInputStream : public download::InputStream {
 public:
  MockInputStream();
  ~MockInputStream() override;

  // download::InputStream functions
  MOCK_METHOD0(IsEmpty, bool());
  MOCK_METHOD1(RegisterDataReadyCallback,
               void(const mojo::SimpleWatcher::ReadyCallback&));
  MOCK_METHOD0(ClearDataReadyCallback, void());
  MOCK_METHOD2(Read,
               download::InputStream::StreamState(scoped_refptr<net::IOBuffer>*,
                                                  size_t*));
  MOCK_METHOD0(GetCompletionStatus, download::DownloadInterruptReason());
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_MOCK_INPUT_STREAM_H_
