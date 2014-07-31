// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/native/aw_contents_io_thread_client_impl.h"

#include <map>
#include <utility>

#include "android_webview/common/devtools_instrumentation.h"
#include "android_webview/native/aw_web_resource_response_impl.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/jni_weak_ref.h"
#include "base/lazy_instance.h"
#include "base/memory/linked_ptr.h"
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
#include "net/http/http_request_headers.h"
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

  virtual void RenderFrameCreated(RenderFrameHost* render_frame_host) OVERRIDE;
  virtual void RenderFrameDeleted(RenderFrameHost* render_frame_host) OVERRIDE;
  virtual void WebContentsDestroyed() OVERRIDE;

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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (java_object_.is_null())
    return AwContentsIoThreadClient::LOAD_DEFAULT;

  JNIEnv* env = AttachCurrentThread();
  return static_cast<AwContentsIoThreadClient::CacheMode>(
      Java_AwContentsIoThreadClient_getCacheMode(
          env, java_object_.obj()));
}

scoped_ptr<AwWebResourceResponse>
AwContentsIoThreadClientImpl::ShouldInterceptRequest(
    const GURL& location,
    const net::URLRequest* request) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (java_object_.is_null())
    return scoped_ptr<AwWebResourceResponse>();
  const content::ResourceRequestInfo* info =
      content::ResourceRequestInfo::ForRequest(request);
  bool is_main_frame = info &&
      info->GetResourceType() == content::RESOURCE_TYPE_MAIN_FRAME;
  bool has_user_gesture = info && info->HasUserGesture();

  vector<string> headers_names;
  vector<string> headers_values;
  {
    net::HttpRequestHeaders headers;
    if (!request->GetFullRequestHeaders(&headers))
      headers = request->extra_request_headers();
    net::HttpRequestHeaders::Iterator headers_iterator(headers);
    while (headers_iterator.GetNext()) {
      headers_names.push_back(headers_iterator.name());
      headers_values.push_back(headers_iterator.value());
    }
  }

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jstring_url =
      ConvertUTF8ToJavaString(env, location.spec());
  ScopedJavaLocalRef<jstring> jstring_method =
      ConvertUTF8ToJavaString(env, request->method());
  ScopedJavaLocalRef<jobjectArray> jstringArray_headers_names =
      ToJavaArrayOfStrings(env, headers_names);
  ScopedJavaLocalRef<jobjectArray> jstringArray_headers_values =
      ToJavaArrayOfStrings(env, headers_values);
  devtools_instrumentation::ScopedEmbedderCallbackTask embedder_callback(
      "shouldInterceptRequest");
  ScopedJavaLocalRef<jobject> ret =
      Java_AwContentsIoThreadClient_shouldInterceptRequest(
          env,
          java_object_.obj(),
          jstring_url.obj(),
          is_main_frame,
          has_user_gesture,
          jstring_method.obj(),
          jstringArray_headers_names.obj(),
          jstringArray_headers_values.obj());
  if (ret.is_null())
    return scoped_ptr<AwWebResourceResponse>();
  return scoped_ptr<AwWebResourceResponse>(
      new AwWebResourceResponseImpl(ret));
}

bool AwContentsIoThreadClientImpl::ShouldBlockContentUrls() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (java_object_.is_null())
    return false;

  JNIEnv* env = AttachCurrentThread();
  return Java_AwContentsIoThreadClient_shouldBlockContentUrls(
      env, java_object_.obj());
}

bool AwContentsIoThreadClientImpl::ShouldBlockFileUrls() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (java_object_.is_null())
    return false;

  JNIEnv* env = AttachCurrentThread();
  return Java_AwContentsIoThreadClient_shouldBlockFileUrls(
      env, java_object_.obj());
}

bool AwContentsIoThreadClientImpl::ShouldAcceptThirdPartyCookies() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (java_object_.is_null())
    return false;

  JNIEnv* env = AttachCurrentThread();
  return Java_AwContentsIoThreadClient_shouldAcceptThirdPartyCookies(
      env, java_object_.obj());
}

bool AwContentsIoThreadClientImpl::ShouldBlockNetworkLoads() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
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
    int64 content_length) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
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

bool RegisterAwContentsIoThreadClientImpl(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

} // namespace android_webview
