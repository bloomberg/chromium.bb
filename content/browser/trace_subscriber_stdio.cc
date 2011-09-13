// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/trace_subscriber_stdio.h"

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
    // FIXME: the file format expects it to start with "[".
    fputc('[', file_);
    return true;
  } else {
    LOG(ERROR) << "Failed to open performance trace file: " << path.value();
    return false;
  }
}

void TraceSubscriberStdio::CloseFile() {
  if (file_) {
    // FIXME: the file format expects it to end with "]".
    fputc(']', file_);
    fclose(file_);
    file_ = 0;
  }
}

bool TraceSubscriberStdio::IsValid() {
  return file_ && (0 == ferror(file_));
}

void TraceSubscriberStdio::OnEndTracingComplete() {
  CloseFile();
}

void TraceSubscriberStdio::OnTraceDataCollected(
    const std::string& json_events) {
  if (!IsValid()) {
    return;
  }

  // FIXME: "json_events" currently comes with "[" and "]". But the file doesn't
  // expect them. So remove them when writing to the file.
  CHECK_GE(json_events.size(), 2u);
  const char* data = json_events.data() + 1;
  size_t size = json_events.size() - 2;

  size_t written = fwrite(data, 1, size, file_);
  if (written != size) {
    LOG(ERROR) << "Error " << ferror(file_) << " when writing to trace file";
    fclose(file_);
    file_ = 0;
  } else {
    fputc(',', file_);
  }
}
