// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/worker_host/worker_process_host.h"

#include <set>
#include <vector>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "content/browser/appcache/appcache_dispatcher_host.h"
#include "content/browser/child_process_security_policy.h"
#include "content/browser/debugger/worker_devtools_message_filter.h"
#include "content/browser/file_system/file_system_dispatcher_host.h"
#include "content/browser/mime_registry_message_filter.h"
#include "content/browser/renderer_host/blob_message_filter.h"
#include "content/browser/renderer_host/database_message_filter.h"
#include "content/browser/renderer_host/file_utilities_message_filter.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/socket_stream_dispatcher_host.h"
#include "content/browser/resource_context.h"
#include "content/browser/user_metrics.h"
#include "content/browser/worker_host/message_port_service.h"
#include "content/browser/worker_host/worker_message_filter.h"
#include "content/browser/worker_host/worker_service.h"
#include "content/common/debug_flags.h"
#include "content/common/view_messages.h"
#include "content/common/worker_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/result_codes.h"
#include "ipc/ipc_switches.h"
#include "net/base/mime_util.h"
#include "net/base/registry_controlled_domain.h"
#include "ui/base/ui_base_switches.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_path_manager.h"
#include "webkit/fileapi/sandbox_mount_point_provider.h"
#include "webkit/glue/resource_type.h"

using content::BrowserThread;

namespace {

// Helper class that we pass to SocketStreamDispatcherHost so that it can find
// the right net::URLRequestContext for a request.
class URLRequestContextSelector
    : public ResourceMessageFilter::URLRequestContextSelector {
 public:
  explicit URLRequestContextSelector(
      net::URLRequestContext* url_request_context)
      : url_request_context_(url_request_context) {
  }
  virtual ~URLRequestContextSelector() {}

  virtual net::URLRequestContext* GetRequestContext(
      ResourceType::Type resource_type) {
    return url_request_context_;
  }

 private:
  net::URLRequestContext* url_request_context_;
};

}  // namespace

// Notifies RenderViewHost that one or more worker objects crashed.
void WorkerCrashCallback(int render_process_unique_id, int render_view_id) {
  RenderViewHost* host =
      RenderViewHost::FromID(render_process_unique_id, render_view_id);
  if (host)
    host->delegate()->WorkerCrashed();
}

WorkerProcessHost::WorkerProcessHost(
    const content::ResourceContext* resource_context,
    ResourceDispatcherHost* resource_dispatcher_host)
    : BrowserChildProcessHost(WORKER_PROCESS),
      resource_context_(resource_context),
      resource_dispatcher_host_(resource_dispatcher_host) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(resource_context);
}

WorkerProcessHost::~WorkerProcessHost() {
  // If we crashed, tell the RenderViewHosts.
  for (Instances::iterator i = instances_.begin(); i != instances_.end(); ++i) {
    const WorkerDocumentSet::DocumentInfoSet& parents =
        i->worker_document_set()->documents();
    for (WorkerDocumentSet::DocumentInfoSet::const_iterator parent_iter =
             parents.begin(); parent_iter != parents.end(); ++parent_iter) {
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::Bind(&WorkerCrashCallback, parent_iter->render_process_id(),
                     parent_iter->render_view_id()));
    }
    WorkerService::GetInstance()->NotifyWorkerDestroyed(this,
                                                        i->worker_route_id());
  }

  ChildProcessSecurityPolicy::GetInstance()->Remove(id());
}

