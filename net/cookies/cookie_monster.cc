// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Portions of this code based on Mozilla:
//   (netwerk/cookie/src/nsCookieService.cpp)
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Daniel Witte (dwitte@stanford.edu)
 *   Michiel van Leeuwen (mvl@exedo.nl)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "net/cookies/cookie_monster.h"

#include <algorithm>
#include <functional>
#include <set>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/profiler/scoped_tracker.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/thread_task_runner_handle.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_util.h"
#include "net/cookies/parsed_cookie.h"
#include "url/origin.h"

using base::Time;
using base::TimeDelta;
using base::TimeTicks;

// In steady state, most cookie requests can be satisfied by the in memory
// cookie monster store. If the cookie request cannot be satisfied by the in
// memory store, the relevant cookies must be fetched from the persistent
// store. The task is queued in CookieMonster::tasks_pending_ if it requires
// all cookies to be loaded from the backend, or tasks_pending_for_key_ if it
// only requires all cookies associated with an eTLD+1.
//
// On the browser critical paths (e.g. for loading initial web pages in a
// session restore) it may take too long to wait for the full load. If a cookie
// request is for a specific URL, DoCookieTaskForURL is called, which triggers a
// priority load if the key is not loaded yet by calling PersistentCookieStore
// :: LoadCookiesForKey. The request is queued in
// CookieMonster::tasks_pending_for_key_ and executed upon receiving
// notification of key load completion via CookieMonster::OnKeyLoaded(). If
// multiple requests for the same eTLD+1 are received before key load
// completion, only the first request calls
// PersistentCookieStore::LoadCookiesForKey, all subsequent requests are queued
// in CookieMonster::tasks_pending_for_key_ and executed upon receiving
// notification of key load completion triggered by the first request for the
// same eTLD+1.

static const int kMinutesInTenYears = 10 * 365 * 24 * 60;

namespace {

const char kFetchWhenNecessaryName[] = "FetchWhenNecessary";
const char kAlwaysFetchName[] = "AlwaysFetch";
const char kCookieMonsterFetchStrategyName[] = "CookieMonsterFetchStrategy";

}  // namespace

namespace net {

// See comments at declaration of these variables in cookie_monster.h
// for details.
const size_t CookieMonster::kDomainMaxCookies = 180;
const size_t CookieMonster::kDomainPurgeCookies = 30;
const size_t CookieMonster::kMaxCookies = 3300;
const size_t CookieMonster::kPurgeCookies = 300;

const size_t CookieMonster::kDomainCookiesQuotaLow = 30;
const size_t CookieMonster::kDomainCookiesQuotaMedium = 50;
const size_t CookieMonster::kDomainCookiesQuotaHigh =
    kDomainMaxCookies - kDomainPurgeCookies - kDomainCookiesQuotaLow -
    kDomainCookiesQuotaMedium;

const int CookieMonster::kSafeFromGlobalPurgeDays = 30;

namespace {

bool ContainsControlCharacter(const std::string& s) {
  for (std::string::const_iterator i = s.begin(); i != s.end(); ++i) {
    if ((*i >= 0) && (*i <= 31))
      return true;
  }

  return false;
}

typedef std::vector<CanonicalCookie*> CanonicalCookieVector;

// Default minimum delay after updating a cookie's LastAccessDate before we
// will update it again.
const int kDefaultAccessUpdateThresholdSeconds = 60;

// Comparator to sort cookies from highest creation date to lowest
// creation date.
struct OrderByCreationTimeDesc {
  bool operator()(const CookieMonster::CookieMap::iterator& a,
                  const CookieMonster::CookieMap::iterator& b) const {
    return a->second->CreationDate() > b->second->CreationDate();
  }
};

// Constants for use in VLOG
const int kVlogPerCookieMonster = 1;
const int kVlogGarbageCollection = 5;
const int kVlogSetCookies = 7;
const int kVlogGetCookies = 9;

// Mozilla sorts on the path length (longest first), and then it
// sorts by creation time (oldest first).
// The RFC says the sort order for the domain attribute is undefined.
bool CookieSorter(CanonicalCookie* cc1, CanonicalCookie* cc2) {
  if (cc1->Path().length() == cc2->Path().length())
    return cc1->CreationDate() < cc2->CreationDate();
  return cc1->Path().length() > cc2->Path().length();
}

bool LRACookieSorter(const CookieMonster::CookieMap::iterator& it1,
                     const CookieMonster::CookieMap::iterator& it2) {
  // Cookies accessed less recently should be deleted first.
  if (it1->second->LastAccessDate() != it2->second->LastAccessDate())
    return it1->second->LastAccessDate() < it2->second->LastAccessDate();

  // In rare cases we might have two cookies with identical last access times.
  // To preserve the stability of the sort, in these cases prefer to delete
  // older cookies over newer ones.  CreationDate() is guaranteed to be unique.
  return it1->second->CreationDate() < it2->second->CreationDate();
}

// Compare cookies using name, domain and path, so that "equivalent" cookies
// (per RFC 2965) are equal to each other.
bool PartialDiffCookieSorter(const CanonicalCookie& a,
                             const CanonicalCookie& b) {
  return a.PartialCompare(b);
}

// This is a stricter ordering than PartialDiffCookieOrdering, where all fields
// are used.
bool FullDiffCookieSorter(const CanonicalCookie& a, const CanonicalCookie& b) {
  return a.FullCompare(b);
}

// Our strategy to find duplicates is:
// (1) Build a map from (cookiename, cookiepath) to
//     {list of cookies with this signature, sorted by creation time}.
// (2) For each list with more than 1 entry, keep the cookie having the
//     most recent creation time, and delete the others.
//
// Two cookies are considered equivalent if they have the same domain,
// name, and path.
struct CookieSignature {
 public:
  CookieSignature(const std::string& name,
                  const std::string& domain,
                  const std::string& path)
      : name(name), domain(domain), path(path) {}

  // To be a key for a map this class needs to be assignable, copyable,
  // and have an operator<.  The default assignment operator
  // and copy constructor are exactly what we want.

  bool operator<(const CookieSignature& cs) const {
    // Name compare dominates, then domain, then path.
    int diff = name.compare(cs.name);
    if (diff != 0)
      return diff < 0;

    diff = domain.compare(cs.domain);
    if (diff != 0)
      return diff < 0;

    return path.compare(cs.path) < 0;
  }

