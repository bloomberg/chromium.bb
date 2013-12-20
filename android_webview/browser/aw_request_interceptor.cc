// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_request_interceptor.h"

#include "android_webview/browser/aw_contents_io_thread_client.h"
#include "android_webview/browser/intercepted_request_data.h"
#include "base/android/jni_string.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/resource_request_info.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_job.h"

using content::BrowserThread;
using content::RenderViewHost;
using content::ResourceRequestInfo;

namespace android_webview {

namespace {

const void* kRequestAlreadyQueriedDataKey = &kRequestAlreadyQueriedDataKey;

} // namespace

AwRequestInterceptor::AwRequestInterceptor() {
}

AwRequestInterceptor::~AwRequestInterceptor() {
}

scoped_ptr<InterceptedRequestData>
AwRequestInterceptor::QueryForInterceptedRequestData(
    const GURL& location,
    net::URLRequest* request) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  int render_process_id, render_frame_id;
  if (!ResourceRequestInfo::GetRenderFrameForRequest(
      request, &render_process_id, &render_frame_id))
    return scoped_ptr<InterceptedRequestData>();

  scoped_ptr<AwContentsIoThreadClient> io_thread_client =
      AwContentsIoThreadClient::FromID(render_process_id, render_frame_id);

  if (!io_thread_client.get())
    return scoped_ptr<InterceptedRequestData>();

  return io_thread_client->ShouldInterceptRequest(location, request).Pass();
}

net::URLRequestJob* AwRequestInterceptor::MaybeCreateJob(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // See if we've already found out the intercepted_request_data for this
  // request.
  // This is done not only for efficiency reasons, but also for correctness
  // as it is possible for the Interceptor chain to be invoked more than once
  // in which case we don't want to query the embedder multiple times.
  // Note: The Interceptor chain is not invoked more than once if we create a
  // URLRequestJob in this method, so this is only caching negative hits.
  if (request->GetUserData(kRequestAlreadyQueriedDataKey))
    return NULL;
  request->SetUserData(kRequestAlreadyQueriedDataKey,
                       new base::SupportsUserData::Data());

  scoped_ptr<InterceptedRequestData> intercepted_request_data =
      QueryForInterceptedRequestData(request->url(), request);

  if (!intercepted_request_data)
    return NULL;

  // The newly created job will own the InterceptedRequestData.
  return InterceptedRequestData::CreateJobFor(
      intercepted_request_data.Pass(), request, network_delegate);
}

} // namespace android_webview
