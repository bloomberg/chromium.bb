// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webrtc/webrtc_event_log_manager.h"

#include "base/task_scheduler/post_task.h"
#include "content/common/media/peer_connection_tracker_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"

namespace content {

namespace {

using BrowserContextId = WebRtcEventLogManager::BrowserContextId;

class PeerConnectionTrackerProxyImpl
    : public WebRtcEventLogManager::PeerConnectionTrackerProxy {
 public:
  ~PeerConnectionTrackerProxyImpl() override = default;

  void SetWebRtcEventLoggingState(WebRtcEventLogPeerConnectionKey key,
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
    if (event_logging_enabled) {
      host->Send(new PeerConnectionTracker_StartEventLogOutput(key.lid));
    } else {
      host->Send(new PeerConnectionTracker_StopEventLog(key.lid));
    }
  }
};

const BrowserContext* GetBrowserContext(int render_process_id) {
  // Since this function is only allowed to be called from the UI thread,
  // it is also reasonable to demand that it only be called with a valid
  // render_process_id (since those only become invalidated on the UI thread).
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderProcessHost* host = RenderProcessHost::FromID(render_process_id);
  CHECK(host);
  return host->GetBrowserContext();
}

}  // namespace

const size_t kWebRtcEventLogManagerUnlimitedFileSize = 0;

WebRtcEventLogManager* WebRtcEventLogManager::g_webrtc_event_log_manager =
    nullptr;

BrowserContextId WebRtcEventLogManager::GetBrowserContextId(
    const BrowserContext* browser_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return reinterpret_cast<BrowserContextId>(browser_context);
}

WebRtcEventLogManager* WebRtcEventLogManager::CreateSingletonInstance() {
  DCHECK(!g_webrtc_event_log_manager);
  g_webrtc_event_log_manager = new WebRtcEventLogManager;
  return g_webrtc_event_log_manager;
}

WebRtcEventLogManager* WebRtcEventLogManager::GetInstance() {
  return g_webrtc_event_log_manager;
}

WebRtcEventLogManager::WebRtcEventLogManager()
    : WebRtcEventLogManager(base::CreateSequencedTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::BACKGROUND,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})) {}

WebRtcEventLogManager::WebRtcEventLogManager(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : local_logs_observer_(nullptr),
      remote_logs_observer_(nullptr),
      local_logs_manager_(this),
      remote_logs_manager_(this),
      pc_tracker_proxy_(new PeerConnectionTrackerProxyImpl),
      task_runner_(task_runner) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
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
  CHECK(!browser_context->IsOffTheRecord());
  // Posting to task queue owned by the unretained object - unretained is safe.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WebRtcEventLogManager::EnableForBrowserContextInternal,
                     base::Unretained(this),
                     GetBrowserContextId(browser_context),
                     browser_context->GetPath(), std::move(reply)));
}

void WebRtcEventLogManager::DisableForBrowserContext(
    BrowserContextId browser_context_id,
    base::OnceClosure reply) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Posting to task queue owned by the unretained object - unretained is safe.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WebRtcEventLogManager::DisableForBrowserContextInternal,
                     base::Unretained(this), browser_context_id,
                     std::move(reply)));
}

void WebRtcEventLogManager::PeerConnectionAdded(
    int render_process_id,
    int lid,
    base::OnceCallback<void(bool)> reply) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  RenderProcessHost* host = RenderProcessHost::FromID(render_process_id);
  auto it = observed_render_process_hosts_.find(host);
  if (it == observed_render_process_hosts_.end()) {
    // This is the first PeerConnection which we see that's associated
    // with this RPH.
    host->AddObserver(this);
    observed_render_process_hosts_.insert(host);
  }

  // Posting to task queue owned by the unretained object - unretained is safe.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WebRtcEventLogManager::PeerConnectionAddedInternal,
                     base::Unretained(this), render_process_id, lid,
                     std::move(reply)));
}

void WebRtcEventLogManager::PeerConnectionRemoved(
    int render_process_id,
    int lid,
    base::OnceCallback<void(bool)> reply) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  RenderProcessHost* host = RenderProcessHost::FromID(render_process_id);
  if (!host) {
    // TODO(eladalon): This should not be possible, but it happens in
    // WebRtcMediaRecorderTest.PeerConnection. We should fix that and remove
    // this check, using GetBrowserContext() instead.
    // https://crbug.com/775415
    if (reply) {
      BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                              base::BindOnce(std::move(reply), false));
    }
    return;
  }
  const BrowserContext* browser_context = host->GetBrowserContext();

  // Posting to task queue owned by the unretained object - unretained is safe.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WebRtcEventLogManager::PeerConnectionRemovedInternal,
                     base::Unretained(this), render_process_id, lid,
                     GetBrowserContextId(browser_context), std::move(reply)));
}

