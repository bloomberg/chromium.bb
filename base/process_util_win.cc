// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process_util.h"

#include <fcntl.h>
#include <io.h>
#include <windows.h>
#include <userenv.h>
#include <psapi.h>

#include <ios>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/debug/stack_trace.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/sys_info.h"
#include "base/win/object_watcher.h"
#include "base/win/scoped_handle.h"
#include "base/win/scoped_process_information.h"
#include "base/win/windows_version.h"

// userenv.dll is required for CreateEnvironmentBlock().
#pragma comment(lib, "userenv.lib")

namespace base {

namespace {

// Exit codes with special meanings on Windows.
const DWORD kNormalTerminationExitCode = 0;
const DWORD kDebuggerInactiveExitCode = 0xC0000354;
const DWORD kKeyboardInterruptExitCode = 0xC000013A;
const DWORD kDebuggerTerminatedExitCode = 0x40010004;

// Maximum amount of time (in milliseconds) to wait for the process to exit.
static const int kWaitInterval = 2000;

// This exit code is used by the Windows task manager when it kills a
// process.  It's value is obviously not that unique, and it's
// surprising to me that the task manager uses this value, but it
// seems to be common practice on Windows to test for it as an
// indication that the task manager has killed something if the
// process goes away.
const DWORD kProcessKilledExitCode = 1;

// HeapSetInformation function pointer.
typedef BOOL (WINAPI* HeapSetFn)(HANDLE, HEAP_INFORMATION_CLASS, PVOID, SIZE_T);

void OnNoMemory() {
  // Kill the process. This is important for security, since WebKit doesn't
  // NULL-check many memory allocations. If a malloc fails, returns NULL, and
  // the buffer is then used, it provides a handy mapping of memory starting at
  // address 0 for an attacker to utilize.
  __debugbreak();
  _exit(1);
}

class TimerExpiredTask : public win::ObjectWatcher::Delegate {
 public:
  explicit TimerExpiredTask(ProcessHandle process);
  ~TimerExpiredTask();

  void TimedOut();

  // MessageLoop::Watcher -----------------------------------------------------
  virtual void OnObjectSignaled(HANDLE object);

 private:
  void KillProcess();

  // The process that we are watching.
  ProcessHandle process_;

  win::ObjectWatcher watcher_;

