// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/native/aw_contents_io_thread_client_impl.h"

#include <map>
#include <utility>

#include "android_webview/common/devtools_instrumentation.h"
#include "android_webview/native/aw_contents_background_thread_client.h"
#include "android_webview/native/aw_web_resource_response_impl.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/jni_weak_ref.h"
#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "jni/AwContentsIoThreadClient_jni.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"
#include "url/gurl.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaArrayOfStrings;
using base::LazyInstance;
using content::BrowserThread;
using content::RenderFrameHost;
using content::ResourceType;
using content::WebContents;
using std::map;
using std::pair;
using std::string;
using std::vector;

namespace android_webview {

namespace {

struct IoThreadClientData {
  bool pending_association;
  JavaObjectWeakGlobalRef io_thread_client;

  IoThreadClientData();
};

IoThreadClientData::IoThreadClientData() : pending_association(false) {}

typedef map<pair<int, int>, IoThreadClientData>
    RenderFrameHostToIoThreadClientType;

static pair<int, int> GetRenderFrameHostIdPair(RenderFrameHost* rfh) {
  return pair<int, int>(rfh->GetProcess()->GetID(), rfh->GetRoutingID());
}

// RfhToIoThreadClientMap -----------------------------------------------------
class RfhToIoThreadClientMap {
 public:
  static RfhToIoThreadClientMap* GetInstance();
  void Set(pair<int, int> rfh_id, const IoThreadClientData& client);
  bool Get(pair<int, int> rfh_id, IoThreadClientData* client);
  void Erase(pair<int, int> rfh_id);

 private:
  static LazyInstance<RfhToIoThreadClientMap> g_instance_;
  base::Lock map_lock_;
  RenderFrameHostToIoThreadClientType rfh_to_io_thread_client_;
};

// static
LazyInstance<RfhToIoThreadClientMap> RfhToIoThreadClientMap::g_instance_ =
    LAZY_INSTANCE_INITIALIZER;

// static
LazyInstance<JavaObjectWeakGlobalRef> g_sw_instance_ =
    LAZY_INSTANCE_INITIALIZER;

// static
RfhToIoThreadClientMap* RfhToIoThreadClientMap::GetInstance() {
  return g_instance_.Pointer();
}

void RfhToIoThreadClientMap::Set(pair<int, int> rfh_id,
                                 const IoThreadClientData& client) {
  base::AutoLock lock(map_lock_);
  rfh_to_io_thread_client_[rfh_id] = client;
}

bool RfhToIoThreadClientMap::Get(
    pair<int, int> rfh_id, IoThreadClientData* client) {
  base::AutoLock lock(map_lock_);
  RenderFrameHostToIoThreadClientType::iterator iterator =
      rfh_to_io_thread_client_.find(rfh_id);
  if (iterator == rfh_to_io_thread_client_.end())
    return false;

  *client = iterator->second;
  return true;
}

void RfhToIoThreadClientMap::Erase(pair<int, int> rfh_id) {
  base::AutoLock lock(map_lock_);
  rfh_to_io_thread_client_.erase(rfh_id);
}

// ClientMapEntryUpdater ------------------------------------------------------

class ClientMapEntryUpdater : public content::WebContentsObserver {
 public:
  ClientMapEntryUpdater(JNIEnv* env, WebContents* web_contents,
                        jobject jdelegate);

  void RenderFrameCreated(RenderFrameHost* render_frame_host) override;
  void RenderFrameDeleted(RenderFrameHost* render_frame_host) override;
  void WebContentsDestroyed() override;

 private:
  JavaObjectWeakGlobalRef jdelegate_;
};

ClientMapEntryUpdater::ClientMapEntryUpdater(JNIEnv* env,
                                             WebContents* web_contents,
                                             jobject jdelegate)
    : content::WebContentsObserver(web_contents),
      jdelegate_(env, jdelegate) {
  DCHECK(web_contents);
  DCHECK(jdelegate);

  if (web_contents->GetMainFrame())
    RenderFrameCreated(web_contents->GetMainFrame());
}

void ClientMapEntryUpdater::RenderFrameCreated(RenderFrameHost* rfh) {
  IoThreadClientData client_data;
  client_data.io_thread_client = jdelegate_;
  client_data.pending_association = false;
  RfhToIoThreadClientMap::GetInstance()->Set(
      GetRenderFrameHostIdPair(rfh), client_data);
}

void ClientMapEntryUpdater::RenderFrameDeleted(RenderFrameHost* rfh) {
  RfhToIoThreadClientMap::GetInstance()->Erase(GetRenderFrameHostIdPair(rfh));
}

void ClientMapEntryUpdater::WebContentsDestroyed() {
  delete this;
}

struct WebResourceRequest {
  std::string url;
  std::string method;
  bool is_main_frame;
  bool has_user_gesture;
  vector<string> header_names;
  vector<string> header_values;

