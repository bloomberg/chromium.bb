// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file/namespace contains utility functions for enumerating, ending and
// computing statistics of processes.

#ifndef BASE_PROCESS_UTIL_H_
#define BASE_PROCESS_UTIL_H_

#include "base/basictypes.h"
#include "base/time/time.h"

#if defined(OS_WIN)
#include <windows.h>
#include <tlhelp32.h>
#elif defined(OS_MACOSX) || defined(OS_BSD)
// malloc_zone_t is defined in <malloc/malloc.h>, but this forward declaration
// is sufficient for GetPurgeableZone() below.
typedef struct _malloc_zone_t malloc_zone_t;
#if !defined(OS_BSD)
#include <mach/mach.h>
#endif
#elif defined(OS_POSIX)
#include <dirent.h>
#include <limits.h>
#include <sys/types.h>
#endif

#include <list>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/base_export.h"
#include "base/files/file_path.h"
#include "base/process.h"
#include "base/process/process_iterator.h"
#include "base/process/process_metrics.h"

#if defined(OS_POSIX)
#include "base/posix/file_descriptor_shuffle.h"
#endif

class CommandLine;

namespace base {

// Return status values from GetTerminationStatus.  Don't use these as
// exit code arguments to KillProcess*(), use platform/application
// specific values instead.
enum TerminationStatus {
  TERMINATION_STATUS_NORMAL_TERMINATION,   // zero exit status
  TERMINATION_STATUS_ABNORMAL_TERMINATION, // non-zero exit status
  TERMINATION_STATUS_PROCESS_WAS_KILLED,   // e.g. SIGKILL or task manager kill
  TERMINATION_STATUS_PROCESS_CRASHED,      // e.g. Segmentation fault
  TERMINATION_STATUS_STILL_RUNNING,        // child hasn't exited yet
  TERMINATION_STATUS_MAX_ENUM
};

#if defined(USE_LINUX_BREAKPAD)
BASE_EXPORT extern size_t g_oom_size;
#endif

#if defined(OS_WIN)
// Output multi-process printf, cout, cerr, etc to the cmd.exe console that ran
// chrome. This is not thread-safe: only call from main thread.
BASE_EXPORT void RouteStdioToConsole();
#endif

// Returns the id of the current process.
BASE_EXPORT ProcessId GetCurrentProcId();

// Returns the ProcessHandle of the current process.
BASE_EXPORT ProcessHandle GetCurrentProcessHandle();

#if defined(OS_WIN)
// Returns the module handle to which an address belongs. The reference count
// of the module is not incremented.
BASE_EXPORT HMODULE GetModuleFromAddress(void* address);
#endif

// Converts a PID to a process handle. This handle must be closed by
// CloseProcessHandle when you are done with it. Returns true on success.
BASE_EXPORT bool OpenProcessHandle(ProcessId pid, ProcessHandle* handle);

// Converts a PID to a process handle. On Windows the handle is opened
// with more access rights and must only be used by trusted code.
// You have to close returned handle using CloseProcessHandle. Returns true
// on success.
// TODO(sanjeevr): Replace all calls to OpenPrivilegedProcessHandle with the
// more specific OpenProcessHandleWithAccess method and delete this.
BASE_EXPORT bool OpenPrivilegedProcessHandle(ProcessId pid,
                                             ProcessHandle* handle);

// Converts a PID to a process handle using the desired access flags. Use a
// combination of the kProcessAccess* flags defined above for |access_flags|.
BASE_EXPORT bool OpenProcessHandleWithAccess(ProcessId pid,
                                             uint32 access_flags,
                                             ProcessHandle* handle);

// Closes the process handle opened by OpenProcessHandle.
BASE_EXPORT void CloseProcessHandle(ProcessHandle process);

// Returns the unique ID for the specified process. This is functionally the
// same as Windows' GetProcessId(), but works on versions of Windows before
// Win XP SP1 as well.
BASE_EXPORT ProcessId GetProcId(ProcessHandle process);

#if defined(OS_LINUX) || defined(OS_ANDROID) || defined(OS_BSD)
// Returns the path to the executable of the given process.
BASE_EXPORT FilePath GetProcessExecutablePath(ProcessHandle process);
#endif

#if defined(OS_LINUX) || defined(OS_ANDROID)
// Get the number of threads of |process| as available in /proc/<pid>/stat.
// This should be used with care as no synchronization with running threads is
// done. This is mostly useful to guarantee being single-threaded.
// Returns 0 on failure.
BASE_EXPORT int GetNumberOfThreads(ProcessHandle process);

// The maximum allowed value for the OOM score.
const int kMaxOomScore = 1000;

// This adjusts /proc/<pid>/oom_score_adj so the Linux OOM killer will
// prefer to kill certain process types over others. The range for the
// adjustment is [-1000, 1000], with [0, 1000] being user accessible.
// If the Linux system doesn't support the newer oom_score_adj range
// of [0, 1000], then we revert to using the older oom_adj, and
// translate the given value into [0, 15].  Some aliasing of values
// may occur in that case, of course.
BASE_EXPORT bool AdjustOOMScore(ProcessId process, int score);

// /proc/self/exe refers to the current executable.
BASE_EXPORT extern const char kProcSelfExe[];
#endif  // defined(OS_LINUX) || defined(OS_ANDROID)

#if defined(OS_POSIX)
// Returns the ID for the parent of the given process.
BASE_EXPORT ProcessId GetParentProcessId(ProcessHandle process);

// Returns the maximum number of file descriptors that can be open by a process
// at once. If the number is unavailable, a conservative best guess is returned.
size_t GetMaxFds();

// Close all file descriptors, except those which are a destination in the
// given multimap. Only call this function in a child process where you know
// that there aren't any other threads.
BASE_EXPORT void CloseSuperfluousFds(const InjectiveMultimap& saved_map);
#endif  // defined(OS_POSIX)

typedef std::vector<std::pair<std::string, std::string> > EnvironmentVector;
typedef std::vector<std::pair<int, int> > FileHandleMappingVector;

// Options for launching a subprocess that are passed to LaunchProcess().
// The default constructor constructs the object with default options.
struct LaunchOptions {
  LaunchOptions()
      : wait(false),
#if defined(OS_WIN)
        start_hidden(false),
        inherit_handles(false),
        as_user(NULL),
        empty_desktop_name(false),
        job_handle(NULL),
        stdin_handle(NULL),
        stdout_handle(NULL),
        stderr_handle(NULL),
        force_breakaway_from_job_(false)
#else
        environ(NULL),
        fds_to_remap(NULL),
        maximize_rlimits(NULL),
        new_process_group(false)
#if defined(OS_LINUX)
        , clone_flags(0)
#endif  // OS_LINUX
#if defined(OS_CHROMEOS)
        , ctrl_terminal_fd(-1)
#endif  // OS_CHROMEOS
#endif  // !defined(OS_WIN)
        {}

