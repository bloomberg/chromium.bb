// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/worker_host/worker_process_host.h"

#include <set>
#include <vector>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/render_messages_params.h"
#include "chrome/common/result_codes.h"
#include "chrome/common/worker_messages.h"
#include "content/browser/appcache/appcache_dispatcher_host.h"
#include "content/browser/browser_thread.h"
#include "content/browser/child_process_security_policy.h"
#include "content/browser/file_system/file_system_dispatcher_host.h"
#include "content/browser/mime_registry_message_filter.h"
#include "content/browser/renderer_host/blob_message_filter.h"
#include "content/browser/renderer_host/database_message_filter.h"
#include "content/browser/renderer_host/file_utilities_message_filter.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/render_view_host_notification_task.h"
#include "content/browser/renderer_host/socket_stream_dispatcher_host.h"
#include "content/browser/worker_host/message_port_service.h"
#include "content/browser/worker_host/worker_message_filter.h"
#include "content/browser/worker_host/worker_service.h"
#include "content/common/debug_flags.h"
#include "net/base/mime_util.h"
#include "ipc/ipc_switches.h"
#include "net/base/registry_controlled_domain.h"
#include "webkit/glue/resource_type.h"
#include "webkit/fileapi/file_system_path_manager.h"

namespace {

// Helper class that we pass to SocketStreamDispatcherHost so that it can find
// the right net::URLRequestContext for a request.
class URLRequestContextOverride
    : public ResourceMessageFilter::URLRequestContextOverride {
 public:
  explicit URLRequestContextOverride(
      net::URLRequestContext* url_request_context)
      : url_request_context_(url_request_context) {
  }
  virtual ~URLRequestContextOverride() {}

  virtual net::URLRequestContext* GetRequestContext(
      ResourceType::Type resource_type) {
    return url_request_context_;
  }

 private:
  net::URLRequestContext* url_request_context_;
};

}  // namespace

// Notifies RenderViewHost that one or more worker objects crashed.
class WorkerCrashTask : public Task {
 public:
  WorkerCrashTask(int render_process_unique_id, int render_view_id)
      : render_process_unique_id_(render_process_unique_id),
        render_view_id_(render_view_id) { }

  void Run() {
    RenderViewHost* host =
        RenderViewHost::FromID(render_process_unique_id_, render_view_id_);
    if (host)
      host->delegate()->WorkerCrashed();
  }

 private:
  int render_process_unique_id_;
  int render_view_id_;
};

WorkerProcessHost::WorkerProcessHost(
    ResourceDispatcherHost* resource_dispatcher_host,
    URLRequestContextGetter* request_context)
    : BrowserChildProcessHost(WORKER_PROCESS, resource_dispatcher_host),
      request_context_(request_context) {
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
          new WorkerCrashTask(parent_iter->render_process_id(),
                              parent_iter->render_view_id()));
    }
  }

  ChildProcessSecurityPolicy::GetInstance()->Remove(id());
}

bool WorkerProcessHost::Init(int render_process_id) {
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
          GetChromeURLRequestContext()->file_system_context()->
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

  CreateMessageFilters(render_process_id);

  return true;
}

