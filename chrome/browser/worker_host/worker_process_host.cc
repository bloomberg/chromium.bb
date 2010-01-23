// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/worker_host/worker_process_host.h"

#include <set>
#include <vector>

#include "base/command_line.h"
#include "base/debug_util.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/child_process_security_policy.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/render_view_host_delegate.h"
#include "chrome/browser/renderer_host/resource_message_filter.h"
#include "chrome/browser/worker_host/message_port_dispatcher.h"
#include "chrome/browser/worker_host/worker_service.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/debug_flags.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/result_codes.h"
#include "chrome/common/worker_messages.h"
#include "ipc/ipc_switches.h"
#include "net/base/registry_controlled_domain.h"

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
    ResourceDispatcherHost* resource_dispatcher_host)
    : ChildProcessHost(WORKER_PROCESS, resource_dispatcher_host) {
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
  for (Instances::iterator i = instances_.begin(); i != instances_.end(); ++i) {
    ChromeThread::PostTask(
        ChromeThread::UI, FROM_HERE,
        new WorkerCrashTask(i->renderer_id(), i->render_view_route_id()));
  }

  ChildProcessSecurityPolicy::GetInstance()->Remove(id());
}

bool WorkerProcessHost::Init() {
  if (!CreateChannel())
    return false;

  FilePath exe_path = GetChildPath();
  if (exe_path.empty())
    return false;

  CommandLine* cmd_line = new CommandLine(exe_path);
  cmd_line->AppendSwitchWithValue(switches::kProcessType,
                                  switches::kWorkerProcess);
  cmd_line->AppendSwitchWithValue(switches::kProcessChannelID,
                                  ASCIIToWide(channel_id()));
  SetCrashReporterCommandLine(cmd_line);

  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableNativeWebWorkers)) {
    cmd_line->AppendSwitch(switches::kEnableNativeWebWorkers);
  }

  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kWebWorkerShareProcesses)) {
    cmd_line->AppendSwitch(switches::kWebWorkerShareProcesses);
  }

  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kWorkerStartupDialog)) {
    cmd_line->AppendSwitch(switches::kWorkerStartupDialog);
  }

  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableLogging)) {
    cmd_line->AppendSwitch(switches::kEnableLogging);
  }
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kLoggingLevel)) {
    const std::wstring level =
        CommandLine::ForCurrentProcess()->GetSwitchValue(
            switches::kLoggingLevel);
    cmd_line->AppendSwitchWithValue(
        switches::kLoggingLevel, level);
  }

  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableWebSockets)) {
    cmd_line->AppendSwitch(switches::kDisableWebSockets);
  }
#if defined(OS_WIN)
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableDesktopNotifications)) {
    cmd_line->AppendSwitch(switches::kDisableDesktopNotifications);
  }
#endif

#if defined(OS_POSIX)
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kRendererCmdPrefix)) {
    const std::wstring prefix =
        CommandLine::ForCurrentProcess()->GetSwitchValue(
            switches::kRendererCmdPrefix);
    cmd_line->PrependWrapper(prefix);
  }
#endif

  Launch(
#if defined(OS_WIN)
      FilePath(),
#elif defined(OS_POSIX)
      base::environment_vector(),
#endif
      cmd_line);

  ChildProcessSecurityPolicy::GetInstance()->Add(id());

  return true;
}

void WorkerProcessHost::CreateWorker(const WorkerInstance& instance) {
  ChildProcessSecurityPolicy::GetInstance()->GrantRequestURL(
      id(), instance.url());

  instances_.push_back(instance);
  Send(new WorkerProcessMsg_CreateWorker(instance.url(),
                                         instance.shared(),
                                         instance.name(),
                                         instance.worker_route_id()));

  UpdateTitle();
  WorkerInstance::SenderInfo info = instances_.back().GetSender();
  info.first->Send(new ViewMsg_WorkerCreated(info.second));
}

