// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cronet_bidirectional_stream_adapter.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "components/cronet/android/cronet_url_request_context_adapter.h"
#include "components/cronet/android/io_buffer_with_byte_buffer.h"
#include "components/cronet/android/url_request_error.h"
#include "jni/CronetBidirectionalStream_jni.h"
#include "net/base/net_errors.h"
#include "net/base/request_priority.h"
#include "net/http/bidirectional_stream_request_info.h"
#include "net/http/http_network_session.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_transaction_factory.h"
#include "net/http/http_util.h"
#include "net/spdy/spdy_header_block.h"
#include "net/ssl/ssl_info.h"
#include "net/url_request/http_user_agent_settings.h"
#include "net/url_request/url_request_context.h"
#include "url/gurl.h"

using base::android::ConvertUTF8ToJavaString;
using base::android::ConvertJavaStringToUTF8;

namespace cronet {

static jlong CreateBidirectionalStream(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbidi_stream,
    jlong jurl_request_context_adapter) {
  CronetURLRequestContextAdapter* context_adapter =
      reinterpret_cast<CronetURLRequestContextAdapter*>(
          jurl_request_context_adapter);
  DCHECK(context_adapter);

  CronetBidirectionalStreamAdapter* adapter =
      new CronetBidirectionalStreamAdapter(context_adapter, env, jbidi_stream);

  return reinterpret_cast<jlong>(adapter);
}

// static
bool CronetBidirectionalStreamAdapter::RegisterJni(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

CronetBidirectionalStreamAdapter::CronetBidirectionalStreamAdapter(
    CronetURLRequestContextAdapter* context,
    JNIEnv* env,
    const JavaParamRef<jobject>& jbidi_stream)
    : context_(context), owner_(env, jbidi_stream), stream_failed_(false) {}

CronetBidirectionalStreamAdapter::~CronetBidirectionalStreamAdapter() {
  DCHECK(context_->IsOnNetworkThread());
}

jint CronetBidirectionalStreamAdapter::Start(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    const JavaParamRef<jstring>& jurl,
    jint jpriority,
    const JavaParamRef<jstring>& jmethod,
    const JavaParamRef<jobjectArray>& jheaders,
    jboolean jend_of_stream) {
  // Prepare request info here to be able to return the error.
  std::unique_ptr<net::BidirectionalStreamRequestInfo> request_info(
      new net::BidirectionalStreamRequestInfo());
  request_info->url = GURL(ConvertJavaStringToUTF8(env, jurl));
  request_info->priority = static_cast<net::RequestPriority>(jpriority);
  // Http method is a token, just as header name.
  request_info->method = ConvertJavaStringToUTF8(env, jmethod);
  if (!net::HttpUtil::IsValidHeaderName(request_info->method))
    return -1;

  std::vector<std::string> headers;
  base::android::AppendJavaStringArrayToStringVector(env, jheaders, &headers);
  for (size_t i = 0; i < headers.size(); i += 2) {
    std::string name(headers[i]);
    std::string value(headers[i + 1]);
    if (!net::HttpUtil::IsValidHeaderName(name) ||
        !net::HttpUtil::IsValidHeaderValue(value)) {
      return i + 1;
    }
    request_info->extra_headers.SetHeader(name, value);
  }
  request_info->end_stream_on_headers = jend_of_stream;

  context_->PostTaskToNetworkThread(
      FROM_HERE,
      base::Bind(&CronetBidirectionalStreamAdapter::StartOnNetworkThread,
                 base::Unretained(this), base::Passed(&request_info)));
  return 0;
}

jboolean CronetBidirectionalStreamAdapter::ReadData(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    const JavaParamRef<jobject>& jbyte_buffer,
    jint jposition,
    jint jlimit) {
  DCHECK_LT(jposition, jlimit);

  void* data = env->GetDirectBufferAddress(jbyte_buffer);
  if (!data)
    return JNI_FALSE;

  scoped_refptr<IOBufferWithByteBuffer> read_buffer(
      new IOBufferWithByteBuffer(env, jbyte_buffer, data, jposition, jlimit));

  int remaining_capacity = jlimit - jposition;

  context_->PostTaskToNetworkThread(
      FROM_HERE,
      base::Bind(&CronetBidirectionalStreamAdapter::ReadDataOnNetworkThread,
                 base::Unretained(this), read_buffer, remaining_capacity));
  return JNI_TRUE;
}

jboolean CronetBidirectionalStreamAdapter::WriteData(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    const JavaParamRef<jobject>& jbyte_buffer,
    jint jposition,
    jint jlimit,
    jboolean jend_of_stream) {
  DCHECK_LE(jposition, jlimit);

  void* data = env->GetDirectBufferAddress(jbyte_buffer);
  if (!data)
    return JNI_FALSE;

  scoped_refptr<IOBufferWithByteBuffer> write_buffer(
      new IOBufferWithByteBuffer(env, jbyte_buffer, data, jposition, jlimit));

  int remaining_capacity = jlimit - jposition;

  context_->PostTaskToNetworkThread(
      FROM_HERE,
      base::Bind(&CronetBidirectionalStreamAdapter::WriteDataOnNetworkThread,
                 base::Unretained(this), write_buffer, remaining_capacity,
                 jend_of_stream));
  return JNI_TRUE;
}

void CronetBidirectionalStreamAdapter::Destroy(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    jboolean jsend_on_canceled) {
  // Destroy could be called from any thread, including network thread (if
  // posting task to executor throws an exception), but is posted, so |this|
  // is valid until calling task is complete. Destroy() is always called from
  // within a synchronized java block that guarantees no future posts to the
  // network thread with the adapter pointer.
  context_->PostTaskToNetworkThread(
      FROM_HERE,
      base::Bind(&CronetBidirectionalStreamAdapter::DestroyOnNetworkThread,
                 base::Unretained(this), jsend_on_canceled));
}

void CronetBidirectionalStreamAdapter::OnHeadersSent() {
  DCHECK(context_->IsOnNetworkThread());
  JNIEnv* env = base::android::AttachCurrentThread();
  cronet::Java_CronetBidirectionalStream_onRequestHeadersSent(env,
                                                              owner_.obj());
}

void CronetBidirectionalStreamAdapter::OnHeadersReceived(
    const net::SpdyHeaderBlock& response_headers) {
  DCHECK(context_->IsOnNetworkThread());
  JNIEnv* env = base::android::AttachCurrentThread();
  // Get http status code from response headers.
  jint http_status_code = 0;
  const auto http_status_header = response_headers.find(":status");
  if (http_status_header != response_headers.end())
    base::StringToInt(http_status_header->second, &http_status_code);

  std::string protocol;
  switch (bidi_stream_->GetProtocol()) {
    case net::kProtoHTTP2:
      protocol = "h2";
      break;
    case net::kProtoQUIC1SPDY3:
      protocol = "quic/1+spdy/3";
      break;
    default:
      break;
  }

  cronet::Java_CronetBidirectionalStream_onResponseHeadersReceived(
      env, owner_.obj(), http_status_code,
      ConvertUTF8ToJavaString(env, protocol).obj(),
      GetHeadersArray(env, response_headers).obj(),
      bidi_stream_->GetTotalReceivedBytes());
}

void CronetBidirectionalStreamAdapter::OnDataRead(int bytes_read) {
  DCHECK(context_->IsOnNetworkThread());
  JNIEnv* env = base::android::AttachCurrentThread();
  cronet::Java_CronetBidirectionalStream_onReadCompleted(
      env, owner_.obj(), read_buffer_->byte_buffer(), bytes_read,
      read_buffer_->initial_position(), read_buffer_->initial_limit(),
      bidi_stream_->GetTotalReceivedBytes());
  // Free the read buffer. This lets the Java ByteBuffer be freed, if the
  // embedder releases it, too.
  read_buffer_ = nullptr;
}

void CronetBidirectionalStreamAdapter::OnDataSent() {
  DCHECK(context_->IsOnNetworkThread());
  JNIEnv* env = base::android::AttachCurrentThread();
  cronet::Java_CronetBidirectionalStream_onWriteCompleted(
      env, owner_.obj(), write_buffer_->byte_buffer(),
      write_buffer_->initial_position(), write_buffer_->initial_limit());
  // Free the write buffer. This lets the Java ByteBuffer be freed, if the
  // embedder releases it, too.
  write_buffer_ = nullptr;
}

void CronetBidirectionalStreamAdapter::OnTrailersReceived(
    const net::SpdyHeaderBlock& response_trailers) {
  DCHECK(context_->IsOnNetworkThread());
  JNIEnv* env = base::android::AttachCurrentThread();
  cronet::Java_CronetBidirectionalStream_onResponseTrailersReceived(
      env, owner_.obj(), GetHeadersArray(env, response_trailers).obj());
}

void CronetBidirectionalStreamAdapter::OnFailed(int error) {
  DCHECK(context_->IsOnNetworkThread());
  stream_failed_ = true;
  JNIEnv* env = base::android::AttachCurrentThread();
  cronet::Java_CronetBidirectionalStream_onError(
      env, owner_.obj(), NetErrorToUrlRequestError(error), error,
      ConvertUTF8ToJavaString(env, net::ErrorToString(error)).obj(),
      bidi_stream_->GetTotalReceivedBytes());
}

void CronetBidirectionalStreamAdapter::StartOnNetworkThread(
    std::unique_ptr<net::BidirectionalStreamRequestInfo> request_info) {
  DCHECK(context_->IsOnNetworkThread());
  DCHECK(!bidi_stream_);
  request_info->extra_headers.SetHeaderIfMissing(
      net::HttpRequestHeaders::kUserAgent, context_->GetURLRequestContext()
                                               ->http_user_agent_settings()
                                               ->GetUserAgent());
  bidi_stream_.reset(new net::BidirectionalStream(
      std::move(request_info), context_->GetURLRequestContext()
                                   ->http_transaction_factory()
                                   ->GetSession(),
      this));
}

void CronetBidirectionalStreamAdapter::ReadDataOnNetworkThread(
    scoped_refptr<IOBufferWithByteBuffer> read_buffer,
    int buffer_size) {
  DCHECK(context_->IsOnNetworkThread());
  DCHECK(read_buffer);
  DCHECK(!read_buffer_);

  read_buffer_ = read_buffer;

  int bytes_read = bidi_stream_->ReadData(read_buffer_.get(), buffer_size);
  // If IO is pending, wait for the BidirectionalStream to call OnDataRead.
  if (bytes_read == net::ERR_IO_PENDING)
    return;

  if (bytes_read < 0) {
    OnFailed(bytes_read);
    return;
  }
  OnDataRead(bytes_read);
}

void CronetBidirectionalStreamAdapter::WriteDataOnNetworkThread(
    scoped_refptr<IOBufferWithByteBuffer> write_buffer,
    int buffer_size,
    bool end_of_stream) {
  DCHECK(context_->IsOnNetworkThread());
  DCHECK(write_buffer);
  DCHECK(!write_buffer_);

  if (stream_failed_) {
    // If stream failed between the time when WriteData is invoked and
    // WriteDataOnNetworkThread is executed, do not call into |bidi_stream_|
    // since the underlying stream might have been destroyed. Do not invoke
    // Java callback either, since onError is posted when |stream_failed_| is
    // set to true.
    return;
  }
  write_buffer_ = write_buffer;
  bidi_stream_->SendData(write_buffer_.get(), buffer_size, end_of_stream);
}

void CronetBidirectionalStreamAdapter::DestroyOnNetworkThread(
    bool send_on_canceled) {
  DCHECK(context_->IsOnNetworkThread());
  if (send_on_canceled) {
    JNIEnv* env = base::android::AttachCurrentThread();
    cronet::Java_CronetBidirectionalStream_onCanceled(env, owner_.obj());
  }
  delete this;
}

base::android::ScopedJavaLocalRef<jobjectArray>
CronetBidirectionalStreamAdapter::GetHeadersArray(
    JNIEnv* env,
    const net::SpdyHeaderBlock& header_block) {
  DCHECK(context_->IsOnNetworkThread());

  std::vector<std::string> headers;
  for (const auto& header : header_block) {
    headers.push_back(header.first.as_string());
    headers.push_back(header.second.as_string());
  }
  return base::android::ToJavaArrayOfStrings(env, headers);
}

}  // namespace cronet
