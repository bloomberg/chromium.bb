// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/download_controller_android_impl.h"

#include <utility>

#include "base/android/context_utils.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "content/browser/android/content_view_core_impl.h"
#include "content/browser/android/deferred_download_observer.h"
#include "content/browser/download/download_item_impl.h"
#include "content/browser/download/download_manager_impl.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_url_parameters.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/common/content_client.h"
#include "content/public/common/referrer.h"
#include "jni/DownloadController_jni.h"
#include "net/cookies/cookie_options.h"
#include "net/cookies/cookie_store.h"
#include "net/http/http_content_disposition.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"

using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;

namespace {
// Guards download_controller_
base::LazyInstance<base::Lock> g_download_controller_lock_;

content::WebContents* GetWebContents(int render_process_id,
                                     int render_view_id) {
  content::RenderViewHost* render_view_host =
      content::RenderViewHost::FromID(render_process_id, render_view_id);

  if (!render_view_host)
    return nullptr;

  return content::WebContents::FromRenderViewHost(render_view_host);
}

void CreateContextMenuDownload(int render_process_id,
                               int render_view_id,
                               const content::ContextMenuParams& params,
                               bool is_link,
                               const std::string& extra_headers,
                               bool granted) {
  if (!granted)
    return;

  content::WebContents* web_contents =
      GetWebContents(render_process_id, render_view_id);
  if (!web_contents)
    return;

  const GURL& url = is_link ? params.link_url : params.src_url;
  const GURL& referring_url =
      params.frame_url.is_empty() ? params.page_url : params.frame_url;
  content::DownloadManagerImpl* dlm =
      static_cast<content::DownloadManagerImpl*>(
          content::BrowserContext::GetDownloadManager(
              web_contents->GetBrowserContext()));
  scoped_ptr<content::DownloadUrlParameters> dl_params(
      content::DownloadUrlParameters::FromWebContents(web_contents, url));
  content::Referrer referrer = content::Referrer::SanitizeForRequest(
      url,
      content::Referrer(referring_url.GetAsReferrer(), params.referrer_policy));
  dl_params->set_referrer(referrer);
  if (is_link)
    dl_params->set_referrer_encoding(params.frame_charset);
  net::HttpRequestHeaders headers;
  headers.AddHeadersFromString(extra_headers);
  for (net::HttpRequestHeaders::Iterator it(headers); it.GetNext();)
    dl_params->add_request_header(it.name(), it.value());
  if (!is_link && extra_headers.empty())
    dl_params->set_prefer_cache(true);
  dl_params->set_prompt(false);
  dlm->DownloadUrl(std::move(dl_params));
}

}  // namespace

namespace content {

// JNI methods
static void Init(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  DownloadControllerAndroidImpl::GetInstance()->Init(env, obj);
}

static void OnRequestFileAccessResult(JNIEnv* env,
                                      const JavaParamRef<jobject>& obj,
                                      jlong callback_id,
                                      jboolean granted) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(callback_id);

  // Convert java long long int to c++ pointer, take ownership.
  scoped_ptr<DownloadControllerAndroid::AcquireFileAccessPermissionCallback> cb(
      reinterpret_cast<
          DownloadControllerAndroid::AcquireFileAccessPermissionCallback*>(
              callback_id));
  cb->Run(granted);
}

struct DownloadControllerAndroidImpl::JavaObject {
  ScopedJavaLocalRef<jobject> Controller(JNIEnv* env) {
    return GetRealObject(env, obj);
  }
  jweak obj;
};

