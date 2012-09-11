// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google_apis/task_util.h"

#include "base/location.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace gdata {

void RunTaskOnThread(scoped_refptr<base::MessageLoopProxy> relay_proxy,
                     const base::Closure& task) {
  if (relay_proxy->BelongsToCurrentThread()) {
    task.Run();
  } else {
    const bool posted = relay_proxy->PostTask(FROM_HERE, task);
    DCHECK(posted);
  }
}

void RunTaskOnUIThread(const base::Closure& task) {
  RunTaskOnThread(
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI), task);
}

}  // namespace gdata
