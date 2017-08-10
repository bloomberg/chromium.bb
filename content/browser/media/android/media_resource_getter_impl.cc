// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/android/media_resource_getter_impl.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/task_scheduler/post_task.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/fileapi/browser_file_system_helper.h"
#include "content/browser/resource_context_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_client.h"
#include "content/public/common/url_constants.h"
#include "jni/MediaResourceGetter_jni.h"
#include "media/base/android/media_url_interceptor.h"
#include "net/base/auth.h"
#include "net/cookies/cookie_store.h"
#include "net/http/http_auth.h"
#include "net/http/http_transaction_factory.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "url/gurl.h"

using base::android::ConvertUTF8ToJavaString;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace content {

static void ReturnResultOnUIThread(
    base::OnceCallback<void(const std::string&)> callback,
    const std::string& result) {
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::BindOnce(std::move(callback), result));
}

static void RequestPlaformPathFromFileSystemURL(
    const GURL& url,
    int render_process_id,
    scoped_refptr<storage::FileSystemContext> file_system_context,
    media::MediaResourceGetter::GetPlatformPathCB callback) {
  DCHECK(file_system_context->default_file_task_runner()
             ->RunsTasksInCurrentSequence());
  base::FilePath platform_path;
  SyncGetPlatformPath(file_system_context.get(),
                      render_process_id,
                      url,
                      &platform_path);
  base::FilePath data_storage_path;
  PathService::Get(base::DIR_ANDROID_APP_DATA, &data_storage_path);
  if (data_storage_path.IsParent(platform_path))
    ReturnResultOnUIThread(std::move(callback), platform_path.value());
  else
    ReturnResultOnUIThread(std::move(callback), std::string());
}

// Posts a task to the UI thread to run the callback function.
static void PostMediaMetadataCallbackTask(
    media::MediaResourceGetter::ExtractMediaMetadataCB callback,
    JNIEnv* env,
    ScopedJavaLocalRef<jobject>& j_metadata) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(
          std::move(callback),
          base::TimeDelta::FromMilliseconds(
              Java_MediaMetadata_getDurationInMilliseconds(env, j_metadata)),
          Java_MediaMetadata_getWidth(env, j_metadata),
          Java_MediaMetadata_getHeight(env, j_metadata),
          Java_MediaMetadata_isSuccess(env, j_metadata)));
}

// Gets the metadata from a media URL. When finished, a task is posted to the UI
// thread to run the callback function.
static void GetMediaMetadata(
    const std::string& url,
    const std::string& cookies,
    const std::string& user_agent,
    media::MediaResourceGetter::ExtractMediaMetadataCB callback) {
  JNIEnv* env = base::android::AttachCurrentThread();

  ScopedJavaLocalRef<jstring> j_url_string = ConvertUTF8ToJavaString(env, url);
  ScopedJavaLocalRef<jstring> j_cookies = ConvertUTF8ToJavaString(env, cookies);
  ScopedJavaLocalRef<jstring> j_user_agent = ConvertUTF8ToJavaString(
      env, user_agent);
  ScopedJavaLocalRef<jobject> j_metadata =
      Java_MediaResourceGetter_extractMediaMetadata(env, j_url_string,
                                                    j_cookies, j_user_agent);
  PostMediaMetadataCallbackTask(std::move(callback), env, j_metadata);
}

// Gets the metadata from a file descriptor. When finished, a task is posted to
// the UI thread to run the callback function.
static void GetMediaMetadataFromFd(
    const int fd,
    const int64_t offset,
    const int64_t size,
    media::MediaResourceGetter::ExtractMediaMetadataCB callback) {
  JNIEnv* env = base::android::AttachCurrentThread();

  ScopedJavaLocalRef<jobject> j_metadata =
      Java_MediaResourceGetter_extractMediaMetadataFromFd(
          env, fd, offset, size);

  PostMediaMetadataCallbackTask(std::move(callback), env, j_metadata);
}

