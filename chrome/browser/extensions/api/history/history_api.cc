// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/history/history_api.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/json/json_writer.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/string_number_conversions.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/history/visit_filter.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/cancelable_task_tracker.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/api/experimental_history.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"

namespace extensions {

using api::experimental_history::MostVisitedItem;
using api::history::HistoryItem;
using api::history::VisitItem;

typedef std::vector<linked_ptr<api::history::HistoryItem> >
    HistoryItemList;
typedef std::vector<linked_ptr<api::history::VisitItem> >
    VisitItemList;

namespace AddUrl = api::history::AddUrl;
namespace DeleteUrl = api::history::DeleteUrl;
namespace DeleteRange = api::history::DeleteRange;
namespace GetMostVisited = api::experimental_history::GetMostVisited;
namespace GetVisits = api::history::GetVisits;
namespace OnVisited = api::history::OnVisited;
namespace OnVisitRemoved = api::history::OnVisitRemoved;
namespace Search = api::history::Search;

namespace {

const char kOnVisited[] = "history.onVisited";
const char kOnVisitRemoved[] = "history.onVisitRemoved";

const char kInvalidIdError[] = "History item id is invalid.";
const char kInvalidUrlError[] = "Url is invalid.";

double MilliSecondsFromTime(const base::Time& time) {
  return 1000 * time.ToDoubleT();
}

scoped_ptr<HistoryItem> GetHistoryItem(const history::URLRow& row) {
  scoped_ptr<HistoryItem> history_item(new HistoryItem());

  history_item->id = base::Int64ToString(row.id());
  history_item->url.reset(new std::string(row.url().spec()));
  history_item->title.reset(new std::string(UTF16ToUTF8(row.title())));
  history_item->last_visit_time.reset(
      new double(MilliSecondsFromTime(row.last_visit())));
  history_item->typed_count.reset(new int(row.typed_count()));
  history_item->visit_count.reset(new int(row.visit_count()));

  return history_item.Pass();
}

scoped_ptr<VisitItem> GetVisitItem(const history::VisitRow& row) {
  scoped_ptr<VisitItem> visit_item(new VisitItem());

  visit_item->id = base::Int64ToString(row.url_id);
  visit_item->visit_id = base::Int64ToString(row.visit_id);
  visit_item->visit_time.reset(
      new double(MilliSecondsFromTime(row.visit_time)));
  visit_item->referring_visit_id = base::Int64ToString(row.referring_visit);

  VisitItem::Transition transition = VisitItem::TRANSITION_LINK;
  switch (row.transition & content::PAGE_TRANSITION_CORE_MASK) {
    case content::PAGE_TRANSITION_LINK:
      transition = VisitItem::TRANSITION_LINK;
      break;
    case content::PAGE_TRANSITION_TYPED:
      transition = VisitItem::TRANSITION_TYPED;
      break;
    case content::PAGE_TRANSITION_AUTO_BOOKMARK:
      transition = VisitItem::TRANSITION_AUTO_BOOKMARK;
      break;
    case content::PAGE_TRANSITION_AUTO_SUBFRAME:
      transition = VisitItem::TRANSITION_AUTO_SUBFRAME;
      break;
    case content::PAGE_TRANSITION_MANUAL_SUBFRAME:
      transition = VisitItem::TRANSITION_MANUAL_SUBFRAME;
      break;
    case content::PAGE_TRANSITION_GENERATED:
      transition = VisitItem::TRANSITION_GENERATED;
      break;
    case content::PAGE_TRANSITION_AUTO_TOPLEVEL:
      transition = VisitItem::TRANSITION_AUTO_TOPLEVEL;
      break;
    case content::PAGE_TRANSITION_FORM_SUBMIT:
      transition = VisitItem::TRANSITION_FORM_SUBMIT;
      break;
    case content::PAGE_TRANSITION_RELOAD:
      transition = VisitItem::TRANSITION_RELOAD;
      break;
    case content::PAGE_TRANSITION_KEYWORD:
      transition = VisitItem::TRANSITION_KEYWORD;
      break;
    case content::PAGE_TRANSITION_KEYWORD_GENERATED:
      transition = VisitItem::TRANSITION_KEYWORD_GENERATED;
      break;
    default:
      DCHECK(false);
  }

  visit_item->transition = transition;

  return visit_item.Pass();
}

}  // namespace

HistoryEventRouter::HistoryEventRouter(Profile* profile) {
  const content::Source<Profile> source = content::Source<Profile>(profile);
  registrar_.Add(this,
                 chrome::NOTIFICATION_HISTORY_URL_VISITED,
                 source);
  registrar_.Add(this,
                 chrome::NOTIFICATION_HISTORY_URLS_DELETED,
                 source);
}

HistoryEventRouter::~HistoryEventRouter() {}

void HistoryEventRouter::Observe(int type,
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

void HistoryEventRouter::HistoryUrlVisited(
    Profile* profile,
    const history::URLVisitedDetails* details) {
  scoped_ptr<HistoryItem> history_item = GetHistoryItem(details->row);
  scoped_ptr<ListValue> args = OnVisited::Create(*history_item);

  DispatchEvent(profile, kOnVisited, args.Pass());
}

void HistoryEventRouter::HistoryUrlsRemoved(
    Profile* profile,
    const history::URLsDeletedDetails* details) {
  OnVisitRemoved::Removed removed;
  removed.all_history = details->all_history;

  std::vector<std::string>* urls = new std::vector<std::string>();
  for (history::URLRows::const_iterator iterator = details->rows.begin();
      iterator != details->rows.end(); ++iterator) {
    urls->push_back(iterator->url().spec());
  }
  removed.urls.reset(urls);

  scoped_ptr<ListValue> args = OnVisitRemoved::Create(removed);
  DispatchEvent(profile, kOnVisitRemoved, args.Pass());
}

void HistoryEventRouter::DispatchEvent(
    Profile* profile,
    const char* event_name,
    scoped_ptr<ListValue> event_args) {
  if (profile && extensions::ExtensionSystem::Get(profile)->event_router()) {
    scoped_ptr<extensions::Event> event(new extensions::Event(
        event_name, event_args.Pass()));
    event->restrict_to_profile = profile;
    extensions::ExtensionSystem::Get(profile)->event_router()->
        BroadcastEvent(event.Pass());
  }
}

HistoryAPI::HistoryAPI(Profile* profile) : profile_(profile) {
  ExtensionSystem::Get(profile_)->event_router()->RegisterObserver(
      this, kOnVisited);
  ExtensionSystem::Get(profile_)->event_router()->RegisterObserver(
      this, kOnVisitRemoved);
}

HistoryAPI::~HistoryAPI() {
}

void HistoryAPI::Shutdown() {
  ExtensionSystem::Get(profile_)->event_router()->UnregisterObserver(this);
}

void HistoryAPI::OnListenerAdded(const EventListenerInfo& details) {
  history_event_router_.reset(new HistoryEventRouter(profile_));
  ExtensionSystem::Get(profile_)->event_router()->UnregisterObserver(this);
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

bool HistoryGetMostVisitedFunction::RunAsyncImpl() {
  scoped_ptr<GetMostVisited::Params> params =
      GetMostVisited::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  history::VisitFilter filter;
  if (params->details.filter_time.get())
    filter.SetFilterTime(GetTime(*params->details.filter_time));
  if (params->details.filter_width.get()) {
    filter.SetFilterWidth(base::TimeDelta::FromMilliseconds(
        static_cast<int64>(*params->details.filter_width)));
  }
  if (params->details.day_of_the_week.get())
    filter.SetDayOfTheWeekFilter(*params->details.day_of_the_week);
  int max_results = 100;
  if (params->details.max_results.get())
    max_results = *params->details.max_results;
  HistoryService* hs =
      HistoryServiceFactory::GetForProfile(profile(), Profile::EXPLICIT_ACCESS);
  hs->QueryFilteredURLs(max_results, filter, false, &cancelable_consumer_,
      base::Bind(&HistoryGetMostVisitedFunction::QueryComplete,
                 base::Unretained(this)));
  return true;
}

void HistoryGetMostVisitedFunction::QueryComplete(
    CancelableRequestProvider::Handle handle,
    const history::FilteredURLList& data) {
  std::vector<linked_ptr<MostVisitedItem> > results;
  results.reserve(data.size());
  for (size_t i = 0; i < data.size(); i++) {
    linked_ptr<MostVisitedItem> item(new MostVisitedItem);
    item->url = data[i].url.spec();
    item->title = UTF16ToUTF8(data[i].title);
    results.push_back(item);
  }
  results_ = GetMostVisited::Results::Create(results);
  SendAsyncResponse();
}

bool HistoryGetVisitsFunction::RunAsyncImpl() {
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
               base::Bind(&HistoryGetVisitsFunction::QueryComplete,
                          base::Unretained(this)));

  return true;
}

void HistoryGetVisitsFunction::QueryComplete(
    HistoryService::Handle request_service,
    bool success,
    const history::URLRow* url_row,
    history::VisitVector* visits) {
  VisitItemList visit_item_vec;
  if (visits && !visits->empty()) {
    for (history::VisitVector::iterator iterator = visits->begin();
         iterator != visits->end();
         ++iterator) {
      visit_item_vec.push_back(make_linked_ptr(
          GetVisitItem(*iterator).release()));
    }
  }

  results_ = GetVisits::Results::Create(visit_item_vec);
  SendAsyncResponse();
}

bool HistorySearchFunction::RunAsyncImpl() {
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
                   base::Bind(&HistorySearchFunction::SearchComplete,
                              base::Unretained(this)));

  return true;
}

void HistorySearchFunction::SearchComplete(
    HistoryService::Handle request_handle,
    history::QueryResults* results) {
  HistoryItemList history_item_vec;
  if (results && !results->empty()) {
    for (history::QueryResults::URLResultVector::const_iterator iterator =
            results->begin();
         iterator != results->end();
        ++iterator) {
      history_item_vec.push_back(make_linked_ptr(
          GetHistoryItem(**iterator).release()));
    }
  }
  results_ = Search::Results::Create(history_item_vec);
  SendAsyncResponse();
}

bool HistoryAddUrlFunction::RunImpl() {
  scoped_ptr<AddUrl::Params> params(AddUrl::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  GURL url;
  if (!ValidateUrl(params->details.url, &url))
    return false;

  HistoryService* hs =
      HistoryServiceFactory::GetForProfile(profile(),
                                           Profile::EXPLICIT_ACCESS);
  hs->AddPage(url, base::Time::Now(), history::SOURCE_EXTENSION);

  SendResponse(true);
  return true;
}

bool HistoryDeleteUrlFunction::RunImpl() {
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

bool HistoryDeleteRangeFunction::RunAsyncImpl() {
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
      base::Bind(&HistoryDeleteRangeFunction::DeleteComplete,
                 base::Unretained(this)),
      &task_tracker_);

  return true;
}

void HistoryDeleteRangeFunction::DeleteComplete() {
  SendAsyncResponse();
}

bool HistoryDeleteAllFunction::RunAsyncImpl() {
  std::set<GURL> restrict_urls;
  HistoryService* hs =
      HistoryServiceFactory::GetForProfile(profile(),
                                           Profile::EXPLICIT_ACCESS);
  hs->ExpireHistoryBetween(
      restrict_urls,
      base::Time::UnixEpoch(),     // From the beginning of the epoch.
      base::Time::Now(),           // To the current time.
      base::Bind(&HistoryDeleteAllFunction::DeleteComplete,
                 base::Unretained(this)),
      &task_tracker_);

  return true;
}

void HistoryDeleteAllFunction::DeleteComplete() {
  SendAsyncResponse();
}

}  // namespace extensions
