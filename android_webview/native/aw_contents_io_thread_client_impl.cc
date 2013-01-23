// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/native/aw_contents_io_thread_client_impl.h"

#include <map>
#include <utility>

#include "android_webview/native/intercepted_request_data_impl.h"
#include "base/android/jni_helper.h"
#include "base/android/jni_string.h"
#include "base/lazy_instance.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_request.h"

#include "jni/AwContentsIoThreadClient_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;
using base::LazyInstance;
using content::BrowserThread;
using content::RenderViewHost;
using content::WebContents;
using std::map;
using std::pair;

namespace android_webview {

namespace {

typedef map<pair<int, int>, JavaObjectWeakGlobalRef>
    RenderViewHostToWeakDelegateMapType;

static pair<int, int> GetRenderViewHostIdPair(RenderViewHost* rvh) {
  return pair<int, int>(rvh->GetProcess()->GetID(), rvh->GetRoutingID());
}

// RvhToIoThreadClientMap -----------------------------------------------------
class RvhToIoThreadClientMap {
 public:
  static RvhToIoThreadClientMap* GetInstance();
  void Insert(pair<int, int> rvh_id, JavaObjectWeakGlobalRef jdelegate);
  ScopedJavaLocalRef<jobject> Get(pair<int, int> rvh_id);
  void Erase(pair<int, int> rvh_id);

 private:
  static LazyInstance<RvhToIoThreadClientMap> g_instance_;
  base::Lock map_lock_;
  RenderViewHostToWeakDelegateMapType rvh_to_weak_delegate_map_;
};

// static
LazyInstance<RvhToIoThreadClientMap> RvhToIoThreadClientMap::g_instance_ =
    LAZY_INSTANCE_INITIALIZER;

// static
RvhToIoThreadClientMap* RvhToIoThreadClientMap::GetInstance() {
  return g_instance_.Pointer();
}

void RvhToIoThreadClientMap::Insert(pair<int, int> rvh_id,
                                  JavaObjectWeakGlobalRef jdelegate) {
  base::AutoLock lock(map_lock_);
  rvh_to_weak_delegate_map_[rvh_id] = jdelegate;
}

ScopedJavaLocalRef<jobject> RvhToIoThreadClientMap::Get(
    pair<int, int> rvh_id) {
  base::AutoLock lock(map_lock_);
  RenderViewHostToWeakDelegateMapType::iterator weak_delegate_iterator =
      rvh_to_weak_delegate_map_.find(rvh_id);
  if (weak_delegate_iterator == rvh_to_weak_delegate_map_.end())
    return ScopedJavaLocalRef<jobject>();

  JNIEnv* env = AttachCurrentThread();
  return weak_delegate_iterator->second.get(env);
}

void RvhToIoThreadClientMap::Erase(pair<int, int> rvh_id) {
  base::AutoLock lock(map_lock_);
  rvh_to_weak_delegate_map_.erase(rvh_id);
}

// ClientMapEntryUpdater ------------------------------------------------------

class ClientMapEntryUpdater : public content::WebContentsObserver {
 public:
  ClientMapEntryUpdater(JNIEnv* env, WebContents* web_contents,
                        jobject jdelegate);

  virtual void RenderViewCreated(RenderViewHost* render_view_host) OVERRIDE;
  virtual void RenderViewForInterstitialPageCreated(
      RenderViewHost* render_view_host) OVERRIDE;
  virtual void RenderViewDeleted(RenderViewHost* render_view_host) OVERRIDE;
  virtual void WebContentsDestroyed(WebContents* web_contents);

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

  if (web_contents->GetRenderViewHost())
    RenderViewCreated(web_contents->GetRenderViewHost());
}

void ClientMapEntryUpdater::RenderViewCreated(RenderViewHost* rvh) {
  RvhToIoThreadClientMap::GetInstance()->Insert(
      GetRenderViewHostIdPair(rvh), jdelegate_);
}

void ClientMapEntryUpdater::RenderViewForInterstitialPageCreated(
    RenderViewHost* rvh) {
  RenderViewCreated(rvh);
}

void ClientMapEntryUpdater::RenderViewDeleted(RenderViewHost* rvh) {
  RvhToIoThreadClientMap::GetInstance()->Erase(GetRenderViewHostIdPair(rvh));
}

void ClientMapEntryUpdater::WebContentsDestroyed(WebContents* web_contents) {
  if (web_contents->GetRenderViewHost())
    RenderViewDeleted(web_contents->GetRenderViewHost());
  delete this;
}

} // namespace

// AwContentsIoThreadClientImpl -----------------------------------------------

// static
scoped_ptr<AwContentsIoThreadClient>
AwContentsIoThreadClient::FromID(int render_process_id, int render_view_id) {
  pair<int, int> rvh_id(render_process_id, render_view_id);
  ScopedJavaLocalRef<jobject> java_delegate =
      RvhToIoThreadClientMap::GetInstance()->Get(rvh_id);
  if (java_delegate.is_null())
    return scoped_ptr<AwContentsIoThreadClient>();

  return scoped_ptr<AwContentsIoThreadClient>(
      new AwContentsIoThreadClientImpl(java_delegate));
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
    const JavaRef<jobject>& obj)
  : java_object_(obj) {
}

AwContentsIoThreadClientImpl::~AwContentsIoThreadClientImpl() {
  // explict, out-of-line destructor.
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

scoped_ptr<InterceptedRequestData>
AwContentsIoThreadClientImpl::ShouldInterceptRequest(
    const GURL& location,
    const net::URLRequest* request) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (java_object_.is_null())
    return scoped_ptr<InterceptedRequestData>();

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jstring_url =
      ConvertUTF8ToJavaString(env, location.spec());
  ScopedJavaLocalRef<jobject> ret =
      Java_AwContentsIoThreadClient_shouldInterceptRequest(
          env, java_object_.obj(), jstring_url.obj());
  if (ret.is_null())
    return scoped_ptr<InterceptedRequestData>();
  return scoped_ptr<InterceptedRequestData>(
      new InterceptedRequestDataImpl(ret));
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
    const std::string& user_agent,
    const std::string& content_disposition,
    const std::string& mime_type,
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

void AwContentsIoThreadClientImpl::NewLoginRequest(const std::string& realm,
                                                   const std::string& account,
                                                   const std::string& args) {
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