void WorkerProcessHost::CreateMessageFilters(int render_process_id) {
  ChromeURLRequestContext* chrome_url_context = GetChromeURLRequestContext();

  worker_message_filter_= new WorkerMessageFilter(
      render_process_id,
      request_context_,
      resource_dispatcher_host(),
      NewCallbackWithReturnValue(
          WorkerService::GetInstance(), &WorkerService::next_worker_route_id));
  AddFilter(worker_message_filter_);
  AddFilter(new AppCacheDispatcherHost(chrome_url_context, id()));
  AddFilter(new FileSystemDispatcherHost(chrome_url_context));
  AddFilter(new FileUtilitiesMessageFilter(id()));
  AddFilter(
      new BlobMessageFilter(id(), chrome_url_context->blob_storage_context()));
  AddFilter(new MimeRegistryMessageFilter());
  AddFilter(new DatabaseMessageFilter(
      chrome_url_context->database_tracker(),
      chrome_url_context->host_content_settings_map()));

  SocketStreamDispatcherHost* socket_stream_dispatcher_host =
      new SocketStreamDispatcherHost();
  socket_stream_dispatcher_host->set_url_request_context_override(
      new URLRequestContextOverride(chrome_url_context));
  AddFilter(socket_stream_dispatcher_host);
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
    IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP_EX()

  if (!msg_is_ok) {
    NOTREACHED();
    UserMetrics::RecordAction(UserMetricsAction("BadMessageTerminate_WPH"));
    base::KillProcess(handle(), ResultCodes::KILLED_BAD_MESSAGE, false);
  }

  if (handled)
    return true;

  for (Instances::iterator i = instances_.begin(); i != instances_.end(); ++i) {
    if (i->worker_route_id() == message.routing_id()) {
      if (!i->shared()) {
        // Don't relay messages from shared workers (all communication is via
        // the message port).
        WorkerInstance::FilterInfo info = i->GetFilter();
        RelayMessage(message, info.first, info.second);
      }

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
  ContentSetting content_setting = GetChromeURLRequestContext()->
      host_content_settings_map()->GetContentSetting(
          url, CONTENT_SETTINGS_TYPE_COOKIES, "");

  *result = content_setting != CONTENT_SETTING_BLOCK;

  // Find the worker instance and forward the message to all attached documents.
  for (Instances::iterator i = instances_.begin(); i != instances_.end(); ++i) {
    if (i->worker_route_id() != worker_route_id)
      continue;
    const WorkerDocumentSet::DocumentInfoSet& documents =
        i->worker_document_set()->documents();
    for (WorkerDocumentSet::DocumentInfoSet::const_iterator doc =
         documents.begin(); doc != documents.end(); ++doc) {
      CallRenderViewHostContentSettingsDelegate(
          doc->render_process_id(), doc->render_view_id(),
          &RenderViewHostDelegate::ContentSettings::OnWebDatabaseAccessed,
          url, name, display_name, estimated_size, !*result);
    }
    break;
  }
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
    return;
  }
}

void WorkerProcessHost::FilterShutdown(WorkerMessageFilter* filter) {
  for (Instances::iterator i = instances_.begin(); i != instances_.end();) {
    bool shutdown = false;
    i->RemoveFilters(filter);
    if (i->shared()) {
      i->worker_document_set()->RemoveAll(filter);
      if (i->worker_document_set()->IsEmpty()) {
        shutdown = true;
      }
    } else if (i->NumFilters() == 0) {
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
    std::string title =
        net::RegistryControlledDomainService::GetDomainAndRegistry(i->url());
    // Use the host name if the domain is empty, i.e. localhost or IP address.
    if (title.empty())
      title = i->url().host();

    // Check if it's an extension-created worker, in which case we want to use
    // the name of the extension.
    std::string extension_name = GetChromeURLRequestContext()->
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

ChromeURLRequestContext* WorkerProcessHost::GetChromeURLRequestContext() {
  return static_cast<ChromeURLRequestContext*>(
      request_context_->GetURLRequestContext());
}

void WorkerProcessHost::DocumentDetached(WorkerMessageFilter* filter,
                                         unsigned long long document_id) {
  // Walk all instances and remove the document from their document set.
  for (Instances::iterator i = instances_.begin(); i != instances_.end();) {
    if (!i->shared()) {
      ++i;
    } else {
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
    URLRequestContextGetter* request_context)
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

void WorkerProcessHost::WorkerInstance::AddFilter(WorkerMessageFilter* filter,
                                                  int route_id) {
  if (!HasFilter(filter, route_id)) {
    FilterInfo info(filter, route_id);
    filters_.push_back(info);
  }
  // Only shared workers can have more than one associated filter.
  DCHECK(shared_ || filters_.size() == 1);
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