// static
bool DownloadControllerAndroidImpl::RegisterDownloadController(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

// static
DownloadControllerAndroid* DownloadControllerAndroid::Get() {
  base::AutoLock lock(g_download_controller_lock_.Get());
  if (!DownloadControllerAndroid::download_controller_)
    download_controller_ = DownloadControllerAndroidImpl::GetInstance();
  return DownloadControllerAndroid::download_controller_;
}

//static
void DownloadControllerAndroid::SetDownloadControllerAndroid(
    DownloadControllerAndroid* download_controller) {
  base::AutoLock lock(g_download_controller_lock_.Get());
  DownloadControllerAndroid::download_controller_ = download_controller;
}

// static
DownloadControllerAndroidImpl* DownloadControllerAndroidImpl::GetInstance() {
  return base::Singleton<DownloadControllerAndroidImpl>::get();
}

DownloadControllerAndroidImpl::DownloadControllerAndroidImpl()
    : java_object_(NULL) {
}

DownloadControllerAndroidImpl::~DownloadControllerAndroidImpl() {
  if (java_object_) {
    JNIEnv* env = base::android::AttachCurrentThread();
    env->DeleteWeakGlobalRef(java_object_->obj);
    delete java_object_;
    base::android::CheckException(env);
  }
}

// Initialize references to Java object.
void DownloadControllerAndroidImpl::Init(JNIEnv* env, jobject obj) {
  java_object_ = new JavaObject;
  java_object_->obj = env->NewWeakGlobalRef(obj);
}

void DownloadControllerAndroidImpl::CancelDeferredDownload(
    DeferredDownloadObserver* observer) {
  for (auto iter = deferred_downloads_.begin();
      iter != deferred_downloads_.end(); ++iter) {
    if (*iter == observer) {
      deferred_downloads_.erase(iter);
      return;
    }
  }
}

void DownloadControllerAndroidImpl::AcquireFileAccessPermission(
    WebContents* web_contents,
    const DownloadControllerAndroid::AcquireFileAccessPermissionCallback& cb) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(web_contents);

  ScopedJavaLocalRef<jobject> view =
      GetContentViewCoreFromWebContents(web_contents);
  if (view.is_null()) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE, base::Bind(cb, false));
    return;
  }

  if (HasFileAccessPermission(view)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE, base::Bind(cb, true));
    return;
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  // Make copy on the heap so we can pass the pointer through JNI.
  intptr_t callback_id = reinterpret_cast<intptr_t>(
      new DownloadControllerAndroid::AcquireFileAccessPermissionCallback(cb));
  Java_DownloadController_requestFileAccess(
      env, GetJavaObject()->Controller(env).obj(), view.obj(), callback_id);
}

bool DownloadControllerAndroidImpl::HasFileAccessPermission(
    ScopedJavaLocalRef<jobject> j_content_view_core) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!j_content_view_core.is_null());

  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_DownloadController_hasFileAccess(
      env, GetJavaObject()->Controller(env).obj(), j_content_view_core.obj());
}

void DownloadControllerAndroidImpl::CreateGETDownload(
    int render_process_id, int render_view_id, int request_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  GlobalRequestID global_id(render_process_id, request_id);

  // We are yielding the UI thread and render_view_host may go away by
  // the time we come back. Pass along render_process_id and render_view_id
  // to retrieve it later (if it still exists).
  GetDownloadInfoCB cb = base::Bind(
        &DownloadControllerAndroidImpl::StartAndroidDownload,
        base::Unretained(this), render_process_id,
        render_view_id);

  PrepareDownloadInfo(
      global_id,
      base::Bind(&DownloadControllerAndroidImpl::StartDownloadOnUIThread,
                 base::Unretained(this), cb));
}

void DownloadControllerAndroidImpl::PrepareDownloadInfo(
    const GlobalRequestID& global_id,
    const GetDownloadInfoCB& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  net::URLRequest* request =
      ResourceDispatcherHostImpl::Get()->GetURLRequest(global_id);
  if (!request) {
    LOG(ERROR) << "Request to download not found.";
    return;
  }

  DownloadInfoAndroid info_android(request);

  net::CookieStore* cookie_store = request->context()->cookie_store();
  if (cookie_store) {
    cookie_store->GetAllCookiesForURLAsync(
        request->url(),
        base::Bind(&DownloadControllerAndroidImpl::CheckPolicyAndLoadCookies,
                   base::Unretained(this), info_android, callback, global_id));
  } else {
    // Can't get any cookies, start android download.
    callback.Run(info_android);
  }
}

