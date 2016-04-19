// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This test validates that the ProcessSingleton class properly makes sure
// that there is only one main browser process.
//
// It is currently compiled and run on Windows and Posix(non-Mac) platforms.
// Mac uses system services and ProcessSingletonMac is a noop.  (Maybe it still
// makes sense to test that the system services are giving the behavior we
// want?)

#include <stddef.h>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/process/process_iterator.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/test_launcher_utils.h"

namespace {

// This is for the code that is to be ran in multiple threads at once,
// to stress a race condition on first process start.
// We use the thread safe ref counted base class so that we can use the
// base::Bind to run the StartChrome methods in many threads.
class ChromeStarter : public base::RefCountedThreadSafe<ChromeStarter> {
 public:
  ChromeStarter(base::TimeDelta timeout, const base::FilePath& user_data_dir)
      : ready_event_(false /* manual */, false /* signaled */),
        done_event_(false /* manual */, false /* signaled */),
        process_terminated_(false),
        timeout_(timeout),
        user_data_dir_(user_data_dir) {
  }

  // We must reset some data members since we reuse the same ChromeStarter
  // object and start/stop it a few times. We must start fresh! :-)
  void Reset() {
    ready_event_.Reset();
    done_event_.Reset();
    if (process_.IsValid())
      process_.Close();
    process_terminated_ = false;
  }

  void StartChrome(base::WaitableEvent* start_event, bool first_run) {
    // TODO(mattm): maybe stuff should be refactored to use
    // UITest::LaunchBrowserHelper somehow?
    base::FilePath program;
    ASSERT_TRUE(PathService::Get(base::FILE_EXE, &program));
    base::CommandLine command_line(program);
    command_line.AppendSwitchPath(switches::kUserDataDir, user_data_dir_);

    if (first_run)
      command_line.AppendSwitch(switches::kForceFirstRun);
    else
      command_line.AppendSwitch(switches::kNoFirstRun);

    // Add the normal test-mode switches, except for the ones we're adding
    // ourselves.
    base::CommandLine standard_switches(base::CommandLine::NO_PROGRAM);
    test_launcher_utils::PrepareBrowserCommandLineForTests(&standard_switches);
    const base::CommandLine::SwitchMap& switch_map =
        standard_switches.GetSwitches();
    for (base::CommandLine::SwitchMap::const_iterator i = switch_map.begin();
         i != switch_map.end(); ++i) {
      const std::string& switch_name = i->first;
      if (switch_name == switches::kUserDataDir ||
          switch_name == switches::kForceFirstRun ||
          switch_name == switches::kNoFirstRun)
        continue;

      command_line.AppendSwitchNative(switch_name, i->second);
    }

    // Try to get all threads to launch the app at the same time.
    // So let the test know we are ready.
    ready_event_.Signal();
    // And then wait for the test to tell us to GO!
    ASSERT_NE(static_cast<base::WaitableEvent*>(NULL), start_event);
    start_event->Wait();

    // Here we don't wait for the app to be terminated because one of the
    // process will stay alive while the others will be restarted. If we would
    // wait here, we would never get a handle to the main process...
    process_ = base::LaunchProcess(command_line, base::LaunchOptions());
    ASSERT_TRUE(process_.IsValid());

    // We can wait on the handle here, we should get stuck on one and only
    // one process. The test below will take care of killing that process
    // to unstuck us once it confirms there is only one.
    int exit_code;
    process_terminated_ = process_.WaitForExitWithTimeout(timeout_, &exit_code);
    // Let the test know we are done.
    done_event_.Signal();
  }

  // Public access to simplify the test code using them.
  base::WaitableEvent ready_event_;
  base::WaitableEvent done_event_;
  base::Process process_;
  bool process_terminated_;

 private:
  friend class base::RefCountedThreadSafe<ChromeStarter>;

  ~ChromeStarter() {}

  base::TimeDelta timeout_;
  base::FilePath user_data_dir_;

  DISALLOW_COPY_AND_ASSIGN(ChromeStarter);
};

}  // namespace

