// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/url_blacklist_manager.h"

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/browser/browser_thread.h"
#include "content/common/notification_details.h"
#include "content/common/notification_source.h"
#include "googleurl/src/gurl.h"

namespace policy {

namespace {

// Time to wait before starting an update of the blacklist. Scheduling another
// update during this period will reset the timer.
const int64 kUpdateDelayMs = 1000;

// Maximum filters per policy. Filters over this index are ignored.
const size_t kMaxFiltersPerPolicy = 100;

typedef std::vector<std::string> StringVector;

StringVector* ListValueToStringVector(const base::ListValue* list) {
  StringVector* vector = new StringVector;

  if (!list)
    return vector;

  vector->reserve(list->GetSize());
  std::string s;
  for (base::ListValue::const_iterator it = list->begin();
       it != list->end() && vector->size() < kMaxFiltersPerPolicy; ++it) {
    if ((*it)->GetAsString(&s))
      vector->push_back(s);
  }

  return vector;
}

// A task that builds the blacklist on the FILE thread. Takes ownership
// of |block| and |allow| but not of |blacklist|.
void BuildBlacklist(URLBlacklist* blacklist,
                    StringVector* block,
                    StringVector* allow) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  scoped_ptr<StringVector> scoped_block(block);
  scoped_ptr<StringVector> scoped_allow(allow);

  // TODO(joaodasilva): This is a work in progress. http://crbug.com/49612
  // Builds |blacklist| using the filters in |block| and |allow|.
}

// A task that owns the URLBlacklist, and passes it to the URLBlacklistManager
// on the IO thread, if the URLBlacklistManager still exists.
void SetBlacklistOnIO(base::WeakPtr<URLBlacklistManager> blacklist_manager,
                      URLBlacklist* blacklist) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (blacklist_manager)
    blacklist_manager->SetBlacklist(blacklist);
  else
    delete blacklist;
}

}  // namespace

URLBlacklist::URLBlacklist() {
}

URLBlacklist::~URLBlacklist() {
}

URLBlacklistManager::URLBlacklistManager(PrefService* pref_service)
    : ALLOW_THIS_IN_INITIALIZER_LIST(ui_method_factory_(this)),
      pref_service_(pref_service),
      ALLOW_THIS_IN_INITIALIZER_LIST(io_weak_ptr_factory_(this)),
      blacklist_(new URLBlacklist) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(prefs::kUrlBlacklist, this);
  pref_change_registrar_.Add(prefs::kUrlWhitelist, this);

  // Start enforcing the policies without a delay when they are present at
  // startup.
  if (pref_service_->HasPrefPath(prefs::kUrlBlacklist))
    Update();
}

void URLBlacklistManager::ShutdownOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Cancel any pending updates, and stop listening for pref change updates.
  ui_method_factory_.RevokeAll();
  pref_change_registrar_.RemoveAll();
}

URLBlacklistManager::~URLBlacklistManager() {
}

void URLBlacklistManager::Observe(int type,
                                  const NotificationSource& source,
                                  const NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(type == chrome::NOTIFICATION_PREF_CHANGED);
  PrefService* prefs = Source<PrefService>(source).ptr();
  DCHECK(prefs == pref_service_);
  std::string* pref_name = Details<std::string>(details).ptr();
  DCHECK(*pref_name == prefs::kUrlBlacklist ||
         *pref_name == prefs::kUrlWhitelist);
  ScheduleUpdate();
}

void URLBlacklistManager::ScheduleUpdate() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Cancel pending updates, if any.
  ui_method_factory_.RevokeAll();
  PostUpdateTask(
      ui_method_factory_.NewRunnableMethod(&URLBlacklistManager::Update));
}

void URLBlacklistManager::PostUpdateTask(Task* task) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // This is overridden in tests to post the task without the delay.
  MessageLoop::current()->PostDelayedTask(FROM_HERE, task, kUpdateDelayMs);
}

void URLBlacklistManager::Update() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // The preferences can only be read on the UI thread.
  StringVector* block =
      ListValueToStringVector(pref_service_->GetList(prefs::kUrlBlacklist));
  StringVector* allow =
      ListValueToStringVector(pref_service_->GetList(prefs::kUrlWhitelist));

  // Go through the IO thread to grab a WeakPtr to |this|. This is safe from
  // here, since this task will always execute before a potential deletion of
  // ProfileIOData on IO.
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&URLBlacklistManager::UpdateOnIO,
                                     base::Unretained(this), block, allow));
}

void URLBlacklistManager::UpdateOnIO(StringVector* block, StringVector* allow) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  URLBlacklist* blacklist = new URLBlacklist;
  // The URLBlacklist is built on the FILE thread. Once it's ready, it is passed
  // to the URLBlacklistManager on IO.
  // |blacklist|, |block| and |allow| can leak on the unlikely event of a
  // policy update and shutdown happening at the same time.
  BrowserThread::PostTaskAndReply(BrowserThread::FILE, FROM_HERE,
                                  base::Bind(&BuildBlacklist,
                                             blacklist, block, allow),
                                  base::Bind(&SetBlacklistOnIO,
                                             io_weak_ptr_factory_.GetWeakPtr(),
                                             blacklist));
}

void URLBlacklistManager::SetBlacklist(URLBlacklist* blacklist) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  blacklist_.reset(blacklist);
}

bool URLBlacklistManager::IsURLBlocked(const GURL& url) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // TODO(joaodasilva): this is a work in progress. http://crbug.com/49612
  return false;
}

// static
void URLBlacklistManager::RegisterPrefs(PrefService* pref_service) {
  pref_service->RegisterListPref(prefs::kUrlBlacklist,
                                 PrefService::UNSYNCABLE_PREF);
  pref_service->RegisterListPref(prefs::kUrlWhitelist,
                                 PrefService::UNSYNCABLE_PREF);
}

}  // namespace policy
