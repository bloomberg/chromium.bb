// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tracing/trace_subscriber_stdio.h"

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/threading/sequenced_worker_pool.h"
#include "content/public/browser/browser_thread.h"

namespace content {

// All method calls on this class are done on a SequencedWorkerPool thread.
class TraceSubscriberStdioImpl
    : public base::RefCountedThreadSafe<TraceSubscriberStdioImpl> {
 public:
  explicit TraceSubscriberStdioImpl(const base::FilePath& path)
      : path_(path),
        file_(0) {}

  void OnStart() {
    DCHECK(!file_);
    trace_buffer_.SetOutputCallback(
        base::Bind(&TraceSubscriberStdioImpl::Write, this));
    file_ = file_util::OpenFile(path_, "w+");
    if (IsValid()) {
      LOG(INFO) << "Logging performance trace to file: " << path_.value();
      trace_buffer_.Start();
    } else {
      LOG(ERROR) << "Failed to open performance trace file: " << path_.value();
    }
  }

  void OnData(const scoped_refptr<base::RefCountedString>& data_ptr) {
    trace_buffer_.AddFragment(data_ptr->data());
  }

  void OnEnd() {
    trace_buffer_.Finish();
    CloseFile();
  }

 private:
  friend class base::RefCountedThreadSafe<TraceSubscriberStdioImpl>;

  ~TraceSubscriberStdioImpl() {
    CloseFile();
  }

  bool IsValid() const {
    return file_ && (0 == ferror(file_));
  }

  void CloseFile() {
    if (file_) {
      fclose(file_);
      file_ = 0;
    }
    // This is important, as it breaks a reference cycle.
    trace_buffer_.SetOutputCallback(
        base::debug::TraceResultBuffer::OutputCallback());
  }

  void Write(const std::string& output_str) {
    if (IsValid()) {
      size_t written = fwrite(output_str.data(), 1, output_str.size(), file_);
      if (written != output_str.size()) {
        LOG(ERROR) << "Error " << ferror(file_) << " in fwrite() to trace file";
        CloseFile();
      }
    }
  }

  base::FilePath path_;
  FILE* file_;
  base::debug::TraceResultBuffer trace_buffer_;
};

TraceSubscriberStdio::TraceSubscriberStdio(const base::FilePath& path)
    : impl_(new TraceSubscriberStdioImpl(path)) {
  BrowserThread::PostBlockingPoolSequencedTask(
      __FILE__, FROM_HERE,
      base::Bind(&TraceSubscriberStdioImpl::OnStart, impl_));
}

TraceSubscriberStdio::~TraceSubscriberStdio() {
}

void TraceSubscriberStdio::OnEndTracingComplete() {
  BrowserThread::PostBlockingPoolSequencedTask(
      __FILE__, FROM_HERE,
      base::Bind(&TraceSubscriberStdioImpl::OnEnd, impl_));
}

void TraceSubscriberStdio::OnTraceDataCollected(
    const scoped_refptr<base::RefCountedString>& data_ptr) {
  BrowserThread::PostBlockingPoolSequencedTask(
      __FILE__, FROM_HERE,
      base::Bind(&TraceSubscriberStdioImpl::OnData, impl_, data_ptr));
}

}  // namespace content
