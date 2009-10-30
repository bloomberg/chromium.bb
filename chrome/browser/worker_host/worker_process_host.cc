// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/worker_host/worker_process_host.h"

#include <set>
#include <vector>

#include "base/command_line.h"
#include "base/debug_util.h"
#if defined(OS_POSIX)
#include "base/global_descriptors_posix.h"
#endif
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/child_process_security_policy.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/render_view_host_delegate.h"
#include "chrome/browser/renderer_host/resource_message_filter.h"
#include "chrome/browser/worker_host/message_port_dispatcher.h"
#include "chrome/browser/worker_host/worker_service.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/debug_flags.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/process_watcher.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/result_codes.h"
#include "chrome/common/worker_messages.h"
#include "ipc/ipc_descriptors.h"
#include "ipc/ipc_switches.h"
#include "net/base/registry_controlled_domain.h"

#if defined(OS_WIN)
#include "chrome/browser/sandbox_policy.h"
#endif

// Notifies RenderViewHost that one or more worker objects crashed.
class WorkerCrashTask : public Task {
 public:
  WorkerCrashTask(int render_process_unique_id, int render_view_id)
      : render_process_unique_id_(render_process_unique_id),
        render_view_id_(render_view_id) { }

  void Run() {
    RenderViewHost* host =
        RenderViewHost::FromID(render_process_unique_id_, render_view_id_);
    if (host) {
      RenderViewHostDelegate::BrowserIntegration* integration_delegate =
          host->delegate()->GetBrowserIntegrationDelegate();
      if (integration_delegate)
        integration_delegate->OnCrashedWorker();
    }
  }

 private:
  int render_process_unique_id_;
  int render_view_id_;
};


WorkerProcessHost::WorkerProcessHost(
    ResourceDispatcherHost* resource_dispatcher_host_)
    : ChildProcessHost(WORKER_PROCESS, resource_dispatcher_host_) {
  next_route_id_callback_.reset(NewCallbackWithReturnValue(
      WorkerService::GetInstance(), &WorkerService::next_worker_route_id));
}

WorkerProcessHost::~WorkerProcessHost() {
  // Let interested observers know we are being deleted.
  NotificationService::current()->Notify(
      NotificationType::WORKER_PROCESS_HOST_SHUTDOWN,
      Source<WorkerProcessHost>(this),
      NotificationService::NoDetails());

  // If we crashed, tell the RenderViewHost.
  MessageLoop* ui_loop = WorkerService::GetInstance()->ui_loop();
  for (Instances::iterator i = instances_.begin(); i != instances_.end(); ++i) {
    ui_loop->PostTask(FROM_HERE, new WorkerCrashTask(
        i->renderer_id, i->render_view_route_id));
  }

  ChildProcessSecurityPolicy::GetInstance()->Remove(id());
}

bool WorkerProcessHost::Init() {
  if (!CreateChannel())
    return false;

  FilePath exe_path = GetChildPath();
  if (exe_path.empty())
    return false;

  CommandLine cmd_line(exe_path);
  cmd_line.AppendSwitchWithValue(switches::kProcessType,
                                 switches::kWorkerProcess);
  cmd_line.AppendSwitchWithValue(switches::kProcessChannelID,
                                 ASCIIToWide(channel_id()));
  SetCrashReporterCommandLine(&cmd_line);

  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableNativeWebWorkers)) {
    cmd_line.AppendSwitch(switches::kEnableNativeWebWorkers);
  }

  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kWebWorkerShareProcesses)) {
    cmd_line.AppendSwitch(switches::kWebWorkerShareProcesses);
  }

  base::ProcessHandle process;
#if defined(OS_WIN)
  process = sandbox::StartProcess(&cmd_line);
#else
  // This code is duplicated with browser_render_process_host.cc, but
  // there's not a good place to de-duplicate it.
  base::file_handle_mapping_vector fds_to_map;
  const int ipcfd = channel().GetClientFileDescriptor();
  if (ipcfd > -1) {
    fds_to_map.push_back(std::pair<int, int>(
        ipcfd, kPrimaryIPCChannel + base::GlobalDescriptors::kBaseDescriptor));
  }
  base::LaunchApp(cmd_line.argv(), fds_to_map, false, &process);
