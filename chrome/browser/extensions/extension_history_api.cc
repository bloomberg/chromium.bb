// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_history_api.h"

#include "base/callback.h"
#include "base/json/json_writer.h"
#include "base/message_loop.h"
#include "base/string_number_conversions.h"
#include "base/task.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_event_router.h"
#include "chrome/browser/extensions/extension_history_api_constants.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/profiles/profile.h"
#include "content/common/notification_service.h"
#include "content/common/notification_type.h"

namespace keys = extension_history_api_constants;

namespace {

double MilliSecondsFromTime(const base::Time& time) {
  return 1000 * time.ToDoubleT();
}

void GetHistoryItemDictionary(const history::URLRow& row,
                              DictionaryValue* value) {
  value->SetString(keys::kIdKey, base::Int64ToString(row.id()));
  value->SetString(keys::kUrlKey, row.url().spec());
  value->SetString(keys::kTitleKey, row.title());
  value->SetDouble(keys::kLastVisitdKey,
                   MilliSecondsFromTime(row.last_visit()));
  value->SetInteger(keys::kTypedCountKey, row.typed_count());
  value->SetInteger(keys::kVisitCountKey, row.visit_count());
}

void AddHistoryNode(const history::URLRow& row, ListValue* list) {
  DictionaryValue* dict = new DictionaryValue();
  GetHistoryItemDictionary(row, dict);
  list->Append(dict);
}

void GetVisitInfoDictionary(const history::VisitRow& row,
                            DictionaryValue* value) {
  value->SetString(keys::kIdKey, base::Int64ToString(row.url_id));
  value->SetString(keys::kVisitId, base::Int64ToString(row.visit_id));
  value->SetDouble(keys::kVisitTime, MilliSecondsFromTime(row.visit_time));
  value->SetString(keys::kReferringVisitId,
                   base::Int64ToString(row.referring_visit));

  const char* trans = PageTransition::CoreTransitionString(row.transition);
  DCHECK(trans) << "Invalid transition.";
  value->SetString(keys::kTransition, trans);
}

void AddVisitNode(const history::VisitRow& row, ListValue* list) {
  DictionaryValue* dict = new DictionaryValue();
  GetVisitInfoDictionary(row, dict);
  list->Append(dict);
}

}  // namespace

ExtensionHistoryEventRouter* ExtensionHistoryEventRouter::GetInstance() {
  return Singleton<ExtensionHistoryEventRouter>::get();
}

void ExtensionHistoryEventRouter::ObserveProfile(Profile* profile) {
  NotificationSource source = Source<Profile>(profile);
  if (profiles_.find(source.map_key()) == profiles_.end())
    profiles_[source.map_key()] = profile;

  if (registrar_.IsEmpty()) {
    registrar_.Add(this,
                   NotificationType::HISTORY_URL_VISITED,
                   NotificationService::AllSources());
    registrar_.Add(this,
                   NotificationType::HISTORY_URLS_DELETED,
                   NotificationService::AllSources());
  }
}

ExtensionHistoryEventRouter::ExtensionHistoryEventRouter() {}

ExtensionHistoryEventRouter::~ExtensionHistoryEventRouter() {}

void ExtensionHistoryEventRouter::Observe(NotificationType type,
                                          const NotificationSource& source,
                                          const NotificationDetails& details) {
  ProfileMap::iterator it = profiles_.find(source.map_key());
  if (it != profiles_.end()) {
    Profile* profile = it->second;
    switch (type.value) {
      case NotificationType::HISTORY_URL_VISITED:
        HistoryUrlVisited(
            profile,
            Details<const history::URLVisitedDetails>(details).ptr());
        break;
      case NotificationType::HISTORY_URLS_DELETED:
        HistoryUrlsRemoved(
            profile,
            Details<const history::URLsDeletedDetails>(details).ptr());
        break;
      default:
        NOTREACHED();
    }
  }
}

void ExtensionHistoryEventRouter::HistoryUrlVisited(
    Profile* profile,
    const history::URLVisitedDetails* details) {
  ListValue args;
  DictionaryValue* dict = new DictionaryValue();
  GetHistoryItemDictionary(details->row, dict);
  args.Append(dict);

  std::string json_args;
  base::JSONWriter::Write(&args, false, &json_args);
  DispatchEvent(profile, keys::kOnVisited, json_args);
}

