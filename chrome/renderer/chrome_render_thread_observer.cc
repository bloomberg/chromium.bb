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

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/metrics/statistics_recorder.h"
#include "base/path_service.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
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
#include "components/variations/variations_util.h"
#include "content/public/child/resource_dispatcher_delegate.h"
#include "content/public/common/service_registry.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "content/public/renderer/render_view_visitor.h"
#include "media/base/media_resources.h"
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
#if defined(ENABLE_EXTENSIONS)
    return ExtensionLocalizationPeer::CreateExtensionLocalizationPeer(
        std::move(current_peer), RenderThread::Get(), mime_type, url);
#else
    return current_peer;
#endif
  }

 private:
  void InformHostOfCacheStats() {
    WebCache::UsageStats stats;
    WebCache::getUsageStats(&stats);
    RenderThread::Get()->Send(new ChromeViewHostMsg_UpdatedCacheStats(
        static_cast<uint64_t>(stats.minDeadCapacity),
        static_cast<uint64_t>(stats.maxDeadCapacity),
        static_cast<uint64_t>(stats.capacity),
        static_cast<uint64_t>(stats.liveSize),
        static_cast<uint64_t>(stats.deadSize)));
  }

  base::WeakPtrFactory<RendererResourceDelegate> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(RendererResourceDelegate);
};

static const int kWaitForWorkersStatsTimeoutMS = 20;

class ResourceUsageReporterImpl : public mojom::ResourceUsageReporter {
 public:
  ResourceUsageReporterImpl(
      base::WeakPtr<ChromeRenderThreadObserver> observer,
      mojo::InterfaceRequest<mojom::ResourceUsageReporter> req)
      : workers_to_go_(0),
        binding_(this, std::move(req)),
        observer_(observer),
        weak_factory_(this) {}
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
    callback_.reset();
    weak_factory_.InvalidateWeakPtrs();
    workers_to_go_ = 0;
  }

  void GetUsageData(const mojo::Callback<void(mojom::ResourceUsageDataPtr)>&
                        callback) override {
    DCHECK(callback_.is_null());
    weak_factory_.InvalidateWeakPtrs();
    usage_data_ = mojom::ResourceUsageData::New();
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
    WebCache::getResourceTypeStats(&stats);
    usage_data_->web_cache_stats = mojom::ResourceTypeStats::From(stats);

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

  mojom::ResourceUsageDataPtr usage_data_;
  mojo::Callback<void(mojom::ResourceUsageDataPtr)> callback_;
  int workers_to_go_;
  mojo::StrongBinding<mojom::ResourceUsageReporter> binding_;
  base::WeakPtr<ChromeRenderThreadObserver> observer_;

  base::WeakPtrFactory<ResourceUsageReporterImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ResourceUsageReporterImpl);
};

void CreateResourceUsageReporter(
    base::WeakPtr<ChromeRenderThreadObserver> observer,
    mojo::InterfaceRequest<mojom::ResourceUsageReporter> request) {
  new ResourceUsageReporterImpl(observer, std::move(request));
}

}  // namespace

bool ChromeRenderThreadObserver::is_incognito_process_ = false;

