// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/worker_host/worker_process_host.h"

#include <set>
#include <string>
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
#include "content/browser/appcache/chrome_appcache_service.h"
#include "content/browser/browser_child_process_host_impl.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/devtools/worker_devtools_manager.h"
#include "content/browser/devtools/worker_devtools_message_filter.h"
#include "content/browser/fileapi/fileapi_message_filter.h"
#include "content/browser/in_process_webkit/indexed_db_dispatcher_host.h"
#include "content/browser/mime_registry_message_filter.h"
#include "content/browser/renderer_host/database_message_filter.h"
#include "content/browser/renderer_host/file_utilities_message_filter.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/socket_stream_dispatcher_host.h"
#include "content/browser/resource_context_impl.h"
#include "content/browser/worker_host/message_port_service.h"
#include "content/browser/worker_host/worker_message_filter.h"
#include "content/browser/worker_host/worker_service_impl.h"
#include "content/common/child_process_host_impl.h"
#include "content/common/debug_flags.h"
#include "content/common/view_messages.h"
#include "content/common/worker_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/result_codes.h"
#include "ipc/ipc_switches.h"
#include "net/base/mime_util.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/url_request/url_request_context_getter.h"
#include "ui/base/ui_base_switches.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/sandbox_mount_point_provider.h"
#include "webkit/glue/resource_type.h"

namespace content {
namespace {

// Helper class that we pass to SocketStreamDispatcherHost so that it can find
// the right net::URLRequestContext for a request.
class URLRequestContextSelector
    : public ResourceMessageFilter::URLRequestContextSelector {
 public:
  explicit URLRequestContextSelector(
      net::URLRequestContextGetter* url_request_context,
      net::URLRequestContextGetter* media_url_request_context)
      : url_request_context_(url_request_context),
        media_url_request_context_(media_url_request_context) {
  }
  virtual ~URLRequestContextSelector() {}

  virtual net::URLRequestContext* GetRequestContext(
      ResourceType::Type resource_type) {
    if (resource_type == ResourceType::MEDIA)
      return media_url_request_context_->GetURLRequestContext();
    return url_request_context_->GetURLRequestContext();
  }

 private:
  net::URLRequestContextGetter* url_request_context_;
  net::URLRequestContextGetter* media_url_request_context_;
};

}  // namespace

// Notifies RenderViewHost that one or more worker objects crashed.
void WorkerCrashCallback(int render_process_unique_id, int render_view_id) {
  RenderViewHostImpl* host =
      RenderViewHostImpl::FromID(render_process_unique_id, render_view_id);
  if (host)
    host->GetDelegate()->WorkerCrashed();
}

WorkerProcessHost::WorkerProcessHost(
    ResourceContext* resource_context,
    const WorkerStoragePartition& partition)
    : resource_context_(resource_context),
      partition_(partition) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(resource_context_);
  process_.reset(
      new BrowserChildProcessHostImpl(PROCESS_TYPE_WORKER, this));
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
    WorkerServiceImpl::GetInstance()->NotifyWorkerDestroyed(
        this, i->worker_route_id());
  }

  ChildProcessSecurityPolicyImpl::GetInstance()->Remove(
      process_->GetData().id);
}

bool WorkerProcessHost::Send(IPC::Message* message) {
  return process_->Send(message);
}