  // If true, wait for the process to complete.
  bool wait;

#if defined(OS_WIN)
  bool start_hidden;

  // If true, the new process inherits handles from the parent. In production
  // code this flag should be used only when running short-lived, trusted
  // binaries, because open handles from other libraries and subsystems will
  // leak to the child process, causing errors such as open socket hangs.
  bool inherit_handles;

  // If non-NULL, runs as if the user represented by the token had launched it.
  // Whether the application is visible on the interactive desktop depends on
  // the token belonging to an interactive logon session.
  //
  // To avoid hard to diagnose problems, when specified this loads the
  // environment variables associated with the user and if this operation fails
  // the entire call fails as well.
  UserTokenHandle as_user;

  // If true, use an empty string for the desktop name.
  bool empty_desktop_name;

  // If non-NULL, launches the application in that job object. The process will
  // be terminated immediately and LaunchProcess() will fail if assignment to
  // the job object fails.
  HANDLE job_handle;

  // Handles for the redirection of stdin, stdout and stderr. The handles must
  // be inheritable. Caller should either set all three of them or none (i.e.
  // there is no way to redirect stderr without redirecting stdin). The
  // |inherit_handles| flag must be set to true when redirecting stdio stream.
  HANDLE stdin_handle;
  HANDLE stdout_handle;
  HANDLE stderr_handle;

  // If set to true, ensures that the child process is launched with the
  // CREATE_BREAKAWAY_FROM_JOB flag which allows it to breakout of the parent
  // job if any.
  bool force_breakaway_from_job_;
#else
  // If non-NULL, set/unset environment variables.
  // See documentation of AlterEnvironment().
  // This pointer is owned by the caller and must live through the
  // call to LaunchProcess().
  const EnvironmentVector* environ;