void DownloadControllerAndroidImpl::CheckPolicyAndLoadCookies(
    const DownloadInfoAndroid& info, const GetDownloadInfoCB& callback,
    const GlobalRequestID& global_id, const net::CookieList& cookie_list) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  net::URLRequest* request =
      ResourceDispatcherHostImpl::Get()->GetURLRequest(global_id);
  if (!request) {
    LOG(ERROR) << "Request to download not found.";
    return;
  }

  if (request->context()->network_delegate()->CanGetCookies(
      *request, cookie_list)) {
    DoLoadCookies(info, callback, global_id);
  } else {
    callback.Run(info);
  }
}

void DownloadControllerAndroidImpl::DoLoadCookies(
    const DownloadInfoAndroid& info, const GetDownloadInfoCB& callback,
    const GlobalRequestID& global_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  net::CookieOptions options;
  options.set_include_httponly();

  net::URLRequest* request =
      ResourceDispatcherHostImpl::Get()->GetURLRequest(global_id);
  if (!request) {
    LOG(ERROR) << "Request to download not found.";
    return;
  }

  request->context()->cookie_store()->GetCookiesWithOptionsAsync(
      info.url, options,
      base::Bind(&DownloadControllerAndroidImpl::OnCookieResponse,
                 base::Unretained(this), info, callback));
}

void DownloadControllerAndroidImpl::OnCookieResponse(
    DownloadInfoAndroid download_info,
    const GetDownloadInfoCB& callback,
    const std::string& cookie) {
  download_info.cookie = cookie;

  // We have everything we need, start Android download.
  callback.Run(download_info);
}

void DownloadControllerAndroidImpl::StartDownloadOnUIThread(
    const GetDownloadInfoCB& callback,
    const DownloadInfoAndroid& info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE, base::Bind(callback, info));
}

void DownloadControllerAndroidImpl::StartAndroidDownload(
    int render_process_id, int render_view_id,
    const DownloadInfoAndroid& info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  WebContents* web_contents = GetWebContents(render_process_id, render_view_id);
  if (!web_contents) {
    // The view went away. Can't proceed.
    LOG(ERROR) << "Download failed on URL:" << info.url.spec();
    return;
  }
  ScopedJavaLocalRef<jobject> view =
      GetContentViewCoreFromWebContents(web_contents);
  if (view.is_null()) {
    // ContentViewCore might not have been created yet, pass a callback to
    // DeferredDownloadTaskManager so that the download can restart when
    // ContentViewCore is created.
    deferred_downloads_.push_back(new DeferredDownloadObserver(
        web_contents,
        base::Bind(&DownloadControllerAndroidImpl::StartAndroidDownload,
                   base::Unretained(this), render_process_id, render_view_id,
                   info)));
    return;
  }

  AcquireFileAccessPermission(
      web_contents,
      base::Bind(&DownloadControllerAndroidImpl::StartAndroidDownloadInternal,
                 base::Unretained(this), render_process_id, render_view_id,
                 info));
}

void DownloadControllerAndroidImpl::StartAndroidDownloadInternal(
    int render_process_id, int render_view_id,
    const DownloadInfoAndroid& info, bool allowed) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!allowed)
    return;

  // Call newHttpGetDownload
  WebContents* web_contents = GetWebContents(render_process_id, render_view_id);
  // The view went away. Can't proceed.
  if (!web_contents)
    return;
  ScopedJavaLocalRef<jobject> view =
      GetContentViewCoreFromWebContents(web_contents);
  if (view.is_null())
    return;

  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jurl =
      ConvertUTF8ToJavaString(env, info.url.spec());
  ScopedJavaLocalRef<jstring> juser_agent =
      ConvertUTF8ToJavaString(env, info.user_agent);
  ScopedJavaLocalRef<jstring> jcontent_disposition =
      ConvertUTF8ToJavaString(env, info.content_disposition);
  ScopedJavaLocalRef<jstring> jmime_type =
      ConvertUTF8ToJavaString(env, info.original_mime_type);
  ScopedJavaLocalRef<jstring> jcookie =
      ConvertUTF8ToJavaString(env, info.cookie);
  ScopedJavaLocalRef<jstring> jreferer =
      ConvertUTF8ToJavaString(env, info.referer);

  // Try parsing the content disposition header to get a
  // explicitly specified filename if available.
  net::HttpContentDisposition header(info.content_disposition, "");
  ScopedJavaLocalRef<jstring> jfilename =
      ConvertUTF8ToJavaString(env, header.filename());

  Java_DownloadController_newHttpGetDownload(
      env, GetJavaObject()->Controller(env).obj(), view.obj(), jurl.obj(),
      juser_agent.obj(), jcontent_disposition.obj(), jmime_type.obj(),
      jcookie.obj(), jreferer.obj(), info.has_user_gesture, jfilename.obj(),
      info.total_bytes);
}