  std::string name;
  std::string domain;
  std::string path;
};

// For a CookieItVector iterator range [|it_begin|, |it_end|),
// sorts the first |num_sort| + 1 elements by LastAccessDate().
// The + 1 element exists so for any interval of length <= |num_sort| starting
// from |cookies_its_begin|, a LastAccessDate() bound can be found.
void SortLeastRecentlyAccessed(CookieMonster::CookieItVector::iterator it_begin,
                               CookieMonster::CookieItVector::iterator it_end,
                               size_t num_sort) {
  DCHECK_LT(static_cast<int>(num_sort), it_end - it_begin);
  std::partial_sort(it_begin, it_begin + num_sort + 1, it_end, LRACookieSorter);
}

// Given a single cookie vector |cookie_its|, pushs all of the secure cookies in
// |cookie_its| into |secure_cookie_its| and all of the non-secure cookies into
// |non_secure_cookie_its|. Both |secure_cookie_its| and |non_secure_cookie_its|
// must be non-NULL.
void SplitCookieVectorIntoSecureAndNonSecure(
    const CookieMonster::CookieItVector& cookie_its,
    CookieMonster::CookieItVector* secure_cookie_its,
    CookieMonster::CookieItVector* non_secure_cookie_its) {
  DCHECK(secure_cookie_its && non_secure_cookie_its);
  for (const auto& curit : cookie_its) {
    if (curit->second->IsSecure())
      secure_cookie_its->push_back(curit);
    else
      non_secure_cookie_its->push_back(curit);
  }
}

// Predicate to support PartitionCookieByPriority().
struct CookiePriorityEqualsTo
    : std::unary_function<const CookieMonster::CookieMap::iterator, bool> {
  explicit CookiePriorityEqualsTo(CookiePriority priority)
      : priority_(priority) {}

  bool operator()(const CookieMonster::CookieMap::iterator it) const {
    return it->second->Priority() == priority_;
  }

  const CookiePriority priority_;
};

// For a CookieItVector iterator range [|it_begin|, |it_end|),
// moves all cookies with a given |priority| to the beginning of the list.
// Returns: An iterator in [it_begin, it_end) to the first element with
// priority != |priority|, or |it_end| if all have priority == |priority|.
CookieMonster::CookieItVector::iterator PartitionCookieByPriority(
    CookieMonster::CookieItVector::iterator it_begin,
    CookieMonster::CookieItVector::iterator it_end,
    CookiePriority priority) {
  return std::partition(it_begin, it_end, CookiePriorityEqualsTo(priority));
}

bool LowerBoundAccessDateComparator(const CookieMonster::CookieMap::iterator it,
                                    const Time& access_date) {
  return it->second->LastAccessDate() < access_date;
}

// For a CookieItVector iterator range [|it_begin|, |it_end|)
// from a CookieItVector sorted by LastAccessDate(), returns the
// first iterator with access date >= |access_date|, or cookie_its_end if this
// holds for all.
CookieMonster::CookieItVector::iterator LowerBoundAccessDate(
    const CookieMonster::CookieItVector::iterator its_begin,
    const CookieMonster::CookieItVector::iterator its_end,
    const Time& access_date) {
  return std::lower_bound(its_begin, its_end, access_date,
                          LowerBoundAccessDateComparator);
}

// Mapping between DeletionCause and CookieMonsterDelegate::ChangeCause; the
// mapping also provides a boolean that specifies whether or not an
// OnCookieChanged notification ought to be generated.
typedef struct ChangeCausePair_struct {
  CookieMonsterDelegate::ChangeCause cause;
  bool notify;
} ChangeCausePair;
ChangeCausePair ChangeCauseMapping[] = {
    // DELETE_COOKIE_EXPLICIT
    {CookieMonsterDelegate::CHANGE_COOKIE_EXPLICIT, true},
    // DELETE_COOKIE_OVERWRITE
    {CookieMonsterDelegate::CHANGE_COOKIE_OVERWRITE, true},
    // DELETE_COOKIE_EXPIRED
    {CookieMonsterDelegate::CHANGE_COOKIE_EXPIRED, true},
    // DELETE_COOKIE_EVICTED
    {CookieMonsterDelegate::CHANGE_COOKIE_EVICTED, true},
    // DELETE_COOKIE_DUPLICATE_IN_BACKING_STORE
    {CookieMonsterDelegate::CHANGE_COOKIE_EXPLICIT, false},
    // DELETE_COOKIE_DONT_RECORD
    {CookieMonsterDelegate::CHANGE_COOKIE_EXPLICIT, false},
    // DELETE_COOKIE_EVICTED_DOMAIN
    {CookieMonsterDelegate::CHANGE_COOKIE_EVICTED, true},
    // DELETE_COOKIE_EVICTED_GLOBAL
    {CookieMonsterDelegate::CHANGE_COOKIE_EVICTED, true},
    // DELETE_COOKIE_EVICTED_DOMAIN_PRE_SAFE
    {CookieMonsterDelegate::CHANGE_COOKIE_EVICTED, true},
    // DELETE_COOKIE_EVICTED_DOMAIN_POST_SAFE
    {CookieMonsterDelegate::CHANGE_COOKIE_EVICTED, true},
    // DELETE_COOKIE_EXPIRED_OVERWRITE
    {CookieMonsterDelegate::CHANGE_COOKIE_EXPIRED_OVERWRITE, true},
    // DELETE_COOKIE_CONTROL_CHAR
    {CookieMonsterDelegate::CHANGE_COOKIE_EVICTED, true},
    // DELETE_COOKIE_NON_SECURE
    {CookieMonsterDelegate::CHANGE_COOKIE_EVICTED, true},
    // DELETE_COOKIE_LAST_ENTRY
    {CookieMonsterDelegate::CHANGE_COOKIE_EXPLICIT, false}};

std::string BuildCookieLine(const CanonicalCookieVector& cookies) {
  std::string cookie_line;
  for (CanonicalCookieVector::const_iterator it = cookies.begin();
       it != cookies.end(); ++it) {
    if (it != cookies.begin())
      cookie_line += "; ";
    // In Mozilla if you set a cookie like AAAA, it will have an empty token
    // and a value of AAAA.  When it sends the cookie back, it will send AAAA,
    // so we need to avoid sending =AAAA for a blank token value.
    if (!(*it)->Name().empty())
      cookie_line += (*it)->Name() + "=";
    cookie_line += (*it)->Value();
  }
  return cookie_line;
}

void RunAsync(scoped_refptr<base::TaskRunner> proxy,
              const CookieStore::CookieChangedCallback& callback,
              const CanonicalCookie& cookie,
              bool removed) {
  proxy->PostTask(FROM_HERE, base::Bind(callback, cookie, removed));
}

}  // namespace

CookieMonster::CookieMonster(PersistentCookieStore* store,
                             CookieMonsterDelegate* delegate)
    : CookieMonster(store, delegate, kDefaultAccessUpdateThresholdSeconds) {
}

CookieMonster::CookieMonster(PersistentCookieStore* store,
                             CookieMonsterDelegate* delegate,
                             int last_access_threshold_milliseconds)
    : initialized_(false),
      started_fetching_all_cookies_(false),
      finished_fetching_all_cookies_(false),
      fetch_strategy_(kUnknownFetch),
      store_(store),
      last_access_threshold_(base::TimeDelta::FromMilliseconds(
          last_access_threshold_milliseconds)),
      delegate_(delegate),
      last_statistic_record_time_(base::Time::Now()),
      keep_expired_cookies_(false),
      persist_session_cookies_(false) {
  InitializeHistograms();
  cookieable_schemes_.insert(
      cookieable_schemes_.begin(), kDefaultCookieableSchemes,
      kDefaultCookieableSchemes + kDefaultCookieableSchemesCount);
}

bool CookieMonster::ImportCookies(const CookieList& list) {
  base::AutoLock autolock(lock_);
  MarkCookieStoreAsInitialized();
  if (ShouldFetchAllCookiesWhenFetchingAnyCookie())
    FetchAllCookiesIfNecessary();
  for (CookieList::const_iterator iter = list.begin(); iter != list.end();
       ++iter) {
    scoped_ptr<CanonicalCookie> cookie(new CanonicalCookie(*iter));
    CookieOptions options;
    options.set_include_httponly();
    options.set_include_same_site();
    if (!SetCanonicalCookie(std::move(cookie), options))
      return false;
  }
  return true;
}

// Task classes for queueing the coming request.

class CookieMonster::CookieMonsterTask
    : public base::RefCountedThreadSafe<CookieMonsterTask> {
 public:
  // Runs the task and invokes the client callback on the thread that
  // originally constructed the task.
  virtual void Run() = 0;

 protected:
  explicit CookieMonsterTask(CookieMonster* cookie_monster);
  virtual ~CookieMonsterTask();

  // Invokes the callback immediately, if the current thread is the one
  // that originated the task, or queues the callback for execution on the
  // appropriate thread. Maintains a reference to this CookieMonsterTask
  // instance until the callback completes.
  void InvokeCallback(base::Closure callback);

  CookieMonster* cookie_monster() { return cookie_monster_; }

 private:
  friend class base::RefCountedThreadSafe<CookieMonsterTask>;

  CookieMonster* cookie_monster_;
  scoped_refptr<base::SingleThreadTaskRunner> thread_;

  DISALLOW_COPY_AND_ASSIGN(CookieMonsterTask);
};

CookieMonster::CookieMonsterTask::CookieMonsterTask(
    CookieMonster* cookie_monster)
    : cookie_monster_(cookie_monster),
      thread_(base::ThreadTaskRunnerHandle::Get()) {
}

CookieMonster::CookieMonsterTask::~CookieMonsterTask() {
}

// Unfortunately, one cannot re-bind a Callback with parameters into a closure.
// Therefore, the closure passed to InvokeCallback is a clumsy binding of
// Callback::Run on a wrapped Callback instance. Since Callback is not
// reference counted, we bind to an instance that is a member of the
// CookieMonsterTask subclass. Then, we cannot simply post the callback to a
// message loop because the underlying instance may be destroyed (along with the
// CookieMonsterTask instance) in the interim. Therefore, we post a callback
// bound to the CookieMonsterTask, which *is* reference counted (thus preventing
// destruction of the original callback), and which invokes the closure (which
// invokes the original callback with the returned data).
void CookieMonster::CookieMonsterTask::InvokeCallback(base::Closure callback) {
  if (thread_->BelongsToCurrentThread()) {
    callback.Run();
  } else {
    thread_->PostTask(FROM_HERE, base::Bind(&CookieMonsterTask::InvokeCallback,
                                            this, callback));
  }
}

// Task class for SetCookieWithDetails call.
class CookieMonster::SetCookieWithDetailsTask : public CookieMonsterTask {
 public:
  SetCookieWithDetailsTask(CookieMonster* cookie_monster,
                           const GURL& url,
                           const std::string& name,
                           const std::string& value,
                           const std::string& domain,
                           const std::string& path,
                           const base::Time creation_time,
                           const base::Time expiration_time,
                           bool secure,
                           bool http_only,
                           bool same_site,
                           bool enforce_strict_secure,
                           CookiePriority priority,
                           const SetCookiesCallback& callback)
      : CookieMonsterTask(cookie_monster),
        url_(url),
        name_(name),
        value_(value),
        domain_(domain),
        path_(path),
        creation_time_(creation_time),
        expiration_time_(expiration_time),
        secure_(secure),
        http_only_(http_only),
        same_site_(same_site),
        enforce_strict_secure_(enforce_strict_secure),
        priority_(priority),
        callback_(callback) {}

  // CookieMonsterTask:
  void Run() override;

 protected:
  ~SetCookieWithDetailsTask() override {}

 private:
  GURL url_;
  std::string name_;
  std::string value_;
  std::string domain_;
  std::string path_;
  base::Time creation_time_;
  base::Time expiration_time_;
  bool secure_;
  bool http_only_;
  bool same_site_;
  bool enforce_strict_secure_;
  CookiePriority priority_;
  SetCookiesCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(SetCookieWithDetailsTask);
};

void CookieMonster::SetCookieWithDetailsTask::Run() {
  bool success = this->cookie_monster()->SetCookieWithDetails(
      url_, name_, value_, domain_, path_, creation_time_, expiration_time_,
      secure_, http_only_, same_site_, enforce_strict_secure_, priority_);
  if (!callback_.is_null()) {
    this->InvokeCallback(base::Bind(&SetCookiesCallback::Run,
                                    base::Unretained(&callback_), success));
  }
}

// Task class for GetAllCookies call.
class CookieMonster::GetAllCookiesTask : public CookieMonsterTask {
 public:
  GetAllCookiesTask(CookieMonster* cookie_monster,
                    const GetCookieListCallback& callback)
      : CookieMonsterTask(cookie_monster), callback_(callback) {}

  // CookieMonsterTask
  void Run() override;

 protected:
  ~GetAllCookiesTask() override {}

 private:
  GetCookieListCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(GetAllCookiesTask);
};

void CookieMonster::GetAllCookiesTask::Run() {
  if (!callback_.is_null()) {
    CookieList cookies = this->cookie_monster()->GetAllCookies();
    this->InvokeCallback(base::Bind(&GetCookieListCallback::Run,
                                    base::Unretained(&callback_), cookies));
  }
}

// Task class for GetAllCookiesForURLWithOptions call.
class CookieMonster::GetAllCookiesForURLWithOptionsTask
    : public CookieMonsterTask {
 public:
  GetAllCookiesForURLWithOptionsTask(CookieMonster* cookie_monster,
                                     const GURL& url,
                                     const CookieOptions& options,
                                     const GetCookieListCallback& callback)
      : CookieMonsterTask(cookie_monster),
        url_(url),
        options_(options),
        callback_(callback) {}

  // CookieMonsterTask:
  void Run() override;

 protected:
  ~GetAllCookiesForURLWithOptionsTask() override {}

 private:
  GURL url_;
  CookieOptions options_;
  GetCookieListCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(GetAllCookiesForURLWithOptionsTask);
};

void CookieMonster::GetAllCookiesForURLWithOptionsTask::Run() {
  if (!callback_.is_null()) {
    CookieList cookies =
        this->cookie_monster()->GetAllCookiesForURLWithOptions(url_, options_);
    this->InvokeCallback(base::Bind(&GetCookieListCallback::Run,
                                    base::Unretained(&callback_), cookies));
  }
}

template <typename Result>
struct CallbackType {
  typedef base::Callback<void(Result)> Type;
};

template <>
struct CallbackType<void> {
  typedef base::Closure Type;
};

// Base task class for Delete*Task.
template <typename Result>
class CookieMonster::DeleteTask : public CookieMonsterTask {
 public:
  DeleteTask(CookieMonster* cookie_monster,
             const typename CallbackType<Result>::Type& callback)
      : CookieMonsterTask(cookie_monster), callback_(callback) {}

  // CookieMonsterTask:
  void Run() override;

 protected:
  ~DeleteTask() override;

 private:
  // Runs the delete task and returns a result.
  virtual Result RunDeleteTask() = 0;
  base::Closure RunDeleteTaskAndBindCallback();
  void FlushDone(const base::Closure& callback);

  typename CallbackType<Result>::Type callback_;

