// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/chrome_render_process_observer.h"

#include <limits>
#include <vector>

#include "base/allocator/allocator_extension.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/metrics/statistics_recorder.h"
#include "base/path_service.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/platform_thread.h"
#include "chrome/common/child_process_logging.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/net/net_resource_provider.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/resource_usage_reporter.mojom.h"
#include "chrome/common/resource_usage_reporter_type_converters.h"
#include "chrome/common/url_constants.h"
#include "chrome/renderer/content_settings_observer.h"
#include "chrome/renderer/security_filter_peer.h"
#include "components/variations/variations_util.h"
#include "content/public/child/resource_dispatcher_delegate.h"
#include "content/public/common/service_registry.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "content/public/renderer/render_view_visitor.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "net/base/net_errors.h"
#include "net/base/net_module.h"
#include "third_party/WebKit/public/web/WebCache.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebRuntimeFeatures.h"
#include "third_party/WebKit/public/web/WebSecurityPolicy.h"
#include "third_party/WebKit/public/web/WebView.h"

#if defined(ENABLE_EXTENSIONS)
#include "chrome/renderer/extensions/extension_localization_peer.h"
#endif

#if !defined(OS_IOS)
#include "chrome/common/media/media_resource_provider.h"
#include "media/base/media_resources.h"
#endif

using blink::WebCache;
using blink::WebRuntimeFeatures;
using blink::WebSecurityPolicy;
using blink::WebString;
using content::RenderThread;

namespace {

const int kCacheStatsDelayMS = 2000;

class RendererResourceDelegate : public content::ResourceDispatcherDelegate {
 public:
  RendererResourceDelegate()
      : weak_factory_(this) {
  }

  content::RequestPeer* OnRequestComplete(content::RequestPeer* current_peer,
                                          content::ResourceType resource_type,
                                          int error_code) override {
    // Update the browser about our cache.
    // Rate limit informing the host of our cache stats.
    if (!weak_factory_.HasWeakPtrs()) {
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE,
          base::Bind(&RendererResourceDelegate::InformHostOfCacheStats,
                     weak_factory_.GetWeakPtr()),
          base::TimeDelta::FromMilliseconds(kCacheStatsDelayMS));
    }

    if (error_code == net::ERR_ABORTED) {
      return NULL;
    }

    // Resource canceled with a specific error are filtered.
    return SecurityFilterPeer::CreateSecurityFilterPeerForDeniedRequest(
        resource_type, current_peer, error_code);
  }

  content::RequestPeer* OnReceivedResponse(content::RequestPeer* current_peer,
                                           const std::string& mime_type,
                                           const GURL& url) override {
#if defined(ENABLE_EXTENSIONS)
    return ExtensionLocalizationPeer::CreateExtensionLocalizationPeer(
        current_peer, RenderThread::Get(), mime_type, url);
#else
    return NULL;
#endif
  }

 private:
  void InformHostOfCacheStats() {
    WebCache::UsageStats stats;
    WebCache::getUsageStats(&stats);
    RenderThread::Get()->Send(new ChromeViewHostMsg_UpdatedCacheStats(stats));
  }

  base::WeakPtrFactory<RendererResourceDelegate> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(RendererResourceDelegate);
};

static const int kWaitForWorkersStatsTimeoutMS = 20;

class ResourceUsageReporterImpl : public ResourceUsageReporter {
 public:
  ResourceUsageReporterImpl(
      base::WeakPtr<ChromeRenderProcessObserver> observer,
      mojo::InterfaceRequest<ResourceUsageReporter> req)
      : binding_(this, req.Pass()), observer_(observer), weak_factory_(this) {}
  ~ResourceUsageReporterImpl() override {}