bool WorkerProcessHost::Init(int render_process_id) {
  if (!CreateChannel())
    return false;

#if defined(OS_LINUX)
  int flags = CHILD_ALLOW_SELF;
#else
  int flags = CHILD_NORMAL;
#endif

  FilePath exe_path = GetChildPath(flags);
  if (exe_path.empty())
    return false;

  CommandLine* cmd_line = new CommandLine(exe_path);
  cmd_line->AppendSwitchASCII(switches::kProcessType, switches::kWorkerProcess);
  cmd_line->AppendSwitchASCII(switches::kProcessChannelID, channel_id());
  std::string locale =
      content::GetContentClient()->browser()->GetApplicationLocale();
  cmd_line->AppendSwitchASCII(switches::kLang, locale);

  static const char* const kSwitchNames[] = {
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

  ChildProcessSecurityPolicy::GetInstance()->AddWorker(
      id(), render_process_id);
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableFileSystem)) {
    // Grant most file permissions to this worker.
    // PLATFORM_FILE_TEMPORARY, PLATFORM_FILE_HIDDEN and
    // PLATFORM_FILE_DELETE_ON_CLOSE are not granted, because no existing API
    // requests them.
    // This is for the filesystem sandbox.
    ChildProcessSecurityPolicy::GetInstance()->GrantPermissionsForFile(
        id(), resource_context_->file_system_context()->
          path_manager()->sandbox_provider()->new_base_path(),
        base::PLATFORM_FILE_OPEN |
        base::PLATFORM_FILE_CREATE |
        base::PLATFORM_FILE_OPEN_ALWAYS |
        base::PLATFORM_FILE_CREATE_ALWAYS |
        base::PLATFORM_FILE_OPEN_TRUNCATED |
        base::PLATFORM_FILE_READ |
        base::PLATFORM_FILE_WRITE |
        base::PLATFORM_FILE_EXCLUSIVE_READ |
        base::PLATFORM_FILE_EXCLUSIVE_WRITE |
        base::PLATFORM_FILE_ASYNC |
        base::PLATFORM_FILE_WRITE_ATTRIBUTES |
        base::PLATFORM_FILE_ENUMERATE);
    // This is so that we can read and move stuff out of the old filesystem
    // sandbox.
    ChildProcessSecurityPolicy::GetInstance()->GrantPermissionsForFile(
        id(), resource_context_->file_system_context()->
          path_manager()->sandbox_provider()->old_base_path(),
        base::PLATFORM_FILE_READ | base::PLATFORM_FILE_WRITE |
            base::PLATFORM_FILE_WRITE_ATTRIBUTES |
            base::PLATFORM_FILE_ENUMERATE);
    // This is so that we can rename the old sandbox out of the way so that
    // we know we've taken care of it.
    ChildProcessSecurityPolicy::GetInstance()->GrantPermissionsForFile(
        id(), resource_context_->file_system_context()->
          path_manager()->sandbox_provider()->renamed_old_base_path(),
        base::PLATFORM_FILE_CREATE | base::PLATFORM_FILE_CREATE_ALWAYS |
            base::PLATFORM_FILE_WRITE);
  }

  CreateMessageFilters(render_process_id);

  return true;
}

void WorkerProcessHost::CreateMessageFilters(int render_process_id) {
  DCHECK(resource_context_);
  net::URLRequestContext* request_context =
      resource_context_->request_context();

  ResourceMessageFilter* resource_message_filter = new ResourceMessageFilter(
      id(), WORKER_PROCESS, resource_context_,
      new URLRequestContextSelector(request_context),
      resource_dispatcher_host_);
  AddFilter(resource_message_filter);

  worker_message_filter_ = new WorkerMessageFilter(
      render_process_id, resource_context_, resource_dispatcher_host_,
      base::Bind(&WorkerService::next_worker_route_id,
                 base::Unretained(WorkerService::GetInstance())));
  AddFilter(worker_message_filter_);
  AddFilter(new AppCacheDispatcherHost(
      resource_context_->appcache_service(), id()));
  AddFilter(new FileSystemDispatcherHost(
      request_context, resource_context_->file_system_context()));
  AddFilter(new FileUtilitiesMessageFilter(id()));
  AddFilter(new BlobMessageFilter(
      id(), resource_context_->blob_storage_context()));
  AddFilter(new MimeRegistryMessageFilter());
  AddFilter(new DatabaseMessageFilter(
      resource_context_->database_tracker()));

  SocketStreamDispatcherHost* socket_stream_dispatcher_host =
      new SocketStreamDispatcherHost(
          new URLRequestContextSelector(request_context), resource_context_);
  AddFilter(socket_stream_dispatcher_host);
  AddFilter(new WorkerDevToolsMessageFilter(id()));
}

void WorkerProcessHost::CreateWorker(const WorkerInstance& instance) {
  ChildProcessSecurityPolicy::GetInstance()->GrantRequestURL(
      id(), instance.url());

  instances_.push_back(instance);

  WorkerProcessMsg_CreateWorker_Params params;
  params.url = instance.url();
  params.name = instance.name();
  params.route_id = instance.worker_route_id();
  params.creator_process_id = instance.parent_process_id();
  params.shared_worker_appcache_id = instance.main_resource_appcache_id();
  Send(new WorkerProcessMsg_CreateWorker(params));

  UpdateTitle();

  // Walk all pending filters and let them know the worker has been created
  // (could be more than one in the case where we had to queue up worker
  // creation because the worker process limit was reached).
  for (WorkerInstance::FilterList::const_iterator i =
           instance.filters().begin();
       i != instance.filters().end(); ++i) {
    i->first->Send(new ViewMsg_WorkerCreated(i->second));
  }
}

bool WorkerProcessHost::FilterMessage(const IPC::Message& message,
                                      WorkerMessageFilter* filter) {
  for (Instances::iterator i = instances_.begin(); i != instances_.end(); ++i) {
    if (!i->closed() && i->HasFilter(filter, message.routing_id())) {
      RelayMessage(message, worker_message_filter_, i->worker_route_id());
      return true;
    }
  }

  return false;
}

