// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/zygote_host_linux.h"

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/command_line.h"
#include "base/eintr_wrapper.h"
#include "base/linux_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/pickle.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "base/unix_domain_socket_posix.h"

#include "chrome/browser/renderer_host/render_sandbox_host_linux.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/process_watcher.h"

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

ZygoteHost::ZygoteHost()
    : pid_(-1),
      init_(false) {
}

ZygoteHost::~ZygoteHost() {
  if (init_)
    close(control_fd_);
}

void ZygoteHost::Init(const std::string& sandbox_cmd) {
  DCHECK(!init_);
  init_ = true;

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
                                   browser_command_line.GetSwitchValueASCII(
                                       switches::kLoggingLevel));
  }
  if (browser_command_line.HasSwitch(switches::kEnableLogging)) {
    // Append with value to support --enable-logging=stderr.
    cmd_line.AppendSwitchWithValue(switches::kEnableLogging,
                                   browser_command_line.GetSwitchValueASCII(
                                       switches::kEnableLogging));
  }
  if (browser_command_line.HasSwitch(switches::kEnableSeccompSandbox)) {
    cmd_line.AppendSwitch(switches::kEnableSeccompSandbox);
  }

  const char* sandbox_binary = sandbox_cmd.c_str();
  struct stat st;

  bool using_suid_sandbox = false;
  if (!sandbox_cmd.empty() && stat(sandbox_binary, &st) == 0) {
    if (access(sandbox_binary, X_OK) == 0 &&
        (st.st_mode & S_ISUID) &&
        (st.st_mode & S_IXOTH)) {
      using_suid_sandbox = true;
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

  int dummy_fd = -1;
  if (using_suid_sandbox) {
    dummy_fd = socket(PF_UNIX, SOCK_DGRAM, 0);
    CHECK(dummy_fd >= 0);
    fds_to_map.push_back(std::make_pair(dummy_fd, 7));
  }

  base::ProcessHandle process;
  base::LaunchApp(cmd_line.argv(), fds_to_map, false, &process);
  CHECK(process != -1) << "Failed to launch zygote process";

  if (using_suid_sandbox) {
    // In the SUID sandbox, the real zygote is forked from the sandbox.
    // We need to look for it.
    // But first, wait for the zygote to tell us it's running.
    // The sending code is in chrome/browser/zygote_main_linux.cc.
    std::vector<int> fds_vec;
    const int kExpectedLength = sizeof(kZygoteMagic);
    char buf[kExpectedLength];
    const ssize_t len = base::RecvMsg(fds[0], buf, sizeof(buf), &fds_vec);
    CHECK(len == kExpectedLength) << "Incorrect zygote magic length";
    CHECK(0 == strcmp(buf, kZygoteMagic)) << "Incorrect zygote magic";

    std::string inode_output;
    ino_t inode = 0;
    // Figure out the inode for |dummy_fd|, close |dummy_fd| on our end,
    // and find the zygote process holding |dummy_fd|.
    if (base::FileDescriptorGetInode(&inode, dummy_fd)) {
      close(dummy_fd);
      std::vector<std::string> get_inode_cmdline;
      get_inode_cmdline.push_back(sandbox_binary);
      get_inode_cmdline.push_back(base::kFindInodeSwitch);
      get_inode_cmdline.push_back(Int64ToString(inode));
      CommandLine get_inode_cmd(get_inode_cmdline);
      if (base::GetAppOutput(get_inode_cmd, &inode_output)) {
        StringToInt(inode_output, &pid_);
      }
    }
    CHECK(pid_ > 0) << "Did not find zygote process";

    if (process != pid_) {
      // Reap the sandbox.
      ProcessWatcher::EnsureProcessGetsReaped(process);
    }
  } else {
    // Not using the SUID sandbox.
    pid_ = process;
  }

  close(fds[1]);
  control_fd_ = fds[0];
}

pid_t ZygoteHost::ForkRenderer(
    const std::vector<std::string>& argv,
    const base::GlobalDescriptors::Mapping& mapping) {
  DCHECK(init_);
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
    return base::kNullProcessHandle;

  pid_t pid;
  if (HANDLE_EINTR(read(control_fd_, &pid, sizeof(pid))) != sizeof(pid))
    return base::kNullProcessHandle;

  return pid;
}

void ZygoteHost::EnsureProcessTerminated(pid_t process) {
  DCHECK(init_);
  Pickle pickle;

  pickle.WriteInt(kCmdReap);
  pickle.WriteInt(process);

  HANDLE_EINTR(write(control_fd_, pickle.data(), pickle.size()));
}

bool ZygoteHost::DidProcessCrash(base::ProcessHandle handle,
                                 bool* child_exited) {
  DCHECK(init_);
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