  DISALLOW_COPY_AND_ASSIGN(TimerExpiredTask);
};

TimerExpiredTask::TimerExpiredTask(ProcessHandle process) : process_(process) {
  watcher_.StartWatching(process_, this);
}

TimerExpiredTask::~TimerExpiredTask() {
  TimedOut();
  DCHECK(!process_) << "Make sure to close the handle.";
}

void TimerExpiredTask::TimedOut() {
  if (process_)
    KillProcess();
}

void TimerExpiredTask::OnObjectSignaled(HANDLE object) {
  CloseHandle(process_);
  process_ = NULL;
}

void TimerExpiredTask::KillProcess() {
  // Stop watching the process handle since we're killing it.
  watcher_.StopWatching();

  // OK, time to get frisky.  We don't actually care when the process
  // terminates.  We just care that it eventually terminates, and that's what
  // TerminateProcess should do for us. Don't check for the result code since
  // it fails quite often. This should be investigated eventually.
  base::KillProcess(process_, kProcessKilledExitCode, false);

  // Now, just cleanup as if the process exited normally.
  OnObjectSignaled(process_);
}

}  // namespace

void RouteStdioToConsole() {
  // Don't change anything if stdout or stderr already point to a
  // valid stream.
  //
  // If we are running under Buildbot or under Cygwin's default
  // terminal (mintty), stderr and stderr will be pipe handles.  In
  // that case, we don't want to open CONOUT$, because its output
  // likely does not go anywhere.
  //
  // We don't use GetStdHandle() to check stdout/stderr here because
  // it can return dangling IDs of handles that were never inherited
  // by this process.  These IDs could have been reused by the time
  // this function is called.  The CRT checks the validity of
  // stdout/stderr on startup (before the handle IDs can be reused).
  // _fileno(stdout) will return -2 (_NO_CONSOLE_FILENO) if stdout was
  // invalid.
  if (_fileno(stdout) >= 0 || _fileno(stderr) >= 0)
    return;

  if (!AttachConsole(ATTACH_PARENT_PROCESS)) {
    unsigned int result = GetLastError();
    // Was probably already attached.
    if (result == ERROR_ACCESS_DENIED)
      return;
    // Don't bother creating a new console for each child process if the
    // parent process is invalid (eg: crashed).
    if (result == ERROR_GEN_FAILURE)
      return;
    // Make a new console if attaching to parent fails with any other error.
    // It should be ERROR_INVALID_HANDLE at this point, which means the browser
    // was likely not started from a console.
    AllocConsole();
  }

  // Arbitrary byte count to use when buffering output lines.  More
  // means potential waste, less means more risk of interleaved
  // log-lines in output.
  enum { kOutputBufferSize = 64 * 1024 };

  if (freopen("CONOUT$", "w", stdout)) {
    setvbuf(stdout, NULL, _IOLBF, kOutputBufferSize);
    // Overwrite FD 1 for the benefit of any code that uses this FD
    // directly.  This is safe because the CRT allocates FDs 0, 1 and
    // 2 at startup even if they don't have valid underlying Windows
    // handles.  This means we won't be overwriting an FD created by
    // _open() after startup.
    _dup2(_fileno(stdout), 1);
  }
  if (freopen("CONOUT$", "w", stderr)) {
    setvbuf(stderr, NULL, _IOLBF, kOutputBufferSize);
    _dup2(_fileno(stderr), 2);
  }

  // Fix all cout, wcout, cin, wcin, cerr, wcerr, clog and wclog.
  std::ios::sync_with_stdio();
}

ProcessId GetCurrentProcId() {
  return ::GetCurrentProcessId();
}

ProcessHandle GetCurrentProcessHandle() {
  return ::GetCurrentProcess();
}

HMODULE GetModuleFromAddress(void* address) {
  HMODULE instance = NULL;
  if (!::GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            static_cast<char*>(address),
                            &instance)) {
    NOTREACHED();
  }
  return instance;
}

bool OpenProcessHandle(ProcessId pid, ProcessHandle* handle) {
  // We try to limit privileges granted to the handle. If you need this
  // for test code, consider using OpenPrivilegedProcessHandle instead of
  // adding more privileges here.
  ProcessHandle result = OpenProcess(PROCESS_TERMINATE |
                                     PROCESS_QUERY_INFORMATION |
                                     SYNCHRONIZE,
                                     FALSE, pid);

  if (result == NULL)
    return false;

  *handle = result;
  return true;
}

bool OpenPrivilegedProcessHandle(ProcessId pid, ProcessHandle* handle) {
  ProcessHandle result = OpenProcess(PROCESS_DUP_HANDLE |
                                     PROCESS_TERMINATE |
                                     PROCESS_QUERY_INFORMATION |
                                     PROCESS_VM_READ |
                                     SYNCHRONIZE,
                                     FALSE, pid);

  if (result == NULL)
    return false;

  *handle = result;
  return true;
}

bool OpenProcessHandleWithAccess(ProcessId pid,
                                 uint32 access_flags,
                                 ProcessHandle* handle) {
  ProcessHandle result = OpenProcess(access_flags, FALSE, pid);

  if (result == NULL)
    return false;

  *handle = result;
  return true;
}

void CloseProcessHandle(ProcessHandle process) {
  CloseHandle(process);
}

ProcessId GetProcId(ProcessHandle process) {
  // This returns 0 if we have insufficient rights to query the process handle.
  return GetProcessId(process);
}

