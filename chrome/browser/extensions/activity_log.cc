// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/activity_log.h"

#include <set>
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

// Concatenate an API call with its arguments.
std::string MakeCallSignature(const std::string& name, const ListValue* args) {
  std::string call_signature = name + "(";
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
  call_signature += ")";
  return call_signature;
}

}  // namespace

namespace extensions {

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
  if (!IsLoggingEnabled()) return;
  FilePath base_dir = profile->GetPath();
  FilePath database_name = base_dir.Append(
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

bool ActivityLog::IsLoggingEnabled() {
  return (log_activity_to_stdout_ || log_activity_to_ui_);
}

void ActivityLog::AddObserver(const Extension* extension,
                              ActivityLog::Observer* observer) {
  if (!IsLoggingEnabled()) return;
  if (observers_.count(extension) == 0) {
    observers_[extension] = new ObserverListThreadSafe<Observer>;
  }
  observers_[extension]->AddObserver(observer);
}

void ActivityLog::RemoveObserver(const Extension* extension,
                                 ActivityLog::Observer* observer) {
  if (observers_.count(extension) == 1) {
    observers_[extension]->RemoveObserver(observer);
  }
}

void ActivityLog::LogAPIAction(const Extension* extension,
                               const std::string& name,
                               const ListValue* args,
                               const std::string& extra) {
  if (!IsLoggingEnabled()) return;
  std::string verb, manager;
  bool matches = RE2::FullMatch(name, "(.*?)\\.(.*)", &manager, &verb);
  if (matches) {
    std::string call_signature = MakeCallSignature(name, args);
    scoped_refptr<APIAction> action = new APIAction(
        extension->id(),
        base::Time::Now(),
        APIAction::StringAsActionType(verb),
        APIAction::StringAsTargetType(manager),
        call_signature,
        extra);
    ScheduleAndForget(&ActivityDatabase::RecordAction, action);

    // Display the action.
    ObserverMap::const_iterator iter = observers_.find(extension);
    if (iter != observers_.end()) {
      iter->second->Notify(&Observer::OnExtensionActivity,
                           extension,
                           ActivityLog::ACTIVITY_EXTENSION_API_CALL,
                           call_signature);
    }
    if (log_activity_to_stdout_)
      LOG(INFO) << action->PrettyPrintForDebug();
  } else {
    LOG(ERROR) << "Unknown API call! " << name;
  }
}

void ActivityLog::LogBlockedAction(const Extension* extension,
                                   const std::string& blocked_name,
                                   const ListValue* args,
                                   const char* reason,
                                   const std::string& extra) {
  if (!IsLoggingEnabled()) return;
  std::string blocked_call = MakeCallSignature(blocked_name, args);
  scoped_refptr<BlockedAction> action = new BlockedAction(extension->id(),
                                                          base::Time::Now(),
                                                          blocked_call,
                                                          std::string(reason),
                                                          extra);
  ScheduleAndForget(&ActivityDatabase::RecordAction, action);
  // Display the action.
  ObserverMap::const_iterator iter = observers_.find(extension);
  if (iter != observers_.end()) {
    iter->second->Notify(&Observer::OnExtensionActivity,
                         extension,
                         ActivityLog::ACTIVITY_EXTENSION_API_BLOCK,
                         blocked_call);
  }
  if (log_activity_to_stdout_)
    LOG(INFO) << action->PrettyPrintForDebug();
}

void ActivityLog::LogUrlAction(const Extension* extension,
                               const UrlAction::UrlActionType verb,
                               const GURL& url,
                               const string16& url_title,
                               const std::string& technical_message,
                               const std::string& extra) {
  if (!IsLoggingEnabled()) return;
  scoped_refptr<UrlAction> action = new UrlAction(
    extension->id(),
    base::Time::Now(),
    verb,
    url,
    url_title,
    technical_message,
    extra);
  ScheduleAndForget(&ActivityDatabase::RecordAction, action);

  // Display the action.
  ObserverMap::const_iterator iter = observers_.find(extension);
  if (iter != observers_.end()) {
    iter->second->Notify(&Observer::OnExtensionActivity,
                         extension,
                         ActivityLog::ACTIVITY_CONTENT_SCRIPT,
                         action->PrettyPrintForDebug());
  }
  if (log_activity_to_stdout_)
    LOG(INFO) << action->PrettyPrintForDebug();
}

void ActivityLog::OnScriptsExecuted(
    const content::WebContents* web_contents,
    const ExecutingScriptsMap& extension_ids,
    int32 on_page_id,
    const GURL& on_url) {
  if (!IsLoggingEnabled()) return;
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
      LogUrlAction(extension,
                   UrlAction::INSERTED,
                   on_url,
                   web_contents->GetTitle(),
                   ext_scripts_str,
                   "");
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
    default:
      NOTREACHED();
      return "";
  }
}

}  // namespace extensions

