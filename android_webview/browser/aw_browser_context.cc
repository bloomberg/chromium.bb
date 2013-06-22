// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_browser_context.h"

#include "android_webview/browser/aw_form_database_service.h"
#include "android_webview/browser/aw_pref_store.h"
#include "android_webview/browser/aw_quota_manager_bridge.h"
#include "android_webview/browser/jni_dependency_factory.h"
#include "android_webview/browser/net/aw_url_request_context_getter.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/pref_service_builder.h"
#include "components/autofill/core/common/autofill_pref_names.h"
#include "components/user_prefs/user_prefs.h"
#include "components/visitedlink/browser/visitedlink_master.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "net/url_request/url_request_context.h"

namespace android_webview {

namespace {

// Shows notifications which correspond to PersistentPrefStore's reading errors.
void HandleReadError(PersistentPrefStore::PrefReadError error) {
}

class AwResourceContext : public content::ResourceContext {
 public:
  explicit AwResourceContext(net::URLRequestContextGetter* getter)
      : getter_(getter) {}
  virtual ~AwResourceContext() {}

  // content::ResourceContext implementation.
  virtual net::HostResolver* GetHostResolver() OVERRIDE {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
    return getter_->GetURLRequestContext()->host_resolver();
  }
  virtual net::URLRequestContext* GetRequestContext() OVERRIDE {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
    return getter_->GetURLRequestContext();
  }

 private:
  net::URLRequestContextGetter* getter_;

  DISALLOW_COPY_AND_ASSIGN(AwResourceContext);
};

AwBrowserContext* g_browser_context = NULL;

}  // namespace

AwBrowserContext::AwBrowserContext(
    const base::FilePath path,
    JniDependencyFactory* native_factory)
    : context_storage_path_(path),
      native_factory_(native_factory),
      user_pref_service_ready_(false) {
  DCHECK(g_browser_context == NULL);
  g_browser_context = this;
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

void AwBrowserContext::InitializeBeforeThreadCreation() {
  DCHECK(!url_request_context_getter_.get());
  url_request_context_getter_ = new AwURLRequestContextGetter(this);
}

void AwBrowserContext::PreMainMessageLoopRun() {
  visitedlink_master_.reset(
      new visitedlink::VisitedLinkMaster(this, this, false));
  visitedlink_master_->Init();
}

void AwBrowserContext::AddVisitedURLs(const std::vector<GURL>& urls) {
  DCHECK(visitedlink_master_);
  visitedlink_master_->AddURLs(urls);
}

net::URLRequestContextGetter* AwBrowserContext::CreateRequestContext(
    content::ProtocolHandlerMap* protocol_handlers) {
  CHECK(url_request_context_getter_.get());
  url_request_context_getter_->SetProtocolHandlers(protocol_handlers);
  return url_request_context_getter_.get();
}

net::URLRequestContextGetter*
AwBrowserContext::CreateRequestContextForStoragePartition(
    const base::FilePath& partition_path,
    bool in_memory,
    content::ProtocolHandlerMap* protocol_handlers) {
  CHECK(url_request_context_getter_.get());
  return url_request_context_getter_.get();
}

AwQuotaManagerBridge* AwBrowserContext::GetQuotaManagerBridge() {
  if (!quota_manager_bridge_) {
    quota_manager_bridge_.reset(
        native_factory_->CreateAwQuotaManagerBridge(this));
  }
  return quota_manager_bridge_.get();
}

AwFormDatabaseService* AwBrowserContext::GetFormDatabaseService() {
  if (!form_database_service_) {
    form_database_service_.reset(
        new AwFormDatabaseService(context_storage_path_));
  }
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

base::FilePath AwBrowserContext::GetPath() {
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

net::URLRequestContextGetter*
AwBrowserContext::GetMediaRequestContextForRenderProcess(
    int renderer_child_id) {
  return GetRequestContext();
}

net::URLRequestContextGetter*
AwBrowserContext::GetMediaRequestContextForStoragePartition(
    const base::FilePath& partition_path,
    bool in_memory) {
  return GetRequestContext();
}

content::ResourceContext* AwBrowserContext::GetResourceContext() {
  if (!resource_context_) {
    CHECK(url_request_context_getter_.get());
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

content::SpeechRecognitionPreferences*
AwBrowserContext::GetSpeechRecognitionPreferences() {
  // By default allows profanities in speech recognition if return NULL.
  return NULL;
}

quota::SpecialStoragePolicy* AwBrowserContext::GetSpecialStoragePolicy() {
  // TODO(boliu): Implement this so we are not relying on default behavior.
  NOTIMPLEMENTED();
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
