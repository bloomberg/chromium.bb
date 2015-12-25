// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_CHILD_CHILD_THREAD_H_
#define CONTENT_PUBLIC_CHILD_CHILD_THREAD_H_

#include "build/build_config.h"
#include "content/common/content_export.h"
#include "ipc/ipc_sender.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

namespace content {

// An abstract base class that contains logic shared between most child
// processes of the embedder.
class CONTENT_EXPORT ChildThread : public IPC::Sender {
 public:
  // Returns the one child thread for this process.  Note that this can only be
  // accessed when running on the child thread itself.
  static ChildThread* Get();

  ~ChildThread() override {}

#if defined(OS_WIN)
  // Request that the given font be loaded by the browser so it's cached by the
  // OS. Please see ChildProcessHost::PreCacheFont for details.
  virtual void PreCacheFont(const LOGFONT& log_font) = 0;

  // Release cached font.
  virtual void ReleaseCachedFonts() = 0;
#endif
};

}  // namespace content

#endif  // CONTENT_PUBLIC_CHILD_CHILD_THREAD_H_