bool GetProcessIntegrityLevel(ProcessHandle process, IntegrityLevel *level) {
  if (!level)
    return false;

  if (win::GetVersion() < base::win::VERSION_VISTA)
    return false;

  HANDLE process_token;
  if (!OpenProcessToken(process, TOKEN_QUERY | TOKEN_QUERY_SOURCE,
      &process_token))
    return false;

  win::ScopedHandle scoped_process_token(process_token);

  DWORD token_info_length = 0;
  if (GetTokenInformation(process_token, TokenIntegrityLevel, NULL, 0,
                          &token_info_length) ||
      GetLastError() != ERROR_INSUFFICIENT_BUFFER)
    return false;

  scoped_ptr<char[]> token_label_bytes(new char[token_info_length]);
  if (!token_label_bytes.get())
    return false;

  TOKEN_MANDATORY_LABEL* token_label =
      reinterpret_cast<TOKEN_MANDATORY_LABEL*>(token_label_bytes.get());
  if (!token_label)
    return false;

  if (!GetTokenInformation(process_token, TokenIntegrityLevel, token_label,
                           token_info_length, &token_info_length))
    return false;

  DWORD integrity_level = *GetSidSubAuthority(token_label->Label.Sid,
      (DWORD)(UCHAR)(*GetSidSubAuthorityCount(token_label->Label.Sid)-1));

  if (integrity_level < SECURITY_MANDATORY_MEDIUM_RID) {
    *level = LOW_INTEGRITY;
  } else if (integrity_level >= SECURITY_MANDATORY_MEDIUM_RID &&
      integrity_level < SECURITY_MANDATORY_HIGH_RID) {
    *level = MEDIUM_INTEGRITY;
  } else if (integrity_level >= SECURITY_MANDATORY_HIGH_RID) {
    *level = HIGH_INTEGRITY;
  } else {
    NOTREACHED();
    return false;
  }

  return true;
}

bool LaunchProcess(const string16& cmdline,
                   const LaunchOptions& options,
                   ProcessHandle* process_handle) {
  STARTUPINFO startup_info = {};
  startup_info.cb = sizeof(startup_info);
  if (options.empty_desktop_name)
    startup_info.lpDesktop = L"";
  startup_info.dwFlags = STARTF_USESHOWWINDOW;
  startup_info.wShowWindow = options.start_hidden ? SW_HIDE : SW_SHOW;

  if (options.stdin_handle || options.stdout_handle || options.stderr_handle) {
    DCHECK(options.inherit_handles);
    DCHECK(options.stdin_handle);
    DCHECK(options.stdout_handle);
    DCHECK(options.stderr_handle);
    startup_info.dwFlags |= STARTF_USESTDHANDLES;
    startup_info.hStdInput = options.stdin_handle;
    startup_info.hStdOutput = options.stdout_handle;
    startup_info.hStdError = options.stderr_handle;
  }

  DWORD flags = 0;

  if (options.job_handle) {
    flags |= CREATE_SUSPENDED;

    // If this code is run under a debugger, the launched process is
    // automatically associated with a job object created by the debugger.
    // The CREATE_BREAKAWAY_FROM_JOB flag is used to prevent this.
    flags |= CREATE_BREAKAWAY_FROM_JOB;
  }

  if (options.force_breakaway_from_job_)
    flags |= CREATE_BREAKAWAY_FROM_JOB;

  base::win::ScopedProcessInformation process_info;

  if (options.as_user) {
    flags |= CREATE_UNICODE_ENVIRONMENT;
    void* enviroment_block = NULL;

    if (!CreateEnvironmentBlock(&enviroment_block, options.as_user, FALSE)) {
      DPLOG(ERROR);
      return false;
    }

    BOOL launched =
        CreateProcessAsUser(options.as_user, NULL,
                            const_cast<wchar_t*>(cmdline.c_str()),
                            NULL, NULL, options.inherit_handles, flags,
                            enviroment_block, NULL, &startup_info,
                            process_info.Receive());
    DestroyEnvironmentBlock(enviroment_block);
    if (!launched) {
      DPLOG(ERROR);
      return false;
    }
  } else {
    if (!CreateProcess(NULL,
                       const_cast<wchar_t*>(cmdline.c_str()), NULL, NULL,
                       options.inherit_handles, flags, NULL, NULL,
                       &startup_info, process_info.Receive())) {
      DPLOG(ERROR);
      return false;
    }
  }

  if (options.job_handle) {
    if (0 == AssignProcessToJobObject(options.job_handle,
                                      process_info.process_handle())) {
      DLOG(ERROR) << "Could not AssignProcessToObject.";
      KillProcess(process_info.process_handle(), kProcessKilledExitCode, true);
      return false;
    }

    ResumeThread(process_info.thread_handle());
  }

  if (options.wait)
    WaitForSingleObject(process_info.process_handle(), INFINITE);

  // If the caller wants the process handle, we won't close it.
  if (process_handle)
    *process_handle = process_info.TakeProcessHandle();

  return true;
}