void WorkerProcessHost::OnProcessLaunched() {
}

bool WorkerProcessHost::OnMessageReceived(const IPC::Message& message) {
  bool msg_is_ok = true;
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(WorkerProcessHost, message, msg_is_ok)
    IPC_MESSAGE_HANDLER(WorkerHostMsg_WorkerContextClosed,
                        OnWorkerContextClosed)
    IPC_MESSAGE_HANDLER(WorkerProcessHostMsg_AllowDatabase, OnAllowDatabase)
    IPC_MESSAGE_HANDLER(WorkerProcessHostMsg_AllowFileSystem, OnAllowFileSystem)
    IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP_EX()

  if (!msg_is_ok) {
    NOTREACHED();
    UserMetrics::RecordAction(UserMetricsAction("BadMessageTerminate_WPH"));
    base::KillProcess(handle(), content::RESULT_CODE_KILLED_BAD_MESSAGE, false);
  }

  if (handled)
    return true;

  if (message.type() == WorkerHostMsg_WorkerContextDestroyed::ID) {
    WorkerService::GetInstance()->NotifyWorkerDestroyed(this,
                                                        message.routing_id());
  }

  for (Instances::iterator i = instances_.begin(); i != instances_.end(); ++i) {
    if (i->worker_route_id() == message.routing_id()) {
      if (message.type() == WorkerHostMsg_WorkerContextDestroyed::ID) {
        instances_.erase(i);
        UpdateTitle();
      }
      return true;
    }
  }
  return false;
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

void WorkerProcessHost::OnAllowDatabase(int worker_route_id,
                                        const GURL& url,
                                        const string16& name,
                                        const string16& display_name,
                                        unsigned long estimated_size,
                                        bool* result) {
  *result = content::GetContentClient()->browser()->AllowWorkerDatabase(
      worker_route_id, url, name, display_name, estimated_size, this);
}

void WorkerProcessHost::OnAllowFileSystem(int worker_route_id,
                                          const GURL& url,
                                          bool* result) {
  *result = content::GetContentClient()->browser()->AllowWorkerFileSystem(
      worker_route_id, url, this);
}

void WorkerProcessHost::RelayMessage(
    const IPC::Message& message,
    WorkerMessageFilter* filter,
    int route_id) {
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
      new_routing_ids[i] = filter->GetNextRoutingID();
      MessagePortService::GetInstance()->UpdateMessagePort(
          sent_message_port_ids[i], filter, new_routing_ids[i]);
    }

    filter->Send(new WorkerMsg_PostMessage(
        route_id, msg, sent_message_port_ids, new_routing_ids));

    // Send any queued messages to the sent message ports.  We can only do this
    // after sending the above message, since it's the one that sets up the
    // message port route which the queued messages are sent to.
    for (size_t i = 0; i < sent_message_port_ids.size(); ++i) {
      MessagePortService::GetInstance()->
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
    new_routing_id = filter->GetNextRoutingID();
    MessagePortService::GetInstance()->UpdateMessagePort(
        sent_message_port_id, filter, new_routing_id);

    // Resend the message with the new routing id.
    filter->Send(new WorkerMsg_Connect(
        route_id, sent_message_port_id, new_routing_id));

    // Send any queued messages for the sent port.
    MessagePortService::GetInstance()->SendQueuedMessagesIfPossible(
        sent_message_port_id);
  } else {
    IPC::Message* new_message = new IPC::Message(message);
    new_message->set_routing_id(route_id);
    filter->Send(new_message);
    if (message.type() == WorkerMsg_StartWorkerContext::ID)
      WorkerService::GetInstance()->NotifyWorkerContextStarted(this, route_id);
    return;
  }
}

