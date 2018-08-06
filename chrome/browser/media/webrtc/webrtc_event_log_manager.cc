// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/webrtc_event_log_manager.h"

#include "base/task/post_task.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"

namespace {

using BrowserContext = content::BrowserContext;
using BrowserThread = content::BrowserThread;
using RenderProcessHost = content::RenderProcessHost;

using BrowserContextId = WebRtcEventLogManager::BrowserContextId;

class PeerConnectionTrackerProxyImpl
    : public WebRtcEventLogManager::PeerConnectionTrackerProxy {
 public:
  ~PeerConnectionTrackerProxyImpl() override = default;

  void SetWebRtcEventLoggingState(const WebRtcEventLogPeerConnectionKey& key,
                                  bool event_logging_enabled) override {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::BindOnce(
            &PeerConnectionTrackerProxyImpl::SetWebRtcEventLoggingStateInternal,
            key, event_logging_enabled));
  }

 private:
  static void SetWebRtcEventLoggingStateInternal(
      WebRtcEventLogPeerConnectionKey key,
      bool event_logging_enabled) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    RenderProcessHost* host = RenderProcessHost::FromID(key.render_process_id);
    if (!host) {
      return;  // The host has been asynchronously removed; not a problem.
    }
    host->SetWebRtcEventLogOutput(key.lid, event_logging_enabled);
  }
};

const BrowserContext* GetBrowserContext(int render_process_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderProcessHost* const host = RenderProcessHost::FromID(render_process_id);
  return host ? host->GetBrowserContext() : nullptr;
}

// Post reply back if non-empty.
template <typename... Args>
inline void MaybeReply(const base::Location& location,
                       base::OnceCallback<void(Args...)> reply,
                       Args... args) {
  if (reply) {
    BrowserThread::PostTask(BrowserThread::UI, location,
                            base::BindOnce(std::move(reply), args...));
  }
}

}  // namespace

WebRtcEventLogManager* WebRtcEventLogManager::g_webrtc_event_log_manager =
    nullptr;

std::unique_ptr<WebRtcEventLogManager>
WebRtcEventLogManager::CreateSingletonInstance() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!g_webrtc_event_log_manager);
  g_webrtc_event_log_manager = new WebRtcEventLogManager;
  return base::WrapUnique<WebRtcEventLogManager>(g_webrtc_event_log_manager);
}

WebRtcEventLogManager* WebRtcEventLogManager::GetInstance() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return g_webrtc_event_log_manager;
}

WebRtcEventLogManager::WebRtcEventLogManager()
    : local_logs_observer_(nullptr),
      remote_logs_observer_(nullptr),
      local_logs_manager_(this),
      pc_tracker_proxy_(new PeerConnectionTrackerProxyImpl),
      first_browser_context_initializations_done_(false),
      task_runner_(base::CreateSequencedTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (IsRemoteLoggingEnabled()) {
    remote_logs_manager_ =
        std::make_unique<WebRtcRemoteEventLogManager>(this, task_runner_);
  }

  DCHECK(!g_webrtc_event_log_manager);
  g_webrtc_event_log_manager = this;
}

WebRtcEventLogManager::~WebRtcEventLogManager() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  for (RenderProcessHost* host : observed_render_process_hosts_) {
    host->RemoveObserver(this);
  }

  DCHECK(g_webrtc_event_log_manager);
  g_webrtc_event_log_manager = nullptr;
}

void WebRtcEventLogManager::EnableForBrowserContext(
    const BrowserContext* browser_context,
    base::OnceClosure reply) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(browser_context);
  CHECK(!browser_context->IsOffTheRecord());

  if (!first_browser_context_initializations_done_) {
    OnFirstBrowserContextLoaded();
    first_browser_context_initializations_done_ = true;
  }

  // |this| is destroyed by ~BrowserProcessImpl(), so base::Unretained(this)
  // will not be dereferenced after destruction.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WebRtcEventLogManager::EnableForBrowserContextInternal,
                     base::Unretained(this),
                     GetBrowserContextId(browser_context),
                     browser_context->GetPath(), std::move(reply)));
}