void DownloadControllerAndroidImpl::OnDownloadStarted(
    DownloadItem* download_item) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!download_item->GetWebContents())
    return;

  JNIEnv* env = base::android::AttachCurrentThread();

  // Register for updates to the DownloadItem.
  download_item->AddObserver(this);

  ScopedJavaLocalRef<jobject> view =
      GetContentViewCoreFromWebContents(download_item->GetWebContents());
  // The view went away. Can't proceed.
  if (view.is_null())
    return;

  ScopedJavaLocalRef<jstring> jmime_type =
      ConvertUTF8ToJavaString(env, download_item->GetMimeType());
  ScopedJavaLocalRef<jstring> jfilename = ConvertUTF8ToJavaString(
      env, download_item->GetTargetFilePath().BaseName().value());
  Java_DownloadController_onDownloadStarted(
      env, GetJavaObject()->Controller(env).obj(), view.obj(), jfilename.obj(),
      jmime_type.obj());
}

void DownloadControllerAndroidImpl::OnDownloadUpdated(DownloadItem* item) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (item->IsDangerous() && (item->GetState() != DownloadItem::CANCELLED))
    OnDangerousDownload(item);

  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jurl =
      ConvertUTF8ToJavaString(env, item->GetURL().spec());
  ScopedJavaLocalRef<jstring> jmime_type =
      ConvertUTF8ToJavaString(env, item->GetMimeType());
  ScopedJavaLocalRef<jstring> jpath =
      ConvertUTF8ToJavaString(env, item->GetTargetFilePath().value());
  ScopedJavaLocalRef<jstring> jfilename = ConvertUTF8ToJavaString(
      env, item->GetTargetFilePath().BaseName().value());

  switch (item->GetState()) {
    case DownloadItem::IN_PROGRESS: {
      base::TimeDelta time_delta;
      item->TimeRemaining(&time_delta);
      Java_DownloadController_onDownloadUpdated(
          env, GetJavaObject()->Controller(env).obj(),
          jurl.obj(), jmime_type.obj(),
          jfilename.obj(), jpath.obj(), item->GetReceivedBytes(), true,
          item->GetId(), item->PercentComplete(), time_delta.InMilliseconds(),
          item->HasUserGesture(), item->IsPaused(),
          // Get all requirements that allows a download to be resumable.
          !item->GetBrowserContext()->IsOffTheRecord());
      break;
    }
    case DownloadItem::COMPLETE:
      // Multiple OnDownloadUpdated() notifications may be issued while the
      // download is in the COMPLETE state. Only handle one.
      item->RemoveObserver(this);

      // Call onDownloadCompleted
      Java_DownloadController_onDownloadCompleted(
          env, GetJavaObject()->Controller(env).obj(), jurl.obj(),
          jmime_type.obj(), jfilename.obj(), jpath.obj(),
          item->GetReceivedBytes(), true, item->GetId(),
          item->HasUserGesture());
      break;
    case DownloadItem::CANCELLED:
      Java_DownloadController_onDownloadCancelled(
          env, GetJavaObject()->Controller(env).obj(), item->GetId());
      break;
    // TODO(shashishekhar): An interrupted download can be resumed. Android
    // currently does not support resumable downloads. Add handling for
    // interrupted case based on item->CanResume().
    case DownloadItem::INTERRUPTED:
      // Call onDownloadCompleted with success = false.
      Java_DownloadController_onDownloadCompleted(
          env, GetJavaObject()->Controller(env).obj(), jurl.obj(),
          jmime_type.obj(), jfilename.obj(), jpath.obj(),
          item->GetReceivedBytes(), false, item->GetId(),
          item->HasUserGesture());
      break;
    case DownloadItem::MAX_DOWNLOAD_STATE:
      NOTREACHED();
  }
}

