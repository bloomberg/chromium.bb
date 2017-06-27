// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_ASYNC_API_FUNCTION_H_
#define EXTENSIONS_BROWSER_API_ASYNC_API_FUNCTION_H_

#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

// AsyncApiFunction provides convenient thread management for APIs that need to
// do essentially all their work on a thread other than the UI thread.
class AsyncApiFunction : public AsyncExtensionFunction {
 protected:
  AsyncApiFunction();
  ~AsyncApiFunction() override;

  // Like Prepare(). A useful place to put common work in an ApiFunction
  // superclass that multiple API functions want to share.
  virtual bool PrePrepare();

  // Set up for work (e.g., validate arguments). Guaranteed to happen on UI
  // thread.
  virtual bool Prepare() = 0;

  // Do actual work. Guaranteed to happen on the task runner specified in
  // |work_task_runner_| if non-null; or on the IO thread otherwise.
  virtual void Work();

  // Start the asynchronous work. Guraranteed to happen on work thread.
  virtual void AsyncWorkStart();

  // Notify AsyncIOApiFunction that the work is completed
  void AsyncWorkCompleted();

  // Respond. Guaranteed to happen on UI thread.
  virtual bool Respond() = 0;

  // ExtensionFunction::RunAsync()
  bool RunAsync() override;

 protected:
  scoped_refptr<base::SequencedTaskRunner> work_task_runner() const {
    return work_task_runner_;
  }
  void set_work_task_runner(
      scoped_refptr<base::SequencedTaskRunner> work_task_runner) {
    work_task_runner_ = work_task_runner;
  }

 private:
  void WorkOnWorkThread();
  void RespondOnUIThread();

  // If you don't want your Work() method to happen on the IO thread, then set
  // this to the SequenceTaskRunner you do want to use, preferably in Prepare().
  scoped_refptr<base::SequencedTaskRunner> work_task_runner_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_ASYNC_API_FUNCTION_H_
