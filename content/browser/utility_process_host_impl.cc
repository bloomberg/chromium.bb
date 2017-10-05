// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/utility_process_host_impl.h"

#include <utility>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/browser/browser_child_process_host_impl.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/service_manager/service_manager_context.h"
#include "content/common/child_process_host_impl.h"
#include "content/common/in_process_child_thread_params.h"
#include "content/common/service_manager/child_connection.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/utility_process_host_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/process_type.h"
#include "content/public/common/sandboxed_process_launcher_delegate.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/service_names.mojom.h"
#include "media/base/media_switches.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "services/service_manager/sandbox/sandbox_type.h"
#include "ui/base/ui_base_switches.h"

#if defined(OS_POSIX) && !defined(OS_ANDROID) && !defined(OS_MACOSX)
#include "content/public/browser/zygote_handle_linux.h"
#endif  // defined(OS_POSIX) && !defined(OS_ANDROID) && !defined(OS_MACOSX)

#if defined(OS_WIN)
#include "sandbox/win/src/sandbox_policy.h"
#include "sandbox/win/src/sandbox_types.h"
#endif

namespace content {

// NOTE: changes to this class need to be reviewed by the security team.
class UtilitySandboxedProcessLauncherDelegate
    : public SandboxedProcessLauncherDelegate {
 public:
  UtilitySandboxedProcessLauncherDelegate(
      const base::FilePath& exposed_dir,
      bool launch_elevated,
      service_manager::SandboxType sandbox_type,
      const base::EnvironmentMap& env)
      : exposed_dir_(exposed_dir),
#if defined(OS_WIN)
        launch_elevated_(launch_elevated),
#elif defined(OS_POSIX)
        env_(env),
#endif  // OS_WIN
        sandbox_type_(sandbox_type) {
    DCHECK(sandbox_type_ == service_manager::SANDBOX_TYPE_NO_SANDBOX ||
           sandbox_type_ == service_manager::SANDBOX_TYPE_UTILITY ||
           sandbox_type_ == service_manager::SANDBOX_TYPE_NETWORK ||
           sandbox_type_ == service_manager::SANDBOX_TYPE_CDM ||
           sandbox_type_ == service_manager::SANDBOX_TYPE_PDF_COMPOSITOR ||
           sandbox_type_ == service_manager::SANDBOX_TYPE_PPAPI);
  }

  ~UtilitySandboxedProcessLauncherDelegate() override {}

#if defined(OS_WIN)
  bool ShouldLaunchElevated() override { return launch_elevated_; }

  bool PreSpawnTarget(sandbox::TargetPolicy* policy) override {
    if (exposed_dir_.empty())
      return true;

    sandbox::ResultCode result;
    result = policy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                             sandbox::TargetPolicy::FILES_ALLOW_ANY,
                             exposed_dir_.value().c_str());
    if (result != sandbox::SBOX_ALL_OK)
      return false;

    base::FilePath exposed_files = exposed_dir_.AppendASCII("*");
    result = policy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                             sandbox::TargetPolicy::FILES_ALLOW_ANY,
                             exposed_files.value().c_str());
    return result == sandbox::SBOX_ALL_OK;
  }

#elif defined(OS_POSIX)

#if !defined(OS_MACOSX) && !defined(OS_ANDROID) && !defined(OS_FUCHSIA)
  ZygoteHandle GetZygote() override {
    if (service_manager::IsUnsandboxedSandboxType(sandbox_type_) ||
        !exposed_dir_.empty()) {
      return nullptr;
    }
    return GetGenericZygote();
  }
#endif  // !defined(OS_MACOSX) && !defined(OS_ANDROID) && !defined(OS_FUCHSIA)
  base::EnvironmentMap GetEnvironment() override { return env_; }
#endif  // OS_WIN

  service_manager::SandboxType GetSandboxType() override {
    return sandbox_type_;
  }

 private:
  base::FilePath exposed_dir_;

#if defined(OS_WIN)
  bool launch_elevated_;
#elif defined(OS_POSIX)
  base::EnvironmentMap env_;
#endif  // OS_WIN
  service_manager::SandboxType sandbox_type_;
};

UtilityMainThreadFactoryFunction g_utility_main_thread_factory = NULL;

UtilityProcessHost* UtilityProcessHost::Create(
    const scoped_refptr<UtilityProcessHostClient>& client,
    const scoped_refptr<base::SequencedTaskRunner>& client_task_runner) {
  return new UtilityProcessHostImpl(client, client_task_runner);
}

