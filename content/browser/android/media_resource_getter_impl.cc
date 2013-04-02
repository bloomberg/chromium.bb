// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/media_resource_getter_impl.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/path_service.h"
#include "base/threading/sequenced_worker_pool.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/fileapi/browser_file_system_helper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "googleurl/src/gurl.h"
#include "jni/MediaResourceGetter_jni.h"
#include "net/cookies/cookie_monster.h"
#include "net/cookies/cookie_store.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

namespace content {

static void ReturnResultOnUIThread(
    const base::Callback<void(const std::string&)>& callback,
    const std::string& result) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE, base::Bind(callback, result));
}

// Get the metadata from a media URL. When finished, a task is posted to the UI
// thread to run the callback function.
static void GetMediaMetadata(
    const std::string& url, const std::string& cookies,
    const media::MediaResourceGetter::ExtractMediaMetadataCB& callback) {
  JNIEnv* env = base::android::AttachCurrentThread();

  base::android::ScopedJavaLocalRef<jstring> j_url_string =
      base::android::ConvertUTF8ToJavaString(env, url);
  base::android::ScopedJavaLocalRef<jstring> j_cookies =
      base::android::ConvertUTF8ToJavaString(env, cookies);
  jobject j_context = base::android::GetApplicationContext();
  base::android::ScopedJavaLocalRef<jobject> j_metadata =
      Java_MediaResourceGetter_extractMediaMetadata(
          env, j_context, j_url_string.obj(), j_cookies.obj());
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(callback, base::TimeDelta::FromMilliseconds(
                     Java_MediaMetadata_getDurationInMilliseconds(
                         env, j_metadata.obj())),
                 Java_MediaMetadata_getWidth(env, j_metadata.obj()),
                 Java_MediaMetadata_getHeight(env, j_metadata.obj()),
                 Java_MediaMetadata_isSuccess(env, j_metadata.obj())));
}

// The task object that retrieves cookie on the IO thread.
// TODO(qinmin): refactor this class to make the code reusable by others as
// there are lots of duplicated functionalities elsewhere.
class CookieGetterTask
     : public base::RefCountedThreadSafe<CookieGetterTask> {
 public:
  CookieGetterTask(BrowserContext* browser_context,
                   int renderer_id, int routing_id);

  // Called by CookieGetterImpl to start getting cookies for a URL.
  void RequestCookies(
      const GURL& url, const GURL& first_party_for_cookies,
      const media::MediaResourceGetter::GetCookieCB& callback);

 private:
  friend class base::RefCountedThreadSafe<CookieGetterTask>;
  virtual ~CookieGetterTask();

  void CheckPolicyForCookies(
      const GURL& url, const GURL& first_party_for_cookies,
      const media::MediaResourceGetter::GetCookieCB& callback,
      const net::CookieList& cookie_list);

  // Context getter used to get the CookieStore.
  net::URLRequestContextGetter* context_getter_;

  // Resource context for checking cookie policies.
  ResourceContext* resource_context_;

  // Render process id, used to check whether the process can access cookies.
  int renderer_id_;

  // Routing id for the render view, used to check tab specific cookie policy.
  int routing_id_;

  DISALLOW_COPY_AND_ASSIGN(CookieGetterTask);
};

CookieGetterTask::CookieGetterTask(
    BrowserContext* browser_context, int renderer_id, int routing_id)
    : context_getter_(browser_context->GetRequestContext()),
      resource_context_(browser_context->GetResourceContext()),
      renderer_id_(renderer_id),
      routing_id_(routing_id) {
}

CookieGetterTask::~CookieGetterTask() {}

void CookieGetterTask::RequestCookies(
    const GURL& url, const GURL& first_party_for_cookies,
    const media::MediaResourceGetter::GetCookieCB& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();
  if (!policy->CanAccessCookiesForOrigin(renderer_id_, url)) {
    callback.Run(std::string());
    return;
  }

  net::CookieStore* cookie_store =
      context_getter_->GetURLRequestContext()->cookie_store();
  if (!cookie_store) {
    callback.Run(std::string());
    return;
  }

  net::CookieMonster* cookie_monster = cookie_store->GetCookieMonster();
  if (cookie_monster) {
    cookie_monster->GetAllCookiesForURLAsync(url, base::Bind(
        &CookieGetterTask::CheckPolicyForCookies, this,
        url, first_party_for_cookies, callback));
  } else {
    callback.Run(std::string());
  }
}