bool LaunchProcess(const CommandLine& cmdline,
                   const LaunchOptions& options,
                   ProcessHandle* process_handle) {
  return LaunchProcess(cmdline.GetCommandLineString(), options, process_handle);
}

bool SetJobObjectAsKillOnJobClose(HANDLE job_object) {
  JOBOBJECT_EXTENDED_LIMIT_INFORMATION limit_info = {0};
  limit_info.BasicLimitInformation.LimitFlags =
      JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
  return 0 != SetInformationJobObject(
      job_object,
      JobObjectExtendedLimitInformation,
      &limit_info,
      sizeof(limit_info));
}

// Attempts to kill the process identified by the given process
// entry structure, giving it the specified exit code.
// Returns true if this is successful, false otherwise.
bool KillProcessById(ProcessId process_id, int exit_code, bool wait) {
  HANDLE process = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE,
                               FALSE,  // Don't inherit handle
                               process_id);
  if (!process) {
    DLOG_GETLASTERROR(ERROR) << "Unable to open process " << process_id;
    return false;
  }
  bool ret = KillProcess(process, exit_code, wait);
  CloseHandle(process);
  return ret;
}

bool GetAppOutput(const CommandLine& cl, std::string* output) {
  HANDLE out_read = NULL;
  HANDLE out_write = NULL;

  SECURITY_ATTRIBUTES sa_attr;
  // Set the bInheritHandle flag so pipe handles are inherited.
  sa_attr.nLength = sizeof(SECURITY_ATTRIBUTES);
  sa_attr.bInheritHandle = TRUE;
  sa_attr.lpSecurityDescriptor = NULL;

  // Create the pipe for the child process's STDOUT.
  if (!CreatePipe(&out_read, &out_write, &sa_attr, 0)) {
    NOTREACHED() << "Failed to create pipe";
    return false;
  }

  // Ensure we don't leak the handles.
  win::ScopedHandle scoped_out_read(out_read);
  win::ScopedHandle scoped_out_write(out_write);

  // Ensure the read handle to the pipe for STDOUT is not inherited.
  if (!SetHandleInformation(out_read, HANDLE_FLAG_INHERIT, 0)) {
    NOTREACHED() << "Failed to disabled pipe inheritance";
    return false;
  }

  FilePath::StringType writable_command_line_string(cl.GetCommandLineString());

  base::win::ScopedProcessInformation proc_info;
  STARTUPINFO start_info = { 0 };

  start_info.cb = sizeof(STARTUPINFO);
  start_info.hStdOutput = out_write;
  // Keep the normal stdin and stderr.
  start_info.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
  start_info.hStdError = GetStdHandle(STD_ERROR_HANDLE);
  start_info.dwFlags |= STARTF_USESTDHANDLES;

  // Create the child process.
  if (!CreateProcess(NULL,
                     &writable_command_line_string[0],
                     NULL, NULL,
                     TRUE,  // Handles are inherited.
                     0, NULL, NULL, &start_info, proc_info.Receive())) {
    NOTREACHED() << "Failed to start process";
    return false;
  }

  // Close our writing end of pipe now. Otherwise later read would not be able
  // to detect end of child's output.
  scoped_out_write.Close();

  // Read output from the child process's pipe for STDOUT
  const int kBufferSize = 1024;
  char buffer[kBufferSize];

  for (;;) {
    DWORD bytes_read = 0;
    BOOL success = ReadFile(out_read, buffer, kBufferSize, &bytes_read, NULL);
    if (!success || bytes_read == 0)
      break;
    output->append(buffer, bytes_read);
  }

  // Let's wait for the process to finish.
  WaitForSingleObject(proc_info.process_handle(), INFINITE);

  return true;
}