  DISALLOW_COPY_AND_ASSIGN(DeleteTask);
};

template <typename Result>
CookieMonster::DeleteTask<Result>::~DeleteTask() {
}

template <typename Result>
base::Closure
CookieMonster::DeleteTask<Result>::RunDeleteTaskAndBindCallback() {
  Result result = RunDeleteTask();
  if (callback_.is_null())
    return base::Closure();
  return base::Bind(callback_, result);
}

template <>
base::Closure CookieMonster::DeleteTask<void>::RunDeleteTaskAndBindCallback() {
  RunDeleteTask();
  return callback_;
}

template <typename Result>
void CookieMonster::DeleteTask<Result>::Run() {
  this->cookie_monster()->FlushStore(base::Bind(
      &DeleteTask<Result>::FlushDone, this, RunDeleteTaskAndBindCallback()));
}

template <typename Result>
void CookieMonster::DeleteTask<Result>::FlushDone(
    const base::Closure& callback) {
  if (!callback.is_null()) {
    this->InvokeCallback(callback);
  }
}

// Task class for DeleteAllCreatedBetween call.
class CookieMonster::DeleteAllCreatedBetweenTask : public DeleteTask<int> {
 public:
  DeleteAllCreatedBetweenTask(CookieMonster* cookie_monster,
                              const Time& delete_begin,
                              const Time& delete_end,
                              const DeleteCallback& callback)
      : DeleteTask<int>(cookie_monster, callback),
        delete_begin_(delete_begin),
        delete_end_(delete_end) {}

  // DeleteTask:
  int RunDeleteTask() override;

 protected:
  ~DeleteAllCreatedBetweenTask() override {}

 private:
  Time delete_begin_;
  Time delete_end_;

  DISALLOW_COPY_AND_ASSIGN(DeleteAllCreatedBetweenTask);
};

int CookieMonster::DeleteAllCreatedBetweenTask::RunDeleteTask() {
  return this->cookie_monster()->DeleteAllCreatedBetween(delete_begin_,
                                                         delete_end_);
}

// Task class for DeleteAllCreatedBetweenForHost call.
class CookieMonster::DeleteAllCreatedBetweenForHostTask
    : public DeleteTask<int> {
 public:
  DeleteAllCreatedBetweenForHostTask(CookieMonster* cookie_monster,
                                     Time delete_begin,
                                     Time delete_end,
                                     const GURL& url,
                                     const DeleteCallback& callback)
      : DeleteTask<int>(cookie_monster, callback),
        delete_begin_(delete_begin),
        delete_end_(delete_end),
        url_(url) {}

  // DeleteTask:
  int RunDeleteTask() override;

 protected:
  ~DeleteAllCreatedBetweenForHostTask() override {}

 private:
  Time delete_begin_;
  Time delete_end_;
  GURL url_;

  DISALLOW_COPY_AND_ASSIGN(DeleteAllCreatedBetweenForHostTask);
};

int CookieMonster::DeleteAllCreatedBetweenForHostTask::RunDeleteTask() {
  return this->cookie_monster()->DeleteAllCreatedBetweenForHost(
      delete_begin_, delete_end_, url_);
}

// Task class for DeleteCanonicalCookie call.
class CookieMonster::DeleteCanonicalCookieTask : public DeleteTask<bool> {
 public:
  DeleteCanonicalCookieTask(CookieMonster* cookie_monster,
                            const CanonicalCookie& cookie,
                            const DeleteCookieCallback& callback)
      : DeleteTask<bool>(cookie_monster, callback), cookie_(cookie) {}

  // DeleteTask:
  bool RunDeleteTask() override;

 protected:
  ~DeleteCanonicalCookieTask() override {}

 private:
  CanonicalCookie cookie_;

  DISALLOW_COPY_AND_ASSIGN(DeleteCanonicalCookieTask);
};

bool CookieMonster::DeleteCanonicalCookieTask::RunDeleteTask() {
  return this->cookie_monster()->DeleteCanonicalCookie(cookie_);
}

// Task class for SetCookieWithOptions call.
class CookieMonster::SetCookieWithOptionsTask : public CookieMonsterTask {
 public:
  SetCookieWithOptionsTask(CookieMonster* cookie_monster,
                           const GURL& url,
                           const std::string& cookie_line,
                           const CookieOptions& options,
                           const SetCookiesCallback& callback)
      : CookieMonsterTask(cookie_monster),
        url_(url),
        cookie_line_(cookie_line),
        options_(options),
        callback_(callback) {}

  // CookieMonsterTask:
  void Run() override;

 protected:
  ~SetCookieWithOptionsTask() override {}

 private:
  GURL url_;
  std::string cookie_line_;
  CookieOptions options_;
  SetCookiesCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(SetCookieWithOptionsTask);
};

void CookieMonster::SetCookieWithOptionsTask::Run() {
  bool result = this->cookie_monster()->SetCookieWithOptions(url_, cookie_line_,
                                                             options_);
  if (!callback_.is_null()) {
    this->InvokeCallback(base::Bind(&SetCookiesCallback::Run,
                                    base::Unretained(&callback_), result));
  }
}

// Task class for SetAllCookies call.
class CookieMonster::SetAllCookiesTask : public CookieMonsterTask {
 public:
  SetAllCookiesTask(CookieMonster* cookie_monster,
                    const CookieList& list,
                    const SetCookiesCallback& callback)
      : CookieMonsterTask(cookie_monster), list_(list), callback_(callback) {}

  // CookieMonsterTask:
  void Run() override;

 protected:
  ~SetAllCookiesTask() override {}

 private:
  CookieList list_;
  SetCookiesCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(SetAllCookiesTask);
};

void CookieMonster::SetAllCookiesTask::Run() {
  CookieList positive_diff;
  CookieList negative_diff;
  CookieList old_cookies = this->cookie_monster()->GetAllCookies();
  this->cookie_monster()->ComputeCookieDiff(&old_cookies, &list_,
                                            &positive_diff, &negative_diff);

  for (CookieList::const_iterator it = negative_diff.begin();
       it != negative_diff.end(); ++it) {
    this->cookie_monster()->DeleteCanonicalCookie(*it);
  }

  bool result = true;
  if (positive_diff.size() > 0)
    result = this->cookie_monster()->SetCanonicalCookies(list_);

  if (!callback_.is_null()) {
    this->InvokeCallback(base::Bind(&SetCookiesCallback::Run,
                                    base::Unretained(&callback_), result));
  }
}

// Task class for GetCookiesWithOptions call.
class CookieMonster::GetCookiesWithOptionsTask : public CookieMonsterTask {
 public:
  GetCookiesWithOptionsTask(CookieMonster* cookie_monster,
                            const GURL& url,
                            const CookieOptions& options,
                            const GetCookiesCallback& callback)
      : CookieMonsterTask(cookie_monster),
        url_(url),
        options_(options),
        callback_(callback) {}

  // CookieMonsterTask:
  void Run() override;

 protected:
  ~GetCookiesWithOptionsTask() override {}

 private:
  GURL url_;
  CookieOptions options_;
  GetCookiesCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(GetCookiesWithOptionsTask);
};

void CookieMonster::GetCookiesWithOptionsTask::Run() {
  // TODO(mkwst): Remove ScopedTracker below once crbug.com/456373 is fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "456373 CookieMonster::GetCookiesWithOptionsTask::Run"));
  std::string cookie =
      this->cookie_monster()->GetCookiesWithOptions(url_, options_);
  if (!callback_.is_null()) {
    this->InvokeCallback(base::Bind(&GetCookiesCallback::Run,
                                    base::Unretained(&callback_), cookie));
  }
}

// Task class for DeleteCookie call.
class CookieMonster::DeleteCookieTask : public DeleteTask<void> {
 public:
  DeleteCookieTask(CookieMonster* cookie_monster,
                   const GURL& url,
                   const std::string& cookie_name,
                   const base::Closure& callback)
      : DeleteTask<void>(cookie_monster, callback),
        url_(url),
        cookie_name_(cookie_name) {}

  // DeleteTask:
  void RunDeleteTask() override;

 protected:
  ~DeleteCookieTask() override {}

 private:
  GURL url_;
  std::string cookie_name_;

  DISALLOW_COPY_AND_ASSIGN(DeleteCookieTask);
};

void CookieMonster::DeleteCookieTask::RunDeleteTask() {
  this->cookie_monster()->DeleteCookie(url_, cookie_name_);
}

// Task class for DeleteSessionCookies call.
class CookieMonster::DeleteSessionCookiesTask : public DeleteTask<int> {
 public:
  DeleteSessionCookiesTask(CookieMonster* cookie_monster,
                           const DeleteCallback& callback)
      : DeleteTask<int>(cookie_monster, callback) {}

  // DeleteTask:
  int RunDeleteTask() override;

 protected:
  ~DeleteSessionCookiesTask() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(DeleteSessionCookiesTask);
};

int CookieMonster::DeleteSessionCookiesTask::RunDeleteTask() {
  return this->cookie_monster()->DeleteSessionCookies();
}

// Asynchronous CookieMonster API

void CookieMonster::SetCookieWithDetailsAsync(
    const GURL& url,
    const std::string& name,
    const std::string& value,
    const std::string& domain,
    const std::string& path,
    const Time creation_time,
    const Time expiration_time,
    bool secure,
    bool http_only,
    bool same_site,
    bool enforce_strict_secure,
    CookiePriority priority,
    const SetCookiesCallback& callback) {
  scoped_refptr<SetCookieWithDetailsTask> task = new SetCookieWithDetailsTask(
      this, url, name, value, domain, path, creation_time, expiration_time,
      secure, http_only, same_site, enforce_strict_secure, priority, callback);
  DoCookieTaskForURL(task, url);
}

void CookieMonster::GetAllCookiesForURLWithOptionsAsync(
    const GURL& url,
    const CookieOptions& options,
    const GetCookieListCallback& callback) {
  scoped_refptr<GetAllCookiesForURLWithOptionsTask> task =
      new GetAllCookiesForURLWithOptionsTask(this, url, options, callback);

  DoCookieTaskForURL(task, url);
}

void CookieMonster::DeleteCanonicalCookieAsync(
    const CanonicalCookie& cookie,
    const DeleteCookieCallback& callback) {
  scoped_refptr<DeleteCanonicalCookieTask> task =
      new DeleteCanonicalCookieTask(this, cookie, callback);

  DoCookieTask(task);
}

void CookieMonster::FlushStore(const base::Closure& callback) {
  base::AutoLock autolock(lock_);
  if (initialized_ && store_.get())
    store_->Flush(callback);
  else if (!callback.is_null())
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, callback);
}

void CookieMonster::SetAllCookiesAsync(const CookieList& list,
                                       const SetCookiesCallback& callback) {
  scoped_refptr<SetAllCookiesTask> task =
      new SetAllCookiesTask(this, list, callback);
  DoCookieTask(task);
}

void CookieMonster::SetCookieWithOptionsAsync(
    const GURL& url,
    const std::string& cookie_line,
    const CookieOptions& options,
    const SetCookiesCallback& callback) {
  scoped_refptr<SetCookieWithOptionsTask> task =
      new SetCookieWithOptionsTask(this, url, cookie_line, options, callback);

  DoCookieTaskForURL(task, url);
}

void CookieMonster::GetCookiesWithOptionsAsync(
    const GURL& url,
    const CookieOptions& options,
    const GetCookiesCallback& callback) {
  scoped_refptr<GetCookiesWithOptionsTask> task =
      new GetCookiesWithOptionsTask(this, url, options, callback);

  DoCookieTaskForURL(task, url);
}

