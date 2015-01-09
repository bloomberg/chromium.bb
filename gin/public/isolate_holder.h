// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_PUBLIC_ISOLATE_HOLDER_H_
#define GIN_PUBLIC_ISOLATE_HOLDER_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "gin/gin_export.h"
#include "v8/include/v8.h"

namespace gin {

class PerIsolateData;
class RunMicrotasksObserver;

// To embed Gin, first initialize gin using IsolateHolder::Initialize and then
// create an instance of IsolateHolder to hold the v8::Isolate in which you
// will execute JavaScript. You might wish to subclass IsolateHolder if you
// want to tie more state to the lifetime of the isolate.
class GIN_EXPORT IsolateHolder {
 public:
  // Controls whether or not V8 should only accept strict mode scripts.
  enum ScriptMode {
    kNonStrictMode,
    kStrictMode
  };

  IsolateHolder();
  ~IsolateHolder();

  // Should be invoked once before creating IsolateHolder instances to
  // initialize V8 and Gin. In case V8_USE_EXTERNAL_STARTUP_DATA is defined,
  // V8's initial snapshot should be loaded (by calling LoadV8Snapshot or
  // LoadV8SnapshotFd) before calling Initialize.
  static void Initialize(ScriptMode mode,
                         v8::ArrayBuffer::Allocator* allocator);

  v8::Isolate* isolate() { return isolate_; }

  // The implementations of Object.observe() and Promise enqueue v8 Microtasks
  // that should be executed just before control is returned to the message
  // loop. This method adds a MessageLoop TaskObserver which runs any pending
  // Microtasks each time a Task is completed. This method should be called
  // once, when a MessageLoop is created and it should be called on the
  // MessageLoop's thread.
  void AddRunMicrotasksObserver();

  // This method should also only be called once, and on the MessageLoop's
  // thread.
  void RemoveRunMicrotasksObserver();

#if defined(V8_USE_EXTERNAL_STARTUP_DATA)
  static const char kNativesFileName[];
  static const char kSnapshotFileName[];

  static bool LoadV8SnapshotFd(int natives_fd,
                               int64 natives_offset,
                               int64 natives_size,
                               int snapshot_fd,
                               int64 snapshot_offset,
                               int64 snapshot_size);
  static bool LoadV8Snapshot();
#endif  // V8_USE_EXTERNAL_STARTUP_DATA
  static void GetV8ExternalSnapshotData(const char** natives_data_out,
                                        int* natives_size_out,
                                        const char** snapshot_data_out,
                                        int* snapshot_size_out);

 private:
  v8::Isolate* isolate_;
  scoped_ptr<PerIsolateData> isolate_data_;
  scoped_ptr<RunMicrotasksObserver> task_observer_;

  DISALLOW_COPY_AND_ASSIGN(IsolateHolder);
};

}  // namespace gin

#endif  // GIN_PUBLIC_ISOLATE_HOLDER_H_