void WorkerProcessHost::FilterShutdown(WorkerMessageFilter* filter) {
  for (Instances::iterator i = instances_.begin(); i != instances_.end();) {
    bool shutdown = false;
    i->RemoveFilters(filter);

    i->worker_document_set()->RemoveAll(filter);
    if (i->worker_document_set()->IsEmpty()) {
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

bool WorkerProcessHost::CanShutdown() {
  return instances_.empty();
}

void WorkerProcessHost::UpdateTitle() {
  std::set<std::string> titles;
  for (Instances::iterator i = instances_.begin(); i != instances_.end(); ++i) {
    // Allow the embedder first crack at special casing the title.
    std::string title = content::GetContentClient()->browser()->
        GetWorkerProcessTitle(i->url(), *resource_context_);

    if (title.empty()) {
      title = net::RegistryControlledDomainService::GetDomainAndRegistry(
          i->url());
    }

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

  set_name(ASCIIToUTF16(display_title));
}

void WorkerProcessHost::DocumentDetached(WorkerMessageFilter* filter,
                                         unsigned long long document_id) {
  // Walk all instances and remove the document from their document set.
  for (Instances::iterator i = instances_.begin(); i != instances_.end();) {
    i->worker_document_set()->Remove(filter, document_id);
    if (i->worker_document_set()->IsEmpty()) {
      // This worker has no more associated documents - shut it down.
      Send(new WorkerMsg_TerminateWorkerContext(i->worker_route_id()));
      i = instances_.erase(i);
    } else {
      ++i;
    }
  }
}

void WorkerProcessHost::TerminateWorker(int worker_route_id) {
  Send(new WorkerMsg_TerminateWorkerContext(worker_route_id));
}

WorkerProcessHost::WorkerInstance::WorkerInstance(
    const GURL& url,
    const string16& name,
    int worker_route_id,
    int parent_process_id,
    int64 main_resource_appcache_id,
    const content::ResourceContext* resource_context)
    : url_(url),
      closed_(false),
      name_(name),
      worker_route_id_(worker_route_id),
      parent_process_id_(parent_process_id),
      main_resource_appcache_id_(main_resource_appcache_id),
      worker_document_set_(new WorkerDocumentSet()),
      resource_context_(resource_context) {
  DCHECK(resource_context_);
}

WorkerProcessHost::WorkerInstance::WorkerInstance(
    const GURL& url,
    bool shared,
    const string16& name,
    const content::ResourceContext* resource_context)
    : url_(url),
      closed_(false),
      name_(name),
      worker_route_id_(MSG_ROUTING_NONE),
      parent_process_id_(0),
      main_resource_appcache_id_(0),
      worker_document_set_(new WorkerDocumentSet()),
      resource_context_(resource_context) {
  DCHECK(resource_context_);
}

WorkerProcessHost::WorkerInstance::~WorkerInstance() {
}

// Compares an instance based on the algorithm in the WebWorkers spec - an
// instance matches if the origins of the URLs match, and:
// a) the names are non-empty and equal
// -or-
// b) the names are both empty, and the urls are equal
bool WorkerProcessHost::WorkerInstance::Matches(
    const GURL& match_url,
    const string16& match_name,
    const content::ResourceContext* resource_context) const {
  // Only match open shared workers.
  if (closed_)
    return false;

  // Have to match the same ResourceContext.
  if (resource_context_ != resource_context)
    return false;

  if (url_.GetOrigin() != match_url.GetOrigin())
    return false;

  if (name_.empty() && match_name.empty())
    return url_ == match_url;

  return name_ == match_name;
}

void WorkerProcessHost::WorkerInstance::AddFilter(WorkerMessageFilter* filter,
                                                  int route_id) {
  if (!HasFilter(filter, route_id)) {
    FilterInfo info(filter, route_id);
    filters_.push_back(info);
  }
}

void WorkerProcessHost::WorkerInstance::RemoveFilter(
    WorkerMessageFilter* filter, int route_id) {
  for (FilterList::iterator i = filters_.begin(); i != filters_.end();) {
    if (i->first == filter && i->second == route_id)
      i = filters_.erase(i);
    else
      ++i;
  }
  // Should not be duplicate copies in the filter set.
  DCHECK(!HasFilter(filter, route_id));
}

void WorkerProcessHost::WorkerInstance::RemoveFilters(
    WorkerMessageFilter* filter) {
  for (FilterList::iterator i = filters_.begin(); i != filters_.end();) {
    if (i->first == filter)
      i = filters_.erase(i);
    else
      ++i;
  }
}

bool WorkerProcessHost::WorkerInstance::HasFilter(
    WorkerMessageFilter* filter, int route_id) const {
  for (FilterList::const_iterator i = filters_.begin(); i != filters_.end();
       ++i) {
    if (i->first == filter && i->second == route_id)
      return true;
  }
  return false;
}

bool WorkerProcessHost::WorkerInstance::RendererIsParent(
    int render_process_id, int render_view_id) const {
  const WorkerDocumentSet::DocumentInfoSet& parents =
      worker_document_set()->documents();
  for (WorkerDocumentSet::DocumentInfoSet::const_iterator parent_iter =
           parents.begin();
       parent_iter != parents.end(); ++parent_iter) {
    if (parent_iter->render_process_id() == render_process_id &&
        parent_iter->render_view_id() == render_view_id) {
      return true;
    }
  }
  return false;
}

WorkerProcessHost::WorkerInstance::FilterInfo
WorkerProcessHost::WorkerInstance::GetFilter() const {
  DCHECK(NumFilters() == 1);
  return *filters_.begin();
}
