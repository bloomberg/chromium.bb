// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CHILD_PROCESS_LAUNCHER_H_
#define CONTENT_BROWSER_CHILD_PROCESS_LAUNCHER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/shared_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/process/kill.h"
#include "base/process/process.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/browser/child_process_launcher_helper.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_termination_info.h"
#include "content/public/common/result_codes.h"
#include "mojo/public/cpp/system/invitation.h"

#if defined(OS_ANDROID)
#include "content/public/browser/android/child_process_importance.h"
#endif

namespace base {
class CommandLine;
}

namespace content {

class SandboxedProcessLauncherDelegate;

// Note: These codes are listed in a histogram and any new codes should be added
// at the end.
enum LaunchResultCode {
  // Launch start code, to not overlap with sandbox::ResultCode.
  LAUNCH_RESULT_START = 1001,
  // Launch success.
  LAUNCH_RESULT_SUCCESS,
  // Generic launch failure.
  LAUNCH_RESULT_FAILURE,
  // Placeholder for last item of the enum.
  LAUNCH_RESULT_CODE_LAST_CODE
};

#if defined(OS_WIN)
static_assert(static_cast<int>(LAUNCH_RESULT_START) >
                  static_cast<int>(sandbox::SBOX_ERROR_LAST),
              "LaunchResultCode must not overlap with sandbox::ResultCode");
#endif

struct ChildProcessLauncherPriority {
  ChildProcessLauncherPriority(bool visible,
                               bool has_media_stream,
                               unsigned int frame_depth,
                               bool intersects_viewport,
                               bool boost_for_pending_views,
                               bool should_boost_for_pending_views
#if defined(OS_ANDROID)
                               ,
                               ChildProcessImportance importance
#endif
                               )
      : visible(visible),
        has_media_stream(has_media_stream),
        frame_depth(frame_depth),
        intersects_viewport(intersects_viewport),
        boost_for_pending_views(boost_for_pending_views),
        should_boost_for_pending_views(should_boost_for_pending_views)
#if defined(OS_ANDROID)
        ,
        importance(importance)
#endif
  {
  }

  // Returns true if the child process is backgrounded.
  bool is_background() const {
    return !visible && !has_media_stream &&
           !(should_boost_for_pending_views && boost_for_pending_views);
  }

  bool operator==(const ChildProcessLauncherPriority& other) const;
  bool operator!=(const ChildProcessLauncherPriority& other) const {
    return !(*this == other);
  }

  // Prefer |is_background()| to inspecting these fields individually (to ensure
  // all logic uses the same notion of "backgrounded").

  // |visible| is true if the process is responsible for one or more widget(s)
  // in foreground tabs. The notion of "visible" is determined by the embedder
  // but is ideally a widget in a non-minimized, non-background, non-occluded
  // tab (i.e. with pixels visible on the screen).
  bool visible;

  // |has_media_stream| is true when the process is responsible for "hearable"
  // content.
  bool has_media_stream;

  // |frame_depth| is the depth of the shallowest frame this process is
  // responsible for which has |visible| visibility. It only makes sense to
  // compare this property for two ChildProcessLauncherPriority instances with
  // matching |visible| properties.
  unsigned int frame_depth;

  // |intersects_viewport| is true if this process is responsible for a frame
  // which intersects a viewport which has |visible| visibility. It only makes
  // sense to compare this property for two ChildProcessLauncherPriority
  // instances with matching |visible| properties.
  bool intersects_viewport;

  // |boost_for_pending_views| is true if this process is responsible for a
  // pending view (this is used to boost priority of a process responsible for
  // foreground content which hasn't yet been added as a visible widget -- i.e.
  // during navigation).
  bool boost_for_pending_views;

  // True iff |boost_for_pending_views| should be considered in
  // |is_background()|. This needs to be a separate parameter as opposed to
  // having the experiment set |boost_for_pending_views == false| when
  // |!should_boost_for_pending_views| as that would result in different
  // |is_background()| logic than before and defeat the purpose of the
  // experiment. TODO(gab): Remove this field when the
  // BoostRendererPriorityForPendingViews desktop experiment is over.
  bool should_boost_for_pending_views;
#if defined(OS_ANDROID)
  ChildProcessImportance importance;
#endif
};

// Launches a process asynchronously and notifies the client of the process
// handle when it's available.  It's used to avoid blocking the calling thread
// on the OS since often it can take > 100 ms to create the process.
class CONTENT_EXPORT ChildProcessLauncher {
 public:
  class CONTENT_EXPORT Client {
   public:
    // Will be called on the thread that the ChildProcessLauncher was
    // constructed on.
    virtual void OnProcessLaunched() = 0;

