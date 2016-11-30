// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/download/download_controller.h"

#include <memory>
#include <utility>

#include "base/android/context_utils.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/android/download/chrome_download_delegate.h"
#include "chrome/browser/android/download/dangerous_download_infobar_delegate.h"
#include "chrome/browser/android/download/download_manager_service.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/ui/android/view_android_helper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/download_url_parameters.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/common/referrer.h"
#include "jni/DownloadController_jni.h"
#include "net/base/filename_util.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
#include "ui/base/page_transition_types.h"

using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using content::BrowserContext;
using content::BrowserThread;
using content::ContextMenuParams;
using content::DownloadItem;
using content::DownloadManager;
using content::WebContents;

namespace {
// Guards download_controller_
base::LazyInstance<base::Lock> g_download_controller_lock_;

WebContents* GetWebContents(int render_process_id, int render_view_id) {
  content::RenderViewHost* render_view_host =
      content::RenderViewHost::FromID(render_process_id, render_view_id);

  if (!render_view_host)
    return nullptr;

  return WebContents::FromRenderViewHost(render_view_host);
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
  content::DownloadManager* dlm =
      content::BrowserContext::GetDownloadManager(
          web_contents->GetBrowserContext());
  std::unique_ptr<content::DownloadUrlParameters> dl_params(
      content::DownloadUrlParameters::CreateForWebContentsMainFrame(
          web_contents, url));
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

// Check if an interrupted download item can be auto resumed.
bool IsInterruptedDownloadAutoResumable(content::DownloadItem* download_item) {
  int interrupt_reason = download_item->GetLastReason();
  DCHECK_NE(interrupt_reason, content::DOWNLOAD_INTERRUPT_REASON_NONE);
  return
      interrupt_reason == content::DOWNLOAD_INTERRUPT_REASON_NETWORK_TIMEOUT ||
      interrupt_reason == content::DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED ||
      interrupt_reason ==
          content::DOWNLOAD_INTERRUPT_REASON_NETWORK_DISCONNECTED;
}

}  // namespace

// JNI methods
static void Init(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  DownloadController::GetInstance()->Init(env, obj);
}

static void OnRequestFileAccessResult(JNIEnv* env,
                                      const JavaParamRef<jobject>& obj,
                                      jlong callback_id,
                                      jboolean granted) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(callback_id);

  // Convert java long long int to c++ pointer, take ownership.
  std::unique_ptr<
      DownloadControllerBase::AcquireFileAccessPermissionCallback>
  cb(reinterpret_cast<
      DownloadControllerBase::AcquireFileAccessPermissionCallback*>(
      callback_id));
  if (!granted) {
    DownloadController::RecordDownloadCancelReason(
        DownloadController::CANCEL_REASON_NO_STORAGE_PERMISSION);
  }
  cb->Run(granted);
}

struct DownloadController::JavaObject {
  ScopedJavaLocalRef<jobject> Controller(JNIEnv* env) {
    return GetRealObject(env, obj_);
  }
  jweak obj_;
};

// static
bool DownloadController::RegisterDownloadController(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

// static
DownloadControllerBase* DownloadControllerBase::Get() {
  base::AutoLock lock(g_download_controller_lock_.Get());
  if (!DownloadControllerBase::download_controller_)
    download_controller_ = DownloadController::GetInstance();
  return DownloadControllerBase::download_controller_;
}

// static
void DownloadControllerBase::SetDownloadControllerBase(
    DownloadControllerBase* download_controller) {
  base::AutoLock lock(g_download_controller_lock_.Get());
  DownloadControllerBase::download_controller_ = download_controller;
}

// static
void DownloadController::RecordDownloadCancelReason(
    DownloadCancelReason reason) {
  UMA_HISTOGRAM_ENUMERATION(
      "MobileDownload.CancelReason", reason, CANCEL_REASON_MAX);
}

// static
DownloadController* DownloadController::GetInstance() {
  return base::Singleton<DownloadController>::get();
}

DownloadController::DownloadController()
    : java_object_(NULL) {
}

DownloadController::~DownloadController() {
  if (java_object_) {
    JNIEnv* env = base::android::AttachCurrentThread();
    env->DeleteWeakGlobalRef(java_object_->obj_);
    delete java_object_;
    base::android::CheckException(env);
  }
}

// Initialize references to Java object.
void DownloadController::Init(JNIEnv* env, jobject obj) {
  java_object_ = new JavaObject;
  java_object_->obj_ = env->NewWeakGlobalRef(obj);
}

void DownloadController::AcquireFileAccessPermission(
    WebContents* web_contents,
    const DownloadControllerBase::AcquireFileAccessPermissionCallback& cb) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(web_contents);

  ui::ViewAndroid* view_android =
      ViewAndroidHelper::FromWebContents(web_contents)->GetViewAndroid();
  if (!view_android) {
    // ViewAndroid may have been gone away.
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE, base::Bind(cb, false));
    return;
  }
  ui::WindowAndroid* window_android = view_android->GetWindowAndroid();
  if (window_android && HasFileAccessPermission(window_android)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE, base::Bind(cb, true));
    return;
  }
  // Make copy on the heap so we can pass the pointer through JNI.
  intptr_t callback_id = reinterpret_cast<intptr_t>(
      new DownloadControllerBase::AcquireFileAccessPermissionCallback(cb));
  ChromeDownloadDelegate::FromWebContents(web_contents)->
      RequestFileAccess(callback_id);
}