bool KillProcess(ProcessHandle process, int exit_code, bool wait) {
  bool result = (TerminateProcess(process, exit_code) != FALSE);
  if (result && wait) {
    // The process may not end immediately due to pending I/O
    if (WAIT_OBJECT_0 != WaitForSingleObject(process, 60 * 1000))
      DLOG_GETLASTERROR(ERROR) << "Error waiting for process exit";
  } else if (!result) {
    DLOG_GETLASTERROR(ERROR) << "Unable to terminate process";
  }
  return result;
}

TerminationStatus GetTerminationStatus(ProcessHandle handle, int* exit_code) {
  DWORD tmp_exit_code = 0;

  if (!::GetExitCodeProcess(handle, &tmp_exit_code)) {
    DLOG_GETLASTERROR(FATAL) << "GetExitCodeProcess() failed";
    if (exit_code) {
      // This really is a random number.  We haven't received any
      // information about the exit code, presumably because this
      // process doesn't have permission to get the exit code, or
      // because of some other cause for GetExitCodeProcess to fail
      // (MSDN docs don't give the possible failure error codes for
      // this function, so it could be anything).  But we don't want
      // to leave exit_code uninitialized, since that could cause
      // random interpretations of the exit code.  So we assume it
      // terminated "normally" in this case.
      *exit_code = kNormalTerminationExitCode;
    }
    // Assume the child has exited normally if we can't get the exit
    // code.
    return TERMINATION_STATUS_NORMAL_TERMINATION;
  }
  if (tmp_exit_code == STILL_ACTIVE) {
    DWORD wait_result = WaitForSingleObject(handle, 0);
    if (wait_result == WAIT_TIMEOUT) {
      if (exit_code)
        *exit_code = wait_result;
      return TERMINATION_STATUS_STILL_RUNNING;
    }

    if (wait_result == WAIT_FAILED) {
      DLOG_GETLASTERROR(ERROR) << "WaitForSingleObject() failed";
    } else {
      DCHECK_EQ(WAIT_OBJECT_0, wait_result);

      // Strange, the process used 0x103 (STILL_ACTIVE) as exit code.
      NOTREACHED();
    }

    return TERMINATION_STATUS_ABNORMAL_TERMINATION;
  }

  if (exit_code)
    *exit_code = tmp_exit_code;

  switch (tmp_exit_code) {
    case kNormalTerminationExitCode:
      return TERMINATION_STATUS_NORMAL_TERMINATION;
    case kDebuggerInactiveExitCode:  // STATUS_DEBUGGER_INACTIVE.
    case kKeyboardInterruptExitCode:  // Control-C/end session.
    case kDebuggerTerminatedExitCode:  // Debugger terminated process.
    case kProcessKilledExitCode:  // Task manager kill.
      return TERMINATION_STATUS_PROCESS_WAS_KILLED;
    default:
      // All other exit codes indicate crashes.
      return TERMINATION_STATUS_PROCESS_CRASHED;
  }
}

bool WaitForExitCode(ProcessHandle handle, int* exit_code) {
  bool success = WaitForExitCodeWithTimeout(
      handle, exit_code, base::TimeDelta::FromMilliseconds(INFINITE));
  CloseProcessHandle(handle);
  return success;
}