void WebRtcEventLogManager::DisableForBrowserContext(
    const content::BrowserContext* browser_context,
    base::OnceClosure reply) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(browser_context);

  // |this| is destroyed by ~BrowserProcessImpl(), so base::Unretained(this)
  // will not be dereferenced after destruction.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WebRtcEventLogManager::DisableForBrowserContextInternal,
                     base::Unretained(this),
                     GetBrowserContextId(browser_context), std::move(reply)));
}

void WebRtcEventLogManager::PeerConnectionAdded(
    int render_process_id,
    int lid,
    const std::string& peer_connection_id,
    base::OnceCallback<void(bool)> reply) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!peer_connection_id.empty());

  RenderProcessHost* rph = RenderProcessHost::FromID(render_process_id);
  if (!rph) {
    // RPH died before processing of this notification.
    MaybeReply(FROM_HERE, std::move(reply), false);
    return;
  }

  auto it = observed_render_process_hosts_.find(rph);
  if (it == observed_render_process_hosts_.end()) {
    // This is the first PeerConnection which we see that's associated
    // with this RPH.
    rph->AddObserver(this);
    observed_render_process_hosts_.insert(rph);
  }

  const auto browser_context_id = GetBrowserContextId(rph->GetBrowserContext());
  DCHECK_NE(browser_context_id, kNullBrowserContextId);

  // |this| is destroyed by ~BrowserProcessImpl(), so base::Unretained(this)
  // will not be dereferenced after destruction.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &WebRtcEventLogManager::PeerConnectionAddedInternal,
          base::Unretained(this),
          PeerConnectionKey(render_process_id, lid, browser_context_id),
          peer_connection_id, std::move(reply)));
}

void WebRtcEventLogManager::PeerConnectionRemoved(
    int render_process_id,
    int lid,
    base::OnceCallback<void(bool)> reply) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  const auto browser_context_id = GetBrowserContextId(render_process_id);
  if (browser_context_id == kNullBrowserContextId) {
    // RPH died before processing of this notification. This is handled by
    // RenderProcessExited() / RenderProcessHostDestroyed.
    MaybeReply(FROM_HERE, std::move(reply), false);
    return;
  }

  // |this| is destroyed by ~BrowserProcessImpl(), so base::Unretained(this)
  // will not be dereferenced after destruction.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &WebRtcEventLogManager::PeerConnectionRemovedInternal,
          base::Unretained(this),
          PeerConnectionKey(render_process_id, lid, browser_context_id),
          std::move(reply)));
}

void WebRtcEventLogManager::PeerConnectionStopped(
    int render_process_id,
    int lid,
    base::OnceCallback<void(bool)> reply) {
  return PeerConnectionRemoved(render_process_id, lid, std::move(reply));
}

void WebRtcEventLogManager::EnableLocalLogging(
    const base::FilePath& base_path,
    base::OnceCallback<void(bool)> reply) {
  EnableLocalLogging(base_path, kDefaultMaxLocalLogFileSizeBytes,
                     std::move(reply));
}

void WebRtcEventLogManager::EnableLocalLogging(
    const base::FilePath& base_path,
    size_t max_file_size_bytes,
    base::OnceCallback<void(bool)> reply) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!base_path.empty());
  // |this| is destroyed by ~BrowserProcessImpl(), so base::Unretained(this)
  // will not be dereferenced after destruction.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WebRtcEventLogManager::EnableLocalLoggingInternal,
                     base::Unretained(this), base_path, max_file_size_bytes,
                     std::move(reply)));
}

void WebRtcEventLogManager::DisableLocalLogging(
    base::OnceCallback<void(bool)> reply) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // |this| is destroyed by ~BrowserProcessImpl(), so base::Unretained(this)
  // will not be dereferenced after destruction.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WebRtcEventLogManager::DisableLocalLoggingInternal,
                     base::Unretained(this), std::move(reply)));
}

