// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_STREAMS_STREAM_CONTEXT_H_
#define CONTENT_BROWSER_STREAMS_STREAM_CONTEXT_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/sequenced_task_runner_helpers.h"
#include "content/public/browser/browser_thread.h"

namespace content {
class BrowserContext;
class StreamRegistry;

// A context class that keeps track of StreamRegistry used by the chrome.
// There is an instance associated with each BrowserContext. There could be
// multiple URLRequestContexts in the same browser context that refers to the
// same instance.
//
// All methods, except the ctor, are expected to be called on
// the IO thread (unless specifically called out in doc comments).
class StreamContext
    : public base::RefCountedThreadSafe<StreamContext,
                                        BrowserThread::DeleteOnIOThread> {
 public:
  StreamContext();

  static StreamContext* GetFor(BrowserContext* browser_context);

  void InitializeOnIOThread();

  StreamRegistry* registry() const { return registry_.get(); }

 protected:
  virtual ~StreamContext();

 private:
  friend class base::DeleteHelper<StreamContext>;
  friend struct BrowserThread::DeleteOnThread<BrowserThread::IO>;

  scoped_ptr<StreamRegistry> registry_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_STREAMS_STREAM_CONTEXT_H_

