// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/content/app/crashpad.h"

#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>

#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/posix/eintr_wrapper.h"
#include "base/posix/global_descriptors.h"
#include "build/build_config.h"
#include "components/crash/content/app/crash_reporter_client.h"
#include "content/public/common/content_descriptors.h"
#include "sandbox/linux/services/syscall_wrappers.h"
#include "third_party/crashpad/crashpad/client/crashpad_client.h"
#include "third_party/crashpad/crashpad/util/linux/exception_handler_client.h"
#include "third_party/crashpad/crashpad/util/linux/exception_information.h"
#include "third_party/crashpad/crashpad/util/misc/from_pointer_cast.h"
#include "third_party/crashpad/crashpad/util/posix/signals.h"

namespace crashpad {
namespace {

// A signal handler for non-browser processes in the sandbox.
// Sends a message to a crashpad::CrashHandlerHost to handle the crash.
class SandboxedHandler {
 public:
  static SandboxedHandler* Get() {
    static SandboxedHandler* instance = new SandboxedHandler();
    return instance;
  }

  bool Initialize() {
    server_fd_ = base::GlobalDescriptors::GetInstance()->Get(kCrashDumpSignal);

    return Signals::InstallCrashHandlers(HandleCrash, 0, nullptr);
  }

 private:
  SandboxedHandler() = default;
  ~SandboxedHandler() = delete;

  int ConnectToHandler(base::ScopedFD* connection) {
    int fds[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) != 0) {
      return errno;
    }
    base::ScopedFD local_connection(fds[0]);
    base::ScopedFD handlers_socket(fds[1]);

    msghdr msg;
    msg.msg_name = nullptr;
    msg.msg_namelen = 0;
    msg.msg_iov = nullptr;
    msg.msg_iovlen = 0;

    char cmsg_buf[CMSG_SPACE(sizeof(int))];
    msg.msg_control = cmsg_buf;
    msg.msg_controllen = sizeof(cmsg_buf);

    cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));
    *reinterpret_cast<int*>(CMSG_DATA(cmsg)) = handlers_socket.get();

    if (HANDLE_EINTR(sendmsg(server_fd_, &msg, MSG_NOSIGNAL)) < 0) {
      return errno;
    }

    *connection = std::move(local_connection);
    return 0;
  }

  static void HandleCrash(int signo, siginfo_t* siginfo, void* context) {
    SandboxedHandler* state = Get();

    base::ScopedFD connection;
    if (state->ConnectToHandler(&connection) == 0) {
      ExceptionInformation exception_information;
      exception_information.siginfo_address =
          FromPointerCast<decltype(exception_information.siginfo_address)>(
              siginfo);
      exception_information.context_address =
          FromPointerCast<decltype(exception_information.context_address)>(
              context);
      exception_information.thread_id = sandbox::sys_gettid();

      ClientInformation info = {};
      info.exception_information_address =
          FromPointerCast<decltype(info.exception_information_address)>(
              &exception_information);

      ExceptionHandlerClient handler_client(connection.get());
      handler_client.SetCanSetPtracer(false);
      handler_client.RequestCrashDump(info);
    }

    Signals::RestoreHandlerAndReraiseSignalOnReturn(siginfo, nullptr);
  }

  int server_fd_;

  DISALLOW_COPY_AND_ASSIGN(SandboxedHandler);
};

}  // namespace
}  // namespace crashpad

namespace crash_reporter {
namespace internal {

bool BuildHandlerArgs(base::FilePath* handler_path,
                      base::FilePath* database_path,
                      base::FilePath* metrics_path,
                      std::string* url,
                      std::map<std::string, std::string>* process_annotations,
                      std::vector<std::string>* arguments) {
  base::FilePath exe_dir;
#if defined(OS_ANDROID)
  if (!PathService::Get(base::DIR_MODULE, &exe_dir)) {
#else
  if (!PathService::Get(base::DIR_EXE, &exe_dir)) {
#endif  // OS_ANDROID
    DCHECK(false);
    return false;
  }
  *handler_path = exe_dir.Append("crashpad_handler");

  CrashReporterClient* crash_reporter_client = GetCrashReporterClient();
  crash_reporter_client->GetCrashDumpLocation(database_path);
  crash_reporter_client->GetCrashMetricsLocation(metrics_path);

#if defined(GOOGLE_CHROME_BUILD) && defined(OFFICIAL_BUILD)
  *url = "https://clients2.google.com/cr/report";
#else
  *url = std::string();
#endif

  const char* product_name;
  const char* product_version;
  const char* channel;
  crash_reporter_client->GetProductNameAndVersion(&product_name,
                                                  &product_version, &channel);
  (*process_annotations)["prod"] = std::string(product_name);
  (*process_annotations)["ver"] = std::string(product_version);

#if defined(GOOGLE_CHROME_BUILD)
  // Empty means stable.
  const bool allow_empty_channel = true;
#else
  const bool allow_empty_channel = false;
#endif
  std::string channel_string(channel);
  if (allow_empty_channel || !channel_string.empty()) {
    (*process_annotations)["channel"] = channel_string;
  }

#if defined(OS_ANDROID)
  (*process_annotations)["plat"] = std::string("Android");
#else
  (*process_annotations)["plat"] = std::string("Linux");
#endif

  if (crash_reporter_client->ShouldMonitorCrashHandlerExpensively()) {
    arguments->push_back("--monitor-self");
  }

  // Set up --monitor-self-annotation even in the absence of --monitor-self
  // so that minidumps produced by Crashpad's generate_dump tool will
  // contain these annotations.
  arguments->push_back("--monitor-self-annotation=ptype=crashpad-handler");
  return true;
}

base::FilePath PlatformCrashpadInitialization(bool initial_client,
                                              bool browser_process,
                                              bool embedded_handler,
                                              const std::string& user_data_dir,
                                              const base::FilePath& exe_path) {
  DCHECK_EQ(initial_client, browser_process);

  // Not used on Linux/Android.
  DCHECK(!embedded_handler);
  DCHECK(exe_path.empty());

  if (browser_process) {
    base::FilePath handler_path;
    base::FilePath database_path;
    base::FilePath metrics_path;
    std::string url;
    std::map<std::string, std::string> process_annotations;
    std::vector<std::string> arguments;
    if (!BuildHandlerArgs(&handler_path, &database_path, &metrics_path, &url,
                          &process_annotations, &arguments)) {
      return base::FilePath();
    }

    bool result = GetCrashpadClient().StartHandlerAtCrash(
        handler_path, database_path, metrics_path, url, process_annotations,
        arguments);
    DCHECK(result);

    return database_path;
  }

  crashpad::SandboxedHandler* handler = crashpad::SandboxedHandler::Get();
  bool result = handler->Initialize();
  DCHECK(result);

  return base::FilePath();
}

}  // namespace internal
}  // namespace crash_reporter