void WebRtcEventLogManager::OnWebRtcEventLogWrite(
    int render_process_id,
    int lid,
    const std::string& message,
    base::OnceCallback<void(std::pair<bool, bool>)> reply) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  const BrowserContext* browser_context = GetBrowserContext(render_process_id);
  if (!browser_context) {
    // RPH died before processing of this notification.
    MaybeReply(FROM_HERE, std::move(reply), std::make_pair(false, false));
    return;
  }

  const auto browser_context_id = GetBrowserContextId(browser_context);
  DCHECK_NE(browser_context_id, kNullBrowserContextId);

  const bool remote_logging_allowed = !browser_context->IsOffTheRecord();

  // |this| is destroyed by ~BrowserProcessImpl(), so base::Unretained(this)
  // will not be dereferenced after destruction.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &WebRtcEventLogManager::OnWebRtcEventLogWriteInternal,
          base::Unretained(this),
          PeerConnectionKey(render_process_id, lid, browser_context_id),
          remote_logging_allowed, message, std::move(reply)));
}

void WebRtcEventLogManager::StartRemoteLogging(
    int render_process_id,
    const std::string& peer_connection_id,
    size_t max_file_size_bytes,
    base::OnceCallback<void(bool, const std::string&, const std::string&)>
        reply) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(reply);

  const BrowserContext* browser_context = GetBrowserContext(render_process_id);
  const char* error = nullptr;

  if (!remote_logs_manager_) {
    error = kStartRemoteLoggingFailureFeatureDisabled;
  } else if (!browser_context) {
    // RPH died before processing of this notification.
    error = kStartRemoteLoggingFailureGeneric;
  } else if (browser_context->IsOffTheRecord()) {
    // Feature disable in incognito. Since the feature can be disabled for
    // non-incognito sessions, this should not expose incognito mode.
    error = kStartRemoteLoggingFailureFeatureDisabled;
  }

  if (error) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::BindOnce(std::move(reply), false, "", std::string(error)));
    return;
  }

  const auto browser_context_id = GetBrowserContextId(browser_context);
  DCHECK_NE(browser_context_id, kNullBrowserContextId);

  // |this| is destroyed by ~BrowserProcessImpl(), so base::Unretained(this)
  // will not be dereferenced after destruction.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WebRtcEventLogManager::StartRemoteLoggingInternal,
                     base::Unretained(this), render_process_id,
                     browser_context_id, peer_connection_id,
                     browser_context->GetPath(), max_file_size_bytes,
                     std::move(reply)));
}

void WebRtcEventLogManager::ClearCacheForBrowserContext(
    const BrowserContext* browser_context,
    const base::Time& delete_begin,
    const base::Time& delete_end,
    base::OnceClosure reply) {
  const auto browser_context_id = GetBrowserContextId(browser_context);
  DCHECK_NE(browser_context_id, kNullBrowserContextId);

  // The object outlives the task queue - base::Unretained(this) is safe.
  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(
          &WebRtcEventLogManager::ClearCacheForBrowserContextInternal,
          base::Unretained(this), browser_context_id, delete_begin, delete_end),
      std::move(reply));
}

void WebRtcEventLogManager::SetLocalLogsObserver(
    WebRtcLocalEventLogsObserver* observer,
    base::OnceClosure reply) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // |this| is destroyed by ~BrowserProcessImpl(), so base::Unretained(this)
  // will not be dereferenced after destruction.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WebRtcEventLogManager::SetLocalLogsObserverInternal,
                     base::Unretained(this), observer, std::move(reply)));
}

