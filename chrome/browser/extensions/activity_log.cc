// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/activity_log.h"

#include <set>
#include <vector>
#include "base/command_line.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/threading/thread_checker.h"
#include "chrome/browser/extensions/api_actions.h"
#include "chrome/browser/extensions/blocked_actions.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/web_contents.h"
#include "googleurl/src/gurl.h"
#include "sql/error_delegate_util.h"
#include "third_party/re2/re2/re2.h"

namespace {

// Concatenate arguments.
std::string MakeArgList(const ListValue* args) {
  std::string call_signature = "";
  ListValue::const_iterator it = args->begin();
  for (; it != args->end(); ++it) {
    std::string arg;
    JSONStringValueSerializer serializer(&arg);
    if (serializer.SerializeAndOmitBinaryValues(**it)) {
      if (it != args->begin())
        call_signature += ", ";
      call_signature += arg;
    }
  }
  return call_signature;
}

// Concatenate an API call with its arguments.
std::string MakeCallSignature(const std::string& name, const ListValue* args) {
  std::string call_signature = name + "(";
  call_signature += MakeArgList(args);
  call_signature += ")";
  return call_signature;
}

// Computes whether the activity log is enabled in this browser (controlled by
// command-line flags) and caches the value (which is assumed never to change).
class LogIsEnabled {
 public:
  LogIsEnabled() {
    ComputeIsEnabled();
  }

  void ComputeIsEnabled() {
    enabled_ = CommandLine::ForCurrentProcess()->
        HasSwitch(switches::kEnableExtensionActivityLogging) ||
        CommandLine::ForCurrentProcess()->
        HasSwitch(switches::kEnableExtensionActivityUI);
  }

  static LogIsEnabled* GetInstance() {
    return Singleton<LogIsEnabled>::get();
  }

  bool enabled() { return enabled_; }

 private:
  bool enabled_;
};

}  // namespace

namespace extensions {

// static
bool ActivityLog::IsLogEnabled() {
  return LogIsEnabled::GetInstance()->enabled();
}

// static
void ActivityLog::RecomputeLoggingIsEnabled() {
  return LogIsEnabled::GetInstance()->ComputeIsEnabled();
}

// This handles errors from the database.
class KillActivityDatabaseErrorDelegate : public sql::ErrorDelegate {
 public:
  explicit KillActivityDatabaseErrorDelegate(ActivityLog* backend)
      : backend_(backend),
        scheduled_death_(false) {}

  virtual int OnError(int error,
                      sql::Connection* connection,
                      sql::Statement* stmt) OVERRIDE {
    if (!scheduled_death_ && sql::IsErrorCatastrophic(error)) {
      ScheduleDeath();
    }
    return error;
  }

  // Schedules death if an error wasn't already reported.
  void ScheduleDeath() {
    if (!scheduled_death_) {
      scheduled_death_ = true;
      backend_->KillActivityLogDatabase();
    }
  }

  bool scheduled_death() const {
    return scheduled_death_;
  }

 private:
  ActivityLog* backend_;
  bool scheduled_death_;

  DISALLOW_COPY_AND_ASSIGN(KillActivityDatabaseErrorDelegate);
};

// ActivityLogFactory

ActivityLogFactory* ActivityLogFactory::GetInstance() {
  return Singleton<ActivityLogFactory>::get();
}

ProfileKeyedService* ActivityLogFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  return new ActivityLog(profile);
}

bool ActivityLogFactory::ServiceRedirectedInIncognito() const {
  return true;
}

// ActivityLog

// Use GetInstance instead of directly creating an ActivityLog.
ActivityLog::ActivityLog(Profile* profile) {
  // enable-extension-activity-logging and enable-extension-activity-ui
  log_activity_to_stdout_ = CommandLine::ForCurrentProcess()->
      HasSwitch(switches::kEnableExtensionActivityLogging);
  log_activity_to_ui_ = CommandLine::ForCurrentProcess()->
      HasSwitch(switches::kEnableExtensionActivityUI);

  // If the database cannot be initialized for some reason, we keep
  // chugging along but nothing will get recorded. If the UI is
  // available, things will still get sent to the UI even if nothing
  // is being written to the database.
  db_ = new ActivityDatabase();
  if (!IsLogEnabled()) return;
  base::FilePath base_dir = profile->GetPath();
  base::FilePath database_name = base_dir.Append(
      chrome::kExtensionActivityLogFilename);
  KillActivityDatabaseErrorDelegate* error_delegate =
      new KillActivityDatabaseErrorDelegate(this);
  db_->SetErrorDelegate(error_delegate);
  ScheduleAndForget(&ActivityDatabase::Init,
                    database_name);
}

