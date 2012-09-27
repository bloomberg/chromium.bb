// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/native/android_stream_reader_url_request_job.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "net/base/io_buffer.h"
#include "net/base/mime_util.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/http/http_util.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_error_job.h"
#include "net/url_request/url_request_file_job.h"
#include "net/url_request/url_request_job_manager.h"
// Disable "Warnings treated as errors" for input_stream_jni as it's a Java
// system class and we have to generate C++ hooks for all methods in the class
// even if they're unused.
#pragma GCC diagnostic ignored "-Wunused-function"
#include "jni/InputStream_jni.h"

using base::android::AttachCurrentThread;
using base::android::ClearException;
using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using JNI_InputStream::Java_InputStream_available;
using JNI_InputStream::Java_InputStream_skip;
using JNI_InputStream::Java_InputStream_read;


namespace {

// Maximum number of bytes to be read in a single read.
const int kBufferSize = 4096;

} // namespace

bool RegisterAndroidStreamReaderUrlRequestJob(JNIEnv* env) {
  return JNI_InputStream::RegisterNativesImpl(env);
}

AndroidStreamReaderURLRequestJob::AndroidStreamReaderURLRequestJob(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate,
    scoped_ptr<Delegate> delegate)
    : URLRequestJob(request, network_delegate),
      delegate_(delegate.Pass()) {
  DCHECK(delegate_.get());
}

AndroidStreamReaderURLRequestJob::~AndroidStreamReaderURLRequestJob() {
}

void AndroidStreamReaderURLRequestJob::Start() {
  JNIEnv* env = AttachCurrentThread();
  DCHECK(env);

  stream_.Reset(env, delegate_->OpenInputStream(env, request()).obj());
  if (!stream_.obj()) {
    NotifyDone(net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                     net::ERR_FAILED));
    return;
  }

  if (VerifyRequestedRange(env) && SkipToRequestedRange(env))
    NotifyHeadersComplete();
}

bool AndroidStreamReaderURLRequestJob::VerifyRequestedRange(JNIEnv* env) {
  int32_t size = Java_InputStream_available(env, stream_.obj());
  if (ClearException(env)) {
    NotifyDone(net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                     net::ERR_FAILED));
    return false;
  }

  if (size <= 0)
    return true;

  // Check that the requested range was valid.
  if (!byte_range_.ComputeBounds(size)) {
    NotifyDone(net::URLRequestStatus(net::URLRequestStatus::FAILED,
               net::ERR_REQUEST_RANGE_NOT_SATISFIABLE));
    return false;
  }

  size = byte_range_.last_byte_position() -
         byte_range_.first_byte_position() + 1;
  DCHECK_GE(size, 0);
  set_expected_content_size(size);

  return true;
}

bool AndroidStreamReaderURLRequestJob::SkipToRequestedRange(JNIEnv* env) {
  // Skip to the start of the requested data. This has to be done in a loop
  // because the underlying InputStream is not guaranteed to skip the requested
  // number of bytes.
  if (byte_range_.first_byte_position() != 0) {
    int64_t skipped, bytes_to_skip = byte_range_.first_byte_position();
    do {
      skipped = Java_InputStream_skip(env, stream_.obj(), bytes_to_skip);
      if (ClearException(env)) {
        NotifyDone(net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                         net::ERR_FAILED));
        return false;
      }
      if (skipped <= 0) {
        NotifyDone(
            net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                  net::ERR_REQUEST_RANGE_NOT_SATISFIABLE));
        return false;
      }
    } while ((bytes_to_skip -= skipped) > 0);
  }
  return true;
}

bool AndroidStreamReaderURLRequestJob::ReadRawData(net::IOBuffer* dest,
                                                   int dest_size,
                                                   int *bytes_read) {
  DCHECK_NE(dest_size, 0);
  DCHECK(bytes_read);
  DCHECK(stream_.obj());

  JNIEnv* env = AttachCurrentThread();
  DCHECK(env);

  if (!buffer_.obj()) {
    // Allocate transfer buffer.
    buffer_.Reset(env, env->NewByteArray(kBufferSize));
    if (ClearException(env)) {
      NotifyDone(net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                       net::ERR_FAILED));
      return false;
    }
  }

  jbyteArray buffer = buffer_.obj();
  *bytes_read = 0;
  if (!dest_size)
    return true;

  // Read data in multiples of the buffer size.
  while (dest_size > 0) {
    int read_size = std::min(dest_size, kBufferSize);
    // TODO(skyostil): Make this non-blocking
    int32_t byte_count =
        Java_InputStream_read(env, stream_.obj(), buffer, 0, read_size);
    if (byte_count <= 0) {
      // net::URLRequestJob will call NotifyDone for us after the end of the
      // file is reached.
      break;
    }

    if (ClearException(env)) {
      NotifyDone(net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                       net::ERR_FAILED));
      return false;
    }

#ifndef NDEBUG
    int32_t buffer_length = env->GetArrayLength(buffer);
    DCHECK_GE(read_size, byte_count);
    DCHECK_GE(buffer_length, byte_count);
#endif // NDEBUG

    // Copy the data over to the provided C++ side buffer.
    DCHECK_GE(dest_size, byte_count);
    env->GetByteArrayRegion(buffer, 0, byte_count,
        reinterpret_cast<jbyte*>(dest->data() + *bytes_read));

    if (ClearException(env)) {
      NotifyDone(net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                       net::ERR_FAILED));
      return false;
    }

    *bytes_read += byte_count;
    dest_size -= byte_count;
  }
  return true;
}

bool AndroidStreamReaderURLRequestJob::GetMimeType(
    std::string* mime_type) const {
  JNIEnv* env = AttachCurrentThread();
  DCHECK(env);

  return delegate_->GetMimeType(env,
                                request(),
                                stream_.obj(),
                                mime_type);
}

bool AndroidStreamReaderURLRequestJob::GetCharset(
    std::string* charset) {
  JNIEnv* env = AttachCurrentThread();
  DCHECK(env);

  return delegate_->GetCharset(env,
                               request(),
                               stream_.obj(),
                               charset);
}

void AndroidStreamReaderURLRequestJob::SetExtraRequestHeaders(
    const net::HttpRequestHeaders& headers) {
  std::string range_header;
  if (headers.GetHeader(net::HttpRequestHeaders::kRange, &range_header)) {
    // We only care about "Range" header here.
    std::vector<net::HttpByteRange> ranges;
    if (net::HttpUtil::ParseRangeHeader(range_header, &ranges)) {
      if (ranges.size() == 1) {
        byte_range_ = ranges[0];
      } else {
        // We don't support multiple range requests in one single URL request,
        // because we need to do multipart encoding here.
        NotifyDone(net::URLRequestStatus(
            net::URLRequestStatus::FAILED,
            net::ERR_REQUEST_RANGE_NOT_SATISFIABLE));
      }
    }
  }
}
