// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/history_extension_api.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/json/json_writer.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_event_router.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/api/history.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"

namespace AddUrl = extensions::api::history::AddUrl;
namespace DeleteUrl = extensions::api::history::DeleteUrl;
namespace DeleteRange = extensions::api::history::DeleteRange;
namespace GetVisits = extensions::api::history::GetVisits;
namespace Search = extensions::api::history::Search;

namespace {

const char kAllHistoryKey[] = "allHistory";
const char kEndTimeKey[] = "endTime";
const char kFaviconUrlKey[] = "favIconUrl";
const char kIdKey[] = "id";
const char kLastVisitdKey[] = "lastVisitTime";
const char kMaxResultsKey[] = "maxResults";
const char kNewKey[] = "new";
const char kReferringVisitId[] = "referringVisitId";
const char kRemovedKey[] = "removed";
const char kStartTimeKey[] = "startTime";
const char kTextKey[] = "text";
const char kTitleKey[] = "title";
const char kTypedCountKey[] = "typedCount";
const char kVisitCountKey[] = "visitCount";
const char kTransition[] = "transition";
const char kUrlKey[] = "url";
const char kUrlsKey[] = "urls";
const char kVisitId[] = "visitId";
const char kVisitTime[] = "visitTime";

const char kOnVisited[] = "history.onVisited";
const char kOnVisitRemoved[] = "history.onVisitRemoved";

const char kInvalidIdError[] = "History item id is invalid.";
const char kInvalidUrlError[] = "Url is invalid.";

double MilliSecondsFromTime(const base::Time& time) {
  return 1000 * time.ToDoubleT();
}

void GetHistoryItemDictionary(const history::URLRow& row,
                              DictionaryValue* value) {
  value->SetString(kIdKey, base::Int64ToString(row.id()));
  value->SetString(kUrlKey, row.url().spec());
  value->SetString(kTitleKey, row.title());
  value->SetDouble(kLastVisitdKey,
                   MilliSecondsFromTime(row.last_visit()));
  value->SetInteger(kTypedCountKey, row.typed_count());
  value->SetInteger(kVisitCountKey, row.visit_count());
}

void AddHistoryNode(const history::URLRow& row, ListValue* list) {
  DictionaryValue* dict = new DictionaryValue();
  GetHistoryItemDictionary(row, dict);
  list->Append(dict);
}

void GetVisitInfoDictionary(const history::VisitRow& row,
                            DictionaryValue* value) {
  value->SetString(kIdKey, base::Int64ToString(row.url_id));
  value->SetString(kVisitId, base::Int64ToString(row.visit_id));
  value->SetDouble(kVisitTime, MilliSecondsFromTime(row.visit_time));
  value->SetString(kReferringVisitId,
                   base::Int64ToString(row.referring_visit));

  const char* trans =
      content::PageTransitionGetCoreTransitionString(row.transition);
  DCHECK(trans) << "Invalid transition.";
  value->SetString(kTransition, trans);
}

void AddVisitNode(const history::VisitRow& row, ListValue* list) {
  DictionaryValue* dict = new DictionaryValue();
  GetVisitInfoDictionary(row, dict);
  list->Append(dict);
}

}  // namespace

HistoryExtensionEventRouter::HistoryExtensionEventRouter() {}

HistoryExtensionEventRouter::~HistoryExtensionEventRouter() {}

void HistoryExtensionEventRouter::ObserveProfile(Profile* profile) {
  CHECK(registrar_.IsEmpty());
  const content::Source<Profile> source = content::Source<Profile>(profile);
  registrar_.Add(this,
                 chrome::NOTIFICATION_HISTORY_URL_VISITED,
                 source);
  registrar_.Add(this,
                 chrome::NOTIFICATION_HISTORY_URLS_DELETED,
                 source);
}

void HistoryExtensionEventRouter::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_HISTORY_URL_VISITED:
      HistoryUrlVisited(
          content::Source<Profile>(source).ptr(),
          content::Details<const history::URLVisitedDetails>(details).ptr());
      break;
    case chrome::NOTIFICATION_HISTORY_URLS_DELETED:
      HistoryUrlsRemoved(
          content::Source<Profile>(source).ptr(),
          content::Details<const history::URLsDeletedDetails>(details).ptr());
      break;
    default:
      NOTREACHED();
  }
}

void HistoryExtensionEventRouter::HistoryUrlVisited(
    Profile* profile,
    const history::URLVisitedDetails* details) {
  ListValue args;
  DictionaryValue* dict = new DictionaryValue();
  GetHistoryItemDictionary(details->row, dict);
  args.Append(dict);

  std::string json_args;
  base::JSONWriter::Write(&args, &json_args);
  DispatchEvent(profile, kOnVisited, json_args);
}

void HistoryExtensionEventRouter::HistoryUrlsRemoved(
    Profile* profile,
    const history::URLsDeletedDetails* details) {
  ListValue args;
  DictionaryValue* dict = new DictionaryValue();
  dict->SetBoolean(kAllHistoryKey, details->all_history);
  ListValue* urls = new ListValue();
  for (history::URLRows::const_iterator iterator = details->rows.begin();
      iterator != details->rows.end(); ++iterator) {
    urls->Append(new StringValue(iterator->url().spec()));
  }
  dict->Set(kUrlsKey, urls);
  args.Append(dict);

  std::string json_args;
  base::JSONWriter::Write(&args, &json_args);
  DispatchEvent(profile, kOnVisitRemoved, json_args);
}

