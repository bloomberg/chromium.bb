// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_API_FUNCTION_H_
#define CHROME_BROWSER_EXTENSIONS_API_API_FUNCTION_H_
#pragma once

#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/extensions/api/api_resource.h"
#include "content/public/browser/browser_thread.h"

class ExtensionService;

namespace extensions {

class APIResourceController;
class APIResourceEventNotifier;

// AsyncAPIFunction provides convenient thread management for APIs that need to
// do essentially all their work on a thread other than the UI thread.
class AsyncAPIFunction : public AsyncExtensionFunction {
 protected:
  AsyncAPIFunction();
  virtual ~AsyncAPIFunction() {}

  // Set up for work (e.g., validate arguments). Guaranteed to happen on UI
  // thread.
  virtual bool Prepare() = 0;

  // Do actual work. Guaranteed to happen on the thread specified in
  // work_thread_id_.
  virtual void Work();

  // Start the asynchronous work. Guraranteed to happen on requested thread.
  virtual void AsyncWorkStart();

  // Notify AsyncIOAPIFunction that the work is completed
  void AsyncWorkCompleted();

  // Respond. Guaranteed to happen on UI thread.
  virtual bool Respond() = 0;

  // Looks for a kSrcId key that might have been added to a create method's
  // options object.
  int ExtractSrcId(size_t argument_position);

  // Utility.
  APIResourceEventNotifier* CreateEventNotifier(int src_id);

  // Access to the controller singleton.
  APIResourceController* controller();

  // ExtensionFunction::RunImpl()
  virtual bool RunImpl() OVERRIDE;

 protected:
  void set_work_thread_id(content::BrowserThread::ID work_thread_id) {
    work_thread_id_ = work_thread_id;
  }

 private:
  void WorkOnWorkThread();
  void RespondOnUIThread();

  // If you don't want your Work() method to happen on the IO thread, then set
  // this to the thread that you do want, preferably in Prepare().
  content::BrowserThread::ID work_thread_id_;

  ExtensionService* extension_service_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_API_FUNCTION_H_