void CookieMonster::GetAllCookiesForURLAsync(
    const GURL& url,
    const GetCookieListCallback& callback) {
  CookieOptions options;
  options.set_include_httponly();
  options.set_include_same_site();
  scoped_refptr<GetAllCookiesForURLWithOptionsTask> task =
      new GetAllCookiesForURLWithOptionsTask(this, url, options, callback);

  DoCookieTaskForURL(task, url);
}

void CookieMonster::GetAllCookiesAsync(const GetCookieListCallback& callback) {
  scoped_refptr<GetAllCookiesTask> task = new GetAllCookiesTask(this, callback);

  DoCookieTask(task);
}

void CookieMonster::DeleteCookieAsync(const GURL& url,
                                      const std::string& cookie_name,
                                      const base::Closure& callback) {
  scoped_refptr<DeleteCookieTask> task =
      new DeleteCookieTask(this, url, cookie_name, callback);

  DoCookieTaskForURL(task, url);
}

void CookieMonster::DeleteAllCreatedBetweenAsync(
    const Time& delete_begin,
    const Time& delete_end,
    const DeleteCallback& callback) {
  scoped_refptr<DeleteAllCreatedBetweenTask> task =
      new DeleteAllCreatedBetweenTask(this, delete_begin, delete_end, callback);

  DoCookieTask(task);
}

void CookieMonster::DeleteAllCreatedBetweenForHostAsync(
    const Time delete_begin,
    const Time delete_end,
    const GURL& url,
    const DeleteCallback& callback) {
  scoped_refptr<DeleteAllCreatedBetweenForHostTask> task =
      new DeleteAllCreatedBetweenForHostTask(this, delete_begin, delete_end,
                                             url, callback);

  DoCookieTaskForURL(task, url);
}

void CookieMonster::DeleteSessionCookiesAsync(
    const CookieStore::DeleteCallback& callback) {
  scoped_refptr<DeleteSessionCookiesTask> task =
      new DeleteSessionCookiesTask(this, callback);

  DoCookieTask(task);
}

CookieMonster* CookieMonster::GetCookieMonster() {
  return this;
}

void CookieMonster::SetCookieableSchemes(
    const std::vector<std::string>& schemes) {
  base::AutoLock autolock(lock_);

  // Calls to this method will have no effect if made after a WebView or
  // CookieManager instance has been created.
  if (initialized_)
    return;

  cookieable_schemes_ = schemes;
}

void CookieMonster::SetKeepExpiredCookies() {
  keep_expired_cookies_ = true;
}

void CookieMonster::SetForceKeepSessionState() {
  if (store_.get()) {
    store_->SetForceKeepSessionState();
  }
}

// This function must be called before the CookieMonster is used.
void CookieMonster::SetPersistSessionCookies(bool persist_session_cookies) {
  DCHECK(!initialized_);
  persist_session_cookies_ = persist_session_cookies;
}

bool CookieMonster::IsCookieableScheme(const std::string& scheme) {
  base::AutoLock autolock(lock_);

  return std::find(cookieable_schemes_.begin(), cookieable_schemes_.end(),
                   scheme) != cookieable_schemes_.end();
}

const char* const CookieMonster::kDefaultCookieableSchemes[] = {"http", "https",
                                                                "ws", "wss"};
const int CookieMonster::kDefaultCookieableSchemesCount =
    arraysize(kDefaultCookieableSchemes);

scoped_ptr<CookieStore::CookieChangedSubscription>
CookieMonster::AddCallbackForCookie(const GURL& gurl,
                                    const std::string& name,
                                    const CookieChangedCallback& callback) {
  base::AutoLock autolock(lock_);
  std::pair<GURL, std::string> key(gurl, name);
  if (hook_map_.count(key) == 0)
    hook_map_[key] = make_linked_ptr(new CookieChangedCallbackList());
  return hook_map_[key]->Add(
      base::Bind(&RunAsync, base::ThreadTaskRunnerHandle::Get(), callback));
}

CookieMonster::~CookieMonster() {
  // Clean up cookies

  // InternalDeleteCookie expects the lock to be held, even though there can be
  // no contention here.
  base::AutoLock autolock(lock_);
  for (CookieMap::iterator cookie_it = cookies_.begin();
       cookie_it != cookies_.end();) {
    CookieMap::iterator current_cookie_it = cookie_it;
    ++cookie_it;
    InternalDeleteCookie(current_cookie_it, false /* sync_to_store */,
                         DELETE_COOKIE_DONT_RECORD);
  }
}

bool CookieMonster::SetCookieWithDetails(const GURL& url,
                                         const std::string& name,
                                         const std::string& value,
                                         const std::string& domain,
                                         const std::string& path,
                                         const base::Time creation_time,
                                         const base::Time expiration_time,
                                         bool secure,
                                         bool http_only,
                                         bool same_site,
                                         bool enforce_strict_secure,
                                         CookiePriority priority) {
  base::AutoLock autolock(lock_);

  if (!HasCookieableScheme(url))
    return false;

  // TODO(mmenke): This class assumes each cookie to have a unique creation
  // time. Allowing the caller to set the creation time violates that
  // assumption. Worth fixing? Worth noting that time changes between browser
  // restarts can cause the same issue.
  base::Time actual_creation_time = creation_time;
  if (creation_time.is_null()) {
    actual_creation_time = CurrentTime();
    last_time_seen_ = actual_creation_time;
  }

  scoped_ptr<CanonicalCookie> cc(CanonicalCookie::Create(
      url, name, value, domain, path, actual_creation_time, expiration_time,
      secure, http_only, same_site, enforce_strict_secure, priority));

  if (!cc.get())
    return false;

  CookieOptions options;
  options.set_include_httponly();
  options.set_include_same_site();
  if (enforce_strict_secure)
    options.set_enforce_strict_secure();
  return SetCanonicalCookie(std::move(cc), options);
}

CookieList CookieMonster::GetAllCookies() {
  base::AutoLock autolock(lock_);

  // This function is being called to scrape the cookie list for management UI
  // or similar.  We shouldn't show expired cookies in this list since it will
  // just be confusing to users, and this function is called rarely enough (and
  // is already slow enough) that it's OK to take the time to garbage collect
  // the expired cookies now.
  //
  // Note that this does not prune cookies to be below our limits (if we've
  // exceeded them) the way that calling GarbageCollect() would.
  GarbageCollectExpired(
      Time::Now(), CookieMapItPair(cookies_.begin(), cookies_.end()), NULL);

  // Copy the CanonicalCookie pointers from the map so that we can use the same
  // sorter as elsewhere, then copy the result out.
  std::vector<CanonicalCookie*> cookie_ptrs;
  cookie_ptrs.reserve(cookies_.size());
  for (CookieMap::iterator it = cookies_.begin(); it != cookies_.end(); ++it)
    cookie_ptrs.push_back(it->second);
  std::sort(cookie_ptrs.begin(), cookie_ptrs.end(), CookieSorter);

  CookieList cookie_list;
  cookie_list.reserve(cookie_ptrs.size());
  for (std::vector<CanonicalCookie*>::const_iterator it = cookie_ptrs.begin();
       it != cookie_ptrs.end(); ++it)
    cookie_list.push_back(**it);

  return cookie_list;
}

CookieList CookieMonster::GetAllCookiesForURLWithOptions(
    const GURL& url,
    const CookieOptions& options) {
  base::AutoLock autolock(lock_);

  std::vector<CanonicalCookie*> cookie_ptrs;
  FindCookiesForHostAndDomain(url, options, false, &cookie_ptrs);
  std::sort(cookie_ptrs.begin(), cookie_ptrs.end(), CookieSorter);

  CookieList cookies;
  cookies.reserve(cookie_ptrs.size());
  for (std::vector<CanonicalCookie*>::const_iterator it = cookie_ptrs.begin();
       it != cookie_ptrs.end(); it++)
    cookies.push_back(**it);

  return cookies;
}

int CookieMonster::DeleteAllCreatedBetween(const Time& delete_begin,
                                           const Time& delete_end) {
  base::AutoLock autolock(lock_);

  int num_deleted = 0;
  for (CookieMap::iterator it = cookies_.begin(); it != cookies_.end();) {
    CookieMap::iterator curit = it;
    CanonicalCookie* cc = curit->second;
    ++it;

    if (cc->CreationDate() >= delete_begin &&
        (delete_end.is_null() || cc->CreationDate() < delete_end)) {
      InternalDeleteCookie(curit, true, /*sync_to_store*/
                           DELETE_COOKIE_EXPLICIT);
      ++num_deleted;
    }
  }

  return num_deleted;
}

int CookieMonster::DeleteAllCreatedBetweenForHost(const Time delete_begin,
                                                  const Time delete_end,
                                                  const GURL& url) {
  base::AutoLock autolock(lock_);

  if (!HasCookieableScheme(url))
    return 0;

  const std::string host(url.host());

  // We store host cookies in the store by their canonical host name;
  // domain cookies are stored with a leading ".".  So this is a pretty
  // simple lookup and per-cookie delete.
  int num_deleted = 0;
  for (CookieMapItPair its = cookies_.equal_range(GetKey(host));
       its.first != its.second;) {
    CookieMap::iterator curit = its.first;
    ++its.first;

    const CanonicalCookie* const cc = curit->second;

    // Delete only on a match as a host cookie.
    if (cc->IsHostCookie() && cc->IsDomainMatch(host) &&
        cc->CreationDate() >= delete_begin &&
        // The assumption that null |delete_end| is equivalent to
        // Time::Max() is confusing.
        (delete_end.is_null() || cc->CreationDate() < delete_end)) {
      num_deleted++;

      InternalDeleteCookie(curit, true, DELETE_COOKIE_EXPLICIT);
    }
  }
  return num_deleted;
}


bool CookieMonster::DeleteCanonicalCookie(const CanonicalCookie& cookie) {
  base::AutoLock autolock(lock_);

  for (CookieMapItPair its = cookies_.equal_range(GetKey(cookie.Domain()));
       its.first != its.second; ++its.first) {
    // The creation date acts as our unique index...
    if (its.first->second->CreationDate() == cookie.CreationDate()) {
      InternalDeleteCookie(its.first, true, DELETE_COOKIE_EXPLICIT);
      return true;
    }
  }
  return false;
}

bool CookieMonster::SetCookieWithOptions(const GURL& url,
                                         const std::string& cookie_line,
                                         const CookieOptions& options) {
  base::AutoLock autolock(lock_);

  if (!HasCookieableScheme(url)) {
    return false;
  }

  return SetCookieWithCreationTimeAndOptions(url, cookie_line, Time(), options);
}

