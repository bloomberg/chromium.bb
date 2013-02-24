// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/process_info_snapshot.h"

#include <sys/types.h>  // For |uid_t| (and |pid_t|).
#include <unistd.h>  // For |getpid()|, |getuid()|, etc.

#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process_util.h"

#include "testing/gtest/include/gtest/gtest.h"

typedef testing::Test ProcessInfoSnapshotMacTest;

TEST_F(ProcessInfoSnapshotMacTest, FindPidOneTest) {
  // Sample process with PID 1, which should exist and presumably belong to
  // root.
  std::vector<base::ProcessId> pid_list;
  pid_list.push_back(1);
  ProcessInfoSnapshot snapshot;
  ASSERT_TRUE(snapshot.Sample(pid_list));

  ProcessInfoSnapshot::ProcInfoEntry proc_info;
  ASSERT_TRUE(snapshot.GetProcInfo(1, &proc_info));
  EXPECT_EQ(1, static_cast<int64>(proc_info.pid));
  EXPECT_EQ(0, static_cast<int64>(proc_info.ppid));
  EXPECT_EQ(0, static_cast<int64>(proc_info.uid));
  EXPECT_EQ(0, static_cast<int64>(proc_info.euid));
  EXPECT_GE(proc_info.rss, 0u);
  EXPECT_GT(proc_info.vsize, 0u);

  // Try out the |Get...OfPID()|, but don't examine the results, since they
  // depend on how we map |ProcInfoEntry| to |...KBytes|.
  base::CommittedKBytes usage;
  EXPECT_TRUE(snapshot.GetCommittedKBytesOfPID(1, &usage));
  base::WorkingSetKBytes ws_usage;
  EXPECT_TRUE(snapshot.GetWorkingSetKBytesOfPID(1, &ws_usage));

  // Make sure it hasn't picked up some other PID (say, 2).
  EXPECT_FALSE(snapshot.GetProcInfo(2, &proc_info));

  // Make sure PID 2 still isn't there (in case I mess up my use of std::map).
  EXPECT_FALSE(snapshot.GetProcInfo(2, &proc_info));

  // Test |Reset()|.
  snapshot.Reset();
  EXPECT_FALSE(snapshot.GetProcInfo(1, &proc_info));
}

TEST_F(ProcessInfoSnapshotMacTest, FindPidSelfTest) {
  // Sample this process and its parent.
  base::ProcessId pid = static_cast<base::ProcessId>(getpid());
  base::ProcessId ppid = static_cast<base::ProcessId>(getppid());
  uid_t uid = getuid();
  uid_t euid = geteuid();
  EXPECT_NE(static_cast<int64>(ppid), 0);

  std::vector<base::ProcessId> pid_list;
  pid_list.push_back(pid);
  pid_list.push_back(ppid);
  ProcessInfoSnapshot snapshot;
  ASSERT_TRUE(snapshot.Sample(pid_list));

  // Find our process.
  ProcessInfoSnapshot::ProcInfoEntry proc_info;
  ASSERT_TRUE(snapshot.GetProcInfo(pid, &proc_info));
  EXPECT_EQ(pid, proc_info.pid);
  EXPECT_EQ(ppid, proc_info.ppid);
  EXPECT_EQ(uid, proc_info.uid);
  EXPECT_EQ(euid, proc_info.euid);
  EXPECT_GE(proc_info.rss, 100u);     // Sanity check: we're running, so we
                                      // should occupy at least 100 kilobytes.
  EXPECT_GE(proc_info.vsize, 1024u);  // Sanity check: our |vsize| is presumably
                                      // at least a megabyte.
  EXPECT_GE(proc_info.rshrd, 1024u);  // Shared memory should also > 1 MB.
  EXPECT_GE(proc_info.rprvt, 1024u);  // Same with private memory.

  // Find our parent.
  ASSERT_TRUE(snapshot.GetProcInfo(ppid, &proc_info));
  EXPECT_EQ(ppid, proc_info.pid);
  EXPECT_NE(static_cast<int64>(proc_info.ppid), 0);
  EXPECT_EQ(uid, proc_info.uid);    // This (and the following) should be true
  EXPECT_EQ(euid, proc_info.euid);  // under reasonable circumstances.
  // Can't say anything definite about its |rss|.
  EXPECT_GT(proc_info.vsize, 0u);   // Its |vsize| should be nonzero though.
}

// To verify that ProcessInfoSnapshot is getting the actual uid and effective
// uid, this test runs top. top should have a uid of the caller and effective
// uid of 0 (root).
TEST_F(ProcessInfoSnapshotMacTest, EffectiveVsRealUserIDTest) {
  // Create a pipe to be able to read top's output.
  int fds[2];
  PCHECK(pipe(fds) == 0);
  base::FileHandleMappingVector fds_to_remap;
  fds_to_remap.push_back(std::make_pair(fds[1], 1));

  // Hook up top's stderr to the test process' stderr.
  fds_to_remap.push_back(std::make_pair(fileno(stderr), 2));

  std::vector<std::string> argv;
  argv.push_back("/usr/bin/top");
  argv.push_back("-l");
  argv.push_back("0");

  base::ProcessHandle process_handle;
  base::LaunchOptions options;
  options.fds_to_remap = &fds_to_remap;
  ASSERT_TRUE(base::LaunchProcess(argv, options, &process_handle));
  PCHECK(HANDLE_EINTR(close(fds[1])) == 0);

  // Wait until there's some output form top. This is an easy way to tell that
  // the exec() call is done and top is actually running.
  char buf[1];
  PCHECK(HANDLE_EINTR(read(fds[0], buf, 1)) == 1);

  std::vector<base::ProcessId> pid_list;
  pid_list.push_back(process_handle);
  ProcessInfoSnapshot snapshot;
  ASSERT_TRUE(snapshot.Sample(pid_list));

  ProcessInfoSnapshot::ProcInfoEntry proc_info;
  ASSERT_TRUE(snapshot.GetProcInfo(process_handle, &proc_info));
  // Effective user ID should be 0 (root).
  EXPECT_EQ(proc_info.euid, 0u);
  // Real user ID should match the calling process's user id.
  EXPECT_EQ(proc_info.uid, geteuid());

  ASSERT_TRUE(base::KillProcess(process_handle, 0, true));
  PCHECK(HANDLE_EINTR(close(fds[0])) == 0);
}