  ScopedJavaLocalRef<jstring> jstring_url;
  ScopedJavaLocalRef<jstring> jstring_method;
  ScopedJavaLocalRef<jobjectArray> jstringArray_header_names;
  ScopedJavaLocalRef<jobjectArray> jstringArray_header_values;

  WebResourceRequest(const net::URLRequest* request)
      : url(request->url().spec()),
        method(request->method()) {
    const content::ResourceRequestInfo* info =
        content::ResourceRequestInfo::ForRequest(request);
    is_main_frame =
        info && info->GetResourceType() == content::RESOURCE_TYPE_MAIN_FRAME;
    has_user_gesture = info && info->HasUserGesture();

    net::HttpRequestHeaders headers;
    if (!request->GetFullRequestHeaders(&headers))
      headers = request->extra_request_headers();
    net::HttpRequestHeaders::Iterator headers_iterator(headers);
    while (headers_iterator.GetNext()) {
      header_names.push_back(headers_iterator.name());
      header_values.push_back(headers_iterator.value());
    }
  }

  WebResourceRequest(JNIEnv* env, const net::URLRequest* request)
      : WebResourceRequest(request) {
    ConvertToJava(env);
  }

  void ConvertToJava(JNIEnv* env) {
    jstring_url = ConvertUTF8ToJavaString(env, url);
    jstring_method = ConvertUTF8ToJavaString(env, method);
    jstringArray_header_names = ToJavaArrayOfStrings(env, header_names);
    jstringArray_header_values = ToJavaArrayOfStrings(env, header_values);
  }
};

} // namespace

// AwContentsIoThreadClientImpl -----------------------------------------------

// static
scoped_ptr<AwContentsIoThreadClient>
AwContentsIoThreadClient::FromID(int render_process_id, int render_frame_id) {
  pair<int, int> rfh_id(render_process_id, render_frame_id);
  IoThreadClientData client_data;
  if (!RfhToIoThreadClientMap::GetInstance()->Get(rfh_id, &client_data))
    return scoped_ptr<AwContentsIoThreadClient>();

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> java_delegate =
      client_data.io_thread_client.get(env);
  DCHECK(!client_data.pending_association || java_delegate.is_null());
  return scoped_ptr<AwContentsIoThreadClient>(new AwContentsIoThreadClientImpl(
      client_data.pending_association, java_delegate));
}

// static
void AwContentsIoThreadClient::SubFrameCreated(int render_process_id,
                                               int parent_render_frame_id,
                                               int child_render_frame_id) {
  pair<int, int> parent_rfh_id(render_process_id, parent_render_frame_id);
  pair<int, int> child_rfh_id(render_process_id, child_render_frame_id);
  IoThreadClientData client_data;
  if (!RfhToIoThreadClientMap::GetInstance()->Get(parent_rfh_id,
                                                  &client_data)) {
    NOTREACHED();
    return;
  }

  RfhToIoThreadClientMap::GetInstance()->Set(child_rfh_id, client_data);
}

// static
void AwContentsIoThreadClientImpl::RegisterPendingContents(
    WebContents* web_contents) {
  IoThreadClientData client_data;
  client_data.pending_association = true;
  RfhToIoThreadClientMap::GetInstance()->Set(
      GetRenderFrameHostIdPair(web_contents->GetMainFrame()), client_data);
}

// static
void AwContentsIoThreadClientImpl::Associate(
    WebContents* web_contents,
    const JavaRef<jobject>& jclient) {
  JNIEnv* env = AttachCurrentThread();
  // The ClientMapEntryUpdater lifespan is tied to the WebContents.
  new ClientMapEntryUpdater(env, web_contents, jclient.obj());
}

// static
void AwContentsIoThreadClientImpl::SetServiceWorkerIoThreadClient(
    const base::android::JavaRef<jobject>& jclient,
    const base::android::JavaRef<jobject>& browser_context) {
  // TODO: currently there is only one browser context so it is ok to
  // store in a global variable, in the future use browser_context to
  // obtain the correct instance.
  JavaObjectWeakGlobalRef temp(AttachCurrentThread(), jclient.obj());
  g_sw_instance_.Get() = temp;
}

// static
scoped_ptr<AwContentsIoThreadClient>
AwContentsIoThreadClient::GetServiceWorkerIoThreadClient() {
  if (g_sw_instance_.Get().is_empty())
    return scoped_ptr<AwContentsIoThreadClient>();

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> java_delegate = g_sw_instance_.Get().get(env);

  DCHECK(!java_delegate.is_null());
  return scoped_ptr<AwContentsIoThreadClient>(new AwContentsIoThreadClientImpl(
      false, java_delegate));
}

AwContentsIoThreadClientImpl::AwContentsIoThreadClientImpl(
    bool pending_association,
    const JavaRef<jobject>& obj)
  : pending_association_(pending_association),
    java_object_(obj) {
}

AwContentsIoThreadClientImpl::~AwContentsIoThreadClientImpl() {
  // explict, out-of-line destructor.
}

bool AwContentsIoThreadClientImpl::PendingAssociation() const {
  return pending_association_;
}

AwContentsIoThreadClient::CacheMode
AwContentsIoThreadClientImpl::GetCacheMode() const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (java_object_.is_null())
    return AwContentsIoThreadClient::LOAD_DEFAULT;

