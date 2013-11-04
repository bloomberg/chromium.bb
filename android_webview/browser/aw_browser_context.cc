// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_browser_context.h"

#include "android_webview/browser/aw_form_database_service.h"
#include "android_webview/browser/aw_pref_store.h"
#include "android_webview/browser/aw_quota_manager_bridge.h"
#include "android_webview/browser/jni_dependency_factory.h"
#include "android_webview/browser/net/aw_url_request_context_getter.h"
#include "android_webview/browser/net/init_native_callback.h"
#include "base/android/path_utils.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/pref_service_builder.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/sequenced_worker_pool.h"
#include "components/autofill/core/common/autofill_pref_names.h"
#include "components/user_prefs/user_prefs.h"
#include "components/visitedlink/browser/visitedlink_master.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/cookie_store_factory.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "net/url_request/url_request_context.h"

using base::FilePath;
using content::BrowserThread;

namespace android_webview {

namespace {

// Shows notifications which correspond to PersistentPrefStore's reading errors.
void HandleReadError(PersistentPrefStore::PrefReadError error) {
}

class AwResourceContext : public content::ResourceContext {
 public:
  explicit AwResourceContext(net::URLRequestContextGetter* getter)
      : getter_(getter) {
    DCHECK(getter_);
  }
  virtual ~AwResourceContext() {}

  // content::ResourceContext implementation.
  virtual net::HostResolver* GetHostResolver() OVERRIDE {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    return getter_->GetURLRequestContext()->host_resolver();
  }
  virtual net::URLRequestContext* GetRequestContext() OVERRIDE {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    return getter_->GetURLRequestContext();
  }
  virtual bool AllowMicAccess(const GURL& origin) OVERRIDE {
    return false;
  }
  virtual bool AllowCameraAccess(const GURL& origin) OVERRIDE {
    return false;
  }

 private:
  net::URLRequestContextGetter* getter_;

  DISALLOW_COPY_AND_ASSIGN(AwResourceContext);
};

AwBrowserContext* g_browser_context = NULL;

void ImportLegacyCookieStore(const FilePath& cookie_store_path) {
  // We use the old cookie store to create the new cookie store only if the
  // new cookie store does not exist.
  if (base::PathExists(cookie_store_path))
    return;

  // WebViewClassic gets the database path from Context and appends a
  // hardcoded name. (see https://android.googlesource.com/platform/frameworks/base/+/bf6f6f9de72c9fd15e6bd/core/java/android/webkit/JniUtil.java and
  // https://android.googlesource.com/platform/external/webkit/+/7151ed0c74599/Source/WebKit/android/WebCoreSupport/WebCookieJar.cpp)
  FilePath old_cookie_store_path;
  base::android::GetDatabaseDirectory(&old_cookie_store_path);
  old_cookie_store_path = old_cookie_store_path.Append(
      FILE_PATH_LITERAL("webviewCookiesChromium.db"));
  if (base::PathExists(old_cookie_store_path) &&
      !base::Move(old_cookie_store_path, cookie_store_path)) {
    LOG(WARNING) << "Failed to move old cookie store path from "
                 << old_cookie_store_path.AsUTF8Unsafe() << " to "
                 << cookie_store_path.AsUTF8Unsafe();
  }
}

}  // namespace

AwBrowserContext::AwBrowserContext(
    const FilePath path,
    JniDependencyFactory* native_factory)
    : context_storage_path_(path),
      native_factory_(native_factory),
      user_pref_service_ready_(false) {
  DCHECK(g_browser_context == NULL);
  g_browser_context = this;

  // This constructor is entered during the creation of ContentBrowserClient,
  // before browser threads are created. Therefore any checks to enforce
  // threading (such as BrowserThread::CurrentlyOn()) will fail here.
}

AwBrowserContext::~AwBrowserContext() {
  DCHECK(g_browser_context == this);
  g_browser_context = NULL;
}

// static
AwBrowserContext* AwBrowserContext::GetDefault() {
  // TODO(joth): rather than store in a global here, lookup this instance
  // from the Java-side peer.
  return g_browser_context;
}

// static
AwBrowserContext* AwBrowserContext::FromWebContents(
    content::WebContents* web_contents) {
  // This is safe; this is the only implementation of the browser context.
  return static_cast<AwBrowserContext*>(web_contents->GetBrowserContext());
}

void AwBrowserContext::PreMainMessageLoopRun() {

  FilePath cookie_store_path = GetPath().Append(FILE_PATH_LITERAL("Cookies"));
  scoped_refptr<base::SequencedTaskRunner> background_task_runner =
      BrowserThread::GetBlockingPool()->GetSequencedTaskRunner(
          BrowserThread::GetBlockingPool()->GetSequenceToken());

  background_task_runner->PostTask(
      FROM_HERE,
      base::Bind(ImportLegacyCookieStore, cookie_store_path));

  cookie_store_ = content::CreatePersistentCookieStore(
      cookie_store_path,
      true,
      NULL,
      NULL,
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO),
      background_task_runner);

  cookie_store_->GetCookieMonster()->SetPersistSessionCookies(true);
  url_request_context_getter_ =
      new AwURLRequestContextGetter(GetPath(), cookie_store_.get());

  DidCreateCookieMonster(cookie_store_->GetCookieMonster());

  visitedlink_master_.reset(
      new visitedlink::VisitedLinkMaster(this, this, false));
  visitedlink_master_->Init();