void ExtensionHistoryEventRouter::HistoryUrlsRemoved(
    Profile* profile,
    const history::URLsDeletedDetails* details) {
  ListValue args;
  DictionaryValue* dict = new DictionaryValue();
  dict->SetBoolean(keys::kAllHistoryKey, details->all_history);
  ListValue* urls = new ListValue();
  for (std::set<GURL>::const_iterator iterator = details->urls.begin();
      iterator != details->urls.end();
      ++iterator) {
    urls->Append(new StringValue(iterator->spec()));
  }
  dict->Set(keys::kUrlsKey, urls);
  args.Append(dict);

  std::string json_args;
  base::JSONWriter::Write(&args, false, &json_args);
  DispatchEvent(profile, keys::kOnVisitRemoved, json_args);
}

void ExtensionHistoryEventRouter::DispatchEvent(Profile* profile,
                                                const char* event_name,
                                                const std::string& json_args) {
  if (profile && profile->GetExtensionEventRouter()) {
    profile->GetExtensionEventRouter()->DispatchEventToRenderers(
        event_name, json_args, profile, GURL());
  }
}

void HistoryFunction::Run() {
  if (!RunImpl()) {
    SendResponse(false);
  }
}

bool HistoryFunction::GetUrlFromValue(Value* value, GURL* url) {
  std::string url_string;
  if (!value->GetAsString(&url_string)) {
    bad_message_ = true;
    return false;
  }

  GURL temp_url(url_string);
  if (!temp_url.is_valid()) {
    error_ = keys::kInvalidUrlError;
    return false;
  }
  url->Swap(&temp_url);
  return true;
}

bool HistoryFunction::GetTimeFromValue(Value* value, base::Time* time) {
  double ms_from_epoch = 0.0;
  if (!value->GetAsDouble(&ms_from_epoch)) {
    int ms_from_epoch_as_int = 0;
    if (!value->GetAsInteger(&ms_from_epoch_as_int))
      return false;
    ms_from_epoch = static_cast<double>(ms_from_epoch_as_int);
  }
  // The history service has seconds resolution, while javascript Date() has
  // milliseconds resolution.
  double seconds_from_epoch = ms_from_epoch / 1000.0;
  // Time::FromDoubleT converts double time 0 to empty Time object. So we need
  // to do special handling here.
  *time = (seconds_from_epoch == 0) ?
      base::Time::UnixEpoch() : base::Time::FromDoubleT(seconds_from_epoch);
  return true;
}

HistoryFunctionWithCallback::HistoryFunctionWithCallback() {
}

HistoryFunctionWithCallback::~HistoryFunctionWithCallback() {
}

bool HistoryFunctionWithCallback::RunImpl() {
  AddRef();  // Balanced in SendAysncRepose() and below.
  bool retval = RunAsyncImpl();
  if (false == retval)
    Release();
  return retval;
}

void HistoryFunctionWithCallback::SendAsyncResponse() {
  MessageLoop::current()->PostTask(
      FROM_HERE,
      NewRunnableMethod(
          this,
          &HistoryFunctionWithCallback::SendResponseToCallback));
}

void HistoryFunctionWithCallback::SendResponseToCallback() {
  SendResponse(true);
  Release();  // Balanced in RunImpl().
}

bool GetVisitsHistoryFunction::RunAsyncImpl() {
  DictionaryValue* json;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &json));

  Value* value;
  EXTENSION_FUNCTION_VALIDATE(json->Get(keys::kUrlKey, &value));

  GURL url;
  if (!GetUrlFromValue(value, &url))
    return false;

  HistoryService* hs = profile()->GetHistoryService(Profile::EXPLICIT_ACCESS);
  hs->QueryURL(url,
               true,  // Retrieve full history of a URL.
               &cancelable_consumer_,
               NewCallback(this, &GetVisitsHistoryFunction::QueryComplete));

  return true;
}

void GetVisitsHistoryFunction::QueryComplete(
    HistoryService::Handle request_service,
    bool success,
    const history::URLRow* url_row,
    history::VisitVector* visits) {
  ListValue* list = new ListValue();
  if (visits && !visits->empty()) {
    for (history::VisitVector::iterator iterator = visits->begin();
         iterator != visits->end();
         ++iterator) {
      AddVisitNode(*iterator, list);
    }
  }
  result_.reset(list);
  SendAsyncResponse();
}