ActivityLog::~ActivityLog() {
}

// static
ActivityLog* ActivityLog::GetInstance(Profile* profile) {
  return ActivityLogFactory::GetForProfile(profile);
}

void ActivityLog::AddObserver(const Extension* extension,
                              ActivityLog::Observer* observer) {
  if (!IsLogEnabled()) return;
  if (observers_.count(extension) == 0)
    observers_[extension] = new ObserverListThreadSafe<Observer>;
  observers_[extension]->AddObserver(observer);
}

void ActivityLog::RemoveObserver(const Extension* extension,
                                 ActivityLog::Observer* observer) {
  if (observers_.count(extension) == 1)
    observers_[extension]->RemoveObserver(observer);
}

void ActivityLog::LogAPIActionInternal(const Extension* extension,
                                       const std::string& api_call,
                                       const ListValue* args,
                                       const std::string& extra,
                                       const APIAction::Type type) {
  std::string verb, manager;
  bool matches = RE2::FullMatch(api_call, "(.*?)\\.(.*)", &manager, &verb);
  if (matches) {
    scoped_refptr<APIAction> action = new APIAction(
        extension->id(),
        base::Time::Now(),
        type,
        APIAction::StringAsVerb(verb),
        APIAction::StringAsTarget(manager),
        api_call,
        MakeArgList(args),
        extra);
    ScheduleAndForget(&ActivityDatabase::RecordAction, action);

    // Display the action.
    ObserverMap::const_iterator iter = observers_.find(extension);
    if (iter != observers_.end()) {
      if (type == APIAction::CALL) {
        iter->second->Notify(&Observer::OnExtensionActivity,
                             extension,
                             ActivityLog::ACTIVITY_EXTENSION_API_CALL,
                             MakeCallSignature(api_call, args));
      } else if (type == APIAction::EVENT_CALLBACK) {
        iter->second->Notify(&Observer::OnExtensionActivity,
                             extension,
                             ActivityLog::ACTIVITY_EVENT_DISPATCH,
                             MakeCallSignature(api_call, args));
      }
    }
    if (log_activity_to_stdout_)
      LOG(INFO) << action->PrettyPrintForDebug();
  } else {
    LOG(ERROR) << "Unknown API call! " << api_call;
  }
}

// A wrapper around LogAPIActionInternal, but we know it's an API call.
void ActivityLog::LogAPIAction(const Extension* extension,
                               const std::string& api_call,
                               const ListValue* args,
                               const std::string& extra) {
  if (!IsLogEnabled()) return;
  LogAPIActionInternal(extension, api_call, args, extra, APIAction::CALL);
}

// A wrapper around LogAPIActionInternal, but we know it's actually an event
// being fired and triggering extension code. Having the two separate methods
// (LogAPIAction vs LogEventAction) lets us hide how we actually choose to
// handle them. Right now they're being handled almost the same.
void ActivityLog::LogEventAction(const Extension* extension,
                                 const std::string& api_call,
                                 const ListValue* args,
                                 const std::string& extra) {
  if (!IsLogEnabled()) return;
  LogAPIActionInternal(extension,
                       api_call,
                       args,
                       extra,
                       APIAction::EVENT_CALLBACK);
}

void ActivityLog::LogBlockedAction(const Extension* extension,
                                   const std::string& blocked_call,
                                   const ListValue* args,
                                   const char* reason,
                                   const std::string& extra) {
  if (!IsLogEnabled()) return;
  scoped_refptr<BlockedAction> action = new BlockedAction(extension->id(),
                                                          base::Time::Now(),
                                                          blocked_call,
                                                          MakeArgList(args),
                                                          std::string(reason),
                                                          extra);
  ScheduleAndForget(&ActivityDatabase::RecordAction, action);
  // Display the action.
  ObserverMap::const_iterator iter = observers_.find(extension);
  if (iter != observers_.end()) {
    std::string blocked_str = MakeCallSignature(blocked_call, args);
    iter->second->Notify(&Observer::OnExtensionActivity,
                         extension,
                         ActivityLog::ACTIVITY_EXTENSION_API_BLOCK,
                         blocked_str);
  }
  if (log_activity_to_stdout_)
    LOG(INFO) << action->PrettyPrintForDebug();
}

