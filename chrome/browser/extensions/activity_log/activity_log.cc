// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <vector>
#include "base/command_line.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_checker.h"
#include "chrome/browser/extensions/activity_log/activity_action_constants.h"
#include "chrome/browser/extensions/activity_log/activity_log.h"
#include "chrome/browser/extensions/activity_log/stream_noargs_ui_policy.h"
#include "chrome/browser/extensions/api/activity_log_private/activity_log_private_api.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/extensions/install_tracker_factory.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"
#include "content/public/browser/web_contents.h"
#include "third_party/re2/re2/re2.h"
#include "url/gurl.h"

namespace constants = activity_log_constants;

namespace {

// Concatenate arguments.
std::string MakeArgList(const base::ListValue* args) {
  std::string call_signature;
  base::ListValue::const_iterator it = args->begin();
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

// This is a hack for AL callers who don't have access to a profile object
// when deciding whether or not to do the work required for logging. The state
// is accessed through the static ActivityLog::IsLogEnabledOnAnyProfile()
// method. It returns true if --enable-extension-activity-logging is set on the
// command line OR *ANY* profile has the activity log whitelisted extension
// installed.
class LogIsEnabled {
 public:
  LogIsEnabled() : any_profile_enabled_(false) {
    ComputeIsFlagEnabled();
  }

  void ComputeIsFlagEnabled() {
    base::AutoLock auto_lock(lock_);
    cmd_line_enabled_ = CommandLine::ForCurrentProcess()->
        HasSwitch(switches::kEnableExtensionActivityLogging);
  }

  static LogIsEnabled* GetInstance() {
    return Singleton<LogIsEnabled>::get();
  }

  bool IsEnabled() {
    base::AutoLock auto_lock(lock_);
    return cmd_line_enabled_ || any_profile_enabled_;
  }

  void SetProfileEnabled(bool any_profile_enabled) {
    base::AutoLock auto_lock(lock_);
    any_profile_enabled_ = any_profile_enabled;
  }

 private:
  base::Lock lock_;
  bool any_profile_enabled_;
  bool cmd_line_enabled_;
};

}  // namespace

namespace extensions {

// static
bool ActivityLog::IsLogEnabledOnAnyProfile() {
  return LogIsEnabled::GetInstance()->IsEnabled();
}

// static
void ActivityLog::RecomputeLoggingIsEnabled(bool profile_enabled) {
  LogIsEnabled::GetInstance()->ComputeIsFlagEnabled();
  LogIsEnabled::GetInstance()->SetProfileEnabled(profile_enabled);
}

// ActivityLogFactory

ActivityLogFactory* ActivityLogFactory::GetInstance() {
  return Singleton<ActivityLogFactory>::get();
}

BrowserContextKeyedService* ActivityLogFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new ActivityLog(static_cast<Profile*>(profile));
}

content::BrowserContext* ActivityLogFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

ActivityLogFactory::ActivityLogFactory()
    : BrowserContextKeyedServiceFactory(
        "ActivityLog",
        BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ExtensionSystemFactory::GetInstance());
  DependsOn(InstallTrackerFactory::GetInstance());
}

ActivityLogFactory::~ActivityLogFactory() {
}

// ActivityLog

void ActivityLog::SetDefaultPolicy(ActivityLogPolicy::PolicyType policy_type) {
  // Can't use IsLogEnabled() here because this is called from inside Init.
  if (policy_type != policy_type_ && enabled_) {
    // Deleting the old policy takes place asynchronously, on the database
    // thread.  Initializing a new policy below similarly happens
    // asynchronously.  Since the two operations are both queued for the
    // database, the queue ordering should ensure that the deletion completes
    // before database initialization occurs.
    //
    // However, changing policies at runtime is still not recommended, and
    // likely only should be done for unit tests.
    if (policy_)
      policy_->Close();

    switch (policy_type) {
      case ActivityLogPolicy::POLICY_FULLSTREAM:
        policy_ = new FullStreamUIPolicy(profile_);
        break;
      case ActivityLogPolicy::POLICY_NOARGS:
        policy_ = new StreamWithoutArgsUIPolicy(profile_);
        break;
      default:
        NOTREACHED();
    }
    policy_type_ = policy_type;
  }
}

// Use GetInstance instead of directly creating an ActivityLog.
ActivityLog::ActivityLog(Profile* profile)
    : policy_(NULL),
      policy_type_(ActivityLogPolicy::POLICY_INVALID),
      profile_(profile),
      enabled_(false),
      initialized_(false),
      policy_chosen_(false),
      testing_mode_(false),
      has_threads_(true),
      tracker_(NULL) {
  // This controls whether logging statements are printed, which policy is set,
  // etc.
  testing_mode_ = CommandLine::ForCurrentProcess()->HasSwitch(
    switches::kEnableExtensionActivityLogTesting);

  // Check that the right threads exist. If not, we shouldn't try to do things
  // that require them.
  if (!BrowserThread::IsMessageLoopValid(BrowserThread::DB) ||
      !BrowserThread::IsMessageLoopValid(BrowserThread::FILE) ||
      !BrowserThread::IsMessageLoopValid(BrowserThread::IO)) {
    LOG(ERROR) << "Missing threads, disabling Activity Logging!";
    has_threads_ = false;
  } else {
    enabled_ = IsLogEnabledOnAnyProfile();
    ExtensionSystem::Get(profile_)->ready().Post(
      FROM_HERE, base::Bind(&ActivityLog::Init, base::Unretained(this)));
  }

  observers_ = new ObserverListThreadSafe<Observer>;
}