void WebRtcEventLogManager::EnableLocalLogging(
    const base::FilePath& base_path,
    size_t max_file_size_bytes,
    base::OnceCallback<void(bool)> reply) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!base_path.empty());
  // Posting to task queue owned by the unretained object - unretained is safe.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WebRtcEventLogManager::EnableLocalLoggingInternal,
                     base::Unretained(this), base_path, max_file_size_bytes,
                     std::move(reply)));
}

void WebRtcEventLogManager::DisableLocalLogging(
    base::OnceCallback<void(bool)> reply) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Posting to task queue owned by the unretained object - unretained is safe.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WebRtcEventLogManager::DisableLocalLoggingInternal,
                     base::Unretained(this), std::move(reply)));
}

void WebRtcEventLogManager::StartRemoteLogging(
    int render_process_id,
    int lid,
    size_t max_file_size_bytes,
    base::OnceCallback<void(bool)> reply) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  const BrowserContext* browser_context = GetBrowserContext(render_process_id);
  base::Optional<BrowserContextId> browser_context_id;
  if (!browser_context->IsOffTheRecord()) {
    browser_context_id = GetBrowserContextId(browser_context);
  }

  // Posting to task queue owned by the unretained object - unretained is safe.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WebRtcEventLogManager::StartRemoteLoggingInternal,
                     base::Unretained(this), render_process_id, lid,
                     browser_context_id, browser_context->GetPath(),
                     max_file_size_bytes, std::move(reply)));
}

void WebRtcEventLogManager::OnWebRtcEventLogWrite(
    int render_process_id,
    int lid,
    const std::string& message,
    base::OnceCallback<void(std::pair<bool, bool>)> reply) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const BrowserContext* browser_context = GetBrowserContext(render_process_id);
  // Posting to task queue owned by the unretained object - unretained is safe.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WebRtcEventLogManager::OnWebRtcEventLogWriteInternal,
                     base::Unretained(this), render_process_id, lid,
                     !browser_context->IsOffTheRecord(), message,
                     std::move(reply)));
}

void WebRtcEventLogManager::SetLocalLogsObserver(
    WebRtcLocalEventLogsObserver* observer,
    base::OnceClosure reply) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Posting to task queue owned by the unretained object - unretained is safe.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WebRtcEventLogManager::SetLocalLogsObserverInternal,
                     base::Unretained(this), observer, std::move(reply)));
}

void WebRtcEventLogManager::SetRemoteLogsObserver(
    WebRtcRemoteEventLogsObserver* observer,
    base::OnceClosure reply) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Posting to task queue owned by the unretained object - unretained is safe.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WebRtcEventLogManager::SetRemoteLogsObserverInternal,
                     base::Unretained(this), observer, std::move(reply)));
}

scoped_refptr<base::SequencedTaskRunner>&
WebRtcEventLogManager::GetTaskRunnerForTesting() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return task_runner_;
}

void WebRtcEventLogManager::RenderProcessExited(RenderProcessHost* host,
                                                base::TerminationStatus status,
                                                int exit_code) {
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

  // Posting to task queue owned by the unretained object - unretained is safe.
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

void WebRtcEventLogManager::EnableForBrowserContextInternal(
    BrowserContextId browser_context_id,
    const base::FilePath& browser_context_dir,
    base::OnceClosure reply) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  remote_logs_manager_.EnableForBrowserContext(browser_context_id,
                                               browser_context_dir);
  if (reply) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, std::move(reply));
  }
}

void WebRtcEventLogManager::DisableForBrowserContextInternal(
    BrowserContextId browser_context_id,
    base::OnceClosure reply) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  remote_logs_manager_.DisableForBrowserContext(browser_context_id);
  if (reply) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, std::move(reply));
  }
}

void WebRtcEventLogManager::PeerConnectionAddedInternal(
    int render_process_id,
    int lid,
    base::OnceCallback<void(bool)> reply) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  const bool local_result =
      local_logs_manager_.PeerConnectionAdded(render_process_id, lid);
  const bool remote_result =
      remote_logs_manager_.PeerConnectionAdded(render_process_id, lid);
  DCHECK_EQ(local_result, remote_result);
  if (reply) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::BindOnce(std::move(reply), local_result));
  }
}