bool SearchHistoryFunction::RunAsyncImpl() {
  DictionaryValue* json;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &json));

  // Initialize the HistoryQuery
  string16 search_text;
  EXTENSION_FUNCTION_VALIDATE(json->GetString(keys::kTextKey, &search_text));

  history::QueryOptions options;
  options.SetRecentDayRange(1);
  options.max_count = 100;

  if (json->HasKey(keys::kStartTimeKey)) {  // Optional.
    Value* value;
    EXTENSION_FUNCTION_VALIDATE(json->Get(keys::kStartTimeKey, &value));
    EXTENSION_FUNCTION_VALIDATE(GetTimeFromValue(value, &options.begin_time));
  }
  if (json->HasKey(keys::kEndTimeKey)) {  // Optional.
    Value* value;
    EXTENSION_FUNCTION_VALIDATE(json->Get(keys::kEndTimeKey, &value));
    EXTENSION_FUNCTION_VALIDATE(GetTimeFromValue(value, &options.end_time));
  }
  if (json->HasKey(keys::kMaxResultsKey)) {  // Optional.
    EXTENSION_FUNCTION_VALIDATE(json->GetInteger(keys::kMaxResultsKey,
                                                 &options.max_count));
  }

  HistoryService* hs = profile()->GetHistoryService(Profile::EXPLICIT_ACCESS);
  hs->QueryHistory(search_text, options, &cancelable_consumer_,
                   NewCallback(this, &SearchHistoryFunction::SearchComplete));

  return true;
}

void SearchHistoryFunction::SearchComplete(
    HistoryService::Handle request_handle,
    history::QueryResults* results) {
  ListValue* list = new ListValue();
  if (results && !results->empty()) {
    for (history::QueryResults::URLResultVector::const_iterator iterator =
            results->begin();
         iterator != results->end();
        ++iterator) {
      AddHistoryNode(**iterator, list);
    }
  }
  result_.reset(list);
  SendAsyncResponse();
}

bool AddUrlHistoryFunction::RunImpl() {
  DictionaryValue* json;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &json));

  Value* value;
  EXTENSION_FUNCTION_VALIDATE(json->Get(keys::kUrlKey, &value));

  GURL url;
  if (!GetUrlFromValue(value, &url))
    return false;

  HistoryService* hs = profile()->GetHistoryService(Profile::EXPLICIT_ACCESS);
  hs->AddPage(url, history::SOURCE_EXTENSION);

  SendResponse(true);
  return true;
}

bool DeleteUrlHistoryFunction::RunImpl() {
  DictionaryValue* json;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &json));

  Value* value;
  EXTENSION_FUNCTION_VALIDATE(json->Get(keys::kUrlKey, &value));

  GURL url;
  if (!GetUrlFromValue(value, &url))
    return false;

  HistoryService* hs = profile()->GetHistoryService(Profile::EXPLICIT_ACCESS);
  hs->DeleteURL(url);

  SendResponse(true);
  return true;
}

bool DeleteRangeHistoryFunction::RunAsyncImpl() {
  DictionaryValue* json;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &json));

  Value* value = NULL;
  EXTENSION_FUNCTION_VALIDATE(json->Get(keys::kStartTimeKey, &value));
  base::Time begin_time;
  EXTENSION_FUNCTION_VALIDATE(GetTimeFromValue(value, &begin_time));

  EXTENSION_FUNCTION_VALIDATE(json->Get(keys::kEndTimeKey, &value));
  base::Time end_time;
  EXTENSION_FUNCTION_VALIDATE(GetTimeFromValue(value, &end_time));

  std::set<GURL> restrict_urls;
  HistoryService* hs = profile()->GetHistoryService(Profile::EXPLICIT_ACCESS);
  hs->ExpireHistoryBetween(
      restrict_urls,
      begin_time,
      end_time,
      &cancelable_consumer_,
      NewCallback(this, &DeleteRangeHistoryFunction::DeleteComplete));

  return true;
}

void DeleteRangeHistoryFunction::DeleteComplete() {
  SendAsyncResponse();
}

bool DeleteAllHistoryFunction::RunAsyncImpl() {
  std::set<GURL> restrict_urls;
  HistoryService* hs = profile()->GetHistoryService(Profile::EXPLICIT_ACCESS);
  hs->ExpireHistoryBetween(
      restrict_urls,
      base::Time::UnixEpoch(),     // From the beginning of the epoch.
      base::Time::Now(),           // To the current time.
      &cancelable_consumer_,
      NewCallback(this, &DeleteAllHistoryFunction::DeleteComplete));

  return true;
}

void DeleteAllHistoryFunction::DeleteComplete() {
  SendAsyncResponse();
}