// Our test fixture that initializes and holds onto a few global vars.
class ProcessSingletonTest : public InProcessBrowserTest {
 public:
  ProcessSingletonTest()
      // We use a manual reset so that all threads wake up at once when signaled
      // and thus we must manually reset it for each attempt.
      : threads_waker_(true /* manual */, false /* signaled */) {
    EXPECT_TRUE(temp_profile_dir_.CreateUniqueTempDir());
  }

  void SetUp() override {
    InProcessBrowserTest::SetUp();
    // Start the threads and create the starters.
    for (size_t i = 0; i < kNbThreads; ++i) {
      chrome_starter_threads_[i].reset(new base::Thread("ChromeStarter"));
      ASSERT_TRUE(chrome_starter_threads_[i]->Start());
      chrome_starters_[i] = new ChromeStarter(
          TestTimeouts::action_max_timeout(), temp_profile_dir_.path());
    }
  }

  void TearDown() override {
    // Stop the threads.
    for (size_t i = 0; i < kNbThreads; ++i)
      chrome_starter_threads_[i]->Stop();
  }

  // This method is used to make sure we kill the main browser process after
  // all of its child processes have successfully attached to it. This was added
  // when we realized that if we just kill the parent process right away, we
  // sometimes end up with dangling child processes. If we Sleep for a certain
  // amount of time, we are OK... So we introduced this method to avoid a
  // flaky wait. Instead, we kill all descendants of the main process after we
  // killed it, relying on the fact that we can still get the parent id of a
  // child process, even when the parent dies.
  void KillProcessTree(const base::Process& process) {
    class ProcessTreeFilter : public base::ProcessFilter {
     public:
      explicit ProcessTreeFilter(base::ProcessId parent_pid) {
        ancestor_pids_.insert(parent_pid);
      }
      bool Includes(const base::ProcessEntry& entry) const override {
        if (ancestor_pids_.find(entry.parent_pid()) != ancestor_pids_.end()) {
          ancestor_pids_.insert(entry.pid());
          return true;
        } else {
          return false;
        }
      }
     private:
      mutable std::set<base::ProcessId> ancestor_pids_;
    } process_tree_filter(process.Pid());

    // Start by explicitly killing the main process we know about...
    static const int kExitCode = 42;
    EXPECT_TRUE(process.Terminate(kExitCode, true /* wait */));

    // Then loop until we can't find any of its descendant.
    // But don't try more than kNbTries times...
    static const int kNbTries = 10;
    int num_tries = 0;
    base::FilePath program;
    ASSERT_TRUE(PathService::Get(base::FILE_EXE, &program));
    base::FilePath::StringType exe_name = program.BaseName().value();
    while (base::GetProcessCount(exe_name, &process_tree_filter) > 0 &&
           num_tries++ < kNbTries) {
      base::KillProcesses(exe_name, kExitCode, &process_tree_filter);
    }
    DLOG_IF(ERROR, num_tries >= kNbTries) << "Failed to kill all processes!";
  }

  // Since this is a hard to reproduce problem, we make a few attempts.
  // We stop the attempts at the first error, and when there are no errors,
  // we don't time-out of any wait, so it executes quite fast anyway.
  static const size_t kNbAttempts = 5;

  // The idea is to start chrome from multiple threads all at once.
  static const size_t kNbThreads = 5;
  scoped_refptr<ChromeStarter> chrome_starters_[kNbThreads];
  std::unique_ptr<base::Thread> chrome_starter_threads_[kNbThreads];

  // The event that will get all threads to wake up simultaneously and try
  // to start a chrome process at the same time.
  base::WaitableEvent threads_waker_;

  // We don't want to use the default profile, but can't use UITest's since we
  // don't use UITest::LaunchBrowser.
  base::ScopedTempDir temp_profile_dir_;
};

