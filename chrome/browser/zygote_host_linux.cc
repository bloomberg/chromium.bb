// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/zygote_host_linux.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include "base/command_line.h"
#include "base/eintr_wrapper.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/pickle.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "base/unix_domain_socket_posix.h"

#include "chrome/browser/renderer_host/render_sandbox_host_linux.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"

#include "sandbox/linux/suid/suid_unsafe_environment_variables.h"

static void SaveSUIDUnsafeEnvironmentVariables() {
  // The ELF loader will clear many environment variables so we save them to
  // different names here so that the SUID sandbox can resolve them for the
  // renderer.

  for (unsigned i = 0; kSUIDUnsafeEnvironmentVariables[i]; ++i) {
    const char* const envvar = kSUIDUnsafeEnvironmentVariables[i];
    char* const saved_envvar = SandboxSavedEnvironmentVariable(envvar);
    if (!saved_envvar)
      continue;

    const char* const value = getenv(envvar);
    if (value)
      setenv(saved_envvar, value, 1 /* overwrite */);
    else
      unsetenv(saved_envvar);

    free(saved_envvar);
  }
}

ZygoteHost::ZygoteHost() {
  FilePath chrome_path;
  CHECK(PathService::Get(base::FILE_EXE, &chrome_path));
  CommandLine cmd_line(chrome_path);

  cmd_line.AppendSwitchWithValue(switches::kProcessType,
                                 switches::kZygoteProcess);

  int fds[2];
  CHECK(socketpair(PF_UNIX, SOCK_SEQPACKET, 0, fds) == 0);
  base::file_handle_mapping_vector fds_to_map;
  fds_to_map.push_back(std::make_pair(fds[1], 3));

  const CommandLine& browser_command_line = *CommandLine::ForCurrentProcess();
  if (browser_command_line.HasSwitch(switches::kZygoteCmdPrefix)) {
    const std::wstring prefix =
        browser_command_line.GetSwitchValue(switches::kZygoteCmdPrefix);
    cmd_line.PrependWrapper(prefix);
  }
  // Append any switches from the browser process that need to be forwarded on
  // to the zygote/renderers.
  // Should this list be obtained from browser_render_process_host.cc?
  if (browser_command_line.HasSwitch(switches::kAllowSandboxDebugging)) {
    cmd_line.AppendSwitch(switches::kAllowSandboxDebugging);
  }
  if (browser_command_line.HasSwitch(switches::kLoggingLevel)) {
    cmd_line.AppendSwitchWithValue(switches::kLoggingLevel,
                                   browser_command_line.GetSwitchValue(
                                       switches::kLoggingLevel));
  }
  if (browser_command_line.HasSwitch(switches::kEnableLogging)) {
    // Append with value to support --enable-logging=stderr.
    cmd_line.AppendSwitchWithValue(switches::kEnableLogging,
                                   browser_command_line.GetSwitchValue(
                                       switches::kEnableLogging));
  }

  const char* sandbox_binary = NULL;
  struct stat st;

  // In Chromium branded builds, developers can set an environment variable to
  // use the development sandbox. See
  // http://code.google.com/p/chromium/wiki/LinuxSUIDSandboxDevelopment
  if (stat("/proc/self/exe", &st) == 0 &&
      st.st_uid == getuid()) {
    sandbox_binary = getenv("CHROME_DEVEL_SANDBOX");
  }

#if defined(LINUX_SANDBOX_PATH)
  if (!sandbox_binary)
    sandbox_binary = LINUX_SANDBOX_PATH;
#endif

  if (sandbox_binary && stat(sandbox_binary, &st) == 0) {
    if (access(sandbox_binary, X_OK) == 0 &&
        (st.st_mode & S_ISUID) &&
        (st.st_mode & S_IXOTH)) {
      cmd_line.PrependWrapper(ASCIIToWide(sandbox_binary));

      SaveSUIDUnsafeEnvironmentVariables();
    } else {
      LOG(FATAL) << "The SUID sandbox helper binary was found, but is not "
                    "configured correctly. Rather than run without sandboxing "
                    "I'm aborting now. You need to make sure that "
                 << sandbox_binary << " is mode 4755.";
    }
  }

  // Start up the sandbox host process and get the file descriptor for the
  // renderers to talk to it.
  const int sfd = Singleton<RenderSandboxHostLinux>()->GetRendererSocket();
  fds_to_map.push_back(std::make_pair(sfd, 5));

  base::ProcessHandle process;
  base::LaunchApp(cmd_line.argv(), fds_to_map, false, &process);
  CHECK(process != -1) << "Failed to launch zygote process";

  pid_ = process;
  close(fds[1]);
  control_fd_ = fds[0];
}

ZygoteHost::~ZygoteHost() {
  close(control_fd_);
}

pid_t ZygoteHost::ForkRenderer(
    const std::vector<std::string>& argv,
    const base::GlobalDescriptors::Mapping& mapping) {
  Pickle pickle;

  pickle.WriteInt(kCmdFork);
  pickle.WriteInt(argv.size());
  for (std::vector<std::string>::const_iterator
       i = argv.begin(); i != argv.end(); ++i)
    pickle.WriteString(*i);

  pickle.WriteInt(mapping.size());

  std::vector<int> fds;
  for (base::GlobalDescriptors::Mapping::const_iterator
       i = mapping.begin(); i != mapping.end(); ++i) {
    pickle.WriteUInt32(i->first);
    fds.push_back(i->second);
  }

  if (!base::SendMsg(control_fd_, pickle.data(), pickle.size(), fds))
    return -1;

  pid_t pid;
  if (HANDLE_EINTR(read(control_fd_, &pid, sizeof(pid))) != sizeof(pid))
    return -1;

  return pid;
}

void ZygoteHost::EnsureProcessTerminated(pid_t process) {
  Pickle pickle;

  pickle.WriteInt(kCmdReap);
  pickle.WriteInt(process);

  HANDLE_EINTR(write(control_fd_, pickle.data(), pickle.size()));
}

bool ZygoteHost::DidProcessCrash(base::ProcessHandle handle,
                                 bool* child_exited) {
  Pickle pickle;
  pickle.WriteInt(kCmdDidProcessCrash);
  pickle.WriteInt(handle);

  HANDLE_EINTR(write(control_fd_, pickle.data(), pickle.size()));

  static const unsigned kMaxMessageLength = 128;
  char buf[kMaxMessageLength];
  const ssize_t len = HANDLE_EINTR(read(control_fd_, buf, sizeof(buf)));

  if (len == -1) {
    LOG(WARNING) << "Error reading message from zygote: " << errno;
    return false;
  } else if (len == 0) {
    LOG(WARNING) << "Socket closed prematurely.";
    return false;
  }

  Pickle read_pickle(buf, len);
  bool did_crash, tmp_child_exited;
  void* iter = NULL;
  if (!read_pickle.ReadBool(&iter, &did_crash) ||
      !read_pickle.ReadBool(&iter, &tmp_child_exited)) {
    LOG(WARNING) << "Error parsing DidProcessCrash response from zygote.";
    return false;
  }

  if (child_exited)
    *child_exited = tmp_child_exited;

  return did_crash;
}
