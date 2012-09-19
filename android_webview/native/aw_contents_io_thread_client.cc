// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/native/aw_contents_io_thread_client.h"

#include <map>
#include <utility>

#include "android_webview/native/intercepted_request_data.h"
#include "base/android/jni_helper.h"
#include "base/android/jni_string.h"
#include "base/lazy_instance.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
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
  static LazyInstance<RvhToIoThreadClientMap> s_instance_;
  base::Lock map_lock_;
  RenderViewHostToWeakDelegateMapType rvh_to_weak_delegate_map_;
};

// static
LazyInstance<RvhToIoThreadClientMap> RvhToIoThreadClientMap::s_instance_ =
    LAZY_INSTANCE_INITIALIZER;

// static
RvhToIoThreadClientMap* RvhToIoThreadClientMap::GetInstance() {
  return s_instance_.Pointer();
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

// AwContentsIoThreadClient ---------------------------------------------------

// static
void AwContentsIoThreadClient::Associate(
    WebContents* web_contents,
    const JavaRef<jobject>& jclient) {
  JNIEnv* env = AttachCurrentThread();
  // The ClientMapEntryUpdater lifespan is tied to the WebContents.
  new ClientMapEntryUpdater(env, web_contents, jclient.obj());
}

// static
AwContentsIoThreadClient
AwContentsIoThreadClient::FromID(int render_process_id, int render_view_id) {
  pair<int, int> rvh_id(render_process_id, render_view_id);
  ScopedJavaLocalRef<jobject> java_delegate =
      RvhToIoThreadClientMap::GetInstance()->Get(rvh_id);
  if (java_delegate.is_null())
    return AwContentsIoThreadClient();

  return AwContentsIoThreadClient(java_delegate);
}

AwContentsIoThreadClient::AwContentsIoThreadClient() {
}

AwContentsIoThreadClient::AwContentsIoThreadClient(const JavaRef<jobject>& obj)
    : java_object_(obj) {
}

AwContentsIoThreadClient::AwContentsIoThreadClient(
    const AwContentsIoThreadClient& orig)
    : java_object_(orig.java_object_) {
}

void AwContentsIoThreadClient::operator=(const AwContentsIoThreadClient& rhs) {
  java_object_.Reset(rhs.java_object_);
}

AwContentsIoThreadClient::~AwContentsIoThreadClient() {
}

scoped_ptr<InterceptedRequestData>
AwContentsIoThreadClient::ShouldInterceptRequest(
    const net::URLRequest* request) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (java_object_.is_null())
    return scoped_ptr<InterceptedRequestData>();

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jstring_url =
      ConvertUTF8ToJavaString(env, request->url().spec());
  ScopedJavaLocalRef<jobject> ret =
      Java_AwContentsIoThreadClient_shouldInterceptRequest(
          env, java_object_.obj(), jstring_url.obj());
  if (ret.is_null())
    return scoped_ptr<InterceptedRequestData>();
  return scoped_ptr<InterceptedRequestData>(
      new InterceptedRequestData(ret));
}

bool RegisterAwContentsIoThreadClient(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

} // namespace android_webview
