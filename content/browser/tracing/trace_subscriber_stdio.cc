// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tracing/trace_subscriber_stdio.h"

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/threading/sequenced_worker_pool.h"
#include "content/public/browser/browser_thread.h"

namespace content {

// All method calls on this class are done on a SequencedWorkerPool thread.
class TraceSubscriberStdio::TraceSubscriberStdioWorker
    : public base::RefCountedThreadSafe<TraceSubscriberStdioWorker> {
 public:
  TraceSubscriberStdioWorker(const base::FilePath& path,
                             FileType file_type,
                             bool has_system_trace)
      : path_(path),
        file_type_(file_type),
        has_system_trace_(has_system_trace),
        file_(0),
        needs_comma_(false),
        wrote_trace_(false),
        has_pending_system_trace_(false),
        wrote_system_trace_(false) {}

  void OnTraceStart() {
    DCHECK(!file_);
    file_ = file_util::OpenFile(path_, "w+");
    if (!IsValid()) {
      LOG(ERROR) << "Failed to open performance trace file: " << path_.value();
      return;
    }

    LOG(INFO) << "Logging performance trace to file: " << path_.value();
    if (file_type_ == FILE_TYPE_PROPERTY_LIST)
      WriteString("{\"traceEvents\":");
    WriteString("[");
  }

  void OnTraceData(const scoped_refptr<base::RefCountedString>& data_ptr) {
    if (!IsValid())
      return;
    DCHECK(!data_ptr->data().empty());
    if (needs_comma_)
      WriteString(",");
    WriteString(data_ptr->data());
    needs_comma_ = true;
  }

  void OnSystemTraceData(
      const scoped_refptr<base::RefCountedString>& data_ptr) {
    if (wrote_trace_) {
      WriteSystemTrace(data_ptr);
      End();
    } else {
      pending_system_trace_ = data_ptr;
      has_pending_system_trace_ = true;
    }
  }

  void OnTraceEnd() {
    if (!IsValid())
      return;
    WriteString("]");

    wrote_trace_ = true;

    if (!has_system_trace_ || wrote_system_trace_) {
      End();
      return;
    }

    WriteString(",");
    if (has_pending_system_trace_) {
      WriteSystemTrace(pending_system_trace_);
      End();
    }
  }

 private:
  friend class base::RefCountedThreadSafe<TraceSubscriberStdioWorker>;

  ~TraceSubscriberStdioWorker() {
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
  }

  void End() {
    if (file_type_ == FILE_TYPE_PROPERTY_LIST)
      WriteString("}");
    CloseFile();
  }

  void WriteSystemTrace(const scoped_refptr<base::RefCountedString>& data_ptr) {
    // Newlines need to be replaced with the string "\n" to be parsed correctly.
    // Double quotes need to be replaced with the string "\"".
    // System logs are ASCII.
    const std::string& data = data_ptr->data();
    const char* chars = data.c_str();
    WriteString("\"systemTraceEvents\":\"");
    size_t old_index = 0;
    for (size_t new_index = data.find_first_of("\n\"");
         std::string::npos != new_index;
         old_index = new_index + 1,
         new_index = data.find_first_of("\n\"", old_index)) {
      WriteChars(chars + old_index, new_index - old_index);
      if (chars[new_index] == '\n')
        WriteChars("\\n", 2);
      else
        WriteChars("\\\"", 2);
    }
    WriteChars(chars + old_index, data.size() - old_index);
    WriteString("\"");
    wrote_system_trace_ = true;
  }

  void WriteChars(const char* output_chars, size_t size) {
    if (size == 0)
      return;

    if (IsValid()) {
      size_t written = fwrite(output_chars, 1, size, file_);
      if (written != size) {
        LOG(ERROR) << "Error " << ferror(file_) << " in fwrite() to trace file";
        CloseFile();
      }
    }
  }

  void WriteString(const std::string& output_str) {
    WriteChars(output_str.data(), output_str.size());
  }

  base::FilePath path_;
  const FileType file_type_;
  const bool has_system_trace_;
  FILE* file_;
  bool needs_comma_;
  bool wrote_trace_;
  bool has_pending_system_trace_;
  bool wrote_system_trace_;
  scoped_refptr<base::RefCountedString> pending_system_trace_;
  DISALLOW_COPY_AND_ASSIGN(TraceSubscriberStdioWorker);
};

TraceSubscriberStdio::TraceSubscriberStdio(const base::FilePath& path,
                                           FileType file_type,
                                           bool has_system_trace)
    : worker_(new TraceSubscriberStdioWorker(path,
                                             file_type,
                                             has_system_trace)) {
  if (has_system_trace)
    CHECK_EQ(FILE_TYPE_PROPERTY_LIST, file_type);
  BrowserThread::PostBlockingPoolSequencedTask(
      __FILE__, FROM_HERE,
      base::Bind(&TraceSubscriberStdioWorker::OnTraceStart, worker_));
}

TraceSubscriberStdio::~TraceSubscriberStdio() {
}

void TraceSubscriberStdio::OnEndTracingComplete() {
  BrowserThread::PostBlockingPoolSequencedTask(
      __FILE__, FROM_HERE,
      base::Bind(&TraceSubscriberStdioWorker::OnTraceEnd, worker_));
}

void TraceSubscriberStdio::OnTraceDataCollected(
    const scoped_refptr<base::RefCountedString>& data_ptr) {
  BrowserThread::PostBlockingPoolSequencedTask(
      __FILE__, FROM_HERE,
      base::Bind(&TraceSubscriberStdioWorker::OnTraceData, worker_, data_ptr));
}

void TraceSubscriberStdio::OnEndSystemTracing(
    const scoped_refptr<base::RefCountedString>& events_str_ptr) {
  BrowserThread::PostBlockingPoolSequencedTask(
      __FILE__, FROM_HERE,
      base::Bind(&TraceSubscriberStdioWorker::OnSystemTraceData,
                 worker_,
                 events_str_ptr));
}

}  // namespace content
