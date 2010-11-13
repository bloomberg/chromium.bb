// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/worker_host/worker_process_host.h"

#include <set>
#include <vector>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/appcache/appcache_dispatcher_host.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/child_process_security_policy.h"
#include "chrome/browser/file_system/file_system_dispatcher_host.h"
#include "chrome/browser/mime_registry_dispatcher.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/blob_dispatcher_host.h"
#include "chrome/browser/renderer_host/database_dispatcher_host.h"
#include "chrome/browser/renderer_host/file_utilities_dispatcher_host.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/render_view_host_delegate.h"
#include "chrome/browser/renderer_host/resource_message_filter.h"
#include "chrome/browser/worker_host/message_port_dispatcher.h"
#include "chrome/browser/worker_host/worker_service.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/debug_flags.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/render_messages_params.h"
#include "chrome/common/result_codes.h"
#include "chrome/common/worker_messages.h"
#include "net/base/mime_util.h"
#include "ipc/ipc_switches.h"
#include "net/base/registry_controlled_domain.h"
#include "webkit/fileapi/file_system_path_manager.h"

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
    ResourceDispatcherHost* resource_dispatcher_host,
    ChromeURLRequestContext *request_context)
    : BrowserChildProcessHost(WORKER_PROCESS, resource_dispatcher_host),
      request_context_(request_context),
      appcache_dispatcher_host_(
          new AppCacheDispatcherHost(request_context)),
      ALLOW_THIS_IN_INITIALIZER_LIST(blob_dispatcher_host_(
          new BlobDispatcherHost(
              this->id(), request_context->blob_storage_context()))),
      ALLOW_THIS_IN_INITIALIZER_LIST(file_system_dispatcher_host_(
          new FileSystemDispatcherHost(this, request_context))),
      ALLOW_THIS_IN_INITIALIZER_LIST(file_utilities_dispatcher_host_(
          new FileUtilitiesDispatcherHost(this, this->id()))),
      ALLOW_THIS_IN_INITIALIZER_LIST(mime_registry_dispatcher_(
          new MimeRegistryDispatcher(this))) {
  next_route_id_callback_.reset(NewCallbackWithReturnValue(
      WorkerService::GetInstance(), &WorkerService::next_worker_route_id));
  db_dispatcher_host_ = new DatabaseDispatcherHost(
      request_context->database_tracker(), this,
      request_context_->host_content_settings_map());
  appcache_dispatcher_host_->Initialize(this);
}

WorkerProcessHost::~WorkerProcessHost() {
  // Shut down the database dispatcher host.
  db_dispatcher_host_->Shutdown();

  // Shut down the blob dispatcher host.
  blob_dispatcher_host_->Shutdown();

  // Shut down the file system dispatcher host.
  file_system_dispatcher_host_->Shutdown();

  // Shut down the file utilities dispatcher host.
  file_utilities_dispatcher_host_->Shutdown();

  // Shut down the mime registry dispatcher host.
  mime_registry_dispatcher_->Shutdown();

  // Let interested observers know we are being deleted.
  NotificationService::current()->Notify(
      NotificationType::WORKER_PROCESS_HOST_SHUTDOWN,
      Source<WorkerProcessHost>(this),
      NotificationService::NoDetails());

  // If we crashed, tell the RenderViewHosts.
  for (Instances::iterator i = instances_.begin(); i != instances_.end(); ++i) {
    const WorkerDocumentSet::DocumentInfoSet& parents =
        i->worker_document_set()->documents();
    for (WorkerDocumentSet::DocumentInfoSet::const_iterator parent_iter =
             parents.begin(); parent_iter != parents.end(); ++parent_iter) {
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          new WorkerCrashTask(parent_iter->renderer_id(),
                              parent_iter->render_view_route_id()));
    }
  }

  ChildProcessSecurityPolicy::GetInstance()->Remove(id());
}

bool WorkerProcessHost::Init() {
  if (!CreateChannel())
    return false;

  FilePath exe_path = GetChildPath(true);
  if (exe_path.empty())
    return false;

  CommandLine* cmd_line = new CommandLine(exe_path);
  cmd_line->AppendSwitchASCII(switches::kProcessType, switches::kWorkerProcess);
  cmd_line->AppendSwitchASCII(switches::kProcessChannelID, channel_id());
  SetCrashReporterCommandLine(cmd_line);

  static const char* const kSwitchNames[] = {
    switches::kEnableNativeWebWorkers,
    switches::kWebWorkerShareProcesses,
    switches::kDisableApplicationCache,
    switches::kDisableDatabases,
    switches::kEnableLogging,
    switches::kLoggingLevel,
    switches::kDisableWebSockets,
#if defined(OS_WIN)
    switches::kDisableDesktopNotifications,
#endif
    switches::kDisableFileSystem,
  };
  cmd_line->CopySwitchesFrom(*CommandLine::ForCurrentProcess(), kSwitchNames,
                             arraysize(kSwitchNames));

#if defined(OS_POSIX)
  bool use_zygote = true;

  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kWaitForDebuggerChildren)) {
    // Look to pass-on the kWaitForDebugger flag.
    std::string value = CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
        switches::kWaitForDebuggerChildren);
    if (value.empty() || value == switches::kWorkerProcess) {
      cmd_line->AppendSwitch(switches::kWaitForDebugger);
      use_zygote = false;
    }
  }

  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kDebugChildren)) {
    // Look to pass-on the kDebugOnStart flag.
    std::string value = CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
        switches::kDebugChildren);
    if (value.empty() || value == switches::kWorkerProcess) {
      // launches a new xterm, and runs the worker process in gdb, reading
      // optional commands from gdb_chrome file in the working directory.
      cmd_line->PrependWrapper("xterm -e gdb -x gdb_chrome --args");
      use_zygote = false;
    }
  }