std::string CookieMonster::GetCookiesWithOptions(const GURL& url,
                                                 const CookieOptions& options) {
  base::AutoLock autolock(lock_);

  if (!HasCookieableScheme(url))
    return std::string();

  std::vector<CanonicalCookie*> cookies;
  FindCookiesForHostAndDomain(url, options, true, &cookies);
  std::sort(cookies.begin(), cookies.end(), CookieSorter);

  std::string cookie_line = BuildCookieLine(cookies);

  VLOG(kVlogGetCookies) << "GetCookies() result: " << cookie_line;

  return cookie_line;
}

void CookieMonster::DeleteCookie(const GURL& url,
                                 const std::string& cookie_name) {
  base::AutoLock autolock(lock_);

  if (!HasCookieableScheme(url))
    return;

  CookieOptions options;
  options.set_include_httponly();
  options.set_include_same_site();
  // Get the cookies for this host and its domain(s).
  std::vector<CanonicalCookie*> cookies;
  FindCookiesForHostAndDomain(url, options, true, &cookies);
  std::set<CanonicalCookie*> matching_cookies;

  for (std::vector<CanonicalCookie*>::const_iterator it = cookies.begin();
       it != cookies.end(); ++it) {
    if ((*it)->Name() != cookie_name)
      continue;
    if (url.path().find((*it)->Path()))
      continue;
    matching_cookies.insert(*it);
  }

  for (CookieMap::iterator it = cookies_.begin(); it != cookies_.end();) {
    CookieMap::iterator curit = it;
    ++it;
    if (matching_cookies.find(curit->second) != matching_cookies.end()) {
      InternalDeleteCookie(curit, true, DELETE_COOKIE_EXPLICIT);
    }
  }
}

bool CookieMonster::SetCookieWithCreationTime(const GURL& url,
                                              const std::string& cookie_line,
                                              const base::Time& creation_time) {
  DCHECK(!store_.get()) << "This method is only to be used by unit-tests.";
  base::AutoLock autolock(lock_);

  if (!HasCookieableScheme(url)) {
    return false;
  }

  MarkCookieStoreAsInitialized();
  if (ShouldFetchAllCookiesWhenFetchingAnyCookie())
    FetchAllCookiesIfNecessary();

  return SetCookieWithCreationTimeAndOptions(url, cookie_line, creation_time,
                                             CookieOptions());
}

int CookieMonster::DeleteSessionCookies() {
  base::AutoLock autolock(lock_);

  int num_deleted = 0;
  for (CookieMap::iterator it = cookies_.begin(); it != cookies_.end();) {
    CookieMap::iterator curit = it;
    CanonicalCookie* cc = curit->second;
    ++it;

    if (!cc->IsPersistent()) {
      InternalDeleteCookie(curit, true, /*sync_to_store*/
                           DELETE_COOKIE_EXPIRED);
      ++num_deleted;
    }
  }

  return num_deleted;
}

void CookieMonster::MarkCookieStoreAsInitialized() {
  initialized_ = true;
}

void CookieMonster::FetchAllCookiesIfNecessary() {
  if (store_.get() && !started_fetching_all_cookies_) {
    started_fetching_all_cookies_ = true;
    FetchAllCookies();
  }
}

void CookieMonster::FetchAllCookies() {
  DCHECK(store_.get()) << "Store must exist to initialize";
  DCHECK(!finished_fetching_all_cookies_)
      << "All cookies have already been fetched.";

  // We bind in the current time so that we can report the wall-clock time for
  // loading cookies.
  store_->Load(base::Bind(&CookieMonster::OnLoaded, this, TimeTicks::Now()));
}

bool CookieMonster::ShouldFetchAllCookiesWhenFetchingAnyCookie() {
  if (fetch_strategy_ == kUnknownFetch) {
    const std::string group_name =
        base::FieldTrialList::FindFullName(kCookieMonsterFetchStrategyName);
    if (group_name == kFetchWhenNecessaryName) {
      fetch_strategy_ = kFetchWhenNecessary;
    } else if (group_name == kAlwaysFetchName) {
      fetch_strategy_ = kAlwaysFetch;
    } else {
      // The logic in the conditional is redundant, but it makes trials of
      // the Finch experiment more explicit.
      fetch_strategy_ = kAlwaysFetch;
    }
  }

  return fetch_strategy_ == kAlwaysFetch;
}

void CookieMonster::OnLoaded(TimeTicks beginning_time,
                             const std::vector<CanonicalCookie*>& cookies) {
  StoreLoadedCookies(cookies);
  histogram_time_blocked_on_load_->AddTime(TimeTicks::Now() - beginning_time);

  // Invoke the task queue of cookie request.
  InvokeQueue();
}

void CookieMonster::OnKeyLoaded(const std::string& key,
                                const std::vector<CanonicalCookie*>& cookies) {
  // This function does its own separate locking.
  StoreLoadedCookies(cookies);

  std::deque<scoped_refptr<CookieMonsterTask>> tasks_pending_for_key;

  // We need to do this repeatedly until no more tasks were added to the queue
  // during the period where we release the lock.
  while (true) {
    {
      base::AutoLock autolock(lock_);
      std::map<std::string,
               std::deque<scoped_refptr<CookieMonsterTask>>>::iterator it =
          tasks_pending_for_key_.find(key);
      if (it == tasks_pending_for_key_.end()) {
        keys_loaded_.insert(key);
        return;
      }
      if (it->second.empty()) {
        keys_loaded_.insert(key);
        tasks_pending_for_key_.erase(it);
        return;
      }
      it->second.swap(tasks_pending_for_key);
    }

    while (!tasks_pending_for_key.empty()) {
      scoped_refptr<CookieMonsterTask> task = tasks_pending_for_key.front();
      task->Run();
      tasks_pending_for_key.pop_front();
    }
  }
}

void CookieMonster::StoreLoadedCookies(
    const std::vector<CanonicalCookie*>& cookies) {
  // TODO(erikwright): Remove ScopedTracker below once crbug.com/457528 is
  // fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "457528 CookieMonster::StoreLoadedCookies"));

  // Initialize the store and sync in any saved persistent cookies.  We don't
  // care if it's expired, insert it so it can be garbage collected, removed,
  // and sync'd.
  base::AutoLock autolock(lock_);

  CookieItVector cookies_with_control_chars;

  for (std::vector<CanonicalCookie*>::const_iterator it = cookies.begin();
       it != cookies.end(); ++it) {
    int64_t cookie_creation_time = (*it)->CreationDate().ToInternalValue();

    if (creation_times_.insert(cookie_creation_time).second) {
      CookieMap::iterator inserted =
          InternalInsertCookie(GetKey((*it)->Domain()), *it, false);
      const Time cookie_access_time((*it)->LastAccessDate());
      if (earliest_access_time_.is_null() ||
          cookie_access_time < earliest_access_time_)
        earliest_access_time_ = cookie_access_time;

      if (ContainsControlCharacter((*it)->Name()) ||
          ContainsControlCharacter((*it)->Value())) {
        cookies_with_control_chars.push_back(inserted);
      }
    } else {
      LOG(ERROR) << base::StringPrintf(
          "Found cookies with duplicate creation "
          "times in backing store: "
          "{name='%s', domain='%s', path='%s'}",
          (*it)->Name().c_str(), (*it)->Domain().c_str(),
          (*it)->Path().c_str());
      // We've been given ownership of the cookie and are throwing it
      // away; reclaim the space.
      delete (*it);
    }
  }

  // Any cookies that contain control characters that we have loaded from the
  // persistent store should be deleted. See http://crbug.com/238041.
  for (CookieItVector::iterator it = cookies_with_control_chars.begin();
       it != cookies_with_control_chars.end();) {
    CookieItVector::iterator curit = it;
    ++it;

    InternalDeleteCookie(*curit, true, DELETE_COOKIE_CONTROL_CHAR);
  }

  // After importing cookies from the PersistentCookieStore, verify that
  // none of our other constraints are violated.
  // In particular, the backing store might have given us duplicate cookies.

  // This method could be called multiple times due to priority loading, thus
  // cookies loaded in previous runs will be validated again, but this is OK
  // since they are expected to be much fewer than total DB.
  EnsureCookiesMapIsValid();
}

void CookieMonster::InvokeQueue() {
  while (true) {
    scoped_refptr<CookieMonsterTask> request_task;
    {
      base::AutoLock autolock(lock_);
      if (tasks_pending_.empty()) {
        finished_fetching_all_cookies_ = true;
        creation_times_.clear();
        keys_loaded_.clear();
        break;
      }
      request_task = tasks_pending_.front();
      tasks_pending_.pop();
    }
    request_task->Run();
  }
}

void CookieMonster::EnsureCookiesMapIsValid() {
  lock_.AssertAcquired();

  // Iterate through all the of the cookies, grouped by host.
  CookieMap::iterator prev_range_end = cookies_.begin();
  while (prev_range_end != cookies_.end()) {
    CookieMap::iterator cur_range_begin = prev_range_end;
    const std::string key = cur_range_begin->first;  // Keep a copy.
    CookieMap::iterator cur_range_end = cookies_.upper_bound(key);
    prev_range_end = cur_range_end;

    // Ensure no equivalent cookies for this host.
    TrimDuplicateCookiesForKey(key, cur_range_begin, cur_range_end);
  }
}

void CookieMonster::TrimDuplicateCookiesForKey(const std::string& key,
                                               CookieMap::iterator begin,
                                               CookieMap::iterator end) {
  lock_.AssertAcquired();

  // Set of cookies ordered by creation time.
  typedef std::set<CookieMap::iterator, OrderByCreationTimeDesc> CookieSet;

  // Helper map we populate to find the duplicates.
  typedef std::map<CookieSignature, CookieSet> EquivalenceMap;
  EquivalenceMap equivalent_cookies;

  // The number of duplicate cookies that have been found.
  int num_duplicates = 0;

  // Iterate through all of the cookies in our range, and insert them into
  // the equivalence map.
  for (CookieMap::iterator it = begin; it != end; ++it) {
    DCHECK_EQ(key, it->first);
    CanonicalCookie* cookie = it->second;

    CookieSignature signature(cookie->Name(), cookie->Domain(), cookie->Path());
    CookieSet& set = equivalent_cookies[signature];

    // We found a duplicate!
    if (!set.empty())
      num_duplicates++;

    // We save the iterator into |cookies_| rather than the actual cookie
    // pointer, since we may need to delete it later.
    bool insert_success = set.insert(it).second;
    DCHECK(insert_success)
        << "Duplicate creation times found in duplicate cookie name scan.";
  }

  // If there were no duplicates, we are done!
  if (num_duplicates == 0)
    return;

  // Make sure we find everything below that we did above.
  int num_duplicates_found = 0;

  // Otherwise, delete all the duplicate cookies, both from our in-memory store
  // and from the backing store.
  for (EquivalenceMap::iterator it = equivalent_cookies.begin();
       it != equivalent_cookies.end(); ++it) {
    const CookieSignature& signature = it->first;
    CookieSet& dupes = it->second;

    if (dupes.size() <= 1)
      continue;  // This cookiename/path has no duplicates.
    num_duplicates_found += dupes.size() - 1;

    // Since |dups| is sorted by creation time (descending), the first cookie
    // is the most recent one, so we will keep it. The rest are duplicates.
    dupes.erase(dupes.begin());

    LOG(ERROR) << base::StringPrintf(
        "Found %d duplicate cookies for host='%s', "
        "with {name='%s', domain='%s', path='%s'}",
        static_cast<int>(dupes.size()), key.c_str(), signature.name.c_str(),
        signature.domain.c_str(), signature.path.c_str());

    // Remove all the cookies identified by |dupes|. It is valid to delete our
    // list of iterators one at a time, since |cookies_| is a multimap (they
    // don't invalidate existing iterators following deletion).
    for (CookieSet::iterator dupes_it = dupes.begin(); dupes_it != dupes.end();
         ++dupes_it) {
      InternalDeleteCookie(*dupes_it, true,
                           DELETE_COOKIE_DUPLICATE_IN_BACKING_STORE);
    }
  }
  DCHECK_EQ(num_duplicates, num_duplicates_found);
}

