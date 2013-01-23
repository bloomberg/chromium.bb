// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_SHELL_MESSAGE_FILTER_H_
#define CONTENT_SHELL_SHELL_MESSAGE_FILTER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "content/public/browser/browser_message_filter.h"

namespace content {

class ShellMessageFilter : public BrowserMessageFilter {
 public:
  explicit ShellMessageFilter(int render_process_id);

 private:
  virtual ~ShellMessageFilter();

  // BrowserMessageFilter implementation.
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok) OVERRIDE;

  void OnReadFileToString(const FilePath& local_file, std::string* contents);
  void OnRegisterIsolatedFileSystem(
      const std::vector<FilePath>& absolute_filenames,
      std::string* filesystem_id);

  int render_process_id_;

  DISALLOW_COPY_AND_ASSIGN(ShellMessageFilter);
};

}  // namespace content

#endif // CONTENT_SHELL_SHELL_MESSAGE_FILTER_H_