 private:
  static void CollectOnWorkerThread(
      const scoped_refptr<base::TaskRunner>& master,
      base::WeakPtr<ResourceUsageReporterImpl> impl) {
    size_t total_bytes = 0;
    size_t used_bytes = 0;
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    if (isolate) {
      v8::HeapStatistics heap_stats;
      isolate->GetHeapStatistics(&heap_stats);
      total_bytes = heap_stats.total_heap_size();
      used_bytes = heap_stats.used_heap_size();
    }
    master->PostTask(FROM_HERE,
                     base::Bind(&ResourceUsageReporterImpl::ReceiveStats, impl,
                                total_bytes, used_bytes));
  }

  void ReceiveStats(size_t total_bytes, size_t used_bytes) {
    usage_data_->v8_bytes_allocated += total_bytes;
    usage_data_->v8_bytes_used += used_bytes;
    workers_to_go_--;
    if (!workers_to_go_)
      SendResults();
  }

  void SendResults() {
    if (!callback_.is_null())
      callback_.Run(usage_data_.Pass());
    callback_.reset();
    weak_factory_.InvalidateWeakPtrs();
    workers_to_go_ = 0;
  }

  void GetUsageData(
      const mojo::Callback<void(ResourceUsageDataPtr)>& callback) override {
    DCHECK(callback_.is_null());
    weak_factory_.InvalidateWeakPtrs();
    usage_data_ = ResourceUsageData::New();
    usage_data_->reports_v8_stats = true;
    callback_ = callback;

    // Since it is not safe to call any Blink or V8 functions until Blink has
    // been initialized (which also initializes V8), early out and send 0 back
    // for all resources.
    if (!observer_ || !observer_->webkit_initialized()) {
      SendResults();
      return;
    }

    WebCache::ResourceTypeStats stats;
    WebCache::getResourceTypeStats(&stats);
    usage_data_->web_cache_stats = ResourceTypeStats::From(stats);

    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    if (isolate) {
      v8::HeapStatistics heap_stats;
      isolate->GetHeapStatistics(&heap_stats);
      usage_data_->v8_bytes_allocated = heap_stats.total_heap_size();
      usage_data_->v8_bytes_used = heap_stats.used_heap_size();
    }
    base::Closure collect = base::Bind(
        &ResourceUsageReporterImpl::CollectOnWorkerThread,
        base::ThreadTaskRunnerHandle::Get(), weak_factory_.GetWeakPtr());
    workers_to_go_ = RenderThread::Get()->PostTaskToAllWebWorkers(collect);
    if (workers_to_go_) {
      // The guard task to send out partial stats
      // in case some workers are not responsive.
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE, base::Bind(&ResourceUsageReporterImpl::SendResults,
                                weak_factory_.GetWeakPtr()),
          base::TimeDelta::FromMilliseconds(kWaitForWorkersStatsTimeoutMS));
    } else {
      // No worker threads so just send out the main thread data right away.
      SendResults();
    }
  }

  ResourceUsageDataPtr usage_data_;
  mojo::Callback<void(ResourceUsageDataPtr)> callback_;
  int workers_to_go_;
  mojo::StrongBinding<ResourceUsageReporter> binding_;
  base::WeakPtr<ChromeRenderProcessObserver> observer_;

  base::WeakPtrFactory<ResourceUsageReporterImpl> weak_factory_;
};

void CreateResourceUsageReporter(
    base::WeakPtr<ChromeRenderProcessObserver> observer,
    mojo::InterfaceRequest<ResourceUsageReporter> request) {
  new ResourceUsageReporterImpl(observer, request.Pass());
}

}  // namespace

bool ChromeRenderProcessObserver::is_incognito_process_ = false;