  // If non-NULL, remap file descriptors according to the mapping of
  // src fd->dest fd to propagate FDs into the child process.
  // This pointer is owned by the caller and must live through the
  // call to LaunchProcess().
  const FileHandleMappingVector* fds_to_remap;

  // Each element is an RLIMIT_* constant that should be raised to its
  // rlim_max.  This pointer is owned by the caller and must live through
  // the call to LaunchProcess().
  const std::set<int>* maximize_rlimits;

  // If true, start the process in a new process group, instead of
  // inheriting the parent's process group.  The pgid of the child process
  // will be the same as its pid.
  bool new_process_group;

#if defined(OS_LINUX)
  // If non-zero, start the process using clone(), using flags as provided.
  int clone_flags;
#endif  // defined(OS_LINUX)

#if defined(OS_CHROMEOS)
  // If non-negative, the specified file descriptor will be set as the launched
  // process' controlling terminal.
  int ctrl_terminal_fd;
#endif  // defined(OS_CHROMEOS)

#endif  // !defined(OS_WIN)
};

// Launch a process via the command line |cmdline|.
// See the documentation of LaunchOptions for details on |options|.
//
// Returns true upon success.
//
// Upon success, if |process_handle| is non-NULL, it will be filled in with the
// handle of the launched process.  NOTE: In this case, the caller is
// responsible for closing the handle so that it doesn't leak!
// Otherwise, the process handle will be implicitly closed.
//
// Unix-specific notes:
// - All file descriptors open in the parent process will be closed in the
//   child process except for any preserved by options::fds_to_remap, and
//   stdin, stdout, and stderr. If not remapped by options::fds_to_remap,
//   stdin is reopened as /dev/null, and the child is allowed to inherit its
//   parent's stdout and stderr.
// - If the first argument on the command line does not contain a slash,
//   PATH will be searched.  (See man execvp.)
BASE_EXPORT bool LaunchProcess(const CommandLine& cmdline,
                               const LaunchOptions& options,
                               ProcessHandle* process_handle);

#if defined(OS_WIN)

enum IntegrityLevel {
  INTEGRITY_UNKNOWN,
  LOW_INTEGRITY,
  MEDIUM_INTEGRITY,
  HIGH_INTEGRITY,
};
// Determine the integrity level of the specified process. Returns false
// if the system does not support integrity levels (pre-Vista) or in the case
// of an underlying system failure.
BASE_EXPORT bool GetProcessIntegrityLevel(ProcessHandle process,
                                          IntegrityLevel* level);

// Windows-specific LaunchProcess that takes the command line as a
// string.  Useful for situations where you need to control the
// command line arguments directly, but prefer the CommandLine version
// if launching Chrome itself.
//
// The first command line argument should be the path to the process,
// and don't forget to quote it.
//
// Example (including literal quotes)
//  cmdline = "c:\windows\explorer.exe" -foo "c:\bar\"
BASE_EXPORT bool LaunchProcess(const string16& cmdline,
                               const LaunchOptions& options,
                               ProcessHandle* process_handle);

#elif defined(OS_POSIX)
// A POSIX-specific version of LaunchProcess that takes an argv array
// instead of a CommandLine.  Useful for situations where you need to
// control the command line arguments directly, but prefer the
// CommandLine version if launching Chrome itself.
BASE_EXPORT bool LaunchProcess(const std::vector<std::string>& argv,
                               const LaunchOptions& options,
                               ProcessHandle* process_handle);

// AlterEnvironment returns a modified environment vector, constructed from the
// given environment and the list of changes given in |changes|. Each key in
// the environment is matched against the first element of the pairs. In the
// event of a match, the value is replaced by the second of the pair, unless
// the second is empty, in which case the key-value is removed.
//
// The returned array is allocated using new[] and must be freed by the caller.
BASE_EXPORT char** AlterEnvironment(const EnvironmentVector& changes,
                                    const char* const* const env);
#endif  // defined(OS_POSIX)

#if defined(OS_WIN)
// Set JOBOBJECT_EXTENDED_LIMIT_INFORMATION to JobObject |job_object|.
// As its limit_info.BasicLimitInformation.LimitFlags has
// JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE.
// When the provide JobObject |job_object| is closed, the binded process will
// be terminated.
BASE_EXPORT bool SetJobObjectAsKillOnJobClose(HANDLE job_object);
#endif  // defined(OS_WIN)

// Executes the application specified by |cl| and wait for it to exit. Stores
// the output (stdout) in |output|. Redirects stderr to /dev/null. Returns true
// on success (application launched and exited cleanly, with exit code
// indicating success).
BASE_EXPORT bool GetAppOutput(const CommandLine& cl, std::string* output);

#if defined(OS_POSIX)
// A POSIX-specific version of GetAppOutput that takes an argv array
// instead of a CommandLine.  Useful for situations where you need to
// control the command line arguments directly.
BASE_EXPORT bool GetAppOutput(const std::vector<std::string>& argv,
                              std::string* output);

// A restricted version of |GetAppOutput()| which (a) clears the environment,
// and (b) stores at most |max_output| bytes; also, it doesn't search the path
// for the command.
BASE_EXPORT bool GetAppOutputRestricted(const CommandLine& cl,
                                        std::string* output, size_t max_output);

// A version of |GetAppOutput()| which also returns the exit code of the
// executed command. Returns true if the application runs and exits cleanly. If
// this is the case the exit code of the application is available in
// |*exit_code|.
BASE_EXPORT bool GetAppOutputWithExitCode(const CommandLine& cl,
                                          std::string* output, int* exit_code);
#endif  // defined(OS_POSIX)

// Attempts to kill all the processes on the current machine that were launched
// from the given executable name, ending them with the given exit code.  If
// filter is non-null, then only processes selected by the filter are killed.
// Returns true if all processes were able to be killed off, false if at least
// one couldn't be killed.
BASE_EXPORT bool KillProcesses(const FilePath::StringType& executable_name,
                               int exit_code, const ProcessFilter* filter);

// Attempts to kill the process identified by the given process
// entry structure, giving it the specified exit code. If |wait| is true, wait
// for the process to be actually terminated before returning.
// Returns true if this is successful, false otherwise.
BASE_EXPORT bool KillProcess(ProcessHandle process, int exit_code, bool wait);

#if defined(OS_POSIX)
// Attempts to kill the process group identified by |process_group_id|. Returns
// true on success.
BASE_EXPORT bool KillProcessGroup(ProcessHandle process_group_id);
#endif  // defined(OS_POSIX)

#if defined(OS_WIN)
BASE_EXPORT bool KillProcessById(ProcessId process_id, int exit_code,
                                 bool wait);
#endif  // defined(OS_WIN)

// Get the termination status of the process by interpreting the
// circumstances of the child process' death. |exit_code| is set to
// the status returned by waitpid() on POSIX, and from
// GetExitCodeProcess() on Windows.  |exit_code| may be NULL if the
// caller is not interested in it.  Note that on Linux, this function
// will only return a useful result the first time it is called after
// the child exits (because it will reap the child and the information
// will no longer be available).
BASE_EXPORT TerminationStatus GetTerminationStatus(ProcessHandle handle,
                                                   int* exit_code);

#if defined(OS_POSIX)
// Wait for the process to exit and get the termination status. See
// GetTerminationStatus for more information. On POSIX systems, we can't call
// WaitForExitCode and then GetTerminationStatus as the child will be reaped
// when WaitForExitCode return and this information will be lost.
BASE_EXPORT TerminationStatus WaitForTerminationStatus(ProcessHandle handle,
                                                       int* exit_code);
#endif  // defined(OS_POSIX)

// Waits for process to exit. On POSIX systems, if the process hasn't been
// signaled then puts the exit code in |exit_code|; otherwise it's considered
// a failure. On Windows |exit_code| is always filled. Returns true on success,
// and closes |handle| in any case.
BASE_EXPORT bool WaitForExitCode(ProcessHandle handle, int* exit_code);

// Waits for process to exit. If it did exit within |timeout_milliseconds|,
// then puts the exit code in |exit_code|, and returns true.
// In POSIX systems, if the process has been signaled then |exit_code| is set
// to -1. Returns false on failure (the caller is then responsible for closing
// |handle|).
// The caller is always responsible for closing the |handle|.
BASE_EXPORT bool WaitForExitCodeWithTimeout(ProcessHandle handle,
                                            int* exit_code,
                                            base::TimeDelta timeout);

// Wait for all the processes based on the named executable to exit.  If filter
// is non-null, then only processes selected by the filter are waited on.
// Returns after all processes have exited or wait_milliseconds have expired.
// Returns true if all the processes exited, false otherwise.
BASE_EXPORT bool WaitForProcessesToExit(
    const FilePath::StringType& executable_name,
    base::TimeDelta wait,
    const ProcessFilter* filter);

// Wait for a single process to exit. Return true if it exited cleanly within
// the given time limit. On Linux |handle| must be a child process, however
// on Mac and Windows it can be any process.
BASE_EXPORT bool WaitForSingleProcess(ProcessHandle handle,
                                      base::TimeDelta wait);

// Waits a certain amount of time (can be 0) for all the processes with a given
// executable name to exit, then kills off any of them that are still around.
// If filter is non-null, then only processes selected by the filter are waited
// on.  Killed processes are ended with the given exit code.  Returns false if
// any processes needed to be killed, true if they all exited cleanly within
// the wait_milliseconds delay.
BASE_EXPORT bool CleanupProcesses(const FilePath::StringType& executable_name,
                                  base::TimeDelta wait,
                                  int exit_code,
                                  const ProcessFilter* filter);

// This method ensures that the specified process eventually terminates, and
// then it closes the given process handle.
//
// It assumes that the process has already been signalled to exit, and it
// begins by waiting a small amount of time for it to exit.  If the process
// does not appear to have exited, then this function starts to become
// aggressive about ensuring that the process terminates.
//
// On Linux this method does not block the calling thread.
// On OS X this method may block for up to 2 seconds.
//
// NOTE: The process handle must have been opened with the PROCESS_TERMINATE
// and SYNCHRONIZE permissions.
//
BASE_EXPORT void EnsureProcessTerminated(ProcessHandle process_handle);

#if defined(OS_POSIX) && !defined(OS_MACOSX)
// The nicer version of EnsureProcessTerminated() that is patient and will
// wait for |process_handle| to finish and then reap it.
BASE_EXPORT void EnsureProcessGetsReaped(ProcessHandle process_handle);
#endif

// Enables low fragmentation heap (LFH) for every heaps of this process. This
// won't have any effect on heaps created after this function call. It will not
// modify data allocated in the heaps before calling this function. So it is
// better to call this function early in initialization and again before
// entering the main loop.
// Note: Returns true on Windows 2000 without doing anything.
BASE_EXPORT bool EnableLowFragmentationHeap();

// Enables 'terminate on heap corruption' flag. Helps protect against heap
// overflow. Has no effect if the OS doesn't provide the necessary facility.
BASE_EXPORT void EnableTerminationOnHeapCorruption();

// Turns on process termination if memory runs out.
BASE_EXPORT void EnableTerminationOnOutOfMemory();

// If supported on the platform, and the user has sufficent rights, increase
// the current process's scheduling priority to a high priority.
BASE_EXPORT void RaiseProcessToHighPriority();

#if defined(OS_MACOSX)
// Restore the default exception handler, setting it to Apple Crash Reporter
// (ReportCrash).  When forking and execing a new process, the child will
// inherit the parent's exception ports, which may be set to the Breakpad
// instance running inside the parent.  The parent's Breakpad instance should
// not handle the child's exceptions.  Calling RestoreDefaultExceptionHandler
// in the child after forking will restore the standard exception handler.
// See http://crbug.com/20371/ for more details.
void RestoreDefaultExceptionHandler();
#endif  // defined(OS_MACOSX)

#if defined(OS_MACOSX)
// Very large images or svg canvases can cause huge mallocs.  Skia
// does tricks on tcmalloc-based systems to allow malloc to fail with
// a NULL rather than hit the oom crasher.  This replicates that for
// OSX.
//
// IF YOU USE THIS WITHOUT CONSULTING YOUR FRIENDLY OSX DEVELOPER,
// YOUR CODE IS LIKELY TO BE REVERTED.  THANK YOU.
//
// TODO(shess): Weird place to put it, but this is where the OOM
// killer currently lives.
BASE_EXPORT void* UncheckedMalloc(size_t size);
#endif  // defined(OS_MACOSX)

}  // namespace base

#endif  // BASE_PROCESS_UTIL_H_
