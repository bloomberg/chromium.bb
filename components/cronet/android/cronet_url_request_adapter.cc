// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/android/cronet_url_request_adapter.h"

#include <limits>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "components/cronet/android/cronet_url_request_context_adapter.h"
#include "components/cronet/android/io_buffer_with_byte_buffer.h"
#include "components/cronet/android/metrics_util.h"
#include "components/cronet/android/url_request_error.h"
#include "jni/CronetUrlRequest_jni.h"
#include "net/base/load_flags.h"
#include "net/base/load_states.h"
#include "net/base/net_errors.h"
#include "net/base/request_priority.h"
#include "net/cert/cert_status_flags.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/proxy/proxy_server.h"
#include "net/quic/core/quic_packets.h"
#include "net/ssl/ssl_info.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/url_request_context.h"

using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;

namespace cronet {

namespace {

// Returns the string representation of the HostPortPair of the proxy server
// that was used to fetch the response.
std::string GetProxy(const net::HttpResponseInfo& info) {
  if (!info.proxy_server.is_valid() || info.proxy_server.is_direct())
    return net::HostPortPair().ToString();
  return info.proxy_server.host_port_pair().ToString();
}

}  // namespace

// Explicitly register static JNI functions.
bool CronetUrlRequestAdapterRegisterJni(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

static jlong CreateRequestAdapter(JNIEnv* env,
                                  const JavaParamRef<jobject>& jurl_request,
                                  jlong jurl_request_context_adapter,
                                  const JavaParamRef<jstring>& jurl_string,
                                  jint jpriority,
                                  jboolean jdisable_cache,
                                  jboolean jdisable_connection_migration,
                                  jboolean jenable_metrics) {
  CronetURLRequestContextAdapter* context_adapter =
      reinterpret_cast<CronetURLRequestContextAdapter*>(
          jurl_request_context_adapter);
  DCHECK(context_adapter);

  GURL url(base::android::ConvertJavaStringToUTF8(env, jurl_string));

  VLOG(1) << "New chromium network request_adapter: "
          << url.possibly_invalid_spec();

  CronetURLRequestAdapter* adapter = new CronetURLRequestAdapter(
      context_adapter, env, jurl_request, url,
      static_cast<net::RequestPriority>(jpriority), jdisable_cache,
      jdisable_connection_migration, jenable_metrics);

  return reinterpret_cast<jlong>(adapter);
}

CronetURLRequestAdapter::CronetURLRequestAdapter(
    CronetURLRequestContextAdapter* context,
    JNIEnv* env,
    jobject jurl_request,
    const GURL& url,
    net::RequestPriority priority,
    jboolean jdisable_cache,
    jboolean jdisable_connection_migration,
    jboolean jenable_metrics)
    : context_(context),
      initial_url_(url),
      initial_priority_(priority),
      initial_method_("GET"),
      load_flags_(net::LOAD_NORMAL),
      enable_metrics_(jenable_metrics == JNI_TRUE),
      metrics_reported_(false) {
  DCHECK(!context_->IsOnNetworkThread());
  owner_.Reset(env, jurl_request);
  if (jdisable_cache == JNI_TRUE)
    load_flags_ |= net::LOAD_DISABLE_CACHE;
  if (jdisable_connection_migration == JNI_TRUE)
    load_flags_ |= net::LOAD_DISABLE_CONNECTION_MIGRATION;
}

CronetURLRequestAdapter::~CronetURLRequestAdapter() {
  DCHECK(context_->IsOnNetworkThread());
}

jboolean CronetURLRequestAdapter::SetHttpMethod(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    const JavaParamRef<jstring>& jmethod) {
  DCHECK(!context_->IsOnNetworkThread());
  std::string method(base::android::ConvertJavaStringToUTF8(env, jmethod));
  // Http method is a token, just as header name.
  if (!net::HttpUtil::IsValidHeaderName(method))
    return JNI_FALSE;
  initial_method_ = method;
  return JNI_TRUE;
}

jboolean CronetURLRequestAdapter::AddRequestHeader(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    const JavaParamRef<jstring>& jname,
    const JavaParamRef<jstring>& jvalue) {
  DCHECK(!context_->IsOnNetworkThread());
  std::string name(base::android::ConvertJavaStringToUTF8(env, jname));
  std::string value(base::android::ConvertJavaStringToUTF8(env, jvalue));
  if (!net::HttpUtil::IsValidHeaderName(name) ||
      !net::HttpUtil::IsValidHeaderValue(value)) {
    return JNI_FALSE;
  }
  initial_request_headers_.SetHeader(name, value);
  return JNI_TRUE;
}

void CronetURLRequestAdapter::SetUpload(
    std::unique_ptr<net::UploadDataStream> upload) {
  DCHECK(!context_->IsOnNetworkThread());
  DCHECK(!upload_);
  upload_ = std::move(upload);
}

void CronetURLRequestAdapter::Start(JNIEnv* env,
                                    const JavaParamRef<jobject>& jcaller) {
  DCHECK(!context_->IsOnNetworkThread());
  context_->PostTaskToNetworkThread(
      FROM_HERE, base::Bind(&CronetURLRequestAdapter::StartOnNetworkThread,
                            base::Unretained(this)));
}

void CronetURLRequestAdapter::GetStatus(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    const JavaParamRef<jobject>& jstatus_listener) const {
  base::android::ScopedJavaGlobalRef<jobject> status_listener_ref;
  status_listener_ref.Reset(env, jstatus_listener);
  context_->PostTaskToNetworkThread(
      FROM_HERE, base::Bind(&CronetURLRequestAdapter::GetStatusOnNetworkThread,
                            base::Unretained(this), status_listener_ref));
}

void CronetURLRequestAdapter::FollowDeferredRedirect(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller) {
  context_->PostTaskToNetworkThread(
      FROM_HERE,
      base::Bind(
          &CronetURLRequestAdapter::FollowDeferredRedirectOnNetworkThread,
          base::Unretained(this)));
}

jboolean CronetURLRequestAdapter::ReadData(
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
      FROM_HERE, base::Bind(&CronetURLRequestAdapter::ReadDataOnNetworkThread,
                            base::Unretained(this),
                            read_buffer,
                            remaining_capacity));
  return JNI_TRUE;
}

void CronetURLRequestAdapter::Destroy(JNIEnv* env,
                                      const JavaParamRef<jobject>& jcaller,
                                      jboolean jsend_on_canceled) {
  // Destroy could be called from any thread, including network thread (if
  // posting task to executor throws an exception), but is posted, so |this|
  // is valid until calling task is complete. Destroy() is always called from
  // within a synchronized java block that guarantees no future posts to the
  // network thread with the adapter pointer.
  context_->PostTaskToNetworkThread(
      FROM_HERE, base::Bind(&CronetURLRequestAdapter::DestroyOnNetworkThread,
                            base::Unretained(this), jsend_on_canceled));
}

void CronetURLRequestAdapter::OnReceivedRedirect(
    net::URLRequest* request,
    const net::RedirectInfo& redirect_info,
    bool* defer_redirect) {
  DCHECK(context_->IsOnNetworkThread());
  JNIEnv* env = base::android::AttachCurrentThread();
  cronet::Java_CronetUrlRequest_onRedirectReceived(
      env, owner_, ConvertUTF8ToJavaString(env, redirect_info.new_url.spec()),
      redirect_info.status_code,
      ConvertUTF8ToJavaString(env,
                              request->response_headers()->GetStatusText()),
      GetResponseHeaders(env),
      request->response_info().was_cached ? JNI_TRUE : JNI_FALSE,
      ConvertUTF8ToJavaString(
          env, request->response_info().alpn_negotiated_protocol),
      ConvertUTF8ToJavaString(env, GetProxy(request->response_info())),
      request->GetTotalReceivedBytes());
  *defer_redirect = true;
}

void CronetURLRequestAdapter::OnCertificateRequested(
    net::URLRequest* request,
    net::SSLCertRequestInfo* cert_request_info) {
  DCHECK(context_->IsOnNetworkThread());
  // Cronet does not support client certificates.
  request->ContinueWithCertificate(nullptr, nullptr);
}

void CronetURLRequestAdapter::OnSSLCertificateError(
    net::URLRequest* request,
    const net::SSLInfo& ssl_info,
    bool fatal) {
  DCHECK(context_->IsOnNetworkThread());
  request->Cancel();
  int net_error = net::MapCertStatusToNetError(ssl_info.cert_status);
  ReportError(request, net_error);
}

void CronetURLRequestAdapter::OnResponseStarted(net::URLRequest* request,
                                                int net_error) {
  DCHECK_NE(net::ERR_IO_PENDING, net_error);
  DCHECK(context_->IsOnNetworkThread());

  if (net_error != net::OK) {
    ReportError(request, net_error);
    return;
  }
  JNIEnv* env = base::android::AttachCurrentThread();
  cronet::Java_CronetUrlRequest_onResponseStarted(
      env, owner_, request->GetResponseCode(),
      ConvertUTF8ToJavaString(env,
                              request->response_headers()->GetStatusText()),
      GetResponseHeaders(env),
      request->response_info().was_cached ? JNI_TRUE : JNI_FALSE,
      ConvertUTF8ToJavaString(
          env, request->response_info().alpn_negotiated_protocol),
      ConvertUTF8ToJavaString(env, GetProxy(request->response_info())));
}

void CronetURLRequestAdapter::OnReadCompleted(net::URLRequest* request,
                                              int bytes_read) {
  DCHECK(context_->IsOnNetworkThread());

  if (bytes_read < 0) {
    ReportError(request, bytes_read);
    return;
  }

  if (bytes_read == 0) {
    JNIEnv* env = base::android::AttachCurrentThread();
    MaybeReportMetrics(env);
    cronet::Java_CronetUrlRequest_onSucceeded(
        env, owner_, url_request_->GetTotalReceivedBytes());
  } else {
    JNIEnv* env = base::android::AttachCurrentThread();
    cronet::Java_CronetUrlRequest_onReadCompleted(
        env, owner_, read_buffer_->byte_buffer(), bytes_read,
        read_buffer_->initial_position(), read_buffer_->initial_limit(),
        request->GetTotalReceivedBytes());
    // Free the read buffer. This lets the Java ByteBuffer be freed, if the
    // embedder releases it, too.
    read_buffer_ = nullptr;
  }
}

void CronetURLRequestAdapter::StartOnNetworkThread() {
  DCHECK(context_->IsOnNetworkThread());
  VLOG(1) << "Starting chromium request: "
          << initial_url_.possibly_invalid_spec().c_str()
          << " priority: " << RequestPriorityToString(initial_priority_);
  url_request_ = context_->GetURLRequestContext()->CreateRequest(
      initial_url_, net::DEFAULT_PRIORITY, this);
  url_request_->SetLoadFlags(load_flags_ | context_->default_load_flags());
  url_request_->set_method(initial_method_);
  url_request_->SetExtraRequestHeaders(initial_request_headers_);
  url_request_->SetPriority(initial_priority_);
  if (upload_)
    url_request_->set_upload(std::move(upload_));
  url_request_->Start();
}

void CronetURLRequestAdapter::GetStatusOnNetworkThread(
    const base::android::ScopedJavaGlobalRef<jobject>& status_listener_ref)
    const {
  DCHECK(context_->IsOnNetworkThread());
  JNIEnv* env = base::android::AttachCurrentThread();
  int status = net::LOAD_STATE_IDLE;
  // |url_request_| is initialized in StartOnNetworkThread, and it is
  // never nulled. If it is null, it must be that StartOnNetworkThread
  // has not been called, pretend that we are in LOAD_STATE_IDLE.
  // See crbug.com/606872.
  if (url_request_)
    status = url_request_->GetLoadState().state;
  cronet::Java_CronetUrlRequest_onStatus(env, owner_, status_listener_ref,
                                         status);
}

base::android::ScopedJavaLocalRef<jobjectArray>
CronetURLRequestAdapter::GetResponseHeaders(JNIEnv* env) {
  DCHECK(context_->IsOnNetworkThread());

  std::vector<std::string> response_headers;
  const net::HttpResponseHeaders* headers = url_request_->response_headers();
  // Returns an empty array if |headers| is nullptr.
  if (headers != nullptr) {
    size_t iter = 0;
    std::string header_name;
    std::string header_value;
    while (headers->EnumerateHeaderLines(&iter, &header_name, &header_value)) {
      response_headers.push_back(header_name);
      response_headers.push_back(header_value);
    }
  }
  return base::android::ToJavaArrayOfStrings(env, response_headers);
}

void CronetURLRequestAdapter::FollowDeferredRedirectOnNetworkThread() {
  DCHECK(context_->IsOnNetworkThread());
  url_request_->FollowDeferredRedirect();
}

void CronetURLRequestAdapter::ReadDataOnNetworkThread(
    scoped_refptr<IOBufferWithByteBuffer> read_buffer,
    int buffer_size) {
  DCHECK(context_->IsOnNetworkThread());
  DCHECK(read_buffer);
  DCHECK(!read_buffer_);

  read_buffer_ = read_buffer;

  int result = url_request_->Read(read_buffer_.get(), buffer_size);
  // If IO is pending, wait for the URLRequest to call OnReadCompleted.
  if (result == net::ERR_IO_PENDING)
    return;

  OnReadCompleted(url_request_.get(), result);
}

void CronetURLRequestAdapter::DestroyOnNetworkThread(bool send_on_canceled) {
  DCHECK(context_->IsOnNetworkThread());
  JNIEnv* env = base::android::AttachCurrentThread();
  MaybeReportMetrics(env);
  if (send_on_canceled)
    cronet::Java_CronetUrlRequest_onCanceled(env, owner_);
  cronet::Java_CronetUrlRequest_onNativeAdapterDestroyed(env, owner_);
  delete this;
}

void CronetURLRequestAdapter::ReportError(net::URLRequest* request,
                                          int net_error) {
  DCHECK_NE(net::ERR_IO_PENDING, net_error);
  DCHECK_LT(net_error, 0);
  DCHECK_EQ(request, url_request_.get());

  net::NetErrorDetails net_error_details;
  url_request_->PopulateNetErrorDetails(&net_error_details);
  VLOG(1) << "Error " << net::ErrorToString(net_error)
          << " on chromium request: " << initial_url_.possibly_invalid_spec();
  JNIEnv* env = base::android::AttachCurrentThread();
  cronet::Java_CronetUrlRequest_onError(
      env, owner_, NetErrorToUrlRequestError(net_error), net_error,
      net_error_details.quic_connection_error,
      ConvertUTF8ToJavaString(env, net::ErrorToString(net_error)),
      request->GetTotalReceivedBytes());
}

void CronetURLRequestAdapter::MaybeReportMetrics(JNIEnv* env) {
  // If there was an exception while starting the CronetUrlRequest, there won't
  // be a native URLRequest. In this case, the caller gets the exception
  // immediately, and the onFailed callback isn't called, so don't report
  // metrics either.
  if (!enable_metrics_ || metrics_reported_ || !url_request_) {
    return;
  }
  metrics_reported_ = true;
  net::LoadTimingInfo metrics;
  url_request_->GetLoadTimingInfo(&metrics);
  base::Time start_time = metrics.request_start_time;
  base::TimeTicks start_ticks = metrics.request_start;
  Java_CronetUrlRequest_onMetricsCollected(
      env, owner_,
      metrics_util::ConvertTime(start_ticks, start_ticks, start_time),
      metrics_util::ConvertTime(metrics.connect_timing.dns_start, start_ticks,
                                start_time),
      metrics_util::ConvertTime(metrics.connect_timing.dns_end, start_ticks,
                                start_time),
      metrics_util::ConvertTime(metrics.connect_timing.connect_start,
                                start_ticks, start_time),
      metrics_util::ConvertTime(metrics.connect_timing.connect_end, start_ticks,
                                start_time),
      metrics_util::ConvertTime(metrics.connect_timing.ssl_start, start_ticks,
                                start_time),
      metrics_util::ConvertTime(metrics.connect_timing.ssl_end, start_ticks,
                                start_time),
      metrics_util::ConvertTime(metrics.send_start, start_ticks, start_time),
      metrics_util::ConvertTime(metrics.send_end, start_ticks, start_time),
      metrics_util::ConvertTime(metrics.push_start, start_ticks, start_time),
      metrics_util::ConvertTime(metrics.push_end, start_ticks, start_time),
      metrics_util::ConvertTime(metrics.receive_headers_end, start_ticks,
                                start_time),
      metrics_util::ConvertTime(base::TimeTicks::Now(), start_ticks,
                                start_time),
      metrics.socket_reused, url_request_->GetTotalSentBytes(),
      url_request_->GetTotalReceivedBytes());
}

}  // namespace cronet