#endif
  if (!process)
    return false;
  SetHandle(process);

  ChildProcessSecurityPolicy::GetInstance()->Add(id());

  return true;
}

void WorkerProcessHost::CreateWorker(const WorkerInstance& instance) {
  ChildProcessSecurityPolicy::GetInstance()->GrantRequestURL(
      id(), instance.url);

  instances_.push_back(instance);
  Send(new WorkerProcessMsg_CreateWorker(
      instance.url, instance.worker_route_id));

  UpdateTitle();
  instances_.back().sender->Send(
      new ViewMsg_WorkerCreated(instance.sender_route_id));
}

bool WorkerProcessHost::FilterMessage(const IPC::Message& message,
                                      int sender_pid) {
  for (Instances::iterator i = instances_.begin(); i != instances_.end(); ++i) {
    if (i->sender_id == sender_pid &&
        i->sender_route_id == message.routing_id()) {
      RelayMessage(
          message, this, i->worker_route_id, next_route_id_callback_.get());
      return true;
    }
  }

  return false;
}

URLRequestContext* WorkerProcessHost::GetRequestContext(
    uint32 request_id,
    const ViewHostMsg_Resource_Request& request_data) {
  return NULL;
}

void WorkerProcessHost::OnMessageReceived(const IPC::Message& message) {
  bool msg_is_ok = true;
  bool handled = MessagePortDispatcher::GetInstance()->OnMessageReceived(
      message, this, next_route_id_callback_.get(), &msg_is_ok);

  if (!handled) {
    handled = true;
    IPC_BEGIN_MESSAGE_MAP_EX(WorkerProcessHost, message, msg_is_ok)
      IPC_MESSAGE_HANDLER(ViewHostMsg_CreateDedicatedWorker,
                          OnCreateDedicatedWorker)
      IPC_MESSAGE_HANDLER(ViewHostMsg_CancelCreateDedicatedWorker,
                          OnCancelCreateDedicatedWorker)
      IPC_MESSAGE_HANDLER(ViewHostMsg_ForwardToWorker,
                          OnForwardToWorker)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP_EX()
  }

  if (!msg_is_ok) {
    NOTREACHED();
    base::KillProcess(handle(), ResultCodes::KILLED_BAD_MESSAGE, false);
  }

  if (handled)
    return;

  for (Instances::iterator i = instances_.begin(); i != instances_.end(); ++i) {
    if (i->worker_route_id == message.routing_id()) {
      CallbackWithReturnValue<int>::Type* next_route_id =
          GetNextRouteIdCallback(i->sender);
      RelayMessage(message, i->sender, i->sender_route_id, next_route_id);

      if (message.type() == WorkerHostMsg_WorkerContextDestroyed::ID) {
        instances_.erase(i);
        UpdateTitle();
      }
      break;
    }
  }
}

CallbackWithReturnValue<int>::Type* WorkerProcessHost::GetNextRouteIdCallback(
    IPC::Message::Sender* sender) {
  // We don't keep callbacks for senders associated with workers, so figure out
  // what kind of sender this is, and cast it to the correct class to get the
  // callback.
  for (ChildProcessHost::Iterator iter(ChildProcessInfo::WORKER_PROCESS);
       !iter.Done(); ++iter) {
    WorkerProcessHost* worker = static_cast<WorkerProcessHost*>(*iter);
    if (static_cast<IPC::Message::Sender*>(worker) == sender)
      return worker->next_route_id_callback_.get();
  }

  // Must be a ResourceMessageFilter.
  return static_cast<ResourceMessageFilter*>(sender)->next_route_id_callback();
}