void CookieMonster::FindCookiesForHostAndDomain(
    const GURL& url,
    const CookieOptions& options,
    bool update_access_time,
    std::vector<CanonicalCookie*>* cookies) {
  lock_.AssertAcquired();

  const Time current_time(CurrentTime());

  // Probe to save statistics relatively frequently.  We do it here rather
  // than in the set path as many websites won't set cookies, and we
  // want to collect statistics whenever the browser's being used.
  RecordPeriodicStats(current_time);

  // Can just dispatch to FindCookiesForKey
  const std::string key(GetKey(url.host()));
  FindCookiesForKey(key, url, options, current_time, update_access_time,
                    cookies);
}

void CookieMonster::FindCookiesForKey(const std::string& key,
                                      const GURL& url,
                                      const CookieOptions& options,
                                      const Time& current,
                                      bool update_access_time,
                                      std::vector<CanonicalCookie*>* cookies) {
  lock_.AssertAcquired();

  for (CookieMapItPair its = cookies_.equal_range(key);
       its.first != its.second;) {
    CookieMap::iterator curit = its.first;
    CanonicalCookie* cc = curit->second;
    ++its.first;

    // If the cookie is expired, delete it.
    if (cc->IsExpired(current) && !keep_expired_cookies_) {
      InternalDeleteCookie(curit, true, DELETE_COOKIE_EXPIRED);
      continue;
    }

    // Filter out cookies that should not be included for a request to the
    // given |url|. HTTP only cookies are filtered depending on the passed
    // cookie |options|.
    if (!cc->IncludeForRequestURL(url, options))
      continue;

    // Add this cookie to the set of matching cookies. Update the access
    // time if we've been requested to do so.
    if (update_access_time) {
      InternalUpdateCookieAccessTime(cc, current);
    }
    cookies->push_back(cc);
  }
}

bool CookieMonster::DeleteAnyEquivalentCookie(const std::string& key,
                                              const CanonicalCookie& ecc,
                                              bool skip_httponly,
                                              bool already_expired,
                                              bool enforce_strict_secure) {
  lock_.AssertAcquired();

  bool found_equivalent_cookie = false;
  bool skipped_httponly = false;
  bool skipped_secure_cookie = false;

  histogram_cookie_delete_equivalent_->Add(COOKIE_DELETE_EQUIVALENT_ATTEMPT);

  for (CookieMapItPair its = cookies_.equal_range(key);
       its.first != its.second;) {
    CookieMap::iterator curit = its.first;
    CanonicalCookie* cc = curit->second;
    ++its.first;

    // If strict secure cookies is being enforced, then the equivalency
    // requirements are looser. If the cookie is being set from an insecure
    // scheme, then if a cookie already exists with the same name and it is
    // Secure, then the cookie should *not* be updated if they domain-match and
    // ignoring the path attribute.
    //
    // See: https://tools.ietf.org/html/draft-west-leave-secure-cookies-alone
    if (enforce_strict_secure && !ecc.Source().SchemeIsCryptographic() &&
        ecc.IsEquivalentForSecureCookieMatching(*cc) && cc->IsSecure()) {
      skipped_secure_cookie = true;
      histogram_cookie_delete_equivalent_->Add(
          COOKIE_DELETE_EQUIVALENT_SKIPPING_SECURE);
      // If the cookie is equivalent to the new cookie and wouldn't have been
      // skipped for being HTTP-only, record that it is a skipped secure cookie
      // that would have been deleted otherwise.
      if (ecc.IsEquivalent(*cc)) {
        found_equivalent_cookie = true;

        if (!skip_httponly || !cc->IsHttpOnly()) {
          histogram_cookie_delete_equivalent_->Add(
              COOKIE_DELETE_EQUIVALENT_WOULD_HAVE_DELETED);
        }
      }
    } else if (ecc.IsEquivalent(*cc)) {
      // We should never have more than one equivalent cookie, since they should
      // overwrite each other, unless secure cookies require secure scheme is
      // being enforced. In that case, cookies with different paths might exist
      // and be considered equivalent.
      CHECK(!found_equivalent_cookie)
          << "Duplicate equivalent cookies found, cookie store is corrupted.";
      if (skip_httponly && cc->IsHttpOnly()) {
        skipped_httponly = true;
      } else {
        histogram_cookie_delete_equivalent_->Add(
            COOKIE_DELETE_EQUIVALENT_FOUND);
        InternalDeleteCookie(curit, true, already_expired
                                              ? DELETE_COOKIE_EXPIRED_OVERWRITE
                                              : DELETE_COOKIE_OVERWRITE);
      }
      found_equivalent_cookie = true;
    }
  }
  return skipped_httponly || skipped_secure_cookie;
}

CookieMonster::CookieMap::iterator CookieMonster::InternalInsertCookie(
    const std::string& key,
    CanonicalCookie* cc,
    bool sync_to_store) {
  // TODO(mkwst): Remove ScopedTracker below once crbug.com/456373 is fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "456373 CookieMonster::InternalInsertCookie"));
  lock_.AssertAcquired();

  if ((cc->IsPersistent() || persist_session_cookies_) && store_.get() &&
      sync_to_store)
    store_->AddCookie(*cc);
  CookieMap::iterator inserted =
      cookies_.insert(CookieMap::value_type(key, cc));
  if (delegate_.get()) {
    delegate_->OnCookieChanged(*cc, false,
                               CookieMonsterDelegate::CHANGE_COOKIE_EXPLICIT);
  }

  // See InitializeHistograms() for details.
  int32_t type_sample = cc->IsSameSite() ? 1 << COOKIE_TYPE_SAME_SITE : 0;
  type_sample |= cc->IsHttpOnly() ? 1 << COOKIE_TYPE_HTTPONLY : 0;
  type_sample |= cc->IsSecure() ? 1 << COOKIE_TYPE_SECURE : 0;
  histogram_cookie_type_->Add(type_sample);

  // Histogram the type of scheme used on URLs that set cookies. This
  // intentionally includes cookies that are set or overwritten by
  // http:// URLs, but not cookies that are cleared by http:// URLs, to
  // understand if the former behavior can be deprecated for Secure
  // cookies.
  if (!cc->Source().is_empty()) {
    CookieSource cookie_source_sample;
    if (cc->Source().SchemeIsCryptographic()) {
      cookie_source_sample =
          cc->IsSecure() ? COOKIE_SOURCE_SECURE_COOKIE_CRYPTOGRAPHIC_SCHEME
                         : COOKIE_SOURCE_NONSECURE_COOKIE_CRYPTOGRAPHIC_SCHEME;
    } else {
      cookie_source_sample =
          cc->IsSecure()
              ? COOKIE_SOURCE_SECURE_COOKIE_NONCRYPTOGRAPHIC_SCHEME
              : COOKIE_SOURCE_NONSECURE_COOKIE_NONCRYPTOGRAPHIC_SCHEME;
    }
    histogram_cookie_source_scheme_->Add(cookie_source_sample);
  }

  RunCallbacks(*cc, false);

  return inserted;
}

bool CookieMonster::SetCookieWithCreationTimeAndOptions(
    const GURL& url,
    const std::string& cookie_line,
    const Time& creation_time_or_null,
    const CookieOptions& options) {
  lock_.AssertAcquired();

  VLOG(kVlogSetCookies) << "SetCookie() line: " << cookie_line;

  Time creation_time = creation_time_or_null;
  if (creation_time.is_null()) {
    creation_time = CurrentTime();
    last_time_seen_ = creation_time;
  }

  scoped_ptr<CanonicalCookie> cc(
      CanonicalCookie::Create(url, cookie_line, creation_time, options));

  if (!cc.get()) {
    VLOG(kVlogSetCookies) << "WARNING: Failed to allocate CanonicalCookie";
    return false;
  }
  return SetCanonicalCookie(std::move(cc), options);
}

bool CookieMonster::SetCanonicalCookie(scoped_ptr<CanonicalCookie> cc,
                                       const CookieOptions& options) {
  Time creation_time = cc->CreationDate();
  const std::string key(GetKey(cc->Domain()));
  bool already_expired = cc->IsExpired(creation_time);

  if (DeleteAnyEquivalentCookie(key, *cc, options.exclude_httponly(),
                                already_expired,
                                options.enforce_strict_secure())) {
    std::string error;
    if (options.enforce_strict_secure()) {
      error =
          "SetCookie() not clobbering httponly cookie or secure cookie for "
          "insecure scheme";
    } else {
      error = "SetCookie() not clobbering httponly cookie";
    }

    VLOG(kVlogSetCookies) << error;
    return false;
  }

  VLOG(kVlogSetCookies) << "SetCookie() key: " << key
                        << " cc: " << cc->DebugString();

  // Realize that we might be setting an expired cookie, and the only point
  // was to delete the cookie which we've already done.
  if (!already_expired || keep_expired_cookies_) {
    // See InitializeHistograms() for details.
    if (cc->IsPersistent()) {
      histogram_expiration_duration_minutes_->Add(
          (cc->ExpiryDate() - creation_time).InMinutes());
    }

    InternalInsertCookie(key, cc.release(), true);
  } else {
    VLOG(kVlogSetCookies) << "SetCookie() not storing already expired cookie.";
  }

  // We assume that hopefully setting a cookie will be less common than
  // querying a cookie.  Since setting a cookie can put us over our limits,
  // make sure that we garbage collect...  We can also make the assumption that
  // if a cookie was set, in the common case it will be used soon after,
  // and we will purge the expired cookies in GetCookies().
  GarbageCollect(creation_time, key, options.enforce_strict_secure());

  return true;
}

bool CookieMonster::SetCanonicalCookies(const CookieList& list) {
  base::AutoLock autolock(lock_);

  CookieOptions options;
  options.set_include_httponly();

  for (const auto& cookie : list) {
    if (!SetCanonicalCookie(make_scoped_ptr(new CanonicalCookie(cookie)),
                            options)) {
      return false;
    }
  }

  return true;
}

