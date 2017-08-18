// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/arc_tracing_agent.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file.h"
#include "base/files/file_descriptor_watcher_posix.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/posix/unix_domain_socket.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "cc/base/ring_buffer.h"
#include "content/public/browser/browser_thread.h"

namespace content {

namespace {

// The maximum size used to store one trace event. The ad hoc trace format for
// atrace is 1024 bytes. Here we add additional size as we're using JSON and
// have additional data fields.
constexpr size_t kArcTraceMessageLength = 1024 + 512;

constexpr char kArcTracingAgentName[] = "arc";
constexpr char kArcTraceLabel[] = "ArcTraceEvents";

// Number of events for the ring buffer.
constexpr size_t kTraceEventBufferSize = 64000;

// A helper class for reading trace data from the client side. We separate this
// from |ArcTracingAgentImpl| to isolate the logic that runs on browser's IO
// thread. All the functions in this class except for constructor, destructor,
// and |GetWeakPtr| are expected to be run on browser's IO thread.
class ArcTracingReader {
 public:
  using StopTracingCallback =
      base::Callback<void(const scoped_refptr<base::RefCountedString>&)>;

  ArcTracingReader() : weak_ptr_factory_(this) {}

  void StartTracing(base::ScopedFD read_fd) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    read_fd_ = std::move(read_fd);
    // We don't use the weak pointer returned by |GetWeakPtr| to avoid using it
    // on different task runner. Instead, we use |base::Unretained| here as
    // |fd_watcher_| is always destroyed before |this| is destroyed.
    fd_watcher_ = base::FileDescriptorWatcher::WatchReadable(
        read_fd_.get(), base::Bind(&ArcTracingReader::OnTraceDataAvailable,
                                   base::Unretained(this)));
  }

  void OnTraceDataAvailable() {
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

  void StopTracing(const StopTracingCallback& callback) {
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

    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::BindOnce(callback, base::RefCountedString::TakeString(&data)));
  }

  base::WeakPtr<ArcTracingReader> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::ScopedFD read_fd_;
  std::unique_ptr<base::FileDescriptorWatcher::Controller> fd_watcher_;
  cc::RingBuffer<std::string, kTraceEventBufferSize> ring_buffer_;
  // NOTE: Weak pointers must be invalidated before all other member variables
  // so it must be the last member.
  base::WeakPtrFactory<ArcTracingReader> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcTracingReader);
};

class ArcTracingAgentImpl : public ArcTracingAgent {
 public:
  // base::trace_event::TracingAgent overrides:
  std::string GetTracingAgentName() override { return kArcTracingAgentName; }

  std::string GetTraceEventLabel() override { return kArcTraceLabel; }

  void StartAgentTracing(const base::trace_event::TraceConfig& trace_config,
                         const StartAgentTracingCallback& callback) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    // delegate_ may be nullptr if ARC is not enabled on the system. In such
    // case, simply do nothing.
    bool success = (delegate_ != nullptr);

    base::ScopedFD write_fd, read_fd;
    success = success && CreateSocketPair(&read_fd, &write_fd);

    if (!success) {
      // Use PostTask as the convention of TracingAgent. The caller expects
      // callback to be called after this function returns.
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(callback, GetTracingAgentName(), false));
      return;
    }

    success =
        delegate_->StartTracing(trace_config, std::move(write_fd),
                                base::Bind(callback, GetTracingAgentName()));

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

  void StopAgentTracing(const StopAgentTracingCallback& callback) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    if (is_stopping_) {
      DLOG(WARNING) << "Already working on stopping ArcTracingAgent.";
      return;
    }
    is_stopping_ = true;
    if (delegate_) {
      delegate_->StopTracing(
          base::Bind(&ArcTracingAgentImpl::OnArcTracingStopped,
                     weak_ptr_factory_.GetWeakPtr(), callback));
    }
  }

  // ArcTracingAgent overrides:
  void SetDelegate(Delegate* delegate) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    delegate_ = delegate;
  }

  static ArcTracingAgentImpl* GetInstance() {
    return base::Singleton<ArcTracingAgentImpl>::get();
  }

 private:
  // This allows constructor and destructor to be private and usable only
  // by the Singleton class.
  friend struct base::DefaultSingletonTraits<ArcTracingAgentImpl>;

  ArcTracingAgentImpl() : weak_ptr_factory_(this) {}

  ~ArcTracingAgentImpl() override = default;

  void OnArcTracingStopped(const StopAgentTracingCallback& callback,
                           bool success) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (!success) {
      DLOG(WARNING) << "Failed to stop ARC tracing.";
      callback.Run(GetTracingAgentName(), GetTraceEventLabel(),
                   new base::RefCountedString());
      is_stopping_ = false;
      return;
    }
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::BindOnce(&ArcTracingReader::StopTracing, reader_.GetWeakPtr(),
                       base::Bind(&ArcTracingAgentImpl::OnTracingReaderStopped,
                                  weak_ptr_factory_.GetWeakPtr(), callback)));
  }

  void OnTracingReaderStopped(
      const StopAgentTracingCallback& callback,
      const scoped_refptr<base::RefCountedString>& data) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    callback.Run(GetTracingAgentName(), GetTraceEventLabel(), data);
    is_stopping_ = false;
  }

  Delegate* delegate_ = nullptr;  // Owned by ArcServiceLauncher.
  // We use |reader_.GetWeakPtr()| when binding callbacks with its functions.
  // Notes that the weak pointer returned by it can only be deferenced or
  // invalided in the same task runner to avoid racing condition. The
  // destruction of |reader_| is also a source of invalidation. However, we're
  // lucky as we're using |ArcTracingAgentImpl| as a singleton, the
  // destruction is always performed after all MessageLoops are destroyed, and
  // thus there are no race conditions in such situation.
  ArcTracingReader reader_;
  bool is_stopping_ = false;
  // NOTE: Weak pointers must be invalidated before all other member variables
  // so it must be the last member.
  base::WeakPtrFactory<ArcTracingAgentImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcTracingAgentImpl);
};

}  // namespace

// static
ArcTracingAgent* ArcTracingAgent::GetInstance() {
  return ArcTracingAgentImpl::GetInstance();
}

ArcTracingAgent::ArcTracingAgent() = default;
ArcTracingAgent::~ArcTracingAgent() = default;

ArcTracingAgent::Delegate::~Delegate() = default;

}  // namespace content
