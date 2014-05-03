// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/log_private/log_private_api.h"

#include <string>
#include <vector>

#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/log_private/filter_handler.h"
#include "chrome/browser/extensions/api/log_private/log_parser.h"
#include "chrome/browser/extensions/api/log_private/syslog_parser.h"
#include "chrome/browser/feedback/system_logs/scrubbed_system_logs_fetcher.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/net/chrome_net_log.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/log_private.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_registry.h"

using content::BrowserThread;

namespace events {
const char kOnAddNetInternalsEntries[] = "logPrivate.onAddNetInternalsEntries";
}  // namespace events

namespace extensions {
namespace {

const int kNetLogEventDelayMilliseconds = 100;

scoped_ptr<LogParser> CreateLogParser(const std::string& log_type) {
  if (log_type == "syslog")
    return scoped_ptr<LogParser>(new SyslogParser());
  // TODO(shinfan): Add more parser here

  NOTREACHED() << "Invalid log type: " << log_type;
  return  scoped_ptr<LogParser>();
}

void CollectLogInfo(
    FilterHandler* filter_handler,
    system_logs::SystemLogsResponse* logs,
    std::vector<linked_ptr<api::log_private::LogEntry> >* output) {
  for (system_logs::SystemLogsResponse::const_iterator
      request_it = logs->begin(); request_it != logs->end(); ++request_it) {
    if (!filter_handler->IsValidSource(request_it->first)) {
      continue;
    }
    scoped_ptr<LogParser> parser(CreateLogParser(request_it->first));
    if (parser) {
      parser->Parse(request_it->second, output, filter_handler);
    }
  }
}

}  // namespace

// static
LogPrivateAPI* LogPrivateAPI::Get(content::BrowserContext* context) {
  return GetFactoryInstance()->Get(context);
}

LogPrivateAPI::LogPrivateAPI(content::BrowserContext* context)
    : browser_context_(context),
      logging_net_internals_(false),
      extension_registry_observer_(this) {
  extension_registry_observer_.Add(ExtensionRegistry::Get(browser_context_));
}

LogPrivateAPI::~LogPrivateAPI() {
}

void LogPrivateAPI::StartNetInternalsWatch(const std::string& extension_id) {
  net_internal_watches_.insert(extension_id);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&LogPrivateAPI::MaybeStartNetInternalLogging,
                 base::Unretained(this)));
}

void LogPrivateAPI::StopNetInternalsWatch(const std::string& extension_id) {
  net_internal_watches_.erase(extension_id);
  MaybeStopNetInternalLogging();
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<LogPrivateAPI> >
    g_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<LogPrivateAPI>*
LogPrivateAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

void LogPrivateAPI::OnAddEntry(const net::NetLog::Entry& entry) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!pending_entries_.get()) {
    pending_entries_.reset(new base::ListValue());
    BrowserThread::PostDelayedTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&LogPrivateAPI::PostPendingEntries, base::Unretained(this)),
        base::TimeDelta::FromMilliseconds(kNetLogEventDelayMilliseconds));
  }
  pending_entries_->Append(entry.ToValue());
}

void LogPrivateAPI::PostPendingEntries() {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&LogPrivateAPI:: AddEntriesOnUI,
                 base::Unretained(this),
                 base::Passed(&pending_entries_)));
}

void LogPrivateAPI::AddEntriesOnUI(scoped_ptr<base::ListValue> value) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  for (std::set<std::string>::iterator ix = net_internal_watches_.begin();
       ix != net_internal_watches_.end(); ++ix) {
    // Create the event's arguments value.
    scoped_ptr<base::ListValue> event_args(new base::ListValue());
    event_args->Append(value->DeepCopy());
    scoped_ptr<Event> event(new Event(events::kOnAddNetInternalsEntries,
                                      event_args.Pass()));
    EventRouter::Get(browser_context_)
        ->DispatchEventToExtension(*ix, event.Pass());
  }
}

void LogPrivateAPI::MaybeStartNetInternalLogging() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!logging_net_internals_) {
    g_browser_process->io_thread()->net_log()->AddThreadSafeObserver(
        this, net::NetLog::LOG_ALL_BUT_BYTES);
    logging_net_internals_ = true;
  }
}

void LogPrivateAPI::MaybeStopNetInternalLogging() {
  if (net_internal_watches_.empty()) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&LogPrivateAPI:: StopNetInternalLogging,
                   base::Unretained(this)));
  }
}

void LogPrivateAPI::StopNetInternalLogging() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (net_log() && logging_net_internals_) {
    net_log()->RemoveThreadSafeObserver(this);
    logging_net_internals_ = false;
  }
}

void LogPrivateAPI::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionInfo::Reason reason) {
  StopNetInternalsWatch(extension->id());
}

LogPrivateGetHistoricalFunction::LogPrivateGetHistoricalFunction() {
}

LogPrivateGetHistoricalFunction::~LogPrivateGetHistoricalFunction() {
}

bool LogPrivateGetHistoricalFunction::RunAsync() {
  // Get parameters
  scoped_ptr<api::log_private::GetHistorical::Params> params(
      api::log_private::GetHistorical::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  filter_handler_.reset(new FilterHandler(params->filter));

  system_logs::SystemLogsFetcherBase* fetcher;
  if ((params->filter).scrub) {
    fetcher = new system_logs::ScrubbedSystemLogsFetcher();
  } else {
    fetcher = new system_logs::AboutSystemLogsFetcher();
  }
  fetcher->Fetch(
      base::Bind(&LogPrivateGetHistoricalFunction::OnSystemLogsLoaded, this));

  return true;
}

void LogPrivateGetHistoricalFunction::OnSystemLogsLoaded(
    scoped_ptr<system_logs::SystemLogsResponse> sys_info) {
  std::vector<linked_ptr<api::log_private::LogEntry> > data;

  CollectLogInfo(filter_handler_.get(), sys_info.get(), &data);

  // Prepare result
  api::log_private::Result result;
  result.data = data;
  api::log_private::Filter::Populate(
      *((filter_handler_->GetFilter())->ToValue()), &result.filter);
  SetResult(result.ToValue().release());
  SendResponse(true);
}

LogPrivateStartNetInternalsWatchFunction::
LogPrivateStartNetInternalsWatchFunction() {
}

LogPrivateStartNetInternalsWatchFunction::
~LogPrivateStartNetInternalsWatchFunction() {
}

bool LogPrivateStartNetInternalsWatchFunction::RunSync() {
  LogPrivateAPI::Get(GetProfile())->StartNetInternalsWatch(extension_id());
  return true;
}

LogPrivateStopNetInternalsWatchFunction::
LogPrivateStopNetInternalsWatchFunction() {
}

LogPrivateStopNetInternalsWatchFunction::
~LogPrivateStopNetInternalsWatchFunction() {
}

bool LogPrivateStopNetInternalsWatchFunction::RunSync() {
  LogPrivateAPI::Get(GetProfile())->StopNetInternalsWatch(extension_id());
  return true;
}

}  // namespace extensions
