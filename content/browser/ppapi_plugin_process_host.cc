// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/ppapi_plugin_process_host.h"

#include <string>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/process_util.h"
#include "base/utf_string_conversions.h"
#include "content/browser/browser_child_process_host_impl.h"
#include "content/browser/plugin_service_impl.h"
#include "content/browser/renderer_host/render_message_filter.h"
#include "content/common/child_process_host_impl.h"
#include "content/common/child_process_messages.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/pepper_plugin_info.h"
#include "content/public/common/process_type.h"
#include "ipc/ipc_switches.h"
#include "net/base/network_change_notifier.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ui/base/ui_base_switches.h"
#include "webkit/plugins/plugin_switches.h"

#if defined(OS_WIN)
#include "content/common/sandbox_win.h"
#include "content/public/common/sandboxed_process_launcher_delegate.h"
#include "sandbox/win/src/sandbox_policy.h"
#endif

namespace content {

#if defined(OS_WIN)
// NOTE: changes to this class need to be reviewed by the security team.
class PpapiPluginSandboxedProcessLauncherDelegate
    : public content::SandboxedProcessLauncherDelegate {
 public:
  PpapiPluginSandboxedProcessLauncherDelegate() {}
  virtual ~PpapiPluginSandboxedProcessLauncherDelegate() {}

  virtual void PreSpawnTarget(sandbox::TargetPolicy* policy,
                              bool* success) {
    // The Pepper process as locked-down as a renderer execpt that it can
    // create the server side of chrome pipes.
    sandbox::ResultCode result;
    result = policy->AddRule(sandbox::TargetPolicy::SUBSYS_NAMED_PIPES,
                             sandbox::TargetPolicy::NAMEDPIPES_ALLOW_ANY,
                             L"\\\\.\\pipe\\chrome.*");
    *success = (result == sandbox::SBOX_ALL_OK);
  }
};
#endif  // OS_WIN

class PpapiPluginProcessHost::PluginNetworkObserver
    : public net::NetworkChangeNotifier::IPAddressObserver,
      public net::NetworkChangeNotifier::ConnectionTypeObserver {
 public:
  explicit PluginNetworkObserver(PpapiPluginProcessHost* process_host)
      : process_host_(process_host) {
    net::NetworkChangeNotifier::AddIPAddressObserver(this);
    net::NetworkChangeNotifier::AddConnectionTypeObserver(this);
  }

  virtual ~PluginNetworkObserver() {
    net::NetworkChangeNotifier::RemoveConnectionTypeObserver(this);
    net::NetworkChangeNotifier::RemoveIPAddressObserver(this);
  }

  // IPAddressObserver implementation.
  virtual void OnIPAddressChanged() OVERRIDE {
    // TODO(brettw) bug 90246: This doesn't seem correct. The online/offline
    // notification seems like it should be sufficient, but I don't see that
    // when I unplug and replug my network cable. Sending this notification when
    // "something" changes seems to make Flash reasonably happy, but seems
    // wrong. We should really be able to provide the real online state in
    // OnConnectionTypeChanged().
    process_host_->Send(new PpapiMsg_SetNetworkState(true));
  }

  // ConnectionTypeObserver implementation.
  virtual void OnConnectionTypeChanged(
      net::NetworkChangeNotifier::ConnectionType type) OVERRIDE {
    process_host_->Send(new PpapiMsg_SetNetworkState(
        type != net::NetworkChangeNotifier::CONNECTION_NONE));
  }

 private:
  PpapiPluginProcessHost* const process_host_;
};

PpapiPluginProcessHost::~PpapiPluginProcessHost() {
  DVLOG(1) << "PpapiPluginProcessHost" << (is_broker_ ? "[broker]" : "")
           << "~PpapiPluginProcessHost()";
  CancelRequests();
}

// static
PpapiPluginProcessHost* PpapiPluginProcessHost::CreatePluginHost(
    const PepperPluginInfo& info,
    const base::FilePath& profile_data_directory,
    net::HostResolver* host_resolver) {
  PpapiPluginProcessHost* plugin_host = new PpapiPluginProcessHost(
      info, profile_data_directory, host_resolver);
  if (plugin_host->Init(info))
    return plugin_host;

  NOTREACHED();  // Init is not expected to fail.
  return NULL;
}

// static
PpapiPluginProcessHost* PpapiPluginProcessHost::CreateBrokerHost(
    const PepperPluginInfo& info) {
  PpapiPluginProcessHost* plugin_host =
      new PpapiPluginProcessHost();
  if (plugin_host->Init(info))
    return plugin_host;

  NOTREACHED();  // Init is not expected to fail.
  return NULL;
}

// static
void PpapiPluginProcessHost::DidCreateOutOfProcessInstance(
    int plugin_process_id,
    int32 pp_instance,
    const PepperRendererInstanceData& instance_data) {
  for (PpapiPluginProcessHostIterator iter; !iter.Done(); ++iter) {
    if (iter->process_.get() &&
        iter->process_->GetData().id == plugin_process_id) {
      // Found the plugin.
      iter->host_impl_->AddInstance(pp_instance, instance_data);
      return;
    }
  }
  // We'll see this passed with a 0 process ID for the browser tag stuff that
  // is currently in the process of being removed.
  //
  // TODO(brettw) When old browser tag impl is removed
  // (PepperPluginDelegateImpl::CreateBrowserPluginModule passes a 0 plugin
  // process ID) this should be converted to a NOTREACHED().
  DCHECK(plugin_process_id == 0)
      << "Renderer sent a bad plugin process host ID";
}

// static
void PpapiPluginProcessHost::DidDeleteOutOfProcessInstance(
    int plugin_process_id,
    int32 pp_instance) {
  for (PpapiPluginProcessHostIterator iter; !iter.Done(); ++iter) {
    if (iter->process_.get() &&
        iter->process_->GetData().id == plugin_process_id) {
      // Found the plugin.
      iter->host_impl_->DeleteInstance(pp_instance);
      return;
    }
  }
  // Note: It's possible that the plugin process has already been deleted by
  // the time this message is received. For example, it could have crashed.
  // That's OK, we can just ignore this message.
}

// static
void PpapiPluginProcessHost::FindByName(
    const string16& name,
    std::vector<PpapiPluginProcessHost*>* hosts) {
  for (PpapiPluginProcessHostIterator iter; !iter.Done(); ++iter) {
    if (iter->process_.get() && iter->process_->GetData().name == name)
      hosts->push_back(*iter);
  }
}

bool PpapiPluginProcessHost::Send(IPC::Message* message) {
  return process_->Send(message);
}

void PpapiPluginProcessHost::OpenChannelToPlugin(Client* client) {
  if (process_->GetHost()->IsChannelOpening()) {
    // The channel is already in the process of being opened.  Put
    // this "open channel" request into a queue of requests that will
    // be run once the channel is open.
    pending_requests_.push_back(client);
    return;
  }

  // We already have an open channel, send a request right away to plugin.
  RequestPluginChannel(client);
}

PpapiPluginProcessHost::PpapiPluginProcessHost(
    const PepperPluginInfo& info,
    const base::FilePath& profile_data_directory,
    net::HostResolver* host_resolver)
    : permissions_(
          ppapi::PpapiPermissions::GetForCommandLine(info.permissions)),
      profile_data_directory_(profile_data_directory),
      is_broker_(false) {
  process_.reset(new BrowserChildProcessHostImpl(
      PROCESS_TYPE_PPAPI_PLUGIN, this));

  filter_ = new PepperMessageFilter(permissions_, host_resolver);

  host_impl_.reset(new BrowserPpapiHostImpl(this, permissions_, info.name,
                                            profile_data_directory,
                                            false));

  process_->GetHost()->AddFilter(filter_.get());
  process_->GetHost()->AddFilter(host_impl_->message_filter());

  GetContentClient()->browser()->DidCreatePpapiPlugin(host_impl_.get());

  // Only request network status updates if the plugin has dev permissions.
  if (permissions_.HasPermission(ppapi::PERMISSION_DEV))
    network_observer_.reset(new PluginNetworkObserver(this));
}

PpapiPluginProcessHost::PpapiPluginProcessHost()
    : is_broker_(true) {
  process_.reset(new BrowserChildProcessHostImpl(
      PROCESS_TYPE_PPAPI_BROKER, this));

  ppapi::PpapiPermissions permissions;  // No permissions.
  // The plugin name and profile data directory shouldn't be needed for the
  // broker.
  std::string plugin_name;
  base::FilePath profile_data_directory;
  host_impl_.reset(new BrowserPpapiHostImpl(this, permissions, plugin_name,
                                            profile_data_directory,
                                            false));
}

bool PpapiPluginProcessHost::Init(const PepperPluginInfo& info) {
  plugin_path_ = info.path;
  if (info.name.empty()) {
    process_->SetName(plugin_path_.BaseName().LossyDisplayName());
  } else {
    process_->SetName(UTF8ToUTF16(info.name));
  }

  std::string channel_id = process_->GetHost()->CreateChannel();
  if (channel_id.empty())
    return false;

  const CommandLine& browser_command_line = *CommandLine::ForCurrentProcess();
  CommandLine::StringType plugin_launcher =
      browser_command_line.GetSwitchValueNative(switches::kPpapiPluginLauncher);

#if defined(OS_LINUX)
  int flags = plugin_launcher.empty() ? ChildProcessHost::CHILD_ALLOW_SELF :
                                        ChildProcessHost::CHILD_NORMAL;
#else
  int flags = ChildProcessHost::CHILD_NORMAL;
#endif
  base::FilePath exe_path = ChildProcessHost::GetChildPath(flags);
  if (exe_path.empty())
    return false;

  CommandLine* cmd_line = new CommandLine(exe_path);
  cmd_line->AppendSwitchASCII(switches::kProcessType,
                              is_broker_ ? switches::kPpapiBrokerProcess
                                         : switches::kPpapiPluginProcess);
  cmd_line->AppendSwitchASCII(switches::kProcessChannelID, channel_id);

  // These switches are forwarded to both plugin and broker pocesses.
  static const char* kCommonForwardSwitches[] = {
    switches::kVModule
  };
  cmd_line->CopySwitchesFrom(browser_command_line, kCommonForwardSwitches,
                             arraysize(kCommonForwardSwitches));

  if (!is_broker_) {
    // TODO(vtl): Stop passing flash args in the command line, on windows is
    // going to explode.
    static const char* kPluginForwardSwitches[] = {
      switches::kDisablePepperThreading,
      switches::kDisableSeccompFilterSandbox,
#if defined(OS_MACOSX)
      switches::kEnableSandboxLogging,
#endif
      switches::kNoSandbox,
      switches::kPpapiFlashArgs,
      switches::kPpapiStartupDialog,
    };
    cmd_line->CopySwitchesFrom(browser_command_line, kPluginForwardSwitches,
                               arraysize(kPluginForwardSwitches));
  }

  std::string locale = GetContentClient()->browser()->GetApplicationLocale();
  if (!locale.empty()) {
    // Pass on the locale so the plugin will know what language we're using.
    cmd_line->AppendSwitchASCII(switches::kLang, locale);
  }

  if (!plugin_launcher.empty())
    cmd_line->PrependWrapper(plugin_launcher);

  // On posix, never use the zygote for the broker. Also, only use the zygote if
  // the plugin is sandboxed, and we are not using a plugin launcher - having a
  // plugin launcher means we need to use another process instead of just
  // forking the zygote.
#if defined(OS_POSIX)
  bool use_zygote = !is_broker_ && plugin_launcher.empty() && info.is_sandboxed;
  if (!info.is_sandboxed)
    cmd_line->AppendSwitchASCII(switches::kNoSandbox, "");
#endif  // OS_POSIX
  process_->Launch(
#if defined(OS_WIN)
      is_broker_ ? NULL : new PpapiPluginSandboxedProcessLauncherDelegate,
#elif defined(OS_POSIX)
      use_zygote,
      base::EnvironmentVector(),
#endif
      cmd_line);
  return true;
}

void PpapiPluginProcessHost::RequestPluginChannel(Client* client) {
  base::ProcessHandle process_handle;
  int renderer_child_id;
  client->GetPpapiChannelInfo(&process_handle, &renderer_child_id);

  base::ProcessId process_id = (process_handle == base::kNullProcessHandle) ?
      0 : base::GetProcId(process_handle);

  // We can't send any sync messages from the browser because it might lead to
  // a hang. See the similar code in PluginProcessHost for more description.
  PpapiMsg_CreateChannel* msg = new PpapiMsg_CreateChannel(
      process_id, renderer_child_id, client->OffTheRecord());
  msg->set_unblock(true);
  if (Send(msg)) {
    sent_requests_.push(client);
  } else {
    client->OnPpapiChannelOpened(IPC::ChannelHandle(), base::kNullProcessId, 0);
  }
}

void PpapiPluginProcessHost::OnProcessLaunched() {
  host_impl_->set_plugin_process_handle(process_->GetHandle());
}

void PpapiPluginProcessHost::OnProcessCrashed(int exit_code) {
  PluginServiceImpl::GetInstance()->RegisterPluginCrash(plugin_path_);
}

bool PpapiPluginProcessHost::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PpapiPluginProcessHost, msg)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_ChannelCreated,
                        OnRendererPluginChannelCreated)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  DCHECK(handled);
  return handled;
}