void WebRtcEventLogManager::PeerConnectionRemovedInternal(
    int render_process_id,
    int lid,
    BrowserContextId browser_context_id,
    base::OnceCallback<void(bool)> reply) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  const bool local_result =
      local_logs_manager_.PeerConnectionRemoved(render_process_id, lid);
  const bool remote_result = remote_logs_manager_.PeerConnectionRemoved(
      render_process_id, lid, browser_context_id);
  DCHECK_EQ(local_result, remote_result);
  if (reply) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::BindOnce(std::move(reply), local_result));
  }
}

void WebRtcEventLogManager::EnableLocalLoggingInternal(
    const base::FilePath& base_path,
    size_t max_file_size_bytes,
    base::OnceCallback<void(bool)> reply) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  const bool result =
      local_logs_manager_.EnableLogging(base_path, max_file_size_bytes);
  if (reply) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::BindOnce(std::move(reply), result));
  }
}

void WebRtcEventLogManager::DisableLocalLoggingInternal(
    base::OnceCallback<void(bool)> reply) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  const bool result = local_logs_manager_.DisableLogging();
  if (reply) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::BindOnce(std::move(reply), result));
  }
}

void WebRtcEventLogManager::StartRemoteLoggingInternal(
    int render_process_id,
    int lid,
    base::Optional<BrowserContextId> browser_context_id,
    const base::FilePath& browser_context_dir,
    size_t max_file_size_bytes,
    base::OnceCallback<void(bool)> reply) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  bool result;
  if (browser_context_id) {  // Not off the records.
    result = remote_logs_manager_.StartRemoteLogging(
        render_process_id, lid, *browser_context_id, browser_context_dir,
        max_file_size_bytes);
  } else {
    result = false;
  }

  if (reply) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::BindOnce(std::move(reply), result));
  }
}

void WebRtcEventLogManager::OnWebRtcEventLogWriteInternal(
    int render_process_id,
    int lid,
    bool remote_logging_allowed,
    const std::string& message,
    base::OnceCallback<void(std::pair<bool, bool>)> reply) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  const bool local_result =
      local_logs_manager_.EventLogWrite(render_process_id, lid, message);
  const bool remote_result =
      remote_logging_allowed
          ? remote_logs_manager_.EventLogWrite(render_process_id, lid, message)
          : false;
  if (reply) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::BindOnce(std::move(reply),
                       std::make_pair(local_result, remote_result)));
  }
}

void WebRtcEventLogManager::RenderProcessExitedInternal(int render_process_id) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  local_logs_manager_.RenderProcessHostExitedDestroyed(render_process_id);
  remote_logs_manager_.RenderProcessHostExitedDestroyed(render_process_id);
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

  auto task = [](WebRtcEventLogManager* manager, base::Clock* clock,
                 base::OnceClosure reply) {
    manager->local_logs_manager_.SetClockForTesting(clock);
    if (reply) {
      BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, std::move(reply));
    }
  };

  // Posting to task queue owned by the unretained object - unretained is safe.
  task_runner_->PostTask(FROM_HERE, base::BindOnce(task, base::Unretained(this),
                                                   clock, std::move(reply)));
}

void WebRtcEventLogManager::SetPeerConnectionTrackerProxyForTesting(
    std::unique_ptr<PeerConnectionTrackerProxy> pc_tracker_proxy,
    base::OnceClosure reply) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto task = [](WebRtcEventLogManager* manager,
                 std::unique_ptr<PeerConnectionTrackerProxy> pc_tracker_proxy,
                 base::OnceClosure reply) {
    manager->pc_tracker_proxy_ = std::move(pc_tracker_proxy);
    if (reply) {
      BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, std::move(reply));
    }
  };

  // Posting to task queue owned by the unretained object - unretained is safe.
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(task, base::Unretained(this),
                                std::move(pc_tracker_proxy), std::move(reply)));
}

void WebRtcEventLogManager::SetWebRtcEventLogUploaderFactoryForTesting(
    std::unique_ptr<WebRtcEventLogUploader::Factory> uploader_factory,
    base::OnceClosure reply) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto task =
      [](WebRtcEventLogManager* manager,
         std::unique_ptr<WebRtcEventLogUploader::Factory> uploader_factory,
         base::OnceClosure reply) {
        auto& remote_logs_manager = manager->remote_logs_manager_;
        remote_logs_manager.SetWebRtcEventLogUploaderFactoryForTesting(
            std::move(uploader_factory));
        if (reply) {
          BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                                  std::move(reply));
        }
      };

  // Posting to task queue owned by the unretained object - unretained is safe.
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(task, base::Unretained(this),
                                std::move(uploader_factory), std::move(reply)));
}

}  // namespace content