void CookieMonster::InternalUpdateCookieAccessTime(CanonicalCookie* cc,
                                                   const Time& current) {
  lock_.AssertAcquired();

  // Based off the Mozilla code.  When a cookie has been accessed recently,
  // don't bother updating its access time again.  This reduces the number of
  // updates we do during pageload, which in turn reduces the chance our storage
  // backend will hit its batch thresholds and be forced to update.
  if ((current - cc->LastAccessDate()) < last_access_threshold_)
    return;

  cc->SetLastAccessDate(current);
  if ((cc->IsPersistent() || persist_session_cookies_) && store_.get())
    store_->UpdateCookieAccessTime(*cc);
}

// InternalDeleteCookies must not invalidate iterators other than the one being
// deleted.
void CookieMonster::InternalDeleteCookie(CookieMap::iterator it,
                                         bool sync_to_store,
                                         DeletionCause deletion_cause) {
  lock_.AssertAcquired();

  // Ideally, this would be asserted up where we define ChangeCauseMapping,
  // but DeletionCause's visibility (or lack thereof) forces us to make
  // this check here.
  static_assert(arraysize(ChangeCauseMapping) == DELETE_COOKIE_LAST_ENTRY + 1,
                "ChangeCauseMapping size should match DeletionCause size");

  // See InitializeHistograms() for details.
  if (deletion_cause != DELETE_COOKIE_DONT_RECORD)
    histogram_cookie_deletion_cause_->Add(deletion_cause);

  CanonicalCookie* cc = it->second;
  VLOG(kVlogSetCookies) << "InternalDeleteCookie()"
                        << ", cause:" << deletion_cause
                        << ", cc: " << cc->DebugString();

  if ((cc->IsPersistent() || persist_session_cookies_) && store_.get() &&
      sync_to_store)
    store_->DeleteCookie(*cc);
  if (delegate_.get()) {
    ChangeCausePair mapping = ChangeCauseMapping[deletion_cause];

    if (mapping.notify)
      delegate_->OnCookieChanged(*cc, true, mapping.cause);
  }
  RunCallbacks(*cc, true);
  cookies_.erase(it);
  delete cc;
}

// Domain expiry behavior is unchanged by key/expiry scheme (the
// meaning of the key is different, but that's not visible to this routine).
size_t CookieMonster::GarbageCollect(const Time& current,
                                     const std::string& key,
                                     bool enforce_strict_secure) {
  lock_.AssertAcquired();

  size_t num_deleted = 0;
  Time safe_date(Time::Now() - TimeDelta::FromDays(kSafeFromGlobalPurgeDays));

  // Collect garbage for this key, minding cookie priorities.
  if (cookies_.count(key) > kDomainMaxCookies) {
    VLOG(kVlogGarbageCollection) << "GarbageCollect() key: " << key;

    CookieItVector* cookie_its;

    CookieItVector non_expired_cookie_its;
    cookie_its = &non_expired_cookie_its;
    num_deleted +=
        GarbageCollectExpired(current, cookies_.equal_range(key), cookie_its);

    CookieItVector secure_cookie_its;
    if (enforce_strict_secure && cookie_its->size() > kDomainMaxCookies) {
      VLOG(kVlogGarbageCollection) << "Garbage collecting non-Secure cookies.";
      num_deleted +=
          GarbageCollectNonSecure(non_expired_cookie_its, &secure_cookie_its);
      cookie_its = &secure_cookie_its;
    }

    if (cookie_its->size() > kDomainMaxCookies) {
      VLOG(kVlogGarbageCollection) << "Deep Garbage Collect domain.";
      size_t purge_goal =
          cookie_its->size() - (kDomainMaxCookies - kDomainPurgeCookies);
      DCHECK(purge_goal > kDomainPurgeCookies);

      // Boundary iterators into |cookie_its| for different priorities.
      CookieItVector::iterator it_bdd[4];
      // Intialize |it_bdd| while sorting |cookie_its| by priorities.
      // Schematic: [MLLHMHHLMM] => [LLL|MMMM|HHH], with 4 boundaries.
      it_bdd[0] = cookie_its->begin();
      it_bdd[3] = cookie_its->end();
      it_bdd[1] =
          PartitionCookieByPriority(it_bdd[0], it_bdd[3], COOKIE_PRIORITY_LOW);
      it_bdd[2] = PartitionCookieByPriority(it_bdd[1], it_bdd[3],
                                            COOKIE_PRIORITY_MEDIUM);
      size_t quota[3] = {kDomainCookiesQuotaLow,
                         kDomainCookiesQuotaMedium,
                         kDomainCookiesQuotaHigh};

      // Purge domain cookies in 3 rounds.
      // Round 1: consider low-priority cookies only: evict least-recently
      //   accessed, while protecting quota[0] of these from deletion.
      // Round 2: consider {low, medium}-priority cookies, evict least-recently
      //   accessed, while protecting quota[0] + quota[1].
      // Round 3: consider all cookies, evict least-recently accessed.
      size_t accumulated_quota = 0;
      CookieItVector::iterator it_purge_begin = it_bdd[0];
      for (int i = 0; i < 3 && purge_goal > 0; ++i) {
        accumulated_quota += quota[i];

        size_t num_considered = it_bdd[i + 1] - it_purge_begin;
        if (num_considered <= accumulated_quota)
          continue;

        // Number of cookies that will be purged in this round.
        size_t round_goal =
            std::min(purge_goal, num_considered - accumulated_quota);
        purge_goal -= round_goal;

        SortLeastRecentlyAccessed(it_purge_begin, it_bdd[i + 1], round_goal);
        // Cookies accessed on or after |safe_date| would have been safe from
        // global purge, and we want to keep track of this.
        CookieItVector::iterator it_purge_end = it_purge_begin + round_goal;
        CookieItVector::iterator it_purge_middle =
            LowerBoundAccessDate(it_purge_begin, it_purge_end, safe_date);
        // Delete cookies accessed before |safe_date|.
        num_deleted += GarbageCollectDeleteRange(
            current, DELETE_COOKIE_EVICTED_DOMAIN_PRE_SAFE, it_purge_begin,
            it_purge_middle);
        // Delete cookies accessed on or after |safe_date|.
        num_deleted += GarbageCollectDeleteRange(
            current, DELETE_COOKIE_EVICTED_DOMAIN_POST_SAFE, it_purge_middle,
            it_purge_end);
        it_purge_begin = it_purge_end;
      }
      DCHECK_EQ(0U, purge_goal);
    }
  }

  // Collect garbage for everything. With firefox style we want to preserve
  // cookies accessed in kSafeFromGlobalPurgeDays, otherwise evict.
  if (cookies_.size() > kMaxCookies && earliest_access_time_ < safe_date) {
    VLOG(kVlogGarbageCollection) << "GarbageCollect() everything";
    CookieItVector cookie_its;

    num_deleted += GarbageCollectExpired(
        current, CookieMapItPair(cookies_.begin(), cookies_.end()),
        &cookie_its);

    if (cookie_its.size() > kMaxCookies) {
      VLOG(kVlogGarbageCollection) << "Deep Garbage Collect everything.";
      size_t purge_goal = cookie_its.size() - (kMaxCookies - kPurgeCookies);
      DCHECK(purge_goal > kPurgeCookies);

      if (enforce_strict_secure) {
        CookieItVector secure_cookie_its;
        CookieItVector non_secure_cookie_its;
        SplitCookieVectorIntoSecureAndNonSecure(cookie_its, &secure_cookie_its,
                                                &non_secure_cookie_its);
        size_t non_secure_purge_goal =
            std::min<size_t>(purge_goal, non_secure_cookie_its.size() - 1);

        size_t just_deleted = GarbageCollectLeastRecentlyAccessed(
            current, safe_date, non_secure_purge_goal, non_secure_cookie_its);
        num_deleted += just_deleted;

        if (just_deleted < purge_goal) {
          size_t secure_purge_goal = std::min<size_t>(
              purge_goal - just_deleted, secure_cookie_its.size() - 1);
          num_deleted += GarbageCollectLeastRecentlyAccessed(
              current, safe_date, secure_purge_goal, secure_cookie_its);
        }
      } else {
        num_deleted += GarbageCollectLeastRecentlyAccessed(
            current, safe_date, purge_goal, cookie_its);
      }
    }
  }

  return num_deleted;
}

size_t CookieMonster::GarbageCollectExpired(const Time& current,
                                            const CookieMapItPair& itpair,
                                            CookieItVector* cookie_its) {
  if (keep_expired_cookies_)
    return 0;

  lock_.AssertAcquired();

  int num_deleted = 0;
  for (CookieMap::iterator it = itpair.first, end = itpair.second; it != end;) {
    CookieMap::iterator curit = it;
    ++it;

    if (curit->second->IsExpired(current)) {
      InternalDeleteCookie(curit, true, DELETE_COOKIE_EXPIRED);
      ++num_deleted;
    } else if (cookie_its) {
      cookie_its->push_back(curit);
    }
  }

  return num_deleted;
}

size_t CookieMonster::GarbageCollectNonSecure(
    const CookieItVector& valid_cookies,
    CookieItVector* cookie_its) {
  lock_.AssertAcquired();

  size_t num_deleted = 0;
  for (const auto& curr_cookie_it : valid_cookies) {
    if (!curr_cookie_it->second->IsSecure()) {
      InternalDeleteCookie(curr_cookie_it, true, DELETE_COOKIE_NON_SECURE);
      ++num_deleted;
    } else if (cookie_its) {
      cookie_its->push_back(curr_cookie_it);
    }
  }

  return num_deleted;
}

size_t CookieMonster::GarbageCollectDeleteRange(
    const Time& current,
    DeletionCause cause,
    CookieItVector::iterator it_begin,
    CookieItVector::iterator it_end) {
  for (CookieItVector::iterator it = it_begin; it != it_end; it++) {
    histogram_evicted_last_access_minutes_->Add(
        (current - (*it)->second->LastAccessDate()).InMinutes());
    InternalDeleteCookie((*it), true, cause);
  }
  return it_end - it_begin;
}

size_t CookieMonster::GarbageCollectLeastRecentlyAccessed(
    const base::Time& current,
    const base::Time& safe_date,
    size_t purge_goal,
    CookieItVector cookie_its) {
  // Sorts up to *and including* |cookie_its[purge_goal]|, so
  // |earliest_access_time| will be properly assigned even if
  // |global_purge_it| == |cookie_its.begin() + purge_goal|.
  SortLeastRecentlyAccessed(cookie_its.begin(), cookie_its.end(), purge_goal);
  // Find boundary to cookies older than safe_date.
  CookieItVector::iterator global_purge_it = LowerBoundAccessDate(
      cookie_its.begin(), cookie_its.begin() + purge_goal, safe_date);
  // Only delete the old cookies, and if strict secure is enabled, delete
  // non-secure ones first.
  size_t num_deleted =
      GarbageCollectDeleteRange(current, DELETE_COOKIE_EVICTED_GLOBAL,
                                cookie_its.begin(), global_purge_it);
  // Set access day to the oldest cookie that wasn't deleted.
  earliest_access_time_ = (*global_purge_it)->second->LastAccessDate();
  return num_deleted;
}