// The task object that retrieves media resources on the IO thread.
// TODO(qinmin): refactor this class to make the code reusable by others as
// there are lots of duplicated functionalities elsewhere.
// http://crbug.com/395762.
class MediaResourceGetterTask
     : public base::RefCountedThreadSafe<MediaResourceGetterTask> {
 public:
  MediaResourceGetterTask(BrowserContext* browser_context,
                          int render_process_id, int render_frame_id);

  // Called by MediaResourceGetterImpl to start getting auth credentials.
  net::AuthCredentials RequestAuthCredentials(const GURL& url) const;

  // Called by MediaResourceGetterImpl to start getting cookies for a URL.
  void RequestCookies(const GURL& url,
                      const GURL& site_for_cookies,
                      media::MediaResourceGetter::GetCookieCB callback);

  // Returns the task runner that all methods should be called.
  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner() const;

 private:
  friend class base::RefCountedThreadSafe<MediaResourceGetterTask>;
  virtual ~MediaResourceGetterTask();

  void CheckPolicyForCookies(const GURL& url,
                             const GURL& site_for_cookies,
                             media::MediaResourceGetter::GetCookieCB callback,
                             const net::CookieList& cookie_list);

  // Context getter used to get the CookieStore and auth cache.
  net::URLRequestContextGetter* context_getter_;

  // Resource context for checking cookie policies.
  ResourceContext* resource_context_;

  // Render process id, used to check whether the process can access cookies.
  int render_process_id_;

  // Render frame id, used to check tab specific cookie policy.
  int render_frame_id_;

  DISALLOW_COPY_AND_ASSIGN(MediaResourceGetterTask);
};

MediaResourceGetterTask::MediaResourceGetterTask(
    BrowserContext* browser_context, int render_process_id, int render_frame_id)
    : context_getter_(BrowserContext::GetDefaultStoragePartition(
          browser_context)->GetURLRequestContext()),
      resource_context_(browser_context->GetResourceContext()),
      render_process_id_(render_process_id),
      render_frame_id_(render_frame_id) {
}

MediaResourceGetterTask::~MediaResourceGetterTask() {}

net::AuthCredentials MediaResourceGetterTask::RequestAuthCredentials(
    const GURL& url) const {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  net::HttpTransactionFactory* factory =
      context_getter_->GetURLRequestContext()->http_transaction_factory();
  if (!factory)
    return net::AuthCredentials();

  net::HttpAuthCache* auth_cache =
      factory->GetSession()->http_auth_cache();
  if (!auth_cache)
    return net::AuthCredentials();

  net::HttpAuthCache::Entry* entry =
      auth_cache->LookupByPath(url.GetOrigin(), url.path());

  // TODO(qinmin): handle other auth schemes. See http://crbug.com/395219.
  if (entry && entry->scheme() == net::HttpAuth::AUTH_SCHEME_BASIC)
    return entry->credentials();
  else
    return net::AuthCredentials();
}

void MediaResourceGetterTask::RequestCookies(
    const GURL& url,
    const GURL& site_for_cookies,
    media::MediaResourceGetter::GetCookieCB callback) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();
  if (!policy->CanAccessDataForOrigin(render_process_id_, url)) {
    std::move(callback).Run(std::string());
    return;
  }

  net::CookieStore* cookie_store =
      context_getter_->GetURLRequestContext()->cookie_store();
  if (!cookie_store) {
    std::move(callback).Run(std::string());
    return;
  }

  cookie_store->GetAllCookiesForURLAsync(
      url, base::BindOnce(&MediaResourceGetterTask::CheckPolicyForCookies, this,
                          url, site_for_cookies, std::move(callback)));
}

scoped_refptr<base::SingleThreadTaskRunner>
MediaResourceGetterTask::GetTaskRunner() const {
  return context_getter_->GetNetworkTaskRunner();
}