  JNIEnv* env = AttachCurrentThread();
  return static_cast<AwContentsIoThreadClient::CacheMode>(
      Java_AwContentsIoThreadClient_getCacheMode(
          env, java_object_.obj()));
}


namespace {

scoped_ptr<AwWebResourceResponse> RunShouldInterceptRequest(
    WebResourceRequest web_request,
    JavaObjectWeakGlobalRef ref) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  JNIEnv* env = AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> obj = ref.get(env);
  if (obj.is_null())
    return nullptr;

  web_request.ConvertToJava(env);

  devtools_instrumentation::ScopedEmbedderCallbackTask embedder_callback(
      "shouldInterceptRequest");
  ScopedJavaLocalRef<jobject> ret =
      AwContentsBackgroundThreadClient::shouldInterceptRequest(
          env,
          obj.obj(),
          web_request.jstring_url.obj(),
          web_request.is_main_frame,
          web_request.has_user_gesture,
          web_request.jstring_method.obj(),
          web_request.jstringArray_header_names.obj(),
          web_request.jstringArray_header_values.obj());
  return scoped_ptr<AwWebResourceResponse>(
      ret.is_null() ? nullptr : new AwWebResourceResponseImpl(ret));
}

scoped_ptr<AwWebResourceResponse> ReturnNull() {
  return scoped_ptr<AwWebResourceResponse>();
}

}  // namespace

void AwContentsIoThreadClientImpl::ShouldInterceptRequestAsync(
    const net::URLRequest* request,
    const ShouldInterceptRequestResultCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  base::Callback<scoped_ptr<AwWebResourceResponse>()> get_response =
      base::Bind(&ReturnNull);
  JNIEnv* env = AttachCurrentThread();
  if (bg_thread_client_object_.is_null() && !java_object_.is_null()) {
    bg_thread_client_object_.Reset(
        Java_AwContentsIoThreadClient_getBackgroundThreadClient(
            env, java_object_.obj()));
  }
  if (!bg_thread_client_object_.is_null()) {
    get_response = base::Bind(
        &RunShouldInterceptRequest, WebResourceRequest(request),
        JavaObjectWeakGlobalRef(env, bg_thread_client_object_.obj()));
  }
  BrowserThread::PostTaskAndReplyWithResult(BrowserThread::FILE, FROM_HERE,
                                            get_response, callback);
}

bool AwContentsIoThreadClientImpl::ShouldBlockContentUrls() const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (java_object_.is_null())
    return false;

  JNIEnv* env = AttachCurrentThread();
  return Java_AwContentsIoThreadClient_shouldBlockContentUrls(
      env, java_object_.obj());
}

bool AwContentsIoThreadClientImpl::ShouldBlockFileUrls() const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (java_object_.is_null())
    return false;

  JNIEnv* env = AttachCurrentThread();
  return Java_AwContentsIoThreadClient_shouldBlockFileUrls(
      env, java_object_.obj());
}

bool AwContentsIoThreadClientImpl::ShouldAcceptThirdPartyCookies() const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (java_object_.is_null())
    return false;

  JNIEnv* env = AttachCurrentThread();
  return Java_AwContentsIoThreadClient_shouldAcceptThirdPartyCookies(
      env, java_object_.obj());
}

bool AwContentsIoThreadClientImpl::ShouldBlockNetworkLoads() const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (java_object_.is_null())
    return false;

  JNIEnv* env = AttachCurrentThread();
  return Java_AwContentsIoThreadClient_shouldBlockNetworkLoads(
      env, java_object_.obj());
}