void ActivityLog::Init() {
  DCHECK(has_threads_);
  DCHECK(!initialized_);
  const Extension* whitelisted_extension = ExtensionSystem::Get(profile_)->
      extension_service()->GetExtensionById(kActivityLogExtensionId, false);
  if (whitelisted_extension) {
    enabled_ = true;
    LogIsEnabled::GetInstance()->SetProfileEnabled(true);
  }
  tracker_ = InstallTrackerFactory::GetForProfile(profile_);
  tracker_->AddObserver(this);
  ChooseDefaultPolicy();
  initialized_ = true;
}

void ActivityLog::ChooseDefaultPolicy() {
  if (policy_chosen_ || !enabled_) return;
  if (testing_mode_)
    SetDefaultPolicy(ActivityLogPolicy::POLICY_FULLSTREAM);
  else
    SetDefaultPolicy(ActivityLogPolicy::POLICY_NOARGS);
}

void ActivityLog::Shutdown() {
  if (tracker_) tracker_->RemoveObserver(this);
}

ActivityLog::~ActivityLog() {
  if (policy_)
    policy_->Close();
}

bool ActivityLog::IsLogEnabled() {
  if (!has_threads_ || !initialized_) return false;
  return enabled_;
}

void ActivityLog::OnExtensionLoaded(const Extension* extension) {
  if (extension->id() != kActivityLogExtensionId) return;
  enabled_ = true;
  LogIsEnabled::GetInstance()->SetProfileEnabled(true);
  ChooseDefaultPolicy();
}

void ActivityLog::OnExtensionUnloaded(const Extension* extension) {
  if (extension->id() != kActivityLogExtensionId) return;
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableExtensionActivityLogging))
    enabled_ = false;
}

// static
ActivityLog* ActivityLog::GetInstance(Profile* profile) {
  return ActivityLogFactory::GetForProfile(profile);
}

void ActivityLog::AddObserver(ActivityLog::Observer* observer) {
  observers_->AddObserver(observer);
}

void ActivityLog::RemoveObserver(ActivityLog::Observer* observer) {
  observers_->RemoveObserver(observer);
}

void ActivityLog::LogAction(scoped_refptr<Action> action) {
  if (IsLogEnabled() &&
      !ActivityLogAPI::IsExtensionWhitelisted(action->extension_id())) {
    if (policy_)
      policy_->ProcessAction(action);
    observers_->Notify(&Observer::OnExtensionActivity, action);
    if (testing_mode_)
      LOG(INFO) << action->PrintForDebug();
  }
}

void ActivityLog::LogAPIActionInternal(const std::string& extension_id,
                                       const std::string& api_call,
                                       base::ListValue* args,
                                       const std::string& extra,
                                       const APIAction::Type type) {
  std::string verb, manager;
  bool matches = RE2::FullMatch(api_call, "(.*?)\\.(.*)", &manager, &verb);
  if (matches) {
    if (!args->empty() && manager == "tabs") {
      APIAction::LookupTabId(api_call, args, profile_);
    }

    DCHECK((type == APIAction::CALL || type == APIAction::EVENT_CALLBACK) &&
           "Unexpected APIAction call type.");

    scoped_refptr<Action> action;
    action = new Action(extension_id,
                        base::Time::Now(),
                        type == APIAction::CALL ? Action::ACTION_API_CALL
                                                : Action::ACTION_API_EVENT,
                        api_call);
    action->set_args(make_scoped_ptr(args->DeepCopy()));
    if (!extra.empty())
      action->mutable_other()->SetString(constants::kActionExtra, extra);

    LogAction(action);
  } else {
    LOG(ERROR) << "Unknown API call! " << api_call;
  }
}

// A wrapper around LogAPIActionInternal, but we know it's an API call.
void ActivityLog::LogAPIAction(const std::string& extension_id,
                               const std::string& api_call,
                               base::ListValue* args,
                               const std::string& extra) {
  if (!IsLogEnabled() ||
      ActivityLogAPI::IsExtensionWhitelisted(extension_id)) return;
  LogAPIActionInternal(extension_id,
                       api_call,
                       args,
                       extra,
                       APIAction::CALL);
}