// A wrapper around registry_controlled_domains::GetDomainAndRegistry
// to make clear we're creating a key for our local map.  Here and
// in FindCookiesForHostAndDomain() are the only two places where
// we need to conditionalize based on key type.
//
// Note that this key algorithm explicitly ignores the scheme.  This is
// because when we're entering cookies into the map from the backing store,
// we in general won't have the scheme at that point.
// In practical terms, this means that file cookies will be stored
// in the map either by an empty string or by UNC name (and will be
// limited by kMaxCookiesPerHost), and extension cookies will be stored
// based on the single extension id, as the extension id won't have the
// form of a DNS host and hence GetKey() will return it unchanged.
//
// Arguably the right thing to do here is to make the key
// algorithm dependent on the scheme, and make sure that the scheme is
// available everywhere the key must be obtained (specfically at backing
// store load time).  This would require either changing the backing store
// database schema to include the scheme (far more trouble than it's worth), or
// separating out file cookies into their own CookieMonster instance and
// thus restricting each scheme to a single cookie monster (which might
// be worth it, but is still too much trouble to solve what is currently a
// non-problem).
std::string CookieMonster::GetKey(const std::string& domain) const {
  std::string effective_domain(
      registry_controlled_domains::GetDomainAndRegistry(
          domain, registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES));
  if (effective_domain.empty())
    effective_domain = domain;

  if (!effective_domain.empty() && effective_domain[0] == '.')
    return effective_domain.substr(1);
  return effective_domain;
}

bool CookieMonster::HasCookieableScheme(const GURL& url) {
  lock_.AssertAcquired();

  // Make sure the request is on a cookie-able url scheme.
  for (size_t i = 0; i < cookieable_schemes_.size(); ++i) {
    // We matched a scheme.
    if (url.SchemeIs(cookieable_schemes_[i].c_str())) {
      // We've matched a supported scheme.
      return true;
    }
  }

  // The scheme didn't match any in our whitelist.
  VLOG(kVlogPerCookieMonster)
      << "WARNING: Unsupported cookie scheme: " << url.scheme();
  return false;
}

// Test to see if stats should be recorded, and record them if so.
// The goal here is to get sampling for the average browser-hour of
// activity.  We won't take samples when the web isn't being surfed,
// and when the web is being surfed, we'll take samples about every
// kRecordStatisticsIntervalSeconds.
// last_statistic_record_time_ is initialized to Now() rather than null
// in the constructor so that we won't take statistics right after
// startup, to avoid bias from browsers that are started but not used.
void CookieMonster::RecordPeriodicStats(const base::Time& current_time) {
  const base::TimeDelta kRecordStatisticsIntervalTime(
      base::TimeDelta::FromSeconds(kRecordStatisticsIntervalSeconds));

  // If we've taken statistics recently, return.
  if (current_time - last_statistic_record_time_ <=
      kRecordStatisticsIntervalTime) {
    return;
  }

  // See InitializeHistograms() for details.
  histogram_count_->Add(cookies_.size());

  // More detailed statistics on cookie counts at different granularities.
  last_statistic_record_time_ = current_time;
}

// Initialize all histogram counter variables used in this class.
//
// Normal histogram usage involves using the macros defined in
// histogram.h, which automatically takes care of declaring these
// variables (as statics), initializing them, and accumulating into
// them, all from a single entry point.  Unfortunately, that solution
// doesn't work for the CookieMonster, as it's vulnerable to races between
// separate threads executing the same functions and hence initializing the
// same static variables.  There isn't a race danger in the histogram
// accumulation calls; they are written to be resilient to simultaneous
// calls from multiple threads.
//
// The solution taken here is to have per-CookieMonster instance
// variables that are constructed during CookieMonster construction.
// Note that these variables refer to the same underlying histogram,
// so we still race (but safely) with other CookieMonster instances
// for accumulation.
//
// To do this we've expanded out the individual histogram macros calls,
// with declarations of the variables in the class decl, initialization here
// (done from the class constructor) and direct calls to the accumulation
// methods where needed.  The specific histogram macro calls on which the
// initialization is based are included in comments below.
void CookieMonster::InitializeHistograms() {
  // From UMA_HISTOGRAM_CUSTOM_COUNTS
  histogram_expiration_duration_minutes_ = base::Histogram::FactoryGet(
      "Cookie.ExpirationDurationMinutes", 1, kMinutesInTenYears, 50,
      base::Histogram::kUmaTargetedHistogramFlag);
  histogram_evicted_last_access_minutes_ = base::Histogram::FactoryGet(
      "Cookie.EvictedLastAccessMinutes", 1, kMinutesInTenYears, 50,
      base::Histogram::kUmaTargetedHistogramFlag);
  histogram_count_ = base::Histogram::FactoryGet(
      "Cookie.Count", 1, 4000, 50, base::Histogram::kUmaTargetedHistogramFlag);

  // From UMA_HISTOGRAM_ENUMERATION
  histogram_cookie_deletion_cause_ = base::LinearHistogram::FactoryGet(
      "Cookie.DeletionCause", 1, DELETE_COOKIE_LAST_ENTRY - 1,
      DELETE_COOKIE_LAST_ENTRY, base::Histogram::kUmaTargetedHistogramFlag);
  histogram_cookie_type_ = base::LinearHistogram::FactoryGet(
      "Cookie.Type", 1, (1 << COOKIE_TYPE_LAST_ENTRY) - 1,
      1 << COOKIE_TYPE_LAST_ENTRY, base::Histogram::kUmaTargetedHistogramFlag);
  histogram_cookie_source_scheme_ = base::LinearHistogram::FactoryGet(
      "Cookie.CookieSourceScheme", 1, COOKIE_SOURCE_LAST_ENTRY - 1,
      COOKIE_SOURCE_LAST_ENTRY, base::Histogram::kUmaTargetedHistogramFlag);
  histogram_cookie_delete_equivalent_ = base::LinearHistogram::FactoryGet(
      "Cookie.CookieDeleteEquivalent", 1,
      COOKIE_DELETE_EQUIVALENT_LAST_ENTRY - 1,
      COOKIE_DELETE_EQUIVALENT_LAST_ENTRY,
      base::Histogram::kUmaTargetedHistogramFlag);

  // From UMA_HISTOGRAM_{CUSTOM_,}TIMES
  histogram_time_blocked_on_load_ = base::Histogram::FactoryTimeGet(
      "Cookie.TimeBlockedOnLoad", base::TimeDelta::FromMilliseconds(1),
      base::TimeDelta::FromMinutes(1), 50,
      base::Histogram::kUmaTargetedHistogramFlag);
}

// The system resolution is not high enough, so we can have multiple
// set cookies that result in the same system time.  When this happens, we
// increment by one Time unit.  Let's hope computers don't get too fast.
Time CookieMonster::CurrentTime() {
  return std::max(Time::Now(), Time::FromInternalValue(
                                   last_time_seen_.ToInternalValue() + 1));
}

void CookieMonster::DoCookieTask(
    const scoped_refptr<CookieMonsterTask>& task_item) {
  {
    base::AutoLock autolock(lock_);
    MarkCookieStoreAsInitialized();
    FetchAllCookiesIfNecessary();
    if (!finished_fetching_all_cookies_ && store_.get()) {
      tasks_pending_.push(task_item);
      return;
    }
  }

  task_item->Run();
}

void CookieMonster::DoCookieTaskForURL(
    const scoped_refptr<CookieMonsterTask>& task_item,
    const GURL& url) {
  {
    base::AutoLock autolock(lock_);
    MarkCookieStoreAsInitialized();
    if (ShouldFetchAllCookiesWhenFetchingAnyCookie())
      FetchAllCookiesIfNecessary();
    // If cookies for the requested domain key (eTLD+1) have been loaded from DB
    // then run the task, otherwise load from DB.
    if (!finished_fetching_all_cookies_ && store_.get()) {
      // Checks if the domain key has been loaded.
      std::string key(
          cookie_util::GetEffectiveDomain(url.scheme(), url.host()));
      if (keys_loaded_.find(key) == keys_loaded_.end()) {
        std::map<std::string,
                 std::deque<scoped_refptr<CookieMonsterTask>>>::iterator it =
            tasks_pending_for_key_.find(key);
        if (it == tasks_pending_for_key_.end()) {
          store_->LoadCookiesForKey(
              key, base::Bind(&CookieMonster::OnKeyLoaded, this, key));
          it = tasks_pending_for_key_
                   .insert(std::make_pair(
                       key, std::deque<scoped_refptr<CookieMonsterTask>>()))
                   .first;
        }
        it->second.push_back(task_item);
        return;
      }
    }
  }
  task_item->Run();
}

void CookieMonster::ComputeCookieDiff(CookieList* old_cookies,
                                      CookieList* new_cookies,
                                      CookieList* cookies_to_add,
                                      CookieList* cookies_to_delete) {
  DCHECK(old_cookies);
  DCHECK(new_cookies);
  DCHECK(cookies_to_add);
  DCHECK(cookies_to_delete);
  DCHECK(cookies_to_add->empty());
  DCHECK(cookies_to_delete->empty());

  // Sort both lists.
  // A set ordered by FullDiffCookieSorter is also ordered by
  // PartialDiffCookieSorter.
  std::sort(old_cookies->begin(), old_cookies->end(), FullDiffCookieSorter);
  std::sort(new_cookies->begin(), new_cookies->end(), FullDiffCookieSorter);

  // Select any old cookie for deletion if no new cookie has the same name,
  // domain, and path.
  std::set_difference(
      old_cookies->begin(), old_cookies->end(), new_cookies->begin(),
      new_cookies->end(),
      std::inserter(*cookies_to_delete, cookies_to_delete->begin()),
      PartialDiffCookieSorter);

  // Select any new cookie for addition (or update) if no old cookie is exactly
  // equivalent.
  std::set_difference(new_cookies->begin(), new_cookies->end(),
                      old_cookies->begin(), old_cookies->end(),
                      std::inserter(*cookies_to_add, cookies_to_add->begin()),
                      FullDiffCookieSorter);
}

void CookieMonster::RunCallbacks(const CanonicalCookie& cookie, bool removed) {
  lock_.AssertAcquired();
  CookieOptions opts;
  opts.set_include_httponly();
  opts.set_include_same_site();
  // Note that the callbacks in hook_map_ are wrapped with MakeAsync(), so they
  // are guaranteed to not take long - they just post a RunAsync task back to
  // the appropriate thread's message loop and return. It is important that this
  // method not run user-supplied callbacks directly, since the CookieMonster
  // lock is held and it is easy to accidentally introduce deadlocks.
  for (CookieChangedHookMap::iterator it = hook_map_.begin();
       it != hook_map_.end(); ++it) {
    std::pair<GURL, std::string> key = it->first;
    if (cookie.IncludeForRequestURL(key.first, opts) &&
        cookie.Name() == key.second) {
      it->second->Notify(cookie, removed);
    }
  }
}

}  // namespace net