void WebRtcEventLogManager::SetRemoteLogsObserver(
    WebRtcRemoteEventLogsObserver* observer,
    base::OnceClosure reply) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // |this| is destroyed by ~BrowserProcessImpl(), so base::Unretained(this)
  // will not be dereferenced after destruction.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WebRtcEventLogManager::SetRemoteLogsObserverInternal,
                     base::Unretained(this), observer, std::move(reply)));
}

bool WebRtcEventLogManager::IsRemoteLoggingEnabled() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

#if defined(OS_ANDROID)
  bool enabled = false;
#else
  // TODO(crbug.com/775415): Even when the feature is generally enabled, it
  // should only be active for sessions where the appropriate policy is set.
  bool enabled = base::FeatureList::IsEnabled(features::kWebRtcRemoteEventLog);
#endif

  VLOG(1) << "WebRTC remote-bound event logging "
          << (enabled ? "enabled" : "disabled") << ".";

  return enabled;
}

std::unique_ptr<LogFileWriter::Factory>
WebRtcEventLogManager::CreateRemoteLogFileWriterFactory() {
#if defined(OS_ANDROID)
  NOTREACHED();
  return nullptr;  // Appease compiler.
#else
  if (remote_log_file_writer_factory_for_testing_) {
    return std::move(remote_log_file_writer_factory_for_testing_);
  } else if (base::FeatureList::IsEnabled(
                 features::kWebRtcRemoteEventLogGzipped)) {
    return std::make_unique<GzippedLogFileWriterFactory>(
        std::make_unique<GzipLogCompressorFactory>(
            std::make_unique<DefaultGzippedSizeEstimator::Factory>()));
  } else {
    return std::make_unique<BaseLogFileWriterFactory>();
  }
#endif
}

void WebRtcEventLogManager::RenderProcessExited(
    RenderProcessHost* host,
    const content::ChildProcessTerminationInfo& info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderProcessHostExitedDestroyed(host);
}

void WebRtcEventLogManager::RenderProcessHostDestroyed(
    RenderProcessHost* host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderProcessHostExitedDestroyed(host);
}

void WebRtcEventLogManager::RenderProcessHostExitedDestroyed(
    RenderProcessHost* host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(host);

  auto it = observed_render_process_hosts_.find(host);
  if (it == observed_render_process_hosts_.end()) {
    return;  // We've never seen PeerConnections associated with this RPH.
  }
  host->RemoveObserver(this);
  observed_render_process_hosts_.erase(host);

  // |this| is destroyed by ~BrowserProcessImpl(), so base::Unretained(this)
  // will not be dereferenced after destruction.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WebRtcEventLogManager::RenderProcessExitedInternal,
                     base::Unretained(this), host->GetID()));
}

void WebRtcEventLogManager::OnLocalLogStarted(PeerConnectionKey peer_connection,
                                              const base::FilePath& file_path) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  OnLoggingTargetStarted(LoggingTarget::kLocalLogging, peer_connection);

  if (local_logs_observer_) {
    local_logs_observer_->OnLocalLogStarted(peer_connection, file_path);
  }
}

void WebRtcEventLogManager::OnLocalLogStopped(
    PeerConnectionKey peer_connection) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  OnLoggingTargetStopped(LoggingTarget::kLocalLogging, peer_connection);

  if (local_logs_observer_) {
    local_logs_observer_->OnLocalLogStopped(peer_connection);
  }
}

void WebRtcEventLogManager::OnRemoteLogStarted(
    PeerConnectionKey key,
    const base::FilePath& file_path) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  OnLoggingTargetStarted(LoggingTarget::kRemoteLogging, key);
  if (remote_logs_observer_) {
    remote_logs_observer_->OnRemoteLogStarted(key, file_path);
  }
}

void WebRtcEventLogManager::OnRemoteLogStopped(
    WebRtcEventLogPeerConnectionKey key) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  OnLoggingTargetStopped(LoggingTarget::kRemoteLogging, key);
  if (remote_logs_observer_) {
    remote_logs_observer_->OnRemoteLogStopped(key);
  }
}