void DownloadControllerAndroidImpl::OnDangerousDownload(DownloadItem* item) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jfilename = ConvertUTF8ToJavaString(
      env, item->GetTargetFilePath().BaseName().value());
  ScopedJavaLocalRef<jobject> view_core = GetContentViewCoreFromWebContents(
      item->GetWebContents());
  if (!view_core.is_null()) {
    Java_DownloadController_onDangerousDownload(
        env, GetJavaObject()->Controller(env).obj(), view_core.obj(),
        jfilename.obj(), item->GetId());
  }
}

ScopedJavaLocalRef<jobject>
    DownloadControllerAndroidImpl::GetContentViewCoreFromWebContents(
    WebContents* web_contents) {
  if (!web_contents)
    return ScopedJavaLocalRef<jobject>();

  ContentViewCore* view_core = ContentViewCore::FromWebContents(web_contents);
  return view_core ? view_core->GetJavaObject() :
      ScopedJavaLocalRef<jobject>();
}

DownloadControllerAndroidImpl::JavaObject*
    DownloadControllerAndroidImpl::GetJavaObject() {
  if (!java_object_) {
    // Initialize Java DownloadController by calling
    // DownloadController.getInstance(), which will call Init()
    // if Java DownloadController is not instantiated already.
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_DownloadController_getInstance(env);
  }

  DCHECK(java_object_);
  return java_object_;
}

void DownloadControllerAndroidImpl::StartContextMenuDownload(
    const ContextMenuParams& params, WebContents* web_contents, bool is_link,
    const std::string& extra_headers) {
  int process_id = web_contents->GetRenderProcessHost()->GetID();
  int routing_id = web_contents->GetRoutingID();
  AcquireFileAccessPermission(
      web_contents, base::Bind(&CreateContextMenuDownload, process_id,
                               routing_id, params, is_link, extra_headers));
}

void DownloadControllerAndroidImpl::DangerousDownloadValidated(
    WebContents* web_contents, int download_id, bool accept) {
  if (!web_contents)
    return;
  DownloadManagerImpl* dlm = static_cast<DownloadManagerImpl*>(
      BrowserContext::GetDownloadManager(web_contents->GetBrowserContext()));
  DownloadItem* item = dlm->GetDownload(download_id);
  if (!item)
    return;
  if (accept)
    item->ValidateDangerousDownload();
  else
    item->Remove();
}

DownloadControllerAndroidImpl::DownloadInfoAndroid::DownloadInfoAndroid(
    net::URLRequest* request)
    : has_user_gesture(false) {
  request->GetResponseHeaderByName("content-disposition", &content_disposition);

  if (request->response_headers())
    request->response_headers()->GetMimeType(&original_mime_type);

  request->extra_request_headers().GetHeader(
      net::HttpRequestHeaders::kUserAgent, &user_agent);
  if (user_agent.empty())
    user_agent = GetContentClient()->GetUserAgent();
  GURL referer_url(request->referrer());
  if (referer_url.is_valid())
    referer = referer_url.spec();
  if (!request->url_chain().empty()) {
    original_url = request->url_chain().front();
    url = request->url_chain().back();
  }

  const ResourceRequestInfo* info = ResourceRequestInfo::ForRequest(request);
  if (info)
    has_user_gesture = info->HasUserGesture();
}

DownloadControllerAndroidImpl::DownloadInfoAndroid::~DownloadInfoAndroid() {}

}  // namespace content
