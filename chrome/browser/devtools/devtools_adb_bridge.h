// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVTOOLS_ADB_BRIDGE_H_
#define CHROME_BROWSER_DEVTOOLS_DEVTOOLS_ADB_BRIDGE_H_

#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"

namespace base {
class Thread;
}

class DevToolsAdbBridge : public base::RefCountedThreadSafe<DevToolsAdbBridge> {
 public:
  typedef base::Callback<void(const std::string& error,
                              const std::string& data)> Callback;

  static DevToolsAdbBridge* Start();
  void Query(const std::string query, const Callback& callback);
  void Stop();

 private:
  friend class base::RefCountedThreadSafe<DevToolsAdbBridge>;

  explicit DevToolsAdbBridge();
  virtual ~DevToolsAdbBridge();

  void StartHandlerOnFileThread();
  void StopHandlerOnFileThread();

  void ResetHandlerAndReleaseOnUIThread();
  void ResetHandlerOnUIThread();

  void QueryOnHandlerThread(const std::string query, const Callback& callback);
  void QueryResponseOnHandlerThread(const Callback& callback,
                                    const std::string& error,
                                    const std::string& response);

  void RespondOnUIThread(const Callback& callback,
                         const std::string& error,
                         const std::string& response);

  // The thread used by the devtools to run client socket.
  scoped_ptr<base::Thread> thread_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsAdbBridge);
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_ADB_BRIDGE_H_
