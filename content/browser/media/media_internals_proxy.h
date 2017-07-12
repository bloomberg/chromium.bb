// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_MEDIA_INTERNALS_PROXY_H_
#define CONTENT_BROWSER_MEDIA_MEDIA_INTERNALS_PROXY_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner_helpers.h"
#include "base/strings/string16.h"
#include "content/browser/media/media_internals.h"
#include "content/public/browser/browser_thread.h"

namespace content {
class MediaInternalsMessageHandler;

// This class is a proxy between MediaInternals (on the IO thread) and
// MediaInternalsMessageHandler (on the UI thread).
// It is ref_counted to ensure that it completes all pending Tasks on both
// threads before destruction.
class MediaInternalsProxy
    : public base::RefCountedThreadSafe<MediaInternalsProxy,
                                        BrowserThread::DeleteOnUIThread> {
 public:
  MediaInternalsProxy();

  // Register a Handler and start receiving callbacks from MediaInternals.
  void Attach(MediaInternalsMessageHandler* handler);

  // Unregister the same and stop receiving callbacks.
  void Detach();

  // Have MediaInternals send all the data it has.
  void GetEverything();

 private:
  friend struct BrowserThread::DeleteOnThread<BrowserThread::UI>;
  friend class base::DeleteHelper<MediaInternalsProxy>;
  virtual ~MediaInternalsProxy();

  void GetEverythingOnIOThread();

  // Callback for MediaInternals to update. Must be called on UI thread.
  void UpdateUIOnUIThread(const base::string16& update);

  MediaInternalsMessageHandler* handler_;
  MediaInternals::UpdateCallback update_callback_;

  DISALLOW_COPY_AND_ASSIGN(MediaInternalsProxy);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_MEDIA_INTERNALS_PROXY_H_
