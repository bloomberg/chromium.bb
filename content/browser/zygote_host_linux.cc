// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/zygote_host_linux.h"

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/command_line.h"
#include "base/eintr_wrapper.h"
#include "base/environment.h"
#include "base/linux_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/pickle.h"
#include "base/process_util.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/scoped_ptr.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/process_watcher.h"
#include "chrome/common/result_codes.h"
#include "chrome/common/unix_domain_socket_posix.h"
#include "content/browser/renderer_host/render_sandbox_host_linux.h"
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

    scoped_ptr<base::Environment> env(base::Environment::Create());
    std::string value;
    if (env->GetVar(envvar, &value))
      env->SetVar(saved_envvar, value);
    else
      env->UnSetVar(saved_envvar);

    free(saved_envvar);
  }
}

ZygoteHost::ZygoteHost()
    : control_fd_(-1),
      pid_(-1),
      init_(false),
      using_suid_sandbox_(false),
      have_read_sandbox_status_word_(false),
      sandbox_status_(0) {
}

ZygoteHost::~ZygoteHost() {
  if (init_)
    close(control_fd_);
}

// static
ZygoteHost* ZygoteHost::GetInstance() {
  return Singleton<ZygoteHost>::get();
}

void ZygoteHost::Init(const std::string& sandbox_cmd) {
  DCHECK(!init_);
  init_ = true;

  FilePath chrome_path;
  CHECK(PathService::Get(base::FILE_EXE, &chrome_path));
  CommandLine cmd_line(chrome_path);

  cmd_line.AppendSwitchASCII(switches::kProcessType, switches::kZygoteProcess);

  int fds[2];
  CHECK(socketpair(PF_UNIX, SOCK_SEQPACKET, 0, fds) == 0);
  base::file_handle_mapping_vector fds_to_map;
  fds_to_map.push_back(std::make_pair(fds[1], 3));

  const CommandLine& browser_command_line = *CommandLine::ForCurrentProcess();
  if (browser_command_line.HasSwitch(switches::kZygoteCmdPrefix)) {
    cmd_line.PrependWrapper(
        browser_command_line.GetSwitchValueNative(switches::kZygoteCmdPrefix));
  }
  // Append any switches from the browser process that need to be forwarded on
  // to the zygote/renderers.
  // Should this list be obtained from browser_render_process_host.cc?
  static const char* kForwardSwitches[] = {
    switches::kAllowSandboxDebugging,
    switches::kLoggingLevel,
    switches::kEnableLogging,  // Support, e.g., --enable-logging=stderr.
    switches::kV,
    switches::kVModule,
    switches::kUserDataDir,  // Make logs go to the right file.
    // Load (in-process) Pepper plugins in-process in the zygote pre-sandbox.
    switches::kPpapiFlashPath,
    switches::kPpapiFlashVersion,
    switches::kRegisterPepperPlugins,
    switches::kDisableSeccompSandbox,
    switches::kEnableSeccompSandbox,
  };
  cmd_line.CopySwitchesFrom(browser_command_line, kForwardSwitches,
                            arraysize(kForwardSwitches));

  sandbox_binary_ = sandbox_cmd.c_str();
  struct stat st;

  if (!sandbox_cmd.empty() && stat(sandbox_binary_.c_str(), &st) == 0) {
    if (access(sandbox_binary_.c_str(), X_OK) == 0 &&
        (st.st_uid == 0) &&
        (st.st_mode & S_ISUID) &&
        (st.st_mode & S_IXOTH)) {
      using_suid_sandbox_ = true;
      cmd_line.PrependWrapper(sandbox_binary_);

      SaveSUIDUnsafeEnvironmentVariables();
    } else {
      LOG(FATAL) << "The SUID sandbox helper binary was found, but is not "
                    "configured correctly. Rather than run without sandboxing "
                    "I'm aborting now. You need to make sure that "
                 << sandbox_binary_ << " is mode 4755 and owned by root.";
    }
  }

  // Start up the sandbox host process and get the file descriptor for the
  // renderers to talk to it.
  const int sfd = RenderSandboxHostLinux::GetInstance()->GetRendererSocket();
  fds_to_map.push_back(std::make_pair(sfd, 5));

  int dummy_fd = -1;
  if (using_suid_sandbox_) {
    dummy_fd = socket(PF_UNIX, SOCK_DGRAM, 0);
    CHECK(dummy_fd >= 0);
    fds_to_map.push_back(std::make_pair(dummy_fd, 7));
  }

  base::ProcessHandle process;
  base::LaunchApp(cmd_line.argv(), fds_to_map, false, &process);
  CHECK(process != -1) << "Failed to launch zygote process";

  if (using_suid_sandbox_) {
    // In the SUID sandbox, the real zygote is forked from the sandbox.
    // We need to look for it.
    // But first, wait for the zygote to tell us it's running.
    // The sending code is in chrome/browser/zygote_main_linux.cc.
    std::vector<int> fds_vec;
    const int kExpectedLength = sizeof(kZygoteMagic);
    char buf[kExpectedLength];
    const ssize_t len = UnixDomainSocket::RecvMsg(fds[0], buf, sizeof(buf),
                                                  &fds_vec);
    CHECK(len == kExpectedLength) << "Incorrect zygote magic length";
    CHECK(0 == strcmp(buf, kZygoteMagic)) << "Incorrect zygote magic";

    std::string inode_output;
    ino_t inode = 0;
    // Figure out the inode for |dummy_fd|, close |dummy_fd| on our end,
    // and find the zygote process holding |dummy_fd|.
    if (base::FileDescriptorGetInode(&inode, dummy_fd)) {
      close(dummy_fd);
      std::vector<std::string> get_inode_cmdline;
      get_inode_cmdline.push_back(sandbox_binary_);
      get_inode_cmdline.push_back(base::kFindInodeSwitch);
      get_inode_cmdline.push_back(base::Int64ToString(inode));
      CommandLine get_inode_cmd(get_inode_cmdline);
      if (base::GetAppOutput(get_inode_cmd, &inode_output)) {
        base::StringToInt(inode_output, &pid_);
      }
    }
    CHECK(pid_ > 0) << "Did not find zygote process (using sandbox binary "
        << sandbox_binary_ << ")";

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

  Pickle pickle;
  pickle.WriteInt(kCmdGetSandboxStatus);
  std::vector<int> empty_fds;
  if (!UnixDomainSocket::SendMsg(control_fd_, pickle.data(), pickle.size(),
                                 empty_fds))
    LOG(FATAL) << "Cannot communicate with zygote";
  // We don't wait for the reply. We'll read it in ReadReply.
}

ssize_t ZygoteHost::ReadReply(void* buf, size_t buf_len) {
  // At startup we send a kCmdGetSandboxStatus request to the zygote, but don't
  // wait for the reply. Thus, the first time that we read from the zygote, we
  // get the reply to that request.
  if (!have_read_sandbox_status_word_) {
    if (HANDLE_EINTR(read(control_fd_, &sandbox_status_,
                          sizeof(sandbox_status_))) !=
        sizeof(sandbox_status_)) {
      return -1;
    }
    have_read_sandbox_status_word_ = true;
  }

  return HANDLE_EINTR(read(control_fd_, buf, buf_len));
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

  pid_t pid;
  {
    base::AutoLock lock(control_lock_);
    if (!UnixDomainSocket::SendMsg(control_fd_, pickle.data(), pickle.size(),
                                   fds))
      return base::kNullProcessHandle;

    if (ReadReply(&pid, sizeof(pid)) != sizeof(pid))
      return base::kNullProcessHandle;
    if (pid <= 0)
      return base::kNullProcessHandle;
  }

  const int kRendererScore = 5;
  AdjustRendererOOMScore(pid, kRendererScore);

  return pid;
}

void ZygoteHost::AdjustRendererOOMScore(base::ProcessHandle pid, int score) {
  // 1) You can't change the oom_adj of a non-dumpable process (EPERM) unless
  //    you're root. Because of this, we can't set the oom_adj from the browser
  //    process.
  //
  // 2) We can't set the oom_adj before entering the sandbox because the
  //    zygote is in the sandbox and the zygote is as critical as the browser
  //    process. Its oom_adj value shouldn't be changed.
  //
  // 3) A non-dumpable process can't even change its own oom_adj because it's
  //    root owned 0644. The sandboxed processes don't even have /proc, but one
  //    could imagine passing in a descriptor from outside.
  //
  // So, in the normal case, we use the SUID binary to change it for us.
  // However, Fedora (and other SELinux systems) don't like us touching other
  // process's oom_adj values
  // (https://bugzilla.redhat.com/show_bug.cgi?id=581256).
  //
  // The offical way to get the SELinux mode is selinux_getenforcemode, but I
  // don't want to add another library to the build as it's sure to cause
  // problems with other, non-SELinux distros.
  //
  // So we just check for /selinux. This isn't foolproof, but it's not bad
  // and it's easy.

  static bool selinux;
  static bool selinux_valid = false;

  if (!selinux_valid) {
    selinux = access("/selinux", X_OK) == 0;
    selinux_valid = true;
  }

  if (using_suid_sandbox_ && !selinux) {
    base::ProcessHandle sandbox_helper_process;
    std::vector<std::string> adj_oom_score_cmdline;

    adj_oom_score_cmdline.push_back(sandbox_binary_);
    adj_oom_score_cmdline.push_back(base::kAdjustOOMScoreSwitch);
    adj_oom_score_cmdline.push_back(base::Int64ToString(pid));
    adj_oom_score_cmdline.push_back(base::IntToString(score));
    CommandLine adj_oom_score_cmd(adj_oom_score_cmdline);
    if (base::LaunchApp(adj_oom_score_cmd, false, true,
                        &sandbox_helper_process)) {
      ProcessWatcher::EnsureProcessGetsReaped(sandbox_helper_process);
    }
  } else if (!using_suid_sandbox_) {
    if (!base::AdjustOOMScore(pid, score))
      PLOG(ERROR) << "Failed to adjust OOM score of renderer with pid " << pid;
  }
}

void ZygoteHost::EnsureProcessTerminated(pid_t process) {
  DCHECK(init_);
  Pickle pickle;

  pickle.WriteInt(kCmdReap);
  pickle.WriteInt(process);

  if (HANDLE_EINTR(write(control_fd_, pickle.data(), pickle.size())) < 0)
    PLOG(ERROR) << "write";
}

base::TerminationStatus ZygoteHost::GetTerminationStatus(
    base::ProcessHandle handle,
    int* exit_code) {
  DCHECK(init_);
  Pickle pickle;
  pickle.WriteInt(kCmdGetTerminationStatus);
  pickle.WriteInt(handle);

  // Set this now to handle the early termination cases.
  if (exit_code)
    *exit_code = ResultCodes::NORMAL_EXIT;

  static const unsigned kMaxMessageLength = 128;
  char buf[kMaxMessageLength];
  ssize_t len;
  {
    base::AutoLock lock(control_lock_);
    if (HANDLE_EINTR(write(control_fd_, pickle.data(), pickle.size())) < 0)
      PLOG(ERROR) << "write";

    len = ReadReply(buf, sizeof(buf));
  }

  if (len == -1) {
    LOG(WARNING) << "Error reading message from zygote: " << errno;
    return base::TERMINATION_STATUS_NORMAL_TERMINATION;
  } else if (len == 0) {
    LOG(WARNING) << "Socket closed prematurely.";
    return base::TERMINATION_STATUS_NORMAL_TERMINATION;
  }

  Pickle read_pickle(buf, len);
  int status, tmp_exit_code;
  void* iter = NULL;
  if (!read_pickle.ReadInt(&iter, &status) ||
      !read_pickle.ReadInt(&iter, &tmp_exit_code)) {
    LOG(WARNING) << "Error parsing GetTerminationStatus response from zygote.";
    return base::TERMINATION_STATUS_NORMAL_TERMINATION;
  }

  if (exit_code)
    *exit_code = tmp_exit_code;

  return static_cast<base::TerminationStatus>(status);
}