void MediaResourceGetterTask::CheckPolicyForCookies(
    const GURL& url,
    const GURL& site_for_cookies,
    media::MediaResourceGetter::GetCookieCB callback,
    const net::CookieList& cookie_list) {
  if (GetContentClient()->browser()->AllowGetCookie(
          url, site_for_cookies, cookie_list, resource_context_,
          render_process_id_, render_frame_id_)) {
    net::CookieStore* cookie_store =
        context_getter_->GetURLRequestContext()->cookie_store();
    net::CookieOptions options;
    options.set_include_httponly();
    cookie_store->GetCookiesWithOptionsAsync(url, options, std::move(callback));
  } else {
    std::move(callback).Run(std::string());
  }
}

MediaResourceGetterImpl::MediaResourceGetterImpl(
    BrowserContext* browser_context,
    storage::FileSystemContext* file_system_context,
    int render_process_id,
    int render_frame_id)
    : browser_context_(browser_context),
      file_system_context_(file_system_context),
      render_process_id_(render_process_id),
      render_frame_id_(render_frame_id),
      weak_factory_(this) {
}

MediaResourceGetterImpl::~MediaResourceGetterImpl() {}

void MediaResourceGetterImpl::GetAuthCredentials(
    const GURL& url,
    GetAuthCredentialsCB callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  scoped_refptr<MediaResourceGetterTask> task = new MediaResourceGetterTask(
      browser_context_, 0, 0);

  PostTaskAndReplyWithResult(
      task->GetTaskRunner().get(), FROM_HERE,
      base::BindOnce(&MediaResourceGetterTask::RequestAuthCredentials, task,
                     url),
      base::BindOnce(&MediaResourceGetterImpl::GetAuthCredentialsCallback,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void MediaResourceGetterImpl::GetCookies(const GURL& url,
                                         const GURL& site_for_cookies,
                                         GetCookieCB callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  scoped_refptr<MediaResourceGetterTask> task = new MediaResourceGetterTask(
      browser_context_, render_process_id_, render_frame_id_);

  GetCookieCB cb =
      base::BindOnce(&MediaResourceGetterImpl::GetCookiesCallback,
                     weak_factory_.GetWeakPtr(), std::move(callback));
  task->GetTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&MediaResourceGetterTask::RequestCookies, task, url,
                     site_for_cookies,
                     base::BindOnce(&ReturnResultOnUIThread, std::move(cb))));
}

void MediaResourceGetterImpl::GetAuthCredentialsCallback(
    GetAuthCredentialsCB callback,
    const net::AuthCredentials& credentials) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::move(callback).Run(credentials.username(), credentials.password());
}

void MediaResourceGetterImpl::GetCookiesCallback(GetCookieCB callback,
                                                 const std::string& cookies) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::move(callback).Run(cookies);
}

void MediaResourceGetterImpl::GetPlatformPathFromURL(
    const GURL& url,
    GetPlatformPathCB callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(url.SchemeIsFileSystem());

  GetPlatformPathCB cb =
      base::BindOnce(&MediaResourceGetterImpl::GetPlatformPathCallback,
                     weak_factory_.GetWeakPtr(), std::move(callback));

  scoped_refptr<storage::FileSystemContext> context(file_system_context_);
  context->default_file_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&RequestPlaformPathFromFileSystemURL, url,
                                render_process_id_, context, std::move(cb)));
}

void MediaResourceGetterImpl::GetPlatformPathCallback(
    GetPlatformPathCB callback,
    const std::string& platform_path) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::move(callback).Run(platform_path);
}

void MediaResourceGetterImpl::ExtractMediaMetadata(
    const std::string& url,
    const std::string& cookies,
    const std::string& user_agent,
    ExtractMediaMetadataCB callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::PostTaskWithTraits(FROM_HERE,
                           {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
                           base::BindOnce(&GetMediaMetadata, url, cookies,
                                          user_agent, std::move(callback)));
}

void MediaResourceGetterImpl::ExtractMediaMetadata(
    const int fd,
    const int64_t offset,
    const int64_t size,
    ExtractMediaMetadataCB callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::PostTaskWithTraits(FROM_HERE,
                           {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
                           base::BindOnce(&GetMediaMetadataFromFd, fd, offset,
                                          size, std::move(callback)));
}

}  // namespace content
