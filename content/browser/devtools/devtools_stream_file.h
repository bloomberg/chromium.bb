// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_STREAM_FILE_H_
#define CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_STREAM_FILE_H_

#include "base/files/file.h"
#include "base/memory/scoped_refptr.h"
#include "content/browser/devtools/devtools_io_context.h"

#include <string>

namespace content {

class DevToolsStreamFile : public DevToolsIOContext::RWStream {
 public:
  explicit DevToolsStreamFile(bool binary);

  void Read(off_t position, size_t max_size, ReadCallback callback) override;
  void Close(bool invoke_pending_callbacks) override {}
  void Append(std::unique_ptr<std::string> data) override;
  const std::string& handle() override;

 private:
  ~DevToolsStreamFile() override;

  void ReadOnFileSequence(off_t pos, size_t max_size, ReadCallback callback);
  void AppendOnFileSequence(std::unique_ptr<std::string> data);
  bool InitOnFileSequenceIfNeeded();

  const std::string handle_;
  base::File file_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  bool had_errors_;
  off_t last_read_pos_;
  bool binary_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsStreamFile);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_STREAM_FILE_H_
