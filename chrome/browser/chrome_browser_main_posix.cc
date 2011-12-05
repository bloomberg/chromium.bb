// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_main_posix.h"

#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <sys/resource.h>
#include <unistd.h>

#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/eintr_wrapper.h"
#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/browser_thread.h"

#if defined(TOOLKIT_USES_GTK) && !defined(OS_CHROMEOS)
#include "chrome/browser/printing/print_dialog_gtk.h"
#endif

using content::BrowserThread;

namespace {

// See comment in |PreEarlyInitialization()|, where sigaction is called.
void SIGCHLDHandler(int signal) {
}

int g_shutdown_pipe_write_fd = -1;
int g_shutdown_pipe_read_fd = -1;

// Common code between SIG{HUP, INT, TERM}Handler.
void GracefulShutdownHandler(int signal) {
  // Reinstall the default handler.  We had one shot at graceful shutdown.
  struct sigaction action;
  memset(&action, 0, sizeof(action));
  action.sa_handler = SIG_DFL;
  RAW_CHECK(sigaction(signal, &action, NULL) == 0);

  RAW_CHECK(g_shutdown_pipe_write_fd != -1);
  RAW_CHECK(g_shutdown_pipe_read_fd != -1);
  size_t bytes_written = 0;
  do {
    int rv = HANDLE_EINTR(
        write(g_shutdown_pipe_write_fd,
              reinterpret_cast<const char*>(&signal) + bytes_written,
              sizeof(signal) - bytes_written));
    RAW_CHECK(rv >= 0);
    bytes_written += rv;
  } while (bytes_written < sizeof(signal));
}

// See comment in |PostMainMessageLoopStart()|, where sigaction is called.
void SIGHUPHandler(int signal) {
  RAW_CHECK(signal == SIGHUP);
  GracefulShutdownHandler(signal);
}

// See comment in |PostMainMessageLoopStart()|, where sigaction is called.
void SIGINTHandler(int signal) {
  RAW_CHECK(signal == SIGINT);
  GracefulShutdownHandler(signal);
}

// See comment in |PostMainMessageLoopStart()|, where sigaction is called.
void SIGTERMHandler(int signal) {
  RAW_CHECK(signal == SIGTERM);
  GracefulShutdownHandler(signal);
}

class ShutdownDetector : public base::PlatformThread::Delegate {
 public:
  explicit ShutdownDetector(int shutdown_fd);

  virtual void ThreadMain();

 private:
  const int shutdown_fd_;

  DISALLOW_COPY_AND_ASSIGN(ShutdownDetector);
};

ShutdownDetector::ShutdownDetector(int shutdown_fd)
    : shutdown_fd_(shutdown_fd) {
  CHECK_NE(shutdown_fd_, -1);
}


// These functions are used to help us diagnose crash dumps that happen
// during the shutdown process.
NOINLINE void ShutdownFDReadError() {
  // Ensure function isn't optimized away.
  asm("");
  sleep(UINT_MAX);
}

NOINLINE void ShutdownFDClosedError() {
  // Ensure function isn't optimized away.
  asm("");
  sleep(UINT_MAX);
}

NOINLINE void ExitPosted() {
  // Ensure function isn't optimized away.
  asm("");
  sleep(UINT_MAX);
}

void ShutdownDetector::ThreadMain() {
  base::PlatformThread::SetName("CrShutdownDetector");

  int signal;
  size_t bytes_read = 0;
  ssize_t ret;
  do {
    ret = HANDLE_EINTR(
        read(shutdown_fd_,
             reinterpret_cast<char*>(&signal) + bytes_read,
             sizeof(signal) - bytes_read));
    if (ret < 0) {
      NOTREACHED() << "Unexpected error: " << strerror(errno);
      ShutdownFDReadError();
      break;
    } else if (ret == 0) {
      NOTREACHED() << "Unexpected closure of shutdown pipe.";
      ShutdownFDClosedError();
      break;
    }
    bytes_read += ret;
  } while (bytes_read < sizeof(signal));
  VLOG(1) << "Handling shutdown for signal " << signal << ".";
#if defined(OS_CHROMEOS)
  // On ChromeOS, exiting on signal should be always clean.
  base::Closure task = base::Bind(&BrowserList::ExitCleanly);
#else
  base::Closure task = base::Bind(&BrowserList::AttemptExit, false);
#endif

  if (!BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, task)) {
    // Without a UI thread to post the exit task to, there aren't many
    // options.  Raise the signal again.  The default handler will pick it up
    // and cause an ungraceful exit.
    RAW_LOG(WARNING, "No UI thread, exiting ungracefully.");
    kill(getpid(), signal);

    // The signal may be handled on another thread.  Give that a chance to
    // happen.
    sleep(3);

    // We really should be dead by now.  For whatever reason, we're not. Exit
    // immediately, with the exit status set to the signal number with bit 8
    // set.  On the systems that we care about, this exit status is what is
    // normally used to indicate an exit by this signal's default handler.
    // This mechanism isn't a de jure standard, but even in the worst case, it
    // should at least result in an immediate exit.
    RAW_LOG(WARNING, "Still here, exiting really ungracefully.");
    _exit(signal | (1 << 7));
  }
  ExitPosted();
}