bool DownloadController::HasFileAccessPermission(
    ui::WindowAndroid* window_android) {
  ScopedJavaLocalRef<jobject> jwindow_android = window_android->GetJavaObject();

  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!jwindow_android.is_null());

  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_DownloadController_hasFileAccess(
      env, GetJavaObject()->Controller(env), jwindow_android);
}

void DownloadController::OnDownloadStarted(
    DownloadItem* download_item) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  WebContents* web_contents = download_item->GetWebContents();
  if (!web_contents)
    return;

  // Register for updates to the DownloadItem.
  download_item->AddObserver(this);

  ChromeDownloadDelegate* delegate =
      ChromeDownloadDelegate::FromWebContents(web_contents);
  if (delegate) {
    delegate->OnDownloadStarted(
        download_item->GetTargetFilePath().BaseName().value());
  }
  OnDownloadUpdated(download_item);
}

void DownloadController::OnDownloadUpdated(DownloadItem* item) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (item->IsDangerous() && (item->GetState() != DownloadItem::CANCELLED)) {
    // Dont't show notification for a dangerous download, as user can resume
    // the download after browser crash through notification.
    OnDangerousDownload(item);
    return;
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_item =
      DownloadManagerService::CreateJavaDownloadInfo(env, item);
  switch (item->GetState()) {
    case DownloadItem::IN_PROGRESS: {
      Java_DownloadController_onDownloadUpdated(
          env, GetJavaObject()->Controller(env), j_item);
      break;
    }
    case DownloadItem::COMPLETE:
      // Multiple OnDownloadUpdated() notifications may be issued while the
      // download is in the COMPLETE state. Only handle one.
      item->RemoveObserver(this);

      // Call onDownloadCompleted
      Java_DownloadController_onDownloadCompleted(
          env, GetJavaObject()->Controller(env), j_item);
      DownloadController::RecordDownloadCancelReason(
             DownloadController::CANCEL_REASON_NOT_CANCELED);
      break;
    case DownloadItem::CANCELLED:
      Java_DownloadController_onDownloadCancelled(
          env, GetJavaObject()->Controller(env), j_item);
      DownloadController::RecordDownloadCancelReason(
          DownloadController::CANCEL_REASON_OTHER_NATIVE_RESONS);
      break;
    case DownloadItem::INTERRUPTED:
      // When device loses/changes network, we get a NETWORK_TIMEOUT,
      // NETWORK_FAILED or NETWORK_DISCONNECTED error. Download should auto
      // resume in this case.
      Java_DownloadController_onDownloadInterrupted(
          env, GetJavaObject()->Controller(env), j_item,
          IsInterruptedDownloadAutoResumable(item));
      item->RemoveObserver(this);
      break;
    case DownloadItem::MAX_DOWNLOAD_STATE:
      NOTREACHED();
  }
}

void DownloadController::OnDangerousDownload(DownloadItem* item) {
  WebContents* web_contents = item->GetWebContents();
  if (!web_contents) {
    item->Remove();
    return;
  }

  DangerousDownloadInfoBarDelegate::Create(
        InfoBarService::FromWebContents(web_contents), item);
}

DownloadController::JavaObject*
    DownloadController::GetJavaObject() {
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

void DownloadController::StartContextMenuDownload(
    const ContextMenuParams& params, WebContents* web_contents, bool is_link,
    const std::string& extra_headers) {
  int process_id = web_contents->GetRenderProcessHost()->GetID();
  int routing_id = web_contents->GetRenderViewHost()->GetRoutingID();
  AcquireFileAccessPermission(
      web_contents, base::Bind(&CreateContextMenuDownload, process_id,
                               routing_id, params, is_link, extra_headers));
}