void WebRtcEventLogManager::OnLoggingTargetStarted(LoggingTarget target,
                                                   PeerConnectionKey key) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  auto it = peer_connections_with_event_logging_enabled_.find(key);
  if (it != peer_connections_with_event_logging_enabled_.end()) {
    DCHECK_EQ((it->second & target), 0u);
    it->second |= target;
  } else {
    // This is the first client for WebRTC event logging - let WebRTC know
    // that it should start informing us of events.
    peer_connections_with_event_logging_enabled_.emplace(key, target);
    pc_tracker_proxy_->SetWebRtcEventLoggingState(key, true);
  }
}

void WebRtcEventLogManager::OnLoggingTargetStopped(LoggingTarget target,
                                                   PeerConnectionKey key) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  // Record that we're no longer performing this type of logging for this PC.
  auto it = peer_connections_with_event_logging_enabled_.find(key);
  CHECK(it != peer_connections_with_event_logging_enabled_.end());
  DCHECK_NE(it->second, 0u);
  it->second &= ~target;

  // If we're not doing any other type of logging for this peer connection,
  // it's time to stop receiving notifications for it from WebRTC.
  if (it->second == 0u) {
    peer_connections_with_event_logging_enabled_.erase(it);
    pc_tracker_proxy_->SetWebRtcEventLoggingState(key, false);
  }
}

void WebRtcEventLogManager::OnFirstBrowserContextLoaded() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!remote_logs_manager_) {
    return;
  }

  content::NetworkConnectionTracker* network_connection_tracker =
      g_browser_process->network_connection_tracker();
  DCHECK(network_connection_tracker);

  net::URLRequestContextGetter* url_request_context_getter =
      g_browser_process->system_request_context();
  DCHECK(url_request_context_getter);

  auto log_file_writer_factory = CreateRemoteLogFileWriterFactory();
  DCHECK(log_file_writer_factory);

  // * |url_request_context_getter| is owned by IOThread. The internal task
  //   runner that uses it (|task_runner_|) stops before IOThread dies, so we
  //   can trust that |url_request_context_getter| will not be used after
  //   destruction.
  // * |network_connection_tracker| is owned by BrowserProcessImpl, which
  //   owns the IOThread, so the logic explaining why using base::Unretained
  //   was safe for with |url_request_context_getter|, also applies to it.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &WebRtcEventLogManager::OnFirstBrowserContextLoadedInternal,
          base::Unretained(this), base::Unretained(network_connection_tracker),
          base::Unretained(url_request_context_getter),
          std::move(log_file_writer_factory)));
}

void WebRtcEventLogManager::OnFirstBrowserContextLoadedInternal(
    content::NetworkConnectionTracker* network_connection_tracker,
    net::URLRequestContextGetter* url_request_context_getter,
    std::unique_ptr<LogFileWriter::Factory> log_file_writer_factory) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(network_connection_tracker);
  DCHECK(url_request_context_getter);
  DCHECK(log_file_writer_factory);
  DCHECK(remote_logs_manager_);
  remote_logs_manager_->SetNetworkConnectionTracker(network_connection_tracker);
  remote_logs_manager_->SetUrlRequestContextGetter(url_request_context_getter);
  remote_logs_manager_->SetLogFileWriterFactory(
      std::move(log_file_writer_factory));
}

void WebRtcEventLogManager::EnableForBrowserContextInternal(
    BrowserContextId browser_context_id,
    const base::FilePath& browser_context_dir,
    base::OnceClosure reply) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK_NE(browser_context_id, kNullBrowserContextId);

  if (remote_logs_manager_) {
    remote_logs_manager_->EnableForBrowserContext(browser_context_id,
                                                  browser_context_dir);
  }

  MaybeReply(FROM_HERE, std::move(reply));
}