void ActivityLog::LogDOMActionInternal(const Extension* extension,
                                       const GURL& url,
                                       const string16& url_title,
                                       const std::string& api_call,
                                       const ListValue* args,
                                       const std::string& extra,
                                       DOMAction::DOMActionType verb) {
  scoped_refptr<DOMAction> action = new DOMAction(
      extension->id(),
      base::Time::Now(),
      verb,
      url,
      url_title,
      api_call,
      MakeArgList(args),
      extra);
  ScheduleAndForget(&ActivityDatabase::RecordAction, action);

  // Display the action.
  ObserverMap::const_iterator iter = observers_.find(extension);
  if (iter != observers_.end()) {
    // TODO(felt): This is a kludge, planning to update this when new
    // UI is in place.
    if (verb == DOMAction::INSERTED) {
      iter->second->Notify(&Observer::OnExtensionActivity,
                           extension,
                           ActivityLog::ACTIVITY_CONTENT_SCRIPT,
                           action->PrettyPrintForDebug());
    } else {
      iter->second->Notify(&Observer::OnExtensionActivity,
                           extension,
                           ActivityLog::ACTIVITY_CONTENT_SCRIPT,
                           MakeCallSignature(api_call, args));
    }
  }
  if (log_activity_to_stdout_)
    LOG(INFO) << action->PrettyPrintForDebug();
}

void ActivityLog::LogDOMAction(const Extension* extension,
                               const GURL& url,
                               const string16& url_title,
                               const std::string& api_call,
                               const ListValue* args,
                               const std::string& extra) {
  if (!IsLogEnabled()) return;
  LogDOMActionInternal(extension,
                       url,
                       url_title,
                       api_call,
                       args,
                       extra,
                       DOMAction::MODIFIED);
}

void ActivityLog::GetActions(
    const std::string& extension_id,
    const int day,
    const base::Callback
        <void(scoped_ptr<std::vector<scoped_refptr<Action> > >)>& callback) {
  if (!db_.get()) return;
  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::DB,
      FROM_HERE,
      base::Bind(&ActivityDatabase::GetActions,
                 db_.get(),
                 extension_id,
                 day),
      callback);
}

void ActivityLog::OnScriptsExecuted(
    const content::WebContents* web_contents,
    const ExecutingScriptsMap& extension_ids,
    int32 on_page_id,
    const GURL& on_url) {
  if (!IsLogEnabled()) return;
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  const ExtensionService* extension_service =
      ExtensionSystem::Get(profile)->extension_service();
  const ExtensionSet* extensions = extension_service->extensions();

  for (ExecutingScriptsMap::const_iterator it = extension_ids.begin();
       it != extension_ids.end(); ++it) {
    const Extension* extension = extensions->GetByID(it->first);
    if (!extension)
      continue;

    // If OnScriptsExecuted is fired because of tabs.executeScript, the list
    // of content scripts will be empty.  We don't want to log it because
    // the call to tabs.executeScript will have already been logged anyway.
    if (!it->second.empty()) {
      std::string ext_scripts_str = "";
      for (std::set<std::string>::const_iterator it2 = it->second.begin();
           it2 != it->second.end(); ++it2) {
        ext_scripts_str += *it2;
        ext_scripts_str += " ";
      }
      scoped_ptr<ListValue> script_names(new ListValue());
      script_names->Set(0, new StringValue(ext_scripts_str));
      LogDOMActionInternal(extension,
                           on_url,
                           web_contents->GetTitle(),
                           "",   // no api call here
                           script_names.get(),
                           "",   // no extras either
                           DOMAction::INSERTED);
    }
  }
}

void ActivityLog::KillActivityLogDatabase() {
  if (db_.get()) {
    ScheduleAndForget(&ActivityDatabase::KillDatabase);
  }
}

// static
const char* ActivityLog::ActivityToString(Activity activity) {
  switch (activity) {
    case ActivityLog::ACTIVITY_EXTENSION_API_CALL:
      return "api_call";
    case ActivityLog::ACTIVITY_EXTENSION_API_BLOCK:
      return "api_block";
    case ActivityLog::ACTIVITY_CONTENT_SCRIPT:
      return "content_script";
    case ActivityLog::ACTIVITY_EVENT_DISPATCH:
      return "event_dispatch";
    default:
      NOTREACHED();
      return "";
  }
}

}  // namespace extensions