#endif

  Launch(
#if defined(OS_WIN)
      FilePath(),
#elif defined(OS_POSIX)
      use_zygote,
      base::environment_vector(),
#endif
      cmd_line);

  ChildProcessSecurityPolicy::GetInstance()->Add(id());
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableFileSystem)) {
      // Grant most file permissions to this worker.
      // PLATFORM_FILE_TEMPORARY, PLATFORM_FILE_HIDDEN and
      // PLATFORM_FILE_DELETE_ON_CLOSE are not granted, because no existing API
      // requests them.
      ChildProcessSecurityPolicy::GetInstance()->GrantPermissionsForFile(
          id(),
          request_context_->browser_file_system_context()->
              path_manager()->base_path(),
          base::PLATFORM_FILE_OPEN |
          base::PLATFORM_FILE_CREATE |
          base::PLATFORM_FILE_OPEN_ALWAYS |
          base::PLATFORM_FILE_CREATE_ALWAYS |
          base::PLATFORM_FILE_READ |
          base::PLATFORM_FILE_WRITE |
          base::PLATFORM_FILE_EXCLUSIVE_READ |
          base::PLATFORM_FILE_EXCLUSIVE_WRITE |
          base::PLATFORM_FILE_ASYNC |
          base::PLATFORM_FILE_TRUNCATE |
          base::PLATFORM_FILE_WRITE_ATTRIBUTES);
  }

  return true;
}