bool WorkerProcessHost::Init(int render_process_id) {
  std::string channel_id = process_->GetHost()->CreateChannel();
  if (channel_id.empty())
    return false;

#if defined(OS_LINUX)
  int flags = ChildProcessHost::CHILD_ALLOW_SELF;
#else
  int flags = ChildProcessHost::CHILD_NORMAL;
#endif

  FilePath exe_path = ChildProcessHost::GetChildPath(flags);
  if (exe_path.empty())
    return false;

  CommandLine* cmd_line = new CommandLine(exe_path);
  cmd_line->AppendSwitchASCII(switches::kProcessType, switches::kWorkerProcess);
  cmd_line->AppendSwitchASCII(switches::kProcessChannelID, channel_id);
  std::string locale = GetContentClient()->browser()->GetApplicationLocale();
  cmd_line->AppendSwitchASCII(switches::kLang, locale);

  static const char* const kSwitchNames[] = {
    switches::kDisableApplicationCache,
    switches::kDisableDatabases,
#if defined(OS_WIN)
    switches::kDisableDesktopNotifications,
#endif
    switches::kDisableFileSystem,
    switches::kDisableSeccompFilterSandbox,
    switches::kDisableWebSockets,
#if defined(OS_MACOSX)
    switches::kEnableSandboxLogging,
#endif
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

  process_->Launch(
#if defined(OS_WIN)
      FilePath(),
#elif defined(OS_POSIX)
      use_zygote,
      base::EnvironmentVector(),
#endif
      cmd_line);

  ChildProcessSecurityPolicyImpl::GetInstance()->AddWorker(
      process_->GetData().id, render_process_id);
  CreateMessageFilters(render_process_id);

  return true;
}

void WorkerProcessHost::CreateMessageFilters(int render_process_id) {
  ChromeBlobStorageContext* blob_storage_context =
      GetChromeBlobStorageContextForResourceContext(resource_context_);

  net::URLRequestContextGetter* url_request_context =
      partition_.url_request_context();
  net::URLRequestContextGetter* media_url_request_context =
      partition_.url_request_context();

  ResourceMessageFilter* resource_message_filter = new ResourceMessageFilter(
      process_->GetData().id, PROCESS_TYPE_WORKER, resource_context_,
      partition_.appcache_service(),
      blob_storage_context,
      partition_.filesystem_context(),
      new URLRequestContextSelector(url_request_context,
                                    media_url_request_context));
  process_->GetHost()->AddFilter(resource_message_filter);

  worker_message_filter_ = new WorkerMessageFilter(
      render_process_id, resource_context_, partition_,
      base::Bind(&WorkerServiceImpl::next_worker_route_id,
                 base::Unretained(WorkerServiceImpl::GetInstance())));
  process_->GetHost()->AddFilter(worker_message_filter_);
  process_->GetHost()->AddFilter(new AppCacheDispatcherHost(
      partition_.appcache_service(),
      process_->GetData().id));
  process_->GetHost()->AddFilter(new FileAPIMessageFilter(
      process_->GetData().id,
      url_request_context,
      partition_.filesystem_context(),
      blob_storage_context));
  process_->GetHost()->AddFilter(new FileUtilitiesMessageFilter(
      process_->GetData().id));
  process_->GetHost()->AddFilter(new MimeRegistryMessageFilter());
  process_->GetHost()->AddFilter(
      new DatabaseMessageFilter(partition_.database_tracker()));

  SocketStreamDispatcherHost* socket_stream_dispatcher_host =
      new SocketStreamDispatcherHost(
          render_process_id,
          new URLRequestContextSelector(url_request_context,
                                        media_url_request_context),
          resource_context_);
  process_->GetHost()->AddFilter(socket_stream_dispatcher_host);
  process_->GetHost()->AddFilter(
      new WorkerDevToolsMessageFilter(process_->GetData().id));
  process_->GetHost()->AddFilter(new IndexedDBDispatcherHost(
      process_->GetData().id, partition_.indexed_db_context()));
}

void WorkerProcessHost::CreateWorker(const WorkerInstance& instance) {
  ChildProcessSecurityPolicyImpl::GetInstance()->GrantRequestURL(
      process_->GetData().id, instance.url());

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
    CHECK(i->first);
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
    IPC_MESSAGE_HANDLER(WorkerProcessHostMsg_AllowIndexedDB, OnAllowIndexedDB)
    IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP_EX()

  if (!msg_is_ok) {
    NOTREACHED();
    RecordAction(UserMetricsAction("BadMessageTerminate_WPH"));
    base::KillProcess(
        process_->GetData().handle, RESULT_CODE_KILLED_BAD_MESSAGE, false);
  }

  if (handled)
    return true;

  if (message.type() == WorkerHostMsg_WorkerContextDestroyed::ID) {
    WorkerServiceImpl::GetInstance()->NotifyWorkerDestroyed(
        this, message.routing_id());
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
  *result = GetContentClient()->browser()->AllowWorkerDatabase(
      url, name, display_name, estimated_size, resource_context_,
      GetRenderViewIDsForWorker(worker_route_id));
}

void WorkerProcessHost::OnAllowFileSystem(int worker_route_id,
                                          const GURL& url,
                                          bool* result) {
  *result = GetContentClient()->browser()->AllowWorkerFileSystem(
      url, resource_context_, GetRenderViewIDsForWorker(worker_route_id));
}

void WorkerProcessHost::OnAllowIndexedDB(int worker_route_id,
                                         const GURL& url,
                                         const string16& name,
                                         bool* result) {
  *result = GetContentClient()->browser()->AllowWorkerIndexedDB(
      url, name, resource_context_, GetRenderViewIDsForWorker(worker_route_id));
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
    if (message.type() == WorkerMsg_StartWorkerContext::ID) {
      WorkerDevToolsManager::GetInstance()->WorkerContextStarted(
          this, route_id);
    }
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
    std::string title = GetContentClient()->browser()->
        GetWorkerProcessTitle(i->url(), resource_context_);

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

  process_->SetName(ASCIIToUTF16(display_title));
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

const ChildProcessData& WorkerProcessHost::GetData() {
  return process_->GetData();
}

std::vector<std::pair<int, int> > WorkerProcessHost::GetRenderViewIDsForWorker(
    int worker_route_id) {
  std::vector<std::pair<int, int> > result;
  WorkerProcessHost::Instances::const_iterator i;
  for (i = instances_.begin(); i != instances_.end(); ++i) {
    if (i->worker_route_id() != worker_route_id)
      continue;
    const WorkerDocumentSet::DocumentInfoSet& documents =
        i->worker_document_set()->documents();
    for (WorkerDocumentSet::DocumentInfoSet::const_iterator doc =
         documents.begin(); doc != documents.end(); ++doc) {
      result.push_back(
          std::make_pair(doc->render_process_id(), doc->render_view_id()));
    }
    break;
  }
  return result;
}

WorkerProcessHost::WorkerInstance::WorkerInstance(
    const GURL& url,
    const string16& name,
    int worker_route_id,
    int parent_process_id,
    int64 main_resource_appcache_id,
    ResourceContext* resource_context,
    const WorkerStoragePartition& partition)
    : url_(url),
      closed_(false),
      name_(name),
      worker_route_id_(worker_route_id),
      parent_process_id_(parent_process_id),
      main_resource_appcache_id_(main_resource_appcache_id),
      worker_document_set_(new WorkerDocumentSet()),
      resource_context_(resource_context),
      partition_(partition) {
  DCHECK(resource_context_);
}

WorkerProcessHost::WorkerInstance::WorkerInstance(
    const GURL& url,
    bool shared,
    const string16& name,
    ResourceContext* resource_context,
    const WorkerStoragePartition& partition)
    : url_(url),
      closed_(false),
      name_(name),
      worker_route_id_(MSG_ROUTING_NONE),
      parent_process_id_(0),
      main_resource_appcache_id_(0),
      worker_document_set_(new WorkerDocumentSet()),
      resource_context_(resource_context),
      partition_(partition) {
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
    const WorkerStoragePartition& partition,
    ResourceContext* resource_context) const {
  // Only match open shared workers.
  if (closed_)
    return false;

  // ResourceContext equivalence is being used as a proxy to ensure we only
  // matched shared workers within the same BrowserContext.
  if (resource_context_ != resource_context)
    return false;

  // We must be in the same storage partition otherwise sharing will violate
  // isolation.
  if (!partition_.Equals(partition))
    return false;

  if (url_.GetOrigin() != match_url.GetOrigin())
    return false;

  if (name_.empty() && match_name.empty())
    return url_ == match_url;

  return name_ == match_name;
}

void WorkerProcessHost::WorkerInstance::AddFilter(WorkerMessageFilter* filter,
                                                  int route_id) {
  CHECK(filter);
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

}  // namespace content