bool WaitForExitCodeWithTimeout(ProcessHandle handle, int* exit_code,
                                base::TimeDelta timeout) {
  if (::WaitForSingleObject(handle, timeout.InMilliseconds()) != WAIT_OBJECT_0)
    return false;
  DWORD temp_code;  // Don't clobber out-parameters in case of failure.
  if (!::GetExitCodeProcess(handle, &temp_code))
    return false;

  *exit_code = temp_code;
  return true;
}

bool WaitForProcessesToExit(const FilePath::StringType& executable_name,
                            base::TimeDelta wait,
                            const ProcessFilter* filter) {
  const ProcessEntry* entry;
  bool result = true;
  DWORD start_time = GetTickCount();

  NamedProcessIterator iter(executable_name, filter);
  while ((entry = iter.NextProcessEntry())) {
    DWORD remaining_wait = std::max<int64>(
        0, wait.InMilliseconds() - (GetTickCount() - start_time));
    HANDLE process = OpenProcess(SYNCHRONIZE,
                                 FALSE,
                                 entry->th32ProcessID);
    DWORD wait_result = WaitForSingleObject(process, remaining_wait);
    CloseHandle(process);
    result = result && (wait_result == WAIT_OBJECT_0);
  }

  return result;
}

bool WaitForSingleProcess(ProcessHandle handle, base::TimeDelta wait) {
  int exit_code;
  if (!WaitForExitCodeWithTimeout(handle, &exit_code, wait))
    return false;
  return exit_code == 0;
}

bool CleanupProcesses(const FilePath::StringType& executable_name,
                      base::TimeDelta wait,
                      int exit_code,
                      const ProcessFilter* filter) {
  bool exited_cleanly = WaitForProcessesToExit(executable_name, wait, filter);
  if (!exited_cleanly)
    KillProcesses(executable_name, exit_code, filter);
  return exited_cleanly;
}

void EnsureProcessTerminated(ProcessHandle process) {
  DCHECK(process != GetCurrentProcess());

  // If already signaled, then we are done!
  if (WaitForSingleObject(process, 0) == WAIT_OBJECT_0) {
    CloseHandle(process);
    return;
  }

  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&TimerExpiredTask::TimedOut,
                 base::Owned(new TimerExpiredTask(process))),
      base::TimeDelta::FromMilliseconds(kWaitInterval));
}

bool EnableLowFragmentationHeap() {
  HMODULE kernel32 = GetModuleHandle(L"kernel32.dll");
  HeapSetFn heap_set = reinterpret_cast<HeapSetFn>(GetProcAddress(
      kernel32,
      "HeapSetInformation"));

  // On Windows 2000, the function is not exported. This is not a reason to
  // fail.
  if (!heap_set)
    return true;

  unsigned number_heaps = GetProcessHeaps(0, NULL);
  if (!number_heaps)
    return false;

  // Gives us some extra space in the array in case a thread is creating heaps
  // at the same time we're querying them.
  static const int MARGIN = 8;
  scoped_ptr<HANDLE[]> heaps(new HANDLE[number_heaps + MARGIN]);
  number_heaps = GetProcessHeaps(number_heaps + MARGIN, heaps.get());
  if (!number_heaps)
    return false;

  for (unsigned i = 0; i < number_heaps; ++i) {
    ULONG lfh_flag = 2;
    // Don't bother with the result code. It may fails on heaps that have the
    // HEAP_NO_SERIALIZE flag. This is expected and not a problem at all.
    heap_set(heaps[i],
             HeapCompatibilityInformation,
             &lfh_flag,
             sizeof(lfh_flag));
  }
  return true;
}

void EnableTerminationOnHeapCorruption() {
  // Ignore the result code. Supported on XP SP3 and Vista.
  HeapSetInformation(NULL, HeapEnableTerminationOnCorruption, NULL, 0);
}

void EnableTerminationOnOutOfMemory() {
  std::set_new_handler(&OnNoMemory);
}

void RaiseProcessToHighPriority() {
  SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
}

}  // namespace base