void UtilityProcessHostImpl::RegisterUtilityMainThreadFactory(
    UtilityMainThreadFactoryFunction create) {
  g_utility_main_thread_factory = create;
}

UtilityProcessHostImpl::UtilityProcessHostImpl(
    const scoped_refptr<UtilityProcessHostClient>& client,
    const scoped_refptr<base::SequencedTaskRunner>& client_task_runner)
    : client_(client),
      client_task_runner_(client_task_runner),
      sandbox_type_(service_manager::SANDBOX_TYPE_UTILITY),
      run_elevated_(false),
#if defined(OS_LINUX)
      child_flags_(ChildProcessHost::CHILD_ALLOW_SELF),
#else
      child_flags_(ChildProcessHost::CHILD_NORMAL),
#endif
      started_(false),
      name_(base::ASCIIToUTF16("utility process")),
      weak_ptr_factory_(this) {
  process_.reset(new BrowserChildProcessHostImpl(
      PROCESS_TYPE_UTILITY, this, mojom::kUtilityServiceName));
}

UtilityProcessHostImpl::~UtilityProcessHostImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

base::WeakPtr<UtilityProcessHost> UtilityProcessHostImpl::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

bool UtilityProcessHostImpl::Send(IPC::Message* message) {
  if (!StartProcess())
    return false;

  return process_->Send(message);
}

void UtilityProcessHostImpl::SetExposedDir(const base::FilePath& dir) {
  exposed_dir_ = dir;
}

void UtilityProcessHostImpl::SetSandboxType(
    service_manager::SandboxType sandbox_type) {
  DCHECK(sandbox_type != service_manager::SANDBOX_TYPE_INVALID);
  sandbox_type_ = sandbox_type;
}

#if defined(OS_WIN)
void UtilityProcessHostImpl::ElevatePrivileges() {
  sandbox_type_ = service_manager::SANDBOX_TYPE_NO_SANDBOX;
  run_elevated_ = true;
}
#endif

const ChildProcessData& UtilityProcessHostImpl::GetData() {
  return process_->GetData();
}

#if defined(OS_POSIX)
void UtilityProcessHostImpl::SetEnv(const base::EnvironmentMap& env) {
  env_ = env;
}
#endif

bool UtilityProcessHostImpl::Start() {
  return StartProcess();
}

void UtilityProcessHostImpl::BindInterface(
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  process_->child_connection()->BindInterface(interface_name,
                                              std::move(interface_pipe));
}

void UtilityProcessHostImpl::SetName(const base::string16& name) {
  name_ = name;
}

void UtilityProcessHostImpl::SetServiceIdentity(
    const service_manager::Identity& identity) {
  service_identity_ = identity;
}

void UtilityProcessHostImpl::AddFilter(BrowserMessageFilter* filter) {
  process_->AddFilter(filter);
}

