// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/trace_subscriber_stdio.h"

#include "base/bind.h"
#include "base/logging.h"

TraceSubscriberStdio::TraceSubscriberStdio() : file_(0) {
}

TraceSubscriberStdio::TraceSubscriberStdio(const FilePath& path) : file_(0) {
  OpenFile(path);
}

TraceSubscriberStdio::~TraceSubscriberStdio() {
  CloseFile();
}

bool TraceSubscriberStdio::OpenFile(const FilePath& path) {
  LOG(INFO) << "Logging performance trace to file: " << path.value();
  CloseFile();
  file_ = file_util::OpenFile(path, "w+");
  if (IsValid()) {
    trace_buffer_.SetOutputCallback(base::Bind(&TraceSubscriberStdio::Write,
                                               base::Unretained(this)));
    trace_buffer_.Start();
    return true;
  } else {
    LOG(ERROR) << "Failed to open performance trace file: " << path.value();
    return false;
  }
}

void TraceSubscriberStdio::CloseFile() {
  if (file_) {
    fclose(file_);
    file_ = 0;
  }
}

bool TraceSubscriberStdio::IsValid() {
  return file_ && (0 == ferror(file_));
}

void TraceSubscriberStdio::OnEndTracingComplete() {
  trace_buffer_.Finish();
  CloseFile();
}

void TraceSubscriberStdio::OnTraceDataCollected(
    const std::string& trace_fragment) {
  trace_buffer_.AddFragment(trace_fragment);
}

void TraceSubscriberStdio::Write(const std::string& output_str) {
  if (IsValid()) {
    size_t written = fwrite(output_str.data(), 1, output_str.size(), file_);
    if (written != output_str.size()) {
      LOG(ERROR) << "Error " << ferror(file_) << " when writing to trace file";
      CloseFile();
    }
  }
}