void HistoryExtensionEventRouter::DispatchEvent(Profile* profile,
                                                const char* event_name,
                                                const std::string& json_args) {
  if (profile && profile->GetExtensionEventRouter()) {
    profile->GetExtensionEventRouter()->DispatchEventToRenderers(
        event_name, json_args, profile, GURL(),
        extensions::EventFilteringInfo());
  }
}

void HistoryFunction::Run() {
  if (!RunImpl()) {
    SendResponse(false);
  }
}

bool HistoryFunction::ValidateUrl(const std::string& url_string, GURL* url) {
  GURL temp_url(url_string);
  if (!temp_url.is_valid()) {
    error_ = kInvalidUrlError;
    return false;
  }
  url->Swap(&temp_url);
  return true;
}

base::Time HistoryFunction::GetTime(double ms_from_epoch) {
  // The history service has seconds resolution, while javascript Date() has
  // milliseconds resolution.
  double seconds_from_epoch = ms_from_epoch / 1000.0;
  // Time::FromDoubleT converts double time 0 to empty Time object. So we need
  // to do special handling here.
  return (seconds_from_epoch == 0) ?
      base::Time::UnixEpoch() : base::Time::FromDoubleT(seconds_from_epoch);
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
      base::Bind(&HistoryFunctionWithCallback::SendResponseToCallback, this));
}

void HistoryFunctionWithCallback::SendResponseToCallback() {
  SendResponse(true);
  Release();  // Balanced in RunImpl().
}

bool GetVisitsHistoryFunction::RunAsyncImpl() {
  scoped_ptr<GetVisits::Params> params(GetVisits::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  GURL url;
  if (!ValidateUrl(params->details.url, &url))
    return false;

  HistoryService* hs =
      HistoryServiceFactory::GetForProfile(profile(),
                                           Profile::EXPLICIT_ACCESS);
  hs->QueryURL(url,
               true,  // Retrieve full history of a URL.
               &cancelable_consumer_,
               base::Bind(&GetVisitsHistoryFunction::QueryComplete,
                          base::Unretained(this)));

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
  SetResult(list);
  SendAsyncResponse();
}

bool SearchHistoryFunction::RunAsyncImpl() {
  scoped_ptr<Search::Params> params(Search::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  string16 search_text = UTF8ToUTF16(params->query.text);

  history::QueryOptions options;
  options.SetRecentDayRange(1);
  options.max_count = 100;

  if (params->query.start_time.get())
    options.begin_time = GetTime(*params->query.start_time);
  if (params->query.end_time.get())
    options.end_time = GetTime(*params->query.end_time);
  if (params->query.max_results.get())
    options.max_count = *params->query.max_results;

  HistoryService* hs =
      HistoryServiceFactory::GetForProfile(profile(),
                                           Profile::EXPLICIT_ACCESS);
  hs->QueryHistory(search_text, options, &cancelable_consumer_,
                   base::Bind(&SearchHistoryFunction::SearchComplete,
                              base::Unretained(this)));

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
  SetResult(list);
  SendAsyncResponse();
}

bool AddUrlHistoryFunction::RunImpl() {
  scoped_ptr<AddUrl::Params> params(AddUrl::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  GURL url;
  if (!ValidateUrl(params->details.url, &url))
    return false;

  HistoryService* hs =
      HistoryServiceFactory::GetForProfile(profile(),
                                           Profile::EXPLICIT_ACCESS);
  hs->AddPage(url, history::SOURCE_EXTENSION);

  SendResponse(true);
  return true;
}

bool DeleteUrlHistoryFunction::RunImpl() {
  scoped_ptr<DeleteUrl::Params> params(DeleteUrl::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  GURL url;
  if (!ValidateUrl(params->details.url, &url))
    return false;

  HistoryService* hs =
      HistoryServiceFactory::GetForProfile(profile(),
                                           Profile::EXPLICIT_ACCESS);
  hs->DeleteURL(url);

  SendResponse(true);
  return true;
}

bool DeleteRangeHistoryFunction::RunAsyncImpl() {
  scoped_ptr<DeleteRange::Params> params(DeleteRange::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  base::Time start_time = GetTime(params->range.start_time);
  base::Time end_time = GetTime(params->range.end_time);

  std::set<GURL> restrict_urls;
  HistoryService* hs =
      HistoryServiceFactory::GetForProfile(profile(),
                                           Profile::EXPLICIT_ACCESS);
  hs->ExpireHistoryBetween(
      restrict_urls,
      start_time,
      end_time,
      &cancelable_consumer_,
      base::Bind(&DeleteRangeHistoryFunction::DeleteComplete,
                 base::Unretained(this)));

  return true;
}

void DeleteRangeHistoryFunction::DeleteComplete() {
  SendAsyncResponse();
}

bool DeleteAllHistoryFunction::RunAsyncImpl() {
  std::set<GURL> restrict_urls;
  HistoryService* hs =
      HistoryServiceFactory::GetForProfile(profile(),
                                           Profile::EXPLICIT_ACCESS);
  hs->ExpireHistoryBetween(
      restrict_urls,
      base::Time::UnixEpoch(),     // From the beginning of the epoch.
      base::Time::Now(),           // To the current time.
      &cancelable_consumer_,
      base::Bind(&DeleteAllHistoryFunction::DeleteComplete,
                 base::Unretained(this)));

  return true;
}

void DeleteAllHistoryFunction::DeleteComplete() {
  SendAsyncResponse();
}
