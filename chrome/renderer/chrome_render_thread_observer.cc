// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/chrome_render_thread_observer.h"

#include <stddef.h>

#include <limits>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram.h"
#include "base/metrics/statistics_recorder.h"
#include "base/path_service.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/common/cache_stats_recorder.mojom.h"
#include "chrome/common/child_process_logging.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/media/media_resource_provider.h"
#include "chrome/common/net/net_resource_provider.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/resource_usage_reporter.mojom.h"
#include "chrome/common/resource_usage_reporter_type_converters.h"
#include "chrome/common/url_constants.h"
#include "chrome/renderer/content_settings_observer.h"
#include "chrome/renderer/security_filter_peer.h"
#include "components/visitedlink/renderer/visitedlink_slave.h"
#include "content/public/child/child_thread.h"
#include "content/public/child/resource_dispatcher_delegate.h"
#include "content/public/common/associated_interface_registry.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/common/simple_connection_filter.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "content/public/renderer/render_view_visitor.h"
#include "extensions/features/features.h"
#include "ipc/ipc_sync_channel.h"
#include "media/base/localized_strings.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "net/base/net_errors.h"
#include "net/base/net_module.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/connector.h"
#include "third_party/WebKit/public/platform/WebCache.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebSecurityPolicy.h"
#include "third_party/WebKit/public/web/WebView.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/renderer/extensions/extension_localization_peer.h"
#endif

using blink::WebCache;
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

  std::unique_ptr<content::RequestPeer> OnRequestComplete(
      std::unique_ptr<content::RequestPeer> current_peer,
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
      return current_peer;
    }

    // Resource canceled with a specific error are filtered.
    return SecurityFilterPeer::CreateSecurityFilterPeerForDeniedRequest(
        resource_type, std::move(current_peer), error_code);
  }

  std::unique_ptr<content::RequestPeer> OnReceivedResponse(
      std::unique_ptr<content::RequestPeer> current_peer,
      const std::string& mime_type,
      const GURL& url) override {
#if BUILDFLAG(ENABLE_EXTENSIONS)
    return ExtensionLocalizationPeer::CreateExtensionLocalizationPeer(
        std::move(current_peer), RenderThread::Get(), mime_type, url);
#else
    return current_peer;
#endif
  }

 private:
  void InformHostOfCacheStats() {
    WebCache::UsageStats stats;
    WebCache::GetUsageStats(&stats);
    if (!cache_stats_recorder_) {
      RenderThread::Get()->GetChannel()->GetRemoteAssociatedInterface(
          &cache_stats_recorder_);
    }
    cache_stats_recorder_->RecordCacheStats(stats.capacity, stats.size);
  }

  chrome::mojom::CacheStatsRecorderAssociatedPtr cache_stats_recorder_;

  base::WeakPtrFactory<RendererResourceDelegate> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(RendererResourceDelegate);
};

static const int kWaitForWorkersStatsTimeoutMS = 20;

class ResourceUsageReporterImpl : public chrome::mojom::ResourceUsageReporter {
 public:
  explicit ResourceUsageReporterImpl(
      base::WeakPtr<ChromeRenderThreadObserver> observer)
      : workers_to_go_(0), observer_(observer), weak_factory_(this) {}
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
      callback_.Run(std::move(usage_data_));
    callback_.Reset();
    weak_factory_.InvalidateWeakPtrs();
    workers_to_go_ = 0;
  }

  void GetUsageData(const GetUsageDataCallback& callback) override {
    DCHECK(callback_.is_null());
    weak_factory_.InvalidateWeakPtrs();
    usage_data_ = chrome::mojom::ResourceUsageData::New();
    usage_data_->reports_v8_stats = true;
    callback_ = callback;

    // Since it is not safe to call any Blink or V8 functions until Blink has
    // been initialized (which also initializes V8), early out and send 0 back
    // for all resources.
    if (!observer_) {
      SendResults();
      return;
    }

    WebCache::ResourceTypeStats stats;
    WebCache::GetResourceTypeStats(&stats);
    usage_data_->web_cache_stats =
        chrome::mojom::ResourceTypeStats::From(stats);

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

  chrome::mojom::ResourceUsageDataPtr usage_data_;
  GetUsageDataCallback callback_;
  int workers_to_go_;
  base::WeakPtr<ChromeRenderThreadObserver> observer_;

  base::WeakPtrFactory<ResourceUsageReporterImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ResourceUsageReporterImpl);
};