void WorkerProcessHost::RelayMessage(
    const IPC::Message& message,
    IPC::Message::Sender* sender,
    int route_id,
    CallbackWithReturnValue<int>::Type* next_route_id) {

  if (message.type() == WorkerMsg_PostMessage::ID) {
    // We want to send the receiver a routing id for the new channel, so
    // crack the message first.
    string16 msg;
    std::vector<int> sent_message_port_ids;
    std::vector<int> new_routing_ids;
    if (!WorkerMsg_PostMessage::Read(
            &message, &msg, &sent_message_port_ids, &new_routing_ids)) {
      return;
    }
    DCHECK(sent_message_port_ids.size() == new_routing_ids.size());

    for (size_t i = 0; i < sent_message_port_ids.size(); ++i) {
      new_routing_ids[i] = next_route_id->Run();
      MessagePortDispatcher::GetInstance()->UpdateMessagePort(
          sent_message_port_ids[i], sender, new_routing_ids[i], next_route_id);
    }

    sender->Send(new WorkerMsg_PostMessage(
        route_id, msg, sent_message_port_ids, new_routing_ids));

    // Send any queued messages to the sent message ports.  We can only do this
    // after sending the above message, since it's the one that sets up the
    // message port route which the queued messages are sent to.
    for (size_t i = 0; i < sent_message_port_ids.size(); ++i) {
      MessagePortDispatcher::GetInstance()->
          SendQueuedMessagesIfPossible(sent_message_port_ids[i]);
    }
  } else if (message.type() == WorkerMsg_Connect::ID) {
    // Crack the SharedWorker Connect message to setup routing for the port.
    int sent_message_port_id;
    int new_routing_id;
    if (!WorkerMsg_Connect::Read(
            &message, &sent_message_port_id, &new_routing_id)) {
      return;
    }
    new_routing_id = next_route_id->Run();
    MessagePortDispatcher::GetInstance()->UpdateMessagePort(
        sent_message_port_id, sender, new_routing_id, next_route_id);

    // Resend the message with the new routing id.
    sender->Send(new WorkerMsg_Connect(
        route_id, sent_message_port_id, new_routing_id));

    // Send any queued messages for the sent port.
    MessagePortDispatcher::GetInstance()->SendQueuedMessagesIfPossible(
        sent_message_port_id);
  } else {
    IPC::Message* new_message = new IPC::Message(message);
    new_message->set_routing_id(route_id);
    sender->Send(new_message);
    return;
  }
}

void WorkerProcessHost::SenderShutdown(IPC::Message::Sender* sender) {
  for (Instances::iterator i = instances_.begin(); i != instances_.end();) {
    if (i->sender == sender) {
      Send(new WorkerMsg_TerminateWorkerContext(i->worker_route_id));
      i = instances_.erase(i);
    } else {
      ++i;
    }
  }
}

void WorkerProcessHost::UpdateTitle() {
  std::set<std::string> titles;
  for (Instances::iterator i = instances_.begin(); i != instances_.end(); ++i) {
    std::string title =
        net::RegistryControlledDomainService::GetDomainAndRegistry(i->url);
    // Use the host name if the domain is empty, i.e. localhost or IP address.
    if (title.empty())
      title = i->url.host();
    // If the host name is empty, i.e. file url, use the path.
    if (title.empty())
      title = i->url.path();
    titles.insert(title);
  }

  std::string display_title;
  for (std::set<std::string>::iterator i = titles.begin();
       i != titles.end(); ++i) {
    if (!display_title.empty())
      display_title += ", ";
    display_title += *i;
  }

  set_name(ASCIIToWide(display_title));
}

void WorkerProcessHost::OnCreateDedicatedWorker(const GURL& url,
                                                int render_view_route_id,
                                                int* route_id) {
  DCHECK(instances_.size() == 1);  // Only called when one process per worker.
  *route_id = WorkerService::GetInstance()->next_worker_route_id();
  WorkerService::GetInstance()->CreateDedicatedWorker(
      url, instances_.front().renderer_id,
      instances_.front().render_view_route_id, this, id(), *route_id);
}

void WorkerProcessHost::OnCancelCreateDedicatedWorker(int route_id) {
  WorkerService::GetInstance()->CancelCreateDedicatedWorker(id(), route_id);
}

void WorkerProcessHost::OnForwardToWorker(const IPC::Message& message) {
  WorkerService::GetInstance()->ForwardMessage(message, id());
}
