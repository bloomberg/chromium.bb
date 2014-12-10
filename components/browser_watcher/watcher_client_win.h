// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_WATCHER_WATCHER_CLIENT_WIN_H_
#define COMPONENTS_BROWSER_WATCHER_WATCHER_CLIENT_WIN_H_

#include "base/command_line.h"
#include "base/macros.h"
#include "base/process/process.h"

namespace browser_watcher {

// An interface class to take care of the details in launching a browser
// watch process.
class WatcherClient {
 public:
  // Constructs a watcher client with a |base_command_line|, which should be
  // a command line sufficient to invoke the watcher process. This command
  // line will be augmented with the value of the inherited handle.
  explicit WatcherClient(const base::CommandLine& base_command_line);

  // Launches the watcher process.
  void LaunchWatcher();

  // Accessors, exposed only for testing.
  bool use_legacy_launch() const { return use_legacy_launch_; }
  void set_use_legacy_launch(bool use_legacy_launch) {
      use_legacy_launch_ = use_legacy_launch;
  }
  base::ProcessHandle process() const { return process_.Handle(); }

 private:
  // Launches |cmd_line| such that the child process is able to inherit
  // |handle|. If |use_legacy_launch_| is true, this uses a non-threadsafe
  // legacy launch mode that's compatible with Windows XP.
  // Returns a handle to the child process.
  void LaunchWatcherProcess(const base::CommandLine& cmd_line,
                            base::Process self);

  // If true, the watcher process will be launched with XP legacy handle
  // inheritance. This is not thread safe and can leak random handles into the
  // child process, but it's the best we can do on XP.
  bool use_legacy_launch_;

  // The base command line passed to the constructor.
  base::CommandLine base_command_line_;

  // A handle to the launched watcher process. Valid after a successful
  // LaunchWatcher() call.
  base::Process process_;

  DISALLOW_COPY_AND_ASSIGN(WatcherClient);
};

}  // namespace browser_watcher

#endif  // COMPONENTS_BROWSER_WATCHER_WATCHER_CLIENT_WIN_H_