void AwContentsIoThreadClientImpl::NewDownload(
    const GURL& url,
    const string& user_agent,
    const string& content_disposition,
    const string& mime_type,
    int64_t content_length) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (java_object_.is_null())
    return;

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jstring_url =
      ConvertUTF8ToJavaString(env, url.spec());
  ScopedJavaLocalRef<jstring> jstring_user_agent =
      ConvertUTF8ToJavaString(env, user_agent);
  ScopedJavaLocalRef<jstring> jstring_content_disposition =
      ConvertUTF8ToJavaString(env, content_disposition);
  ScopedJavaLocalRef<jstring> jstring_mime_type =
      ConvertUTF8ToJavaString(env, mime_type);

  Java_AwContentsIoThreadClient_onDownloadStart(
      env,
      java_object_.obj(),
      jstring_url.obj(),
      jstring_user_agent.obj(),
      jstring_content_disposition.obj(),
      jstring_mime_type.obj(),
      content_length);
}

void AwContentsIoThreadClientImpl::NewLoginRequest(const string& realm,
                                                   const string& account,
                                                   const string& args) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (java_object_.is_null())
    return;

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jrealm = ConvertUTF8ToJavaString(env, realm);
  ScopedJavaLocalRef<jstring> jargs = ConvertUTF8ToJavaString(env, args);

  ScopedJavaLocalRef<jstring> jaccount;
  if (!account.empty())
    jaccount = ConvertUTF8ToJavaString(env, account);

  Java_AwContentsIoThreadClient_newLoginRequest(
      env, java_object_.obj(), jrealm.obj(), jaccount.obj(), jargs.obj());
}

void AwContentsIoThreadClientImpl::OnReceivedError(
    const net::URLRequest* request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (java_object_.is_null())
    return;

  JNIEnv* env = AttachCurrentThread();
  WebResourceRequest web_request(env, request);

  int error_code = request->status().error();
  ScopedJavaLocalRef<jstring> jstring_description = ConvertUTF8ToJavaString(
      env, net::ErrorToString(request->status().error()));

  Java_AwContentsIoThreadClient_onReceivedError(
      env,
      java_object_.obj(),
      web_request.jstring_url.obj(),
      web_request.is_main_frame,
      web_request.has_user_gesture,
      web_request.jstring_method.obj(),
      web_request.jstringArray_header_names.obj(),
      web_request.jstringArray_header_values.obj(),
      error_code,
      jstring_description.obj());
}

void AwContentsIoThreadClientImpl::OnReceivedHttpError(
    const net::URLRequest* request,
    const net::HttpResponseHeaders* response_headers) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (java_object_.is_null())
    return;

  JNIEnv* env = AttachCurrentThread();
  WebResourceRequest web_request(env, request);

  vector<string> response_header_names;
  vector<string> response_header_values;
  {
    size_t headers_iterator = 0;
    string header_name, header_value;
    while (response_headers->EnumerateHeaderLines(
        &headers_iterator, &header_name, &header_value)) {
      response_header_names.push_back(header_name);
      response_header_values.push_back(header_value);
    }
  }

  string mime_type, encoding;
  response_headers->GetMimeTypeAndCharset(&mime_type, &encoding);
  ScopedJavaLocalRef<jstring> jstring_mime_type =
      ConvertUTF8ToJavaString(env, mime_type);
  ScopedJavaLocalRef<jstring> jstring_encoding =
      ConvertUTF8ToJavaString(env, encoding);
  int status_code = response_headers->response_code();
  ScopedJavaLocalRef<jstring> jstring_reason =
      ConvertUTF8ToJavaString(env, response_headers->GetStatusText());
  ScopedJavaLocalRef<jobjectArray> jstringArray_response_header_names =
      ToJavaArrayOfStrings(env, response_header_names);
  ScopedJavaLocalRef<jobjectArray> jstringArray_response_header_values =
      ToJavaArrayOfStrings(env, response_header_values);

  Java_AwContentsIoThreadClient_onReceivedHttpError(
      env,
      java_object_.obj(),
      web_request.jstring_url.obj(),
      web_request.is_main_frame,
      web_request.has_user_gesture,
      web_request.jstring_method.obj(),
      web_request.jstringArray_header_names.obj(),
      web_request.jstringArray_header_values.obj(),
      jstring_mime_type.obj(),
      jstring_encoding.obj(),
      status_code,
      jstring_reason.obj(),
      jstringArray_response_header_names.obj(),
      jstringArray_response_header_values.obj());
}

bool RegisterAwContentsIoThreadClientImpl(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

} // namespace android_webview