// Disabled on all platforms after code rot due to http://crbug.com/513534.
// Originally disabled on some platforms due to http://crbug.com/58219.
IN_PROC_BROWSER_TEST_F(ProcessSingletonTest, DISABLED_StartupRaceCondition) {
  // We use this to stop the attempts loop on the first failure.
  bool failed = false;
  for (size_t attempt = 0; attempt < kNbAttempts && !failed; ++attempt) {
    SCOPED_TRACE(testing::Message() << "Attempt: " << attempt << ".");
    // We use a single event to get all threads to do the AppLaunch at the same
    // time...
    threads_waker_.Reset();

    // Test both with and without the first-run dialog, since they exercise
    // different paths.
#if defined(OS_POSIX)
    // TODO(mattm): test first run dialog singleton handling on linux too.
    // On posix if we test the first run dialog, GracefulShutdownHandler gets
    // the TERM signal, but since the message loop isn't running during the gtk
    // first run dialog, the ShutdownDetector never handles it, and KillProcess
    // has to time out (60 sec!) and SIGKILL.
    bool first_run = false;
#else
    // Test for races in both regular start up and first run start up cases.
    bool first_run = attempt % 2;
#endif

    // Here we prime all the threads with a ChromeStarter that will wait for
    // our signal to launch its chrome process.
    for (size_t i = 0; i < kNbThreads; ++i) {
      ASSERT_NE(static_cast<ChromeStarter*>(NULL), chrome_starters_[i].get());
      chrome_starters_[i]->Reset();

      ASSERT_TRUE(chrome_starter_threads_[i]->IsRunning());
      ASSERT_NE(static_cast<base::MessageLoop*>(NULL),
                chrome_starter_threads_[i]->message_loop());

      chrome_starter_threads_[i]->task_runner()->PostTask(
          FROM_HERE,
          base::Bind(&ChromeStarter::StartChrome, chrome_starters_[i].get(),
                     &threads_waker_, first_run));
    }

    // Wait for all the starters to be ready.
    // We could replace this loop if we ever implement a WaitAll().
    for (size_t i = 0; i < kNbThreads; ++i) {
      SCOPED_TRACE(testing::Message() << "Waiting on thread: " << i << ".");
      chrome_starters_[i]->ready_event_.Wait();
    }
    // GO!
    threads_waker_.Signal();

    // As we wait for all threads to signal that they are done, we remove their
    // index from this vector so that we get left with only the index of
    // the thread that started the main process.
    std::vector<size_t> pending_starters(kNbThreads);
    for (size_t i = 0; i < kNbThreads; ++i)
      pending_starters[i] = i;

    // We use a local array of starter's done events we must wait on...
    // These are collected from the starters that we have not yet been removed
    // from the pending_starters vector.
    base::WaitableEvent* starters_done_events[kNbThreads];
    // At the end, "There can be only one" main browser process alive.
    while (pending_starters.size() > 1) {
      SCOPED_TRACE(testing::Message() << pending_starters.size() <<
                   " starters left.");
      for (size_t i = 0; i < pending_starters.size(); ++i) {
        starters_done_events[i] =
            &chrome_starters_[pending_starters[i]]->done_event_;
      }
      size_t done_index = base::WaitableEvent::WaitMany(
          starters_done_events, pending_starters.size());
      size_t starter_index = pending_starters[done_index];
      // If the starter is done but has not marked itself as terminated,
      // it is because it timed out of its WaitForExitCodeWithTimeout(). Only
      // the last one standing should be left waiting... So we failed...
      EXPECT_TRUE(chrome_starters_[starter_index]->process_terminated_ ||
                  failed) << "There is more than one main process.";
      if (!chrome_starters_[starter_index]->process_terminated_) {
        // This will stop the "for kNbAttempts" loop.
        failed = true;
        // But we let the last loop turn finish so that we can properly
        // kill all remaining processes. Starting with this one...
        if (chrome_starters_[starter_index]->process_.IsValid()) {
          KillProcessTree(chrome_starters_[starter_index]->process_);
        }
      }
      pending_starters.erase(pending_starters.begin() + done_index);
    }

    // "There can be only one!" :-)
    ASSERT_EQ(static_cast<size_t>(1), pending_starters.size());
    size_t last_index = pending_starters.front();
    pending_starters.clear();
    if (chrome_starters_[last_index]->process_.IsValid()) {
      KillProcessTree(chrome_starters_[last_index]->process_);
      chrome_starters_[last_index]->done_event_.Wait();
    }
  }
}
