// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tracing/arc_tracing_agent_impl.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/singleton.h"
#include "base/posix/unix_domain_socket.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/trace_config.h"
#include "content/public/browser/browser_thread.h"
#include "services/resource_coordinator/public/interfaces/service_constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace content {

namespace {

ArcTracingAgentImpl* g_arc_tracing_agent_impl = nullptr;

// The maximum size used to store one trace event. The ad hoc trace format for
// atrace is 1024 bytes. Here we add additional size as we're using JSON and
// have additional data fields.
constexpr size_t kArcTraceMessageLength = 1024 + 512;

constexpr char kChromeTraceEventLabel[] = "traceEvents";

}  // namespace

// static
ArcTracingAgent* ArcTracingAgent::GetInstance() {
  return ArcTracingAgentImpl::GetInstance();
}

ArcTracingAgent::ArcTracingAgent() = default;

ArcTracingAgent::~ArcTracingAgent() = default;

// static
ArcTracingAgentImpl* ArcTracingAgentImpl::GetInstance() {
  DCHECK(g_arc_tracing_agent_impl);
  return g_arc_tracing_agent_impl;
}

ArcTracingAgentImpl::ArcTracingAgentImpl(service_manager::Connector* connector)
    : binding_(this), weak_ptr_factory_(this) {
  DCHECK(!g_arc_tracing_agent_impl);

  // Connecto to the agent registry interface.
  tracing::mojom::AgentRegistryPtr agent_registry;
  connector->BindInterface(resource_coordinator::mojom::kServiceName,
                           &agent_registry);

  // Register this agent.
  tracing::mojom::AgentPtr agent;
  binding_.Bind(mojo::MakeRequest(&agent));
  agent_registry->RegisterAgent(std::move(agent), kChromeTraceEventLabel,
                                tracing::mojom::TraceDataType::ARRAY,
                                false /* supports_explicit_clock_sync */);

  g_arc_tracing_agent_impl = this;
}

ArcTracingAgentImpl::~ArcTracingAgentImpl() = default;

void ArcTracingAgentImpl::SetDelegate(Delegate* delegate) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  delegate_ = delegate;
}

void ArcTracingAgentImpl::StartTracing(
    const std::string& config,
    base::TimeTicks coordinator_time,
    const Agent::StartTracingCallback& callback) {
  base::trace_event::TraceConfig trace_config(config);
  // delegate_ may be nullptr if ARC is not enabled on the system. In such
  // case, simply do nothing.
  bool success = trace_config.IsSystraceEnabled() && (delegate_ != nullptr);

  base::ScopedFD write_fd, read_fd;
  success = success && CreateSocketPair(&read_fd, &write_fd);

  if (!success) {
    // Use PostTask as the convention of TracingAgent. The caller expects
    // callback to be called after this function returns.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(callback, false /* success */));
    return;
  }

  success =
      delegate_->StartTracing(trace_config, std::move(write_fd), callback);

  if (!success) {
    // In the event of a failure, we don't use PostTask, because the
    // delegate_->StartTracing call will have already done it.
    return;
  }

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&ArcTracingReader::StartTracing, reader_.GetWeakPtr(),
                     base::Passed(&read_fd)));
}

void ArcTracingAgentImpl::StopAndFlush(tracing::mojom::RecorderPtr recorder) {
  if (is_stopping_) {
    DLOG(WARNING) << "Already working on stopping ArcTracingAgent.";
    return;
  }
  if (!delegate_)
    return;
  is_stopping_ = true;
  recorder_ = std::move(recorder);
  delegate_->StopTracing(base::Bind(&ArcTracingAgentImpl::OnArcTracingStopped,
                                    weak_ptr_factory_.GetWeakPtr()));
}

void ArcTracingAgentImpl::RequestClockSyncMarker(
    const std::string& sync_id,
    const Agent::RequestClockSyncMarkerCallback& callback) {
  NOTREACHED();
}

void ArcTracingAgentImpl::GetCategories(
    const Agent::GetCategoriesCallback& callback) {
  callback.Run("");
}

void ArcTracingAgentImpl::RequestBufferStatus(
    const Agent::RequestBufferStatusCallback& callback) {
  callback.Run(0, 0);
}

void ArcTracingAgentImpl::OnArcTracingStopped(bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!success) {
    DLOG(WARNING) << "Failed to stop ARC tracing.";
    is_stopping_ = false;
    return;
  }
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&ArcTracingReader::StopTracing, reader_.GetWeakPtr(),
                     base::Bind(&ArcTracingAgentImpl::OnTracingReaderStopped,
                                weak_ptr_factory_.GetWeakPtr())));
}

void ArcTracingAgentImpl::OnTracingReaderStopped(const std::string& data) {
  recorder_->AddChunk(data);
  recorder_.reset();
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  is_stopping_ = false;
}

ArcTracingAgent::Delegate::~Delegate() = default;

ArcTracingAgentImpl::ArcTracingReader::ArcTracingReader()
    : weak_ptr_factory_(this) {}

ArcTracingAgentImpl::ArcTracingReader::~ArcTracingReader() {
  DCHECK(!fd_watcher_);
}

void ArcTracingAgentImpl::ArcTracingReader::StartTracing(
    base::ScopedFD read_fd) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  read_fd_ = std::move(read_fd);
  // We don't use the weak pointer returned by |GetWeakPtr| to avoid using it
  // on different task runner. Instead, we use |base::Unretained| here as
  // |fd_watcher_| is always destroyed before |this| is destroyed.
  fd_watcher_ = base::FileDescriptorWatcher::WatchReadable(
      read_fd_.get(), base::Bind(&ArcTracingReader::OnTraceDataAvailable,
                                 base::Unretained(this)));
}

void ArcTracingAgentImpl::ArcTracingReader::OnTraceDataAvailable() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  char buf[kArcTraceMessageLength];
  std::vector<base::ScopedFD> unused_fds;
  ssize_t n = base::UnixDomainSocket::RecvMsg(
      read_fd_.get(), buf, kArcTraceMessageLength, &unused_fds);
  if (n < 0) {
    DPLOG(WARNING) << "Unexpected error while reading trace from client.";
    // Do nothing here as StopTracing will do the clean up and the existing
    // trace logs will be returned.
    return;
  }

  if (n == 0) {
    // When EOF is reached, stop listening for changes since there's never
    // going to be any more data to be read. The rest of the cleanup will be
    // done in StopTracing.
    fd_watcher_.reset();
    return;
  }

  if (n > static_cast<ssize_t>(kArcTraceMessageLength)) {
    DLOG(WARNING) << "Unexpected data size when reading trace from client.";
    return;
  }
  ring_buffer_.SaveToBuffer(std::string(buf, n));
}

void ArcTracingAgentImpl::ArcTracingReader::StopTracing(
    base::OnceCallback<void(const std::string&)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  fd_watcher_.reset();
  read_fd_.reset();

  bool append_comma = false;
  std::string data;
  for (auto it = ring_buffer_.Begin(); it; ++it) {
    if (append_comma)
      data.append(",");
    else
      append_comma = true;
    data.append(**it);
  }
  ring_buffer_.Clear();

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::BindOnce(std::move(callback), data));
}

base::WeakPtr<ArcTracingAgentImpl::ArcTracingReader>
ArcTracingAgentImpl::ArcTracingReader::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace content