ChromeRenderProcessObserver::ChromeRenderProcessObserver()
    : webkit_initialized_(false), weak_factory_(this) {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

#if defined(ENABLE_AUTOFILL_DIALOG)
  WebRuntimeFeatures::enableRequestAutocomplete(true);
#endif

  if (command_line.HasSwitch(switches::kDisableJavaScriptHarmonyShipping)) {
    std::string flag("--noharmony-shipping");
    v8::V8::SetFlagsFromString(flag.c_str(), static_cast<int>(flag.size()));
  }

  if (command_line.HasSwitch(switches::kJavaScriptHarmony)) {
    std::string flag("--harmony");
    v8::V8::SetFlagsFromString(flag.c_str(), static_cast<int>(flag.size()));
  }

  RenderThread* thread = RenderThread::Get();
  resource_delegate_.reset(new RendererResourceDelegate());
  thread->SetResourceDispatcherDelegate(resource_delegate_.get());

  content::ServiceRegistry* service_registry = thread->GetServiceRegistry();
  if (service_registry) {
    service_registry->AddService<ResourceUsageReporter>(
        base::Bind(CreateResourceUsageReporter, weak_factory_.GetWeakPtr()));
  }

  // Configure modules that need access to resources.
  net::NetModule::SetResourceProvider(chrome_common_net::NetResourceProvider);
#if !defined(OS_IOS)
  media::SetLocalizedStringProvider(
      chrome_common_media::LocalizedStringProvider);
#endif

  // Setup initial set of crash dump data for Field Trials in this renderer.
  variations::SetVariationListCrashKeys();
  // Listen for field trial activations to report them to the browser.
  base::FieldTrialList::AddObserver(this);
}

ChromeRenderProcessObserver::~ChromeRenderProcessObserver() {
}

bool ChromeRenderProcessObserver::OnControlMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ChromeRenderProcessObserver, message)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SetIsIncognitoProcess,
                        OnSetIsIncognitoProcess)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SetFieldTrialGroup, OnSetFieldTrialGroup)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SetContentSettingRules,
                        OnSetContentSettingRules)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ChromeRenderProcessObserver::WebKitInitialized() {
  webkit_initialized_ = true;
  // chrome-native: is a scheme used for placeholder navigations that allow
  // UIs to be drawn with platform native widgets instead of HTML.  These pages
  // should not be accessible, and should also be treated as empty documents
  // that can commit synchronously.  No code should be runnable in these pages,
  // so it should not need to access anything nor should it allow javascript
  // URLs since it should never be visible to the user.
  WebString native_scheme(base::ASCIIToUTF16(chrome::kChromeNativeScheme));
  WebSecurityPolicy::registerURLSchemeAsDisplayIsolated(native_scheme);
  WebSecurityPolicy::registerURLSchemeAsEmptyDocument(native_scheme);
  WebSecurityPolicy::registerURLSchemeAsNoAccess(native_scheme);
  WebSecurityPolicy::registerURLSchemeAsNotAllowingJavascriptURLs(
      native_scheme);
}

void ChromeRenderProcessObserver::OnRenderProcessShutdown() {
  webkit_initialized_ = false;
}

void ChromeRenderProcessObserver::OnSetIsIncognitoProcess(
    bool is_incognito_process) {
  is_incognito_process_ = is_incognito_process;
}

void ChromeRenderProcessObserver::OnSetContentSettingRules(
    const RendererContentSettingRules& rules) {
  content_setting_rules_ = rules;
}

void ChromeRenderProcessObserver::OnSetFieldTrialGroup(
    const std::string& field_trial_name,
    const std::string& group_name) {
  base::FieldTrial* trial =
      base::FieldTrialList::CreateFieldTrial(field_trial_name, group_name);
  // TODO(mef): Remove this check after the investigation of 359406 is complete.
  CHECK(trial) << field_trial_name << ":" << group_name;
  // Ensure the trial is marked as "used" by calling group() on it if it is
  // marked as activated.
  trial->group();
  variations::SetVariationListCrashKeys();
}

const RendererContentSettingRules*
ChromeRenderProcessObserver::content_setting_rules() const {
  return &content_setting_rules_;
}

void ChromeRenderProcessObserver::OnFieldTrialGroupFinalized(
    const std::string& trial_name,
    const std::string& group_name) {
  content::RenderThread::Get()->Send(
      new ChromeViewHostMsg_FieldTrialActivated(trial_name));
}
