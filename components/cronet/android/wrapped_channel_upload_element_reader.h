// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRONET_ANDROID_WRAPPED_CHANNEL_UPLOAD_ELEMENT_READER_H_
#define COMPONENTS_CRONET_ANDROID_WRAPPED_CHANNEL_UPLOAD_ELEMENT_READER_H_

#include <jni.h>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "net/base/completion_callback.h"
#include "net/base/upload_element_reader.h"

namespace net {
class IOBuffer;
}  // namespace net

namespace cronet {

bool WrappedChannelRegisterJni(JNIEnv* env);

// An UploadElementReader implementation for
// java.nio.channels.ReadableByteChannel.
// |channel_| should outlive this class because this class does not take the
// ownership of the data.
class WrappedChannelElementReader : public net::UploadElementReader {
 public:
  WrappedChannelElementReader(JNIEnv* env, jobject channel, uint64 length);
  virtual ~WrappedChannelElementReader();

  // UploadElementReader overrides:
  virtual int Init(const net::CompletionCallback& callback) OVERRIDE;
  virtual uint64 GetContentLength() const OVERRIDE;
  virtual uint64 BytesRemaining() const OVERRIDE;
  virtual bool IsInMemory() const OVERRIDE;
  virtual int Read(net::IOBuffer* buf,
                   int buf_length,
                   const net::CompletionCallback& callback) OVERRIDE;

 private:
  const uint64 length_;
  uint64 offset_;
  jobject channel_;

  DISALLOW_COPY_AND_ASSIGN(WrappedChannelElementReader);
};

}  // namespace cronet

#endif  // COMPONENTS_CRONET_ANDROID_WRAPPED_CHANNEL_UPLOAD_ELEMENT_READER_H_