void CookieGetterTask::CheckPolicyForCookies(
    const GURL& url, const GURL& first_party_for_cookies,
    const media::MediaResourceGetter::GetCookieCB& callback,
    const net::CookieList& cookie_list) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (GetContentClient()->browser()->AllowGetCookie(
      url, first_party_for_cookies, cookie_list,
      resource_context_, renderer_id_, routing_id_)) {
    net::CookieStore* cookie_store =
        context_getter_->GetURLRequestContext()->cookie_store();
    cookie_store->GetCookiesWithOptionsAsync(
        url, net::CookieOptions(), callback);
  } else {
    callback.Run(std::string());
  }
}

// The task object that retrieves platform path on the FILE thread.
class PlatformPathGetterTask
    : public base::RefCountedThreadSafe<PlatformPathGetterTask> {
 public:
  PlatformPathGetterTask(fileapi::FileSystemContext* file_system_context,
                         int renderer_id);

  // Called by MediaResourceGetterImpl to get the platform path from a file
  // system URL.
  void RequestPlaformPath(
      const GURL& url,
      const media::MediaResourceGetter::GetPlatformPathCB& callback);

 private:
  friend class base::RefCountedThreadSafe<PlatformPathGetterTask>;
  virtual ~PlatformPathGetterTask();

  // File system context for getting the platform path.
  fileapi::FileSystemContext* file_system_context_;

  // Render process id, used to check whether the process can access the URL.
  int renderer_id_;

  DISALLOW_COPY_AND_ASSIGN(PlatformPathGetterTask);
};

PlatformPathGetterTask::PlatformPathGetterTask(
    fileapi::FileSystemContext* file_system_context, int renderer_id)
    : file_system_context_(file_system_context),
      renderer_id_(renderer_id) {
}

PlatformPathGetterTask::~PlatformPathGetterTask() {}

void PlatformPathGetterTask::RequestPlaformPath(
    const GURL& url,
    const media::MediaResourceGetter::GetPlatformPathCB& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  base::FilePath platform_path;
  SyncGetPlatformPath(file_system_context_,
                      renderer_id_,
                      url,
                      &platform_path);
  base::FilePath data_storage_path;
  PathService::Get(base::DIR_ANDROID_APP_DATA, &data_storage_path);
  if (data_storage_path.IsParent(platform_path))
    callback.Run(platform_path.value());
  else
    callback.Run(std::string());
}

MediaResourceGetterImpl::MediaResourceGetterImpl(
    BrowserContext* browser_context,
    fileapi::FileSystemContext* file_system_context,
    int renderer_id, int routing_id)
    : browser_context_(browser_context),
      file_system_context_(file_system_context),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_this_(this)),
      renderer_id_(renderer_id),
      routing_id_(routing_id) {
}

MediaResourceGetterImpl::~MediaResourceGetterImpl() {}

void MediaResourceGetterImpl::GetCookies(
    const GURL& url, const GURL& first_party_for_cookies,
    const GetCookieCB& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  scoped_refptr<CookieGetterTask> task = new CookieGetterTask(
      browser_context_, renderer_id_, routing_id_);

  GetCookieCB cb = base::Bind(&MediaResourceGetterImpl::GetCookiesCallback,
                              weak_this_.GetWeakPtr(), callback);
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&CookieGetterTask::RequestCookies,
                 task, url, first_party_for_cookies,
                 base::Bind(&ReturnResultOnUIThread, cb)));
}

void MediaResourceGetterImpl::GetCookiesCallback(
    const GetCookieCB& callback, const std::string& cookies) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  callback.Run(cookies);
}

void MediaResourceGetterImpl::GetPlatformPathFromFileSystemURL(
      const GURL& url, const GetPlatformPathCB& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  scoped_refptr<PlatformPathGetterTask> task = new PlatformPathGetterTask(
      file_system_context_, renderer_id_);

  GetPlatformPathCB cb = base::Bind(
      &MediaResourceGetterImpl::GetPlatformPathCallback,
      weak_this_.GetWeakPtr(), callback);
  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&PlatformPathGetterTask::RequestPlaformPath,
                 task, url,
                 base::Bind(&ReturnResultOnUIThread, cb)));
}

void MediaResourceGetterImpl::GetPlatformPathCallback(
    const GetPlatformPathCB& callback, const std::string& platform_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  callback.Run(platform_path);
}

void MediaResourceGetterImpl::ExtractMediaMetadata(
    const std::string& url, const std::string& cookies,
    const ExtractMediaMetadataCB& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  base::SequencedWorkerPool* pool = content::BrowserThread::GetBlockingPool();
  pool->PostWorkerTask(
      FROM_HERE, base::Bind(&GetMediaMetadata, url, cookies, callback));
}

// static
bool MediaResourceGetterImpl::RegisterMediaResourceGetter(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace content