// A wrapper around LogAPIActionInternal, but we know it's actually an event
// being fired and triggering extension code. Having the two separate methods
// (LogAPIAction vs LogEventAction) lets us hide how we actually choose to
// handle them. Right now they're being handled almost the same.
void ActivityLog::LogEventAction(const std::string& extension_id,
                                 const std::string& api_call,
                                 base::ListValue* args,
                                 const std::string& extra) {
  if (!IsLogEnabled() ||
      ActivityLogAPI::IsExtensionWhitelisted(extension_id)) return;
  LogAPIActionInternal(extension_id,
                       api_call,
                       args,
                       extra,
                       APIAction::EVENT_CALLBACK);
}

void ActivityLog::LogBlockedAction(const std::string& extension_id,
                                   const std::string& blocked_call,
                                   base::ListValue* args,
                                   BlockedAction::Reason reason,
                                   const std::string& extra) {
  if (!IsLogEnabled() ||
      ActivityLogAPI::IsExtensionWhitelisted(extension_id)) return;

  scoped_refptr<Action> action;
  action = new Action(extension_id,
                      base::Time::Now(),
                      Action::ACTION_API_BLOCKED,
                      blocked_call);
  action->set_args(make_scoped_ptr(args->DeepCopy()));
  action->mutable_other()
      ->SetInteger(constants::kActionBlockedReason, static_cast<int>(reason));
  if (!extra.empty())
    action->mutable_other()->SetString(constants::kActionExtra, extra);

  LogAction(action);
}

void ActivityLog::LogDOMAction(const std::string& extension_id,
                               const GURL& url,
                               const string16& url_title,
                               const std::string& api_call,
                               const base::ListValue* args,
                               DomActionType::Type call_type,
                               const std::string& extra) {
  if (!IsLogEnabled() ||
      ActivityLogAPI::IsExtensionWhitelisted(extension_id)) return;

  Action::ActionType action_type = Action::ACTION_DOM_ACCESS;
  if (call_type == DomActionType::INSERTED) {
    action_type = Action::ACTION_CONTENT_SCRIPT;
  } else if (call_type == DomActionType::METHOD &&
             api_call == "XMLHttpRequest.open") {
    call_type = DomActionType::XHR;
    action_type = Action::ACTION_DOM_XHR;
  }

  scoped_refptr<Action> action;
  action = new Action(extension_id, base::Time::Now(), action_type, api_call);
  if (args)
    action->set_args(make_scoped_ptr(args->DeepCopy()));
  action->set_page_url(url);
  action->set_page_title(base::UTF16ToUTF8(url_title));
  action->mutable_other()
      ->SetInteger(constants::kActionDomVerb, static_cast<int>(call_type));
  if (!extra.empty())
    action->mutable_other()->SetString(constants::kActionExtra, extra);

  LogAction(action);
}

void ActivityLog::LogWebRequestAction(const std::string& extension_id,
                                      const GURL& url,
                                      const std::string& api_call,
                                      scoped_ptr<DictionaryValue> details,
                                      const std::string& extra) {
  if (!IsLogEnabled() ||
      ActivityLogAPI::IsExtensionWhitelisted(extension_id)) return;

  scoped_refptr<Action> action;
  action = new Action(
      extension_id, base::Time::Now(), Action::ACTION_WEB_REQUEST, api_call);
  action->set_page_url(url);
  action->mutable_other()->Set(constants::kActionWebRequest, details.release());
  if (!extra.empty())
    action->mutable_other()->SetString(constants::kActionExtra, extra);

  LogAction(action);
}

void ActivityLog::GetActions(
    const std::string& extension_id,
    const int day,
    const base::Callback
        <void(scoped_ptr<std::vector<scoped_refptr<Action> > >)>& callback) {
  if (policy_) {
    policy_->ReadData(extension_id, day, callback);
  }
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
  const prerender::PrerenderManager* prerender_manager =
      prerender::PrerenderManagerFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()));
  std::string extra;

  if (prerender_manager &&
      prerender_manager->IsWebContentsPrerendering(web_contents, NULL))
    extra = "(prerender)";

  for (ExecutingScriptsMap::const_iterator it = extension_ids.begin();
       it != extension_ids.end(); ++it) {
    const Extension* extension = extensions->GetByID(it->first);
    if (!extension || ActivityLogAPI::IsExtensionWhitelisted(extension->id()))
      continue;

    // If OnScriptsExecuted is fired because of tabs.executeScript, the list
    // of content scripts will be empty.  We don't want to log it because
    // the call to tabs.executeScript will have already been logged anyway.
    if (!it->second.empty()) {
      std::string ext_scripts_str;
      for (std::set<std::string>::const_iterator it2 = it->second.begin();
           it2 != it->second.end();
           ++it2) {
        ext_scripts_str += *it2;
        ext_scripts_str += " ";
      }
      scoped_ptr<base::ListValue> script_names(new base::ListValue());
      script_names->Set(0, new base::StringValue(ext_scripts_str));
      LogDOMAction(extension->id(),
                   on_url,
                   web_contents->GetTitle(),
                   std::string(),  // no api call here
                   script_names.get(),
                   DomActionType::INSERTED,
                   extra);
    }
  }
}

}  // namespace extensions