void WebRtcEventLogManager::DisableForBrowserContextInternal(
    BrowserContextId browser_context_id,
    base::OnceClosure reply) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (remote_logs_manager_) {
    remote_logs_manager_->DisableForBrowserContext(browser_context_id);
  }

  MaybeReply(FROM_HERE, std::move(reply));
}

void WebRtcEventLogManager::PeerConnectionAddedInternal(
    PeerConnectionKey key,
    const std::string& peer_connection_id,
    base::OnceCallback<void(bool)> reply) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  const bool local_result = local_logs_manager_.PeerConnectionAdded(key);

  if (remote_logs_manager_) {
    const bool remote_result =
        remote_logs_manager_->PeerConnectionAdded(key, peer_connection_id);
    DCHECK_EQ(local_result, remote_result);
  }

  MaybeReply(FROM_HERE, std::move(reply), local_result);
}

void WebRtcEventLogManager::PeerConnectionRemovedInternal(
    PeerConnectionKey key,
    base::OnceCallback<void(bool)> reply) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  const bool local_result = local_logs_manager_.PeerConnectionRemoved(key);
  if (remote_logs_manager_) {
    const bool remote_result = remote_logs_manager_->PeerConnectionRemoved(key);
    DCHECK_EQ(local_result, remote_result);
  }

  MaybeReply(FROM_HERE, std::move(reply), local_result);
}

void WebRtcEventLogManager::EnableLocalLoggingInternal(
    const base::FilePath& base_path,
    size_t max_file_size_bytes,
    base::OnceCallback<void(bool)> reply) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  const bool result =
      local_logs_manager_.EnableLogging(base_path, max_file_size_bytes);

  MaybeReply(FROM_HERE, std::move(reply), result);
}

void WebRtcEventLogManager::DisableLocalLoggingInternal(
    base::OnceCallback<void(bool)> reply) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  const bool result = local_logs_manager_.DisableLogging();

  MaybeReply(FROM_HERE, std::move(reply), result);
}

void WebRtcEventLogManager::OnWebRtcEventLogWriteInternal(
    PeerConnectionKey key,
    bool remote_logging_allowed,
    const std::string& message,
    base::OnceCallback<void(std::pair<bool, bool>)> reply) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  const bool local_result = local_logs_manager_.EventLogWrite(key, message);
  const bool remote_result =
      (remote_logging_allowed && remote_logs_manager_)
          ? remote_logs_manager_->EventLogWrite(key, message)
          : false;

  MaybeReply(FROM_HERE, std::move(reply),
             std::make_pair(local_result, remote_result));
}

void WebRtcEventLogManager::StartRemoteLoggingInternal(
    int render_process_id,
    BrowserContextId browser_context_id,
    const std::string& peer_connection_id,
    const base::FilePath& browser_context_dir,
    size_t max_file_size_bytes,
    base::OnceCallback<void(bool, const std::string&, const std::string&)>
        reply) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  std::string log_id;
  std::string error_message;
  const bool result = remote_logs_manager_->StartRemoteLogging(
      render_process_id, browser_context_id, peer_connection_id,
      browser_context_dir, max_file_size_bytes, &log_id, &error_message);

  // |log_id| set only if successful; |error_message| set only if unsuccessful.
  DCHECK_EQ(result, !log_id.empty());
  DCHECK_EQ(!result, !error_message.empty());

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(std::move(reply), result, log_id, error_message));
}

void WebRtcEventLogManager::ClearCacheForBrowserContextInternal(
    BrowserContextId browser_context_id,
    const base::Time& delete_begin,
    const base::Time& delete_end) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (remote_logs_manager_) {
    remote_logs_manager_->ClearCacheForBrowserContext(browser_context_id,
                                                      delete_begin, delete_end);
  }
}

void WebRtcEventLogManager::RenderProcessExitedInternal(int render_process_id) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  local_logs_manager_.RenderProcessHostExitedDestroyed(render_process_id);
  if (remote_logs_manager_) {
    remote_logs_manager_->RenderProcessHostExitedDestroyed(render_process_id);
  }
}