  form_database_service_.reset(
      new AwFormDatabaseService(context_storage_path_));
}

void AwBrowserContext::AddVisitedURLs(const std::vector<GURL>& urls) {
  DCHECK(visitedlink_master_);
  visitedlink_master_->AddURLs(urls);
}

net::URLRequestContextGetter* AwBrowserContext::CreateRequestContext(
    content::ProtocolHandlerMap* protocol_handlers) {
  // This function cannot actually create the request context because
  // there is a reentrant dependency on GetResourceContext() via
  // content::StoragePartitionImplMap::Create(). This is not fixable
  // until http://crbug.com/159193. Until then, assert that the context
  // has already been allocated and just handle setting the protocol_handlers.
  DCHECK(url_request_context_getter_);
  url_request_context_getter_->SetProtocolHandlers(protocol_handlers);
  return url_request_context_getter_;
}

net::URLRequestContextGetter*
AwBrowserContext::CreateRequestContextForStoragePartition(
    const base::FilePath& partition_path,
    bool in_memory,
    content::ProtocolHandlerMap* protocol_handlers) {
  NOTREACHED();
  return NULL;
}

AwQuotaManagerBridge* AwBrowserContext::GetQuotaManagerBridge() {
  if (!quota_manager_bridge_.get()) {
    quota_manager_bridge_ = native_factory_->CreateAwQuotaManagerBridge(this);
  }
  return quota_manager_bridge_.get();
}

AwFormDatabaseService* AwBrowserContext::GetFormDatabaseService() {
  return form_database_service_.get();
}

// Create user pref service for autofill functionality.
void AwBrowserContext::CreateUserPrefServiceIfNecessary() {
  if (user_pref_service_ready_)
    return;

  user_pref_service_ready_ = true;
  PrefRegistrySimple* pref_registry = new PrefRegistrySimple();
  // We only use the autocomplete feature of the Autofill, which is
  // controlled via the manager_delegate. We don't use the rest
  // of autofill, which is why it is hardcoded as disabled here.
  pref_registry->RegisterBooleanPref(
      autofill::prefs::kAutofillEnabled, false);
  pref_registry->RegisterDoublePref(
      autofill::prefs::kAutofillPositiveUploadRate, 0.0);
  pref_registry->RegisterDoublePref(
      autofill::prefs::kAutofillNegativeUploadRate, 0.0);

  PrefServiceBuilder pref_service_builder;
  pref_service_builder.WithUserPrefs(new AwPrefStore());
  pref_service_builder.WithReadErrorCallback(base::Bind(&HandleReadError));

  user_prefs::UserPrefs::Set(this,
                             pref_service_builder.Create(pref_registry));
}

base::FilePath AwBrowserContext::GetPath() const {
  return context_storage_path_;
}

bool AwBrowserContext::IsOffTheRecord() const {
  // Android WebView does not support off the record profile yet.
  return false;
}

net::URLRequestContextGetter* AwBrowserContext::GetRequestContext() {
  return GetDefaultStoragePartition(this)->GetURLRequestContext();
}

net::URLRequestContextGetter*
AwBrowserContext::GetRequestContextForRenderProcess(
    int renderer_child_id) {
  return GetRequestContext();
}

net::URLRequestContextGetter* AwBrowserContext::GetMediaRequestContext() {
  return GetRequestContext();
}

void AwBrowserContext::RequestMIDISysExPermission(
      int render_process_id,
      int render_view_id,
      int bridge_id,
      const GURL& requesting_frame,
      const MIDISysExPermissionCallback& callback) {
  // TODO(toyoshim): Android is not supported yet.
  callback.Run(false);
}

void AwBrowserContext::CancelMIDISysExPermissionRequest(
    int render_process_id,
    int render_view_id,
    int bridge_id,
    const GURL& requesting_frame) {
}

net::URLRequestContextGetter*
AwBrowserContext::GetMediaRequestContextForRenderProcess(
    int renderer_child_id) {
  return GetRequestContext();
}

net::URLRequestContextGetter*
AwBrowserContext::GetMediaRequestContextForStoragePartition(
    const base::FilePath& partition_path,
    bool in_memory) {
  NOTREACHED();
  return NULL;
}

content::ResourceContext* AwBrowserContext::GetResourceContext() {
  if (!resource_context_) {
    resource_context_.reset(
        new AwResourceContext(url_request_context_getter_.get()));
  }
  return resource_context_.get();
}

content::DownloadManagerDelegate*
AwBrowserContext::GetDownloadManagerDelegate() {
  return &download_manager_delegate_;
}

content::GeolocationPermissionContext*
AwBrowserContext::GetGeolocationPermissionContext() {
  if (!geolocation_permission_context_.get()) {
    geolocation_permission_context_ =
        native_factory_->CreateGeolocationPermission(this);
  }
  return geolocation_permission_context_.get();
}

quota::SpecialStoragePolicy* AwBrowserContext::GetSpecialStoragePolicy() {
  // Intentionally returning NULL as 'Extensions' and 'Apps' not supported.
  return NULL;
}

void AwBrowserContext::RebuildTable(
    const scoped_refptr<URLEnumerator>& enumerator) {
  // Android WebView rebuilds from WebChromeClient.getVisitedHistory. The client
  // can change in the lifetime of this WebView and may not yet be set here.
  // Therefore this initialization path is not used.
  enumerator->OnComplete(true);
}

}  // namespace android_webview