bool WorkerProcessHost::FilterMessage(const IPC::Message& message,
                                      IPC::Message::Sender* sender) {
  for (Instances::iterator i = instances_.begin(); i != instances_.end(); ++i) {
    if (!i->closed() && i->HasSender(sender, message.routing_id())) {
      RelayMessage(
          message, this, i->worker_route_id(), next_route_id_callback_.get());
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

// Sent to notify the browser process when a worker context invokes close(), so
// no new connections are sent to shared workers.
void WorkerProcessHost::OnWorkerContextClosed(int worker_route_id) {
  for (Instances::iterator i = instances_.begin(); i != instances_.end(); ++i) {
    if (i->worker_route_id() == worker_route_id) {
      // Set the closed flag - this will stop any further messages from
      // being sent to the worker (messages can still be sent from the worker,
      // for exception reporting, etc).
      i->set_closed(true);
      break;
    }
  }
}

void WorkerProcessHost::OnMessageReceived(const IPC::Message& message) {
  bool msg_is_ok = true;
  bool handled = MessagePortDispatcher::GetInstance()->OnMessageReceived(
      message, this, next_route_id_callback_.get(), &msg_is_ok);

  if (!handled) {
    handled = true;
    IPC_BEGIN_MESSAGE_MAP_EX(WorkerProcessHost, message, msg_is_ok)
      IPC_MESSAGE_HANDLER(ViewHostMsg_CreateWorker, OnCreateWorker)
      IPC_MESSAGE_HANDLER(ViewHostMsg_LookupSharedWorker, OnLookupSharedWorker)
      IPC_MESSAGE_HANDLER(ViewHostMsg_CancelCreateDedicatedWorker,
                          OnCancelCreateDedicatedWorker)
      IPC_MESSAGE_HANDLER(WorkerHostMsg_WorkerContextClosed,
                          OnWorkerContextClosed);
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
    if (i->worker_route_id() == message.routing_id()) {
      if (!i->shared()) {
        // Don't relay messages from shared workers (all communication is via
        // the message port).
        WorkerInstance::SenderInfo info = i->GetSender();
        CallbackWithReturnValue<int>::Type* next_route_id =
            GetNextRouteIdCallback(info.first);
        RelayMessage(message, info.first, info.second, next_route_id);
      }

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
    bool shutdown = false;
    i->RemoveSenders(sender);
    if (i->shared()) {
      i->RemoveAllAssociatedDocuments(sender);
      if (i->IsDocumentSetEmpty()) {
        shutdown = true;
      }
    } else if (i->NumSenders() == 0) {
      shutdown = true;
    }
    if (shutdown) {
      Send(new WorkerMsg_TerminateWorkerContext(i->worker_route_id()));
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
        net::RegistryControlledDomainService::GetDomainAndRegistry(i->url());
    // Use the host name if the domain is empty, i.e. localhost or IP address.
    if (title.empty())
      title = i->url().host();
    // If the host name is empty, i.e. file url, use the path.
    if (title.empty())
      title = i->url().path();
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

void WorkerProcessHost::OnLookupSharedWorker(const GURL& url,
                                             const string16& name,
                                             unsigned long long document_id,
                                             int* route_id,
                                             bool* url_mismatch) {
  int new_route_id = WorkerService::GetInstance()->next_worker_route_id();
  // TODO(atwilson): Add code to merge document sets for nested shared workers.
  bool worker_found = WorkerService::GetInstance()->LookupSharedWorker(
      url, name, instances_.front().off_the_record(), document_id, this,
      new_route_id, url_mismatch);
  *route_id = worker_found ? new_route_id : MSG_ROUTING_NONE;
}

void WorkerProcessHost::OnCreateWorker(const GURL& url,
                                       bool shared,
                                       const string16& name,
                                       int render_view_route_id,
                                       int* route_id) {
  DCHECK(instances_.size() == 1);  // Only called when one process per worker.
  *route_id = WorkerService::GetInstance()->next_worker_route_id();
  WorkerService::GetInstance()->CreateWorker(
      url, shared, instances_.front().off_the_record(), name,
      instances_.front().renderer_id(),
      instances_.front().render_view_route_id(), this, *route_id);
  // TODO(atwilson): Add code to merge document sets for nested shared workers.
}

void WorkerProcessHost::OnCancelCreateDedicatedWorker(int route_id) {
  WorkerService::GetInstance()->CancelCreateDedicatedWorker(this, route_id);
}

void WorkerProcessHost::OnForwardToWorker(const IPC::Message& message) {
  WorkerService::GetInstance()->ForwardMessage(message, this);
}

void WorkerProcessHost::DocumentDetached(IPC::Message::Sender* parent,
                                         unsigned long long document_id)
{
  // Walk all instances and remove the document from their document set.
  for (Instances::iterator i = instances_.begin(); i != instances_.end();) {
    if (!i->shared()) {
      ++i;
    } else {
      i->RemoveFromDocumentSet(parent, document_id);
      if (i->IsDocumentSetEmpty()) {
        // This worker has no more associated documents - shut it down.
        Send(new WorkerMsg_TerminateWorkerContext(i->worker_route_id()));
        i = instances_.erase(i);
      } else {
        ++i;
      }
    }
  }
}

WorkerProcessHost::WorkerInstance::WorkerInstance(const GURL& url,
                                                  bool shared,
                                                  bool off_the_record,
                                                  const string16& name,
                                                  int renderer_id,
                                                  int render_view_route_id,
                                                  int worker_route_id)
    : url_(url),
      shared_(shared),
      off_the_record_(off_the_record),
      closed_(false),
      name_(name),
      renderer_id_(renderer_id),
      render_view_route_id_(render_view_route_id),
      worker_route_id_(worker_route_id) {
}

// Compares an instance based on the algorithm in the WebWorkers spec - an
// instance matches if the origins of the URLs match, and:
// a) the names are non-empty and equal
// -or-
// b) the names are both empty, and the urls are equal
bool WorkerProcessHost::WorkerInstance::Matches(
    const GURL& match_url, const string16& match_name,
    bool off_the_record) const {
  // Only match open shared workers.
  if (!shared_ || closed_)
    return false;

  // Incognito workers don't match non-incognito workers.
  if (off_the_record_ != off_the_record)
    return false;

  if (url_.GetOrigin() != match_url.GetOrigin())
    return false;

  if (name_.empty() && match_name.empty())
    return url_ == match_url;

  return name_ == match_name;
}

void WorkerProcessHost::WorkerInstance::AddToDocumentSet(
    IPC::Message::Sender* parent, unsigned long long document_id) {
  if (!IsInDocumentSet(parent, document_id)) {
    DocumentInfo info(parent, document_id);
    document_set_.push_back(info);
  }
}

bool WorkerProcessHost::WorkerInstance::IsInDocumentSet(
    IPC::Message::Sender* parent, unsigned long long document_id) const {
  for (DocumentSet::const_iterator i = document_set_.begin();
       i != document_set_.end(); ++i) {
    if (i->first == parent && i->second == document_id)
      return true;
  }
  return false;
}

void WorkerProcessHost::WorkerInstance::RemoveFromDocumentSet(
    IPC::Message::Sender* parent, unsigned long long document_id) {
  for (DocumentSet::iterator i = document_set_.begin();
       i != document_set_.end(); i++) {
    if (i->first == parent && i->second == document_id) {
      document_set_.erase(i);
      break;
    }
  }
  // Should not be duplicate copies in the document set.
  DCHECK(!IsInDocumentSet(parent, document_id));
}

void WorkerProcessHost::WorkerInstance::RemoveAllAssociatedDocuments(
    IPC::Message::Sender* parent) {
  for (DocumentSet::iterator i = document_set_.begin();
       i != document_set_.end();) {
    if (i->first == parent)
      i = document_set_.erase(i);
    else
      ++i;
  }
}

void WorkerProcessHost::WorkerInstance::AddSender(IPC::Message::Sender* sender,
                                                  int sender_route_id) {
  if (!HasSender(sender, sender_route_id)) {
    SenderInfo info(sender, sender_route_id);
    senders_.push_back(info);
  }
  // Only shared workers can have more than one associated sender.
  DCHECK(shared_ || senders_.size() == 1);
}

void WorkerProcessHost::WorkerInstance::RemoveSender(
    IPC::Message::Sender* sender, int sender_route_id) {
  for (SenderList::iterator i = senders_.begin(); i != senders_.end();) {
    if (i->first == sender && i->second == sender_route_id)
      i = senders_.erase(i);
    else
      ++i;
  }
  // Should not be duplicate copies in the sender set.
  DCHECK(!HasSender(sender, sender_route_id));
}

void WorkerProcessHost::WorkerInstance::RemoveSenders(
    IPC::Message::Sender* sender) {
  for (SenderList::iterator i = senders_.begin(); i != senders_.end();) {
    if (i->first == sender)
      i = senders_.erase(i);
    else
      ++i;
  }
}

bool WorkerProcessHost::WorkerInstance::HasSender(
    IPC::Message::Sender* sender, int sender_route_id) const {
  for (SenderList::const_iterator i = senders_.begin(); i != senders_.end();
       ++i) {
    if (i->first == sender && i->second == sender_route_id)
      return true;
  }
  return false;
}

WorkerProcessHost::WorkerInstance::SenderInfo
WorkerProcessHost::WorkerInstance::GetSender() const {
  DCHECK(NumSenders() == 1);
  return *senders_.begin();
}