    virtual void OnProcessLaunchFailed(int error_code) {}

   protected:
    virtual ~Client() {}
  };

  // Launches the process asynchronously, calling the client when the result is
  // ready.  Deleting this object before the process is created is safe, since
  // the callback won't be called.  If the process is still running by the time
  // this object destructs, it will be terminated.
  // Takes ownership of cmd_line.
  //
  // If |process_error_callback| is provided, it will be called if a Mojo error
  // is encountered when processing messages from the child process. This
  // callback must be safe to call from any thread.
  ChildProcessLauncher(
      std::unique_ptr<SandboxedProcessLauncherDelegate> delegate,
      std::unique_ptr<base::CommandLine> cmd_line,
      int child_process_id,
      Client* client,
      mojo::OutgoingInvitation mojo_invitation,
      const mojo::ProcessErrorCallback& process_error_callback,
      bool terminate_on_shutdown = true);
  ~ChildProcessLauncher();

  // True if the process is being launched and so the handle isn't available.
  bool IsStarting();

  // Getter for the process.  Only call after the process has started.
  const base::Process& GetProcess() const;

  // Call this when the child process exits to know what happened to it.
  // |known_dead| can be true if we already know the process is dead as it can
  // help the implemention figure the proper TerminationStatus.
  // On Linux, the use of |known_dead| is subtle and can be crucial if an
  // accurate status is important. With |known_dead| set to false, a dead
  // process could be seen as running. With |known_dead| set to true, the
  // process will be killed if it was still running. See ZygoteHostImpl for
  // more discussion of Linux implementation details.
  ChildProcessTerminationInfo GetChildTerminationInfo(bool known_dead);

  // Changes whether the process runs in the background or not.  Only call
  // this after the process has started.
  void SetProcessPriority(const ChildProcessLauncherPriority& priority);

  // Terminates the process associated with this ChildProcessLauncher.
  // Returns true if the process was stopped, false if the process had not been
  // started yet or could not be stopped.
  // Note that |exit_code| is not used on Android.
  bool Terminate(int exit_code);

  // Similar to Terminate() but takes in a |process|.
  // On Android |process| must have been started by ChildProcessLauncher for
  // this method to work.
  static bool TerminateProcess(const base::Process& process, int exit_code);

  // Replaces the ChildProcessLauncher::Client for testing purposes. Returns the
  // previous  client.
  Client* ReplaceClientForTest(Client* client);

  // Sets the files that should be mapped when a new child process is created
  // for the service |service_name|.
  static void SetRegisteredFilesForService(
      const std::string& service_name,
      catalog::RequiredFileMap required_files);

  // Resets all files registered by |SetRegisteredFilesForService|. Used to
  // support multiple shell context creation in unit_tests.
  static void ResetRegisteredFilesForTesting();

 private:
  friend class internal::ChildProcessLauncherHelper;

  // Notifies the client about the result of the operation.
  void Notify(internal::ChildProcessLauncherHelper::Process process,
              int error_code);

  Client* client_;
  BrowserThread::ID client_thread_id_;

  // The process associated with this ChildProcessLauncher. Set in Notify by
  // ChildProcessLauncherHelper once the process was started.
  internal::ChildProcessLauncherHelper::Process process_;

  ChildProcessTerminationInfo termination_info_;
  bool starting_;
  base::TimeTicks start_time_;

  // Controls whether the child process should be terminated on browser
  // shutdown. Default behavior is to terminate the child.
  const bool terminate_child_on_shutdown_;

  scoped_refptr<internal::ChildProcessLauncherHelper> helper_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<ChildProcessLauncher> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ChildProcessLauncher);
};

}  // namespace content

#endif  // CONTENT_BROWSER_CHILD_PROCESS_LAUNCHER_H_