// Called when the browser <--> plugin channel has been established.
void PpapiPluginProcessHost::OnChannelConnected(int32 peer_pid) {
  // This will actually load the plugin. Errors will actually not be reported
  // back at this point. Instead, the plugin will fail to establish the
  // connections when we request them on behalf of the renderer(s).
  Send(new PpapiMsg_LoadPlugin(plugin_path_, permissions_));

  // Process all pending channel requests from the renderers.
  for (size_t i = 0; i < pending_requests_.size(); i++)
    RequestPluginChannel(pending_requests_[i]);
  pending_requests_.clear();
}

// Called when the browser <--> plugin channel has an error. This normally
// means the plugin has crashed.
void PpapiPluginProcessHost::OnChannelError() {
  DVLOG(1) << "PpapiPluginProcessHost" << (is_broker_ ? "[broker]" : "")
           << "::OnChannelError()";
  // We don't need to notify the renderers that were communicating with the
  // plugin since they have their own channels which will go into the error
  // state at the same time. Instead, we just need to notify any renderers
  // that have requested a connection but have not yet received one.
  CancelRequests();
}

void PpapiPluginProcessHost::CancelRequests() {
  DVLOG(1) << "PpapiPluginProcessHost" << (is_broker_ ? "[broker]" : "")
           << "CancelRequests()";
  for (size_t i = 0; i < pending_requests_.size(); i++) {
    pending_requests_[i]->OnPpapiChannelOpened(IPC::ChannelHandle(),
                                               base::kNullProcessId, 0);
  }
  pending_requests_.clear();

  while (!sent_requests_.empty()) {
    sent_requests_.front()->OnPpapiChannelOpened(IPC::ChannelHandle(),
                                                 base::kNullProcessId, 0);
    sent_requests_.pop();
  }
}

// Called when a new plugin <--> renderer channel has been created.
void PpapiPluginProcessHost::OnRendererPluginChannelCreated(
    const IPC::ChannelHandle& channel_handle) {
  if (sent_requests_.empty())
    return;

  // All requests should be processed FIFO, so the next item in the
  // sent_requests_ queue should be the one that the plugin just created.
  Client* client = sent_requests_.front();
  sent_requests_.pop();

  const ChildProcessData& data = process_->GetData();
  client->OnPpapiChannelOpened(channel_handle, base::GetProcId(data.handle),
                               data.id);
}

}  // namespace content
