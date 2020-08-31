// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_CHILD_PROCESS_DATA_H_
#define CONTENT_PUBLIC_BROWSER_CHILD_PROCESS_DATA_H_

#include "base/process/process.h"
#include "base/strings/string16.h"
#include "content/common/content_export.h"
#include "content/public/browser/sandbox_type.h"

namespace content {

// Holds information about a child process.
struct CONTENT_EXPORT ChildProcessData {
  // The type of the process. See the content::ProcessType enum for the
  // well-known process types.
  int process_type;

  // The name of the process.  i.e. for plugins it might be Flash, while for
  // for workers it might be the domain that it's from.
  base::string16 name;

  // The non-localized name of the process used for metrics reporting.
  std::string metrics_name;

  // The unique identifier for this child process. This identifier is NOT a
  // process ID, and will be unique for all types of child process for
  // one run of the browser.
  int id = 0;

  // The content::SandboxType that this process was launched at. May be
  // invalid prior to process launch.
  content::SandboxType sandbox_type;

  const base::Process& GetProcess() const { return process_; }
  // Since base::Process is non-copyable, the caller has to provide a rvalue.
  void SetProcess(base::Process process) { process_ = std::move(process); }

  explicit ChildProcessData(int process_type);
  ~ChildProcessData();

  ChildProcessData(ChildProcessData&& rhs);

  // Copying these objects requires duplicating the handle which is moderately
  // expensive, so make it an explicit action.
  ChildProcessData Duplicate() const;

 private:
  // May be invalid if the process isn't started or is the current process.
  base::Process process_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_CHILD_PROCESS_DATA_H_
