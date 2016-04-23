// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRONET_ANDROID_CRONET_BIDIRECTIONAL_STREAM_ADAPTER_H_
#define COMPONENTS_CRONET_ANDROID_CRONET_BIDIRECTIONAL_STREAM_ADAPTER_H_

#include <jni.h>

#include <memory>
#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "net/http/bidirectional_stream.h"

namespace net {
struct BidirectionalStreamRequestInfo;
class SpdyHeaderBlock;
}  // namespace net

namespace cronet {

class CronetURLRequestContextAdapter;
class IOBufferWithByteBuffer;

// An adapter from Java BidirectionalStream object to net::BidirectionalStream.
// Created and configured from a Java thread. Start, ReadData, WriteData and
// Destroy can be called on any thread (including network thread), and post
// calls to corresponding {Start|ReadData|WriteData|Destroy}OnNetworkThread to
// the network thread. The object is always deleted on network thread. All
// callbacks into the Java BidirectionalStream are done on the network thread.
// Java BidirectionalStream is expected to initiate the next step like ReadData
// or Destroy. Public methods can be called on any thread.
class CronetBidirectionalStreamAdapter
    : public net::BidirectionalStream::Delegate {
 public:
  static bool RegisterJni(JNIEnv* env);

  CronetBidirectionalStreamAdapter(
      CronetURLRequestContextAdapter* context,
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jbidi_stream);
  ~CronetBidirectionalStreamAdapter() override;

  // Validates method and headers, initializes and starts the request. If
  // |jend_of_stream| is true, then stream is half-closed after sending header
  // frame and no data is expected to be written.
  // Returns 0 if request is valid and started successfully,
  // Returns -1 if |jmethod| is not valid HTTP method name.
  // Returns position of invalid header value in |jheaders| if header name is
  // not valid.
  jint Start(JNIEnv* env,
             const base::android::JavaParamRef<jobject>& jcaller,
             const base::android::JavaParamRef<jstring>& jurl,
             jint jpriority,
             const base::android::JavaParamRef<jstring>& jmethod,
             const base::android::JavaParamRef<jobjectArray>& jheaders,
             jboolean jend_of_stream);

  // Reads more data into |jbyte_buffer| starting at |jposition| and not
  // exceeding |jlimit|. Arguments are preserved to ensure that |jbyte_buffer|
  // is not modified by the application during read.
  jboolean ReadData(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& jcaller,
                    const base::android::JavaParamRef<jobject>& jbyte_buffer,
                    jint jposition,
                    jint jlimit);

  // Writes more data from |jbyte_buffer| starting at |jposition| and ending at
  // |jlimit|-1. Arguments are preserved to ensure that |jbyte_buffer|
  // is not modified by the application during write. The |jend_of_stream| is
  // passed to remote to indicate end of stream.
  jboolean WriteData(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& jcaller,
                     const base::android::JavaParamRef<jobject>& jbyte_buffer,
                     jint jposition,
                     jint jlimit,
                     jboolean jend_of_stream);

  // Releases all resources for the request and deletes the object itself.
  // |jsend_on_canceled| indicates if Java onCanceled callback should be
  // issued to indicate that no more callbacks will be issued.
  void Destroy(JNIEnv* env,
               const base::android::JavaParamRef<jobject>& jcaller,
               jboolean jsend_on_canceled);

 private:
  // net::BidirectionalStream::Delegate implementations:
  void OnHeadersSent() override;
  void OnHeadersReceived(const net::SpdyHeaderBlock& response_headers) override;
  void OnDataRead(int bytes_read) override;
  void OnDataSent() override;
  void OnTrailersReceived(const net::SpdyHeaderBlock& trailers) override;
  void OnFailed(int error) override;

  void StartOnNetworkThread(
      std::unique_ptr<net::BidirectionalStreamRequestInfo> request_info);
  void ReadDataOnNetworkThread(
      scoped_refptr<IOBufferWithByteBuffer> read_buffer,
      int buffer_size);
  void WriteDataOnNetworkThread(
      scoped_refptr<IOBufferWithByteBuffer> read_buffer,
      int buffer_size,
      bool end_of_stream);
  void DestroyOnNetworkThread(bool send_on_canceled);
  // Gets headers as a Java array.
  base::android::ScopedJavaLocalRef<jobjectArray> GetHeadersArray(
      JNIEnv* env,
      const net::SpdyHeaderBlock& header_block);

  CronetURLRequestContextAdapter* const context_;

  // Java object that owns this CronetBidirectionalStreamAdapter.
  base::android::ScopedJavaGlobalRef<jobject> owner_;

  scoped_refptr<IOBufferWithByteBuffer> read_buffer_;
  scoped_refptr<IOBufferWithByteBuffer> write_buffer_;
  std::unique_ptr<net::BidirectionalStream> bidi_stream_;

  // Whether BidirectionalStream::Delegate::OnFailed callback is invoked.
  bool stream_failed_;

  DISALLOW_COPY_AND_ASSIGN(CronetBidirectionalStreamAdapter);
};

}  // namespace cronet

#endif  // COMPONENTS_CRONET_ANDROID_CRONET_BIDIRECTIONAL_STREAM_ADAPTER_H_