ChromeRenderThreadObserver::ChromeRenderThreadObserver()
    : weak_factory_(this) {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

#if defined(ENABLE_AUTOFILL_DIALOG)
  WebRuntimeFeatures::enableRequestAutocomplete(true);
#endif

  RenderThread* thread = RenderThread::Get();
  resource_delegate_.reset(new RendererResourceDelegate());
  thread->SetResourceDispatcherDelegate(resource_delegate_.get());

  thread->GetServiceRegistry()->AddService(
      base::Bind(CreateResourceUsageReporter, weak_factory_.GetWeakPtr()));

  // Configure modules that need access to resources.
  net::NetModule::SetResourceProvider(chrome_common_net::NetResourceProvider);
  media::SetLocalizedStringProvider(
      chrome_common_media::LocalizedStringProvider);

  InitFieldTrialObserving(command_line);

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

ChromeRenderThreadObserver::~ChromeRenderThreadObserver() {}

void ChromeRenderThreadObserver::InitFieldTrialObserving(
    const base::CommandLine& command_line) {
  // Set up initial set of crash dump data for field trials in this renderer.
  variations::SetVariationListCrashKeys();

  // Listen for field trial activations to report them to the browser.
  base::FieldTrialList::AddObserver(this);

  // Some field trials may have been activated before this point. Notify the
  // browser of these activations now. To detect these, take the set difference
  // of currently active trials with the initially active trials.
  base::FieldTrial::ActiveGroups initially_active_trials;
  base::FieldTrialList::GetActiveFieldTrialGroupsFromString(
      command_line.GetSwitchValueASCII(switches::kForceFieldTrials),
      &initially_active_trials);
  std::set<std::string> initially_active_trials_set;
  for (const auto& entry : initially_active_trials) {
    initially_active_trials_set.insert(std::move(entry.trial_name));
  }

  base::FieldTrial::ActiveGroups current_active_trials;
  base::FieldTrialList::GetActiveFieldTrialGroups(&current_active_trials);
  for (const auto& trial : current_active_trials) {
    if (!ContainsKey(initially_active_trials_set, trial.trial_name))
      OnFieldTrialGroupFinalized(trial.trial_name, trial.group_name);
  }
}

bool ChromeRenderThreadObserver::OnControlMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ChromeRenderThreadObserver, message)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SetIsIncognitoProcess,
                        OnSetIsIncognitoProcess)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SetFieldTrialGroup, OnSetFieldTrialGroup)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SetContentSettingRules,
                        OnSetContentSettingRules)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ChromeRenderThreadObserver::OnSetIsIncognitoProcess(
    bool is_incognito_process) {
  is_incognito_process_ = is_incognito_process;
}

void ChromeRenderThreadObserver::OnSetContentSettingRules(
    const RendererContentSettingRules& rules) {
  content_setting_rules_ = rules;
}

void ChromeRenderThreadObserver::OnSetFieldTrialGroup(
    const std::string& field_trial_name,
    const std::string& group_name,
    base::ProcessId sender_pid,
    int32_t debug_token) {
  // Check that the sender's PID doesn't change between messages. We expect
  // these IPCs to always be delivered from the same browser process, whose pid
  // should not change.
  // TODO(asvitkine): Remove this after http://crbug.com/359406 is fixed.
  static base::ProcessId sender_pid_cached = sender_pid;
  CHECK_EQ(sender_pid_cached, sender_pid) << sender_pid_cached << "/"
                                          << sender_pid;
  // Check that the sender's debug_token doesn't change between messages.
  // TODO(asvitkine): Remove this after http://crbug.com/359406 is fixed.
  static int32_t debug_token_cached = debug_token;
  CHECK_EQ(debug_token_cached, debug_token) << debug_token_cached << "/"
                                            << debug_token;
  // The debug token should also correspond to what was specified on the
  // --force-fieldtrials command line for DebugToken trial.
  const std::string debug_token_trial_value =
      base::FieldTrialList::FindFullName("DebugToken");
  if (!debug_token_trial_value.empty()) {
    CHECK_EQ(base::IntToString(debug_token), debug_token_trial_value)
        << debug_token << "/" << debug_token_trial_value;
  }

  base::FieldTrial* trial =
      base::FieldTrialList::CreateFieldTrial(field_trial_name, group_name);
  // TODO(asvitkine): Remove this after http://crbug.com/359406 is fixed.
  if (!trial) {
    // Log the --force-fieldtrials= switch value for debugging purposes. Take
    // its substring starting with the trial name, since otherwise the end of
    // it can get truncated in the dump.
    std::string switch_substr = base::CommandLine::ForCurrentProcess()->
        GetSwitchValueASCII(switches::kForceFieldTrials);
    size_t index = switch_substr.find(field_trial_name);
    if (index != std::string::npos) {
      // If possible, log the string one char before the trial name, as there
      // may be a leading * to indicate it should be activated.
      switch_substr = switch_substr.substr(index > 0 ? index - 1 : index);
    }
    CHECK(trial) << field_trial_name << ":" << group_name << "=>"
                 << base::FieldTrialList::FindFullName(field_trial_name)
                 << " ] " << switch_substr;
  }
  // Ensure the trial is marked as "used" by calling group() on it if it is
  // marked as activated.
  trial->group();
  variations::SetVariationListCrashKeys();
}

const RendererContentSettingRules*
ChromeRenderThreadObserver::content_setting_rules() const {
  return &content_setting_rules_;
}

void ChromeRenderThreadObserver::OnFieldTrialGroupFinalized(
    const std::string& trial_name,
    const std::string& group_name) {
  content::RenderThread::Get()->Send(
      new ChromeViewHostMsg_FieldTrialActivated(trial_name));
}