void CreateResourceUsageReporter(
    base::WeakPtr<ChromeRenderThreadObserver> observer,
    chrome::mojom::ResourceUsageReporterRequest request) {
  mojo::MakeStrongBinding(base::MakeUnique<ResourceUsageReporterImpl>(observer),
                          std::move(request));
}

}  // namespace

bool ChromeRenderThreadObserver::is_incognito_process_ = false;

ChromeRenderThreadObserver::ChromeRenderThreadObserver()
    : visited_link_slave_(new visitedlink::VisitedLinkSlave),
      weak_factory_(this) {
  RenderThread* thread = RenderThread::Get();
  resource_delegate_.reset(new RendererResourceDelegate());
  thread->SetResourceDispatcherDelegate(resource_delegate_.get());

  // Configure modules that need access to resources.
  net::NetModule::SetResourceProvider(chrome_common_net::NetResourceProvider);
  media::SetLocalizedStringProvider(
      chrome_common_media::LocalizedStringProvider);

  // chrome-native: is a scheme used for placeholder navigations that allow
  // UIs to be drawn with platform native widgets instead of HTML.  These pages
  // should not be accessible.  No code should be runnable in these pages,
  // so it should not need to access anything nor should it allow javascript
  // URLs since it should never be visible to the user.
  // See also ChromeContentClient::AddAdditionalSchemes that adds it as an
  // empty document scheme.
  WebString native_scheme(WebString::FromASCII(chrome::kChromeNativeScheme));
  WebSecurityPolicy::RegisterURLSchemeAsDisplayIsolated(native_scheme);
  WebSecurityPolicy::RegisterURLSchemeAsNotAllowingJavascriptURLs(
      native_scheme);

  auto registry = base::MakeUnique<service_manager::BinderRegistry>();
  registry->AddInterface(
      base::Bind(CreateResourceUsageReporter, weak_factory_.GetWeakPtr()),
      base::ThreadTaskRunnerHandle::Get());
  registry->AddInterface(visited_link_slave_->GetBindCallback(),
                         base::ThreadTaskRunnerHandle::Get());
  if (content::ChildThread::Get()) {
    content::ChildThread::Get()
        ->GetServiceManagerConnection()
        ->AddConnectionFilter(base::MakeUnique<content::SimpleConnectionFilter>(
            std::move(registry)));
  }
}

ChromeRenderThreadObserver::~ChromeRenderThreadObserver() {}

void ChromeRenderThreadObserver::RegisterMojoInterfaces(
    content::AssociatedInterfaceRegistry* associated_interfaces) {
  associated_interfaces->AddInterface(base::Bind(
      &ChromeRenderThreadObserver::OnRendererConfigurationAssociatedRequest,
      base::Unretained(this)));
}

void ChromeRenderThreadObserver::UnregisterMojoInterfaces(
    content::AssociatedInterfaceRegistry* associated_interfaces) {
  associated_interfaces->RemoveInterface(
      chrome::mojom::RendererConfiguration::Name_);
}

void ChromeRenderThreadObserver::SetInitialConfiguration(
    bool is_incognito_process) {
  is_incognito_process_ = is_incognito_process;
}

void ChromeRenderThreadObserver::SetContentSettingRules(
    const RendererContentSettingRules& rules) {
  content_setting_rules_ = rules;
}

void ChromeRenderThreadObserver::SetFieldTrialGroup(
    const std::string& trial_name,
    const std::string& group_name) {
  RenderThread::Get()->SetFieldTrialGroup(trial_name, group_name);
}

void ChromeRenderThreadObserver::OnRendererConfigurationAssociatedRequest(
    chrome::mojom::RendererConfigurationAssociatedRequest request) {
  renderer_configuration_bindings_.AddBinding(this, std::move(request));
}

const RendererContentSettingRules*
ChromeRenderThreadObserver::content_setting_rules() const {
  return &content_setting_rules_;
}