void WebRtcEventLogManager::SetLocalLogsObserverInternal(
    WebRtcLocalEventLogsObserver* observer,
    base::OnceClosure reply) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  local_logs_observer_ = observer;

  if (reply) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, std::move(reply));
  }
}

void WebRtcEventLogManager::SetRemoteLogsObserverInternal(
    WebRtcRemoteEventLogsObserver* observer,
    base::OnceClosure reply) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  remote_logs_observer_ = observer;

  if (reply) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, std::move(reply));
  }
}

void WebRtcEventLogManager::SetClockForTesting(base::Clock* clock,
                                               base::OnceClosure reply) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(reply);

  auto task = [](WebRtcEventLogManager* manager, base::Clock* clock,
                 base::OnceClosure reply) {
    manager->local_logs_manager_.SetClockForTesting(clock);

    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, std::move(reply));
  };

  // |this| is destroyed by ~BrowserProcessImpl(), so base::Unretained(this)
  // will not be dereferenced after destruction.
  task_runner_->PostTask(FROM_HERE, base::BindOnce(task, base::Unretained(this),
                                                   clock, std::move(reply)));
}

void WebRtcEventLogManager::SetPeerConnectionTrackerProxyForTesting(
    std::unique_ptr<PeerConnectionTrackerProxy> pc_tracker_proxy,
    base::OnceClosure reply) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(reply);

  auto task = [](WebRtcEventLogManager* manager,
                 std::unique_ptr<PeerConnectionTrackerProxy> pc_tracker_proxy,
                 base::OnceClosure reply) {
    manager->pc_tracker_proxy_ = std::move(pc_tracker_proxy);

    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, std::move(reply));
  };

  // |this| is destroyed by ~BrowserProcessImpl(), so base::Unretained(this)
  // will not be dereferenced after destruction.
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(task, base::Unretained(this),
                                std::move(pc_tracker_proxy), std::move(reply)));
}

void WebRtcEventLogManager::SetWebRtcEventLogUploaderFactoryForTesting(
    std::unique_ptr<WebRtcEventLogUploader::Factory> uploader_factory,
    base::OnceClosure reply) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(remote_logs_manager_);  // The test would otherwise be meaningless.
  DCHECK(reply);

  auto task =
      [](WebRtcEventLogManager* manager,
         std::unique_ptr<WebRtcEventLogUploader::Factory> uploader_factory,
         base::OnceClosure reply) {
        auto* remote_logs_manager = manager->remote_logs_manager_.get();
        remote_logs_manager->SetWebRtcEventLogUploaderFactoryForTesting(
            std::move(uploader_factory));

        BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, std::move(reply));
      };

  // |this| is destroyed by ~BrowserProcessImpl(), so base::Unretained(this)
  // will not be dereferenced after destruction.
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(task, base::Unretained(this),
                                std::move(uploader_factory), std::move(reply)));
}

void WebRtcEventLogManager::SetRemoteLogFileWriterFactoryForTesting(
    std::unique_ptr<LogFileWriter::Factory> factory) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!first_browser_context_initializations_done_) << "Too late.";
  DCHECK(!remote_log_file_writer_factory_for_testing_) << "Already called.";
  remote_log_file_writer_factory_for_testing_ = std::move(factory);
}

void WebRtcEventLogManager::UploadConditionsHoldForTesting(
    base::OnceCallback<void(bool)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(remote_logs_manager_);
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &WebRtcRemoteEventLogManager::UploadConditionsHoldForTesting,
          base::Unretained(remote_logs_manager_.get()), std::move(callback)));
}

scoped_refptr<base::SequencedTaskRunner>&
WebRtcEventLogManager::GetTaskRunnerForTesting() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return task_runner_;
}

void WebRtcEventLogManager::PostNullTaskForTesting(base::OnceClosure reply) {
  task_runner_->PostTask(FROM_HERE, std::move(reply));
}
