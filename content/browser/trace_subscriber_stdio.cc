// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/trace_subscriber_stdio.h"

#include "base/logging.h"

TraceSubscriberStdio::TraceSubscriberStdio(const FilePath& path) {
  LOG(INFO) << "Logging performance trace to file: " << path.value();
  m_file = file_util::OpenFile(path, "w+");
  if (IsValid()) {
    // FIXME: the file format expects it to start with "[".
    fputc('[', m_file);
  } else {
    LOG(ERROR) << "Failed to open performance trace file: " << path.value();
  }
}

TraceSubscriberStdio::~TraceSubscriberStdio() {
  OnEndTracingComplete();
}

bool TraceSubscriberStdio::IsValid() {
  return m_file && (0 == ferror(m_file));
}

void TraceSubscriberStdio::OnEndTracingComplete() {
  if (m_file) {
    // FIXME: the file format expects it to end with "]".
    fputc(']', m_file);
    fclose(m_file);
    m_file = 0;
  }
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

  size_t written = fwrite(data, 1, size, m_file);
  if (written != size) {
    LOG(ERROR) << "Error " << ferror(m_file) << " when writing to trace file";
    fclose(m_file);
    m_file = 0;
  } else {
    fputc(',', m_file);
  }
}
