// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CLOUD_PRINT_SERVICE_CHROME_LAUNCHER_H_
#define CLOUD_PRINT_SERVICE_CHROME_LAUNCHER_H_

#include <string>

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/simple_thread.h"

class CommandLine;

class ChromeLauncher : public base::DelegateSimpleThread::Delegate {
 public:
  explicit ChromeLauncher(const base::FilePath& user_data);

  virtual ~ChromeLauncher();

  bool Start();
  void Stop();

  virtual void Run() OVERRIDE;

  // Copy additional chrome switches.
  static void CopySwitchesFromCurrent(CommandLine* destination);

 private:
  base::FilePath user_data_;
  base::WaitableEvent stop_event_;
  scoped_ptr<base::DelegateSimpleThread> thread_;

  DISALLOW_COPY_AND_ASSIGN(ChromeLauncher);
};

#endif  // CLOUD_PRINT_SERVICE_CHROME_LAUNCHER_H_