// Sets the file descriptor soft limit to |max_descriptors| or the OS hard
// limit, whichever is lower.
void SetFileDescriptorLimit(unsigned int max_descriptors) {
  struct rlimit limits;
  if (getrlimit(RLIMIT_NOFILE, &limits) == 0) {
    unsigned int new_limit = max_descriptors;
    if (limits.rlim_max > 0 && limits.rlim_max < max_descriptors) {
      new_limit = limits.rlim_max;
    }
    limits.rlim_cur = new_limit;
    if (setrlimit(RLIMIT_NOFILE, &limits) != 0) {
      PLOG(INFO) << "Failed to set file descriptor limit";
    }
  } else {
    PLOG(INFO) << "Failed to get file descriptor limit";
  }
}

}  // namespace

// ChromeBrowserMainPartsPosix -------------------------------------------------

ChromeBrowserMainPartsPosix::ChromeBrowserMainPartsPosix(
    const content::MainFunctionParams& parameters)
    : ChromeBrowserMainParts(parameters) {
}

void ChromeBrowserMainPartsPosix::PreEarlyInitialization() {
  ChromeBrowserMainParts::PreEarlyInitialization();

  // We need to accept SIGCHLD, even though our handler is a no-op because
  // otherwise we cannot wait on children. (According to POSIX 2001.)
  struct sigaction action;
  memset(&action, 0, sizeof(action));
  action.sa_handler = SIGCHLDHandler;
  CHECK(sigaction(SIGCHLD, &action, NULL) == 0);

  const std::string fd_limit_string =
      parsed_command_line().GetSwitchValueASCII(
          switches::kFileDescriptorLimit);
  int fd_limit = 0;
  if (!fd_limit_string.empty()) {
    base::StringToInt(fd_limit_string, &fd_limit);
  }
#if defined(OS_MACOSX)
  // We use quite a few file descriptors for our IPC, and the default limit on
  // the Mac is low (256), so bump it up if there is no explicit override.
  if (fd_limit == 0) {
    fd_limit = 1024;
  }
#endif  // OS_MACOSX
  if (fd_limit > 0)
    SetFileDescriptorLimit(fd_limit);
}

void ChromeBrowserMainPartsPosix::PostMainMessageLoopStart() {
  ChromeBrowserMainParts::PostMainMessageLoopStart();

  int pipefd[2];
  int ret = pipe(pipefd);
  if (ret < 0) {
    PLOG(DFATAL) << "Failed to create pipe";
  } else {
    g_shutdown_pipe_read_fd = pipefd[0];
    g_shutdown_pipe_write_fd = pipefd[1];
    const size_t kShutdownDetectorThreadStackSize = 4096;
    // TODO(viettrungluu,willchan): crbug.com/29675 - This currently leaks, so
    // if you change this, you'll probably need to change the suppression.
    if (!base::PlatformThread::CreateNonJoinable(
            kShutdownDetectorThreadStackSize,
            new ShutdownDetector(g_shutdown_pipe_read_fd))) {
      LOG(DFATAL) << "Failed to create shutdown detector task.";
    }
  }
  // Setup signal handlers for shutdown AFTER shutdown pipe is setup because
  // it may be called right away after handler is set.

  // If adding to this list of signal handlers, note the new signal probably
  // needs to be reset in child processes. See
  // base/process_util_posix.cc:LaunchProcess.

  // We need to handle SIGTERM, because that is how many POSIX-based distros ask
  // processes to quit gracefully at shutdown time.
  struct sigaction action;
  memset(&action, 0, sizeof(action));
  action.sa_handler = SIGTERMHandler;
  CHECK(sigaction(SIGTERM, &action, NULL) == 0);
  // Also handle SIGINT - when the user terminates the browser via Ctrl+C. If
  // the browser process is being debugged, GDB will catch the SIGINT first.
  action.sa_handler = SIGINTHandler;
  CHECK(sigaction(SIGINT, &action, NULL) == 0);
  // And SIGHUP, for when the terminal disappears. On shutdown, many Linux
  // distros send SIGHUP, SIGTERM, and then SIGKILL.
  action.sa_handler = SIGHUPHandler;
  CHECK(sigaction(SIGHUP, &action, NULL) == 0);

#if defined(TOOLKIT_USES_GTK) && !defined(OS_CHROMEOS)
  printing::PrintingContextGtk::SetCreatePrintDialogFunction(
      &PrintDialogGtk::CreatePrintDialog);
#endif
}