bool UtilityProcessHostImpl::StartProcess() {
  if (started_)
    return true;

  started_ = true;
  process_->SetName(name_);
  process_->GetHost()->CreateChannelMojo();

  if (RenderProcessHost::run_renderer_in_process()) {
    DCHECK(g_utility_main_thread_factory);
    // See comment in RenderProcessHostImpl::Init() for the background on why we
    // support single process mode this way.
    in_process_thread_.reset(
        g_utility_main_thread_factory(InProcessChildThreadParams(
            BrowserThread::GetTaskRunnerForThread(BrowserThread::IO),
            process_->GetInProcessBrokerClientInvitation(),
            process_->child_connection()->service_token())));
    in_process_thread_->Start();
  } else {
    const base::CommandLine& browser_command_line =
        *base::CommandLine::ForCurrentProcess();

    bool has_cmd_prefix = browser_command_line.HasSwitch(
        switches::kUtilityCmdPrefix);

    #if defined(OS_ANDROID)
      // readlink("/prof/self/exe") sometimes fails on Android at startup.
      // As a workaround skip calling it here, since the executable name is
      // not needed on Android anyway. See crbug.com/500854.
    std::unique_ptr<base::CommandLine> cmd_line =
        base::MakeUnique<base::CommandLine>(base::CommandLine::NO_PROGRAM);
    #else
      int child_flags = child_flags_;

      // When running under gdb, forking /proc/self/exe ends up forking the gdb
      // executable instead of Chromium. It is almost safe to assume that no
      // updates will happen while a developer is running with
      // |switches::kUtilityCmdPrefix|. See ChildProcessHost::GetChildPath() for
      // a similar case with Valgrind.
      if (has_cmd_prefix)
        child_flags = ChildProcessHost::CHILD_NORMAL;

      base::FilePath exe_path = ChildProcessHost::GetChildPath(child_flags);
      if (exe_path.empty()) {
        NOTREACHED() << "Unable to get utility process binary name.";
        return false;
      }

      std::unique_ptr<base::CommandLine> cmd_line =
          base::MakeUnique<base::CommandLine>(exe_path);
    #endif

    cmd_line->AppendSwitchASCII(switches::kProcessType,
                                switches::kUtilityProcess);
    BrowserChildProcessHostImpl::CopyFeatureAndFieldTrialFlags(cmd_line.get());
    std::string locale = GetContentClient()->browser()->GetApplicationLocale();
    cmd_line->AppendSwitchASCII(switches::kLang, locale);

#if defined(OS_WIN)
    cmd_line->AppendArg(switches::kPrefetchArgumentOther);
#endif  // defined(OS_WIN)

    service_manager::SetCommandLineFlagsForSandboxType(cmd_line.get(),
                                                       sandbox_type_);

    // Browser command-line switches to propagate to the utility process.
    static const char* const kSwitchNames[] = {
      switches::kHostResolverRules,
      switches::kLogNetLog,
      switches::kNoSandbox,
      switches::kProxyServer,
#if defined(OS_MACOSX)
      switches::kEnableSandboxLogging,
#endif
      switches::kUseFakeDeviceForMediaStream,
      switches::kUseFileForFakeVideoCapture,
      switches::kUtilityStartupDialog,
    };
    cmd_line->CopySwitchesFrom(browser_command_line, kSwitchNames,
                               arraysize(kSwitchNames));

    network_session_configurator::CopyNetworkSwitches(browser_command_line,
                                                      cmd_line.get());

    if (has_cmd_prefix) {
      // Launch the utility child process with some prefix
      // (usually "xterm -e gdb --args").
      cmd_line->PrependWrapper(browser_command_line.GetSwitchValueNative(
          switches::kUtilityCmdPrefix));
    }

    if (!exposed_dir_.empty()) {
      cmd_line->AppendSwitchPath(switches::kUtilityProcessAllowedDir,
                                 exposed_dir_);
    }

#if defined(OS_WIN)
    // Let the utility process know if it is intended to be elevated.
    if (run_elevated_)
      cmd_line->AppendSwitch(switches::kUtilityProcessRunningElevated);
#endif

    const bool is_service = service_identity_.has_value();
    if (is_service) {
      GetContentClient()->browser()->AdjustUtilityServiceProcessCommandLine(
          *service_identity_, cmd_line.get());
    }

    process_->Launch(base::MakeUnique<UtilitySandboxedProcessLauncherDelegate>(
                         exposed_dir_, run_elevated_, sandbox_type_, env_),
                     std::move(cmd_line), true);
  }

  return true;
}

bool UtilityProcessHostImpl::OnMessageReceived(const IPC::Message& message) {
  if (!client_.get())
    return true;

  client_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          base::IgnoreResult(&UtilityProcessHostClient::OnMessageReceived),
          client_.get(), message));

  return true;
}

void UtilityProcessHostImpl::OnProcessLaunchFailed(int error_code) {
  if (!client_.get())
    return;

  client_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&UtilityProcessHostClient::OnProcessLaunchFailed, client_,
                     error_code));
}

void UtilityProcessHostImpl::OnProcessCrashed(int exit_code) {
  if (!client_.get())
    return;

  client_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&UtilityProcessHostClient::OnProcessCrashed,
                                client_, exit_code));
}

void UtilityProcessHostImpl::NotifyAndDelete(int error_code) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&UtilityProcessHostImpl::NotifyLaunchFailedAndDelete,
                     weak_ptr_factory_.GetWeakPtr(), error_code));
}

// static
void UtilityProcessHostImpl::NotifyLaunchFailedAndDelete(
    base::WeakPtr<UtilityProcessHostImpl> host,
    int error_code) {
  if (!host)
    return;

  host->OnProcessLaunchFailed(error_code);
  delete host.get();
}

}  // namespace content
