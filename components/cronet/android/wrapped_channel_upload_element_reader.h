// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRONET_ANDROID_WRAPPED_CHANNEL_UPLOAD_ELEMENT_READER_H_
#define COMPONENTS_CRONET_ANDROID_WRAPPED_CHANNEL_UPLOAD_ELEMENT_READER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "components/cronet/android/url_request_adapter.h"
#include "net/base/completion_callback.h"
#include "net/base/upload_element_reader.h"

namespace net {
class IOBuffer;
}  // namespace net

namespace cronet {

// An UploadElementReader implementation for
// java.nio.channels.ReadableByteChannel.
// |channel_| should outlive this class because this class does not take the
// ownership of the data.
class WrappedChannelElementReader : public net::UploadElementReader {
 public:
  WrappedChannelElementReader(
      scoped_refptr<URLRequestAdapter::URLRequestAdapterDelegate> delegate,
      uint64 length);
  ~WrappedChannelElementReader() override;

  // UploadElementReader overrides:
  int Init(const net::CompletionCallback& callback) override;
  uint64 GetContentLength() const override;
  uint64 BytesRemaining() const override;
  bool IsInMemory() const override;
  int Read(net::IOBuffer* buf,
           int buf_length,
           const net::CompletionCallback& callback) override;

 private:
  const uint64 length_;
  uint64 offset_;
  scoped_refptr<URLRequestAdapter::URLRequestAdapterDelegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(WrappedChannelElementReader);
};

}  // namespace cronet

#endif  // COMPONENTS_CRONET_ANDROID_WRAPPED_CHANNEL_UPLOAD_ELEMENT_READER_H_