void WorkerProcessHost::CreateWorker(const WorkerInstance& instance) {
  ChildProcessSecurityPolicy::GetInstance()->GrantRequestURL(
      id(), instance.url());

  instances_.push_back(instance);

  WorkerProcessMsg_CreateWorker_Params params;
  params.url = instance.url();
  params.is_shared = instance.shared();
  params.name = instance.name();
  params.route_id = instance.worker_route_id();
  params.creator_process_id = instance.parent_process_id();
  params.creator_appcache_host_id = instance.parent_appcache_host_id();
  params.shared_worker_appcache_id = instance.main_resource_appcache_id();
  Send(new WorkerProcessMsg_CreateWorker(params));

  UpdateTitle();

  // Walk all pending senders and let them know the worker has been created
  // (could be more than one in the case where we had to queue up worker
  // creation because the worker process limit was reached).
  for (WorkerInstance::SenderList::const_iterator i =
           instance.senders().begin();
       i != instance.senders().end(); ++i) {
    i->first->Send(new ViewMsg_WorkerCreated(i->second));
  }
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
  return request_context_;
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
  bool handled =
      appcache_dispatcher_host_->OnMessageReceived(message, &msg_is_ok) ||
      db_dispatcher_host_->OnMessageReceived(message, &msg_is_ok) ||
      blob_dispatcher_host_->OnMessageReceived(message, &msg_is_ok) ||
      file_system_dispatcher_host_->OnMessageReceived(message, &msg_is_ok) ||
      file_utilities_dispatcher_host_->OnMessageReceived(message) ||
      mime_registry_dispatcher_->OnMessageReceived(message) ||
      MessagePortDispatcher::GetInstance()->OnMessageReceived(
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

void WorkerProcessHost::OnProcessLaunched() {
  db_dispatcher_host_->Init(handle());
  file_system_dispatcher_host_->Init(handle());
  file_utilities_dispatcher_host_->Init(handle());
}

CallbackWithReturnValue<int>::Type* WorkerProcessHost::GetNextRouteIdCallback(
    IPC::Message::Sender* sender) {
  // We don't keep callbacks for senders associated with workers, so figure out
  // what kind of sender this is, and cast it to the correct class to get the
  // callback.
  for (BrowserChildProcessHost::Iterator iter(ChildProcessInfo::WORKER_PROCESS);
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
    if (sent_message_port_ids.size() != new_routing_ids.size())
      return;

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
      i->worker_document_set()->RemoveAll(sender);
      if (i->worker_document_set()->IsEmpty()) {
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

    // Check if it's an extension-created worker, in which case we want to use
    // the name of the extension.
    std::string extension_name = static_cast<ChromeURLRequestContext*>(
        Profile::GetDefaultRequestContext()->GetURLRequestContext())->
        extension_info_map()->GetNameForExtension(title);
    if (!extension_name.empty()) {
      titles.insert(extension_name);
      continue;
    }

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

void WorkerProcessHost::OnLookupSharedWorker(
    const ViewHostMsg_CreateWorker_Params& params,
    bool* exists,
    int* route_id,
    bool* url_mismatch) {
  *route_id = WorkerService::GetInstance()->next_worker_route_id();
  // TODO(atwilson): Add code to pass in the current worker's document set for
  // these nested workers. Code below will not work for SharedWorkers as it
  // only looks at a single parent.
  DCHECK(instances_.front().worker_document_set()->documents().size() == 1);
  WorkerDocumentSet::DocumentInfoSet::const_iterator first_parent =
      instances_.front().worker_document_set()->documents().begin();
  *exists = WorkerService::GetInstance()->LookupSharedWorker(
      params.url, params.name, instances_.front().off_the_record(),
      params.document_id, first_parent->renderer_id(),
      first_parent->render_view_route_id(), this, *route_id, url_mismatch);
}

void WorkerProcessHost::OnCreateWorker(
    const ViewHostMsg_CreateWorker_Params& params, int* route_id) {
  DCHECK(instances_.size() == 1);  // Only called when one process per worker.
  // TODO(atwilson): Add code to pass in the current worker's document set for
  // these nested workers. Code below will not work for SharedWorkers as it
  // only looks at a single parent.
  DCHECK(instances_.front().worker_document_set()->documents().size() == 1);
  WorkerDocumentSet::DocumentInfoSet::const_iterator first_parent =
      instances_.front().worker_document_set()->documents().begin();
  *route_id = params.route_id == MSG_ROUTING_NONE ?
      WorkerService::GetInstance()->next_worker_route_id() : params.route_id;

  if (params.is_shared)
    WorkerService::GetInstance()->CreateSharedWorker(
        params.url, instances_.front().off_the_record(),
        params.name, params.document_id, first_parent->renderer_id(),
        first_parent->render_view_route_id(), this, *route_id,
        params.script_resource_appcache_id, request_context_);
  else
    WorkerService::GetInstance()->CreateDedicatedWorker(
        params.url, instances_.front().off_the_record(),
        params.document_id, first_parent->renderer_id(),
        first_parent->render_view_route_id(), this, *route_id,
        id(), params.parent_appcache_host_id, request_context_);
}

void WorkerProcessHost::OnCancelCreateDedicatedWorker(int route_id) {
  WorkerService::GetInstance()->CancelCreateDedicatedWorker(this, route_id);
}

void WorkerProcessHost::OnForwardToWorker(const IPC::Message& message) {
  WorkerService::GetInstance()->ForwardMessage(message, this);
}

void WorkerProcessHost::DocumentDetached(IPC::Message::Sender* parent,
                                         unsigned long long document_id) {
  // Walk all instances and remove the document from their document set.
  for (Instances::iterator i = instances_.begin(); i != instances_.end();) {
    if (!i->shared()) {
      ++i;
    } else {
      i->worker_document_set()->Remove(parent, document_id);
      if (i->worker_document_set()->IsEmpty()) {
        // This worker has no more associated documents - shut it down.
        Send(new WorkerMsg_TerminateWorkerContext(i->worker_route_id()));
        i = instances_.erase(i);
      } else {
        ++i;
      }
    }
  }
}

WorkerProcessHost::WorkerInstance::WorkerInstance(
    const GURL& url,
    bool shared,
    bool off_the_record,
    const string16& name,
    int worker_route_id,
    int parent_process_id,
    int parent_appcache_host_id,
    int64 main_resource_appcache_id,
    ChromeURLRequestContext* request_context)
    : url_(url),
      shared_(shared),
      off_the_record_(off_the_record),
      closed_(false),
      name_(name),
      worker_route_id_(worker_route_id),
      parent_process_id_(parent_process_id),
      parent_appcache_host_id_(parent_appcache_host_id),
      main_resource_appcache_id_(main_resource_appcache_id),
      request_context_(request_context),
      worker_document_set_(new WorkerDocumentSet()) {
  DCHECK(!request_context ||
         (off_the_record == request_context->is_off_the_record()));
}

WorkerProcessHost::WorkerInstance::~WorkerInstance() {
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

bool WorkerProcessHost::WorkerInstance::RendererIsParent(
    int renderer_id, int render_view_route_id) const {
  const WorkerDocumentSet::DocumentInfoSet& parents =
      worker_document_set()->documents();
  for (WorkerDocumentSet::DocumentInfoSet::const_iterator parent_iter =
           parents.begin();
       parent_iter != parents.end(); ++parent_iter) {
    if (parent_iter->renderer_id() == renderer_id &&
        parent_iter->render_view_route_id() == render_view_route_id) {
      return true;
    }
  }
  return false;
}

WorkerProcessHost::WorkerInstance::SenderInfo
WorkerProcessHost::WorkerInstance::GetSender() const {
  DCHECK(NumSenders() == 1);
  return *senders_.begin();
}
