// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_cookie_store.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace prerender {

PrerenderCookieStore::PrerenderCookieStore(
    scoped_refptr<net::CookieMonster> default_cookie_monster,
    const base::Closure& cookie_conflict_cb)
    : in_forwarding_mode_(false),
      default_cookie_monster_(default_cookie_monster),
      changes_cookie_monster_(new net::CookieMonster(NULL, NULL)),
      cookie_conflict_cb_(cookie_conflict_cb),
      cookie_conflict_(false) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(default_cookie_monster_.get() != NULL);
  DCHECK(default_cookie_monster_->loaded());
}

PrerenderCookieStore::~PrerenderCookieStore() {
}

void PrerenderCookieStore::SetCookieWithOptionsAsync(
    const GURL& url,
    const std::string& cookie_line,
    const net::CookieOptions& options,
    const SetCookiesCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  CookieOperation op;
  op.op = COOKIE_OP_SET_COOKIE_WITH_OPTIONS_ASYNC;
  op.url = url;
  op.cookie_line = cookie_line;
  op.options = options;

  GetCookieStoreForCookieOpAndLog(op)->
      SetCookieWithOptionsAsync(url, cookie_line, options, callback);
}

void PrerenderCookieStore::GetCookiesWithOptionsAsync(
    const GURL& url,
    const net::CookieOptions& options,
    const GetCookiesCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  CookieOperation op;
  op.op = COOKIE_OP_GET_COOKIES_WITH_OPTIONS_ASYNC;
  op.url = url;
  op.options = options;

  GetCookieStoreForCookieOpAndLog(op)->
      GetCookiesWithOptionsAsync(url, options, callback);
}

void PrerenderCookieStore::GetAllCookiesForURLAsync(
    const GURL& url,
    const GetCookieListCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  CookieOperation op;
  op.op = COOKIE_OP_GET_ALL_COOKIES_FOR_URL_ASYNC;
  op.url = url;

  GetCookieStoreForCookieOpAndLog(op)->GetAllCookiesForURLAsync(url, callback);
}


void PrerenderCookieStore::DeleteCookieAsync(const GURL& url,
                                             const std::string& cookie_name,
                                             const base::Closure& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  CookieOperation op;
  op.op = COOKIE_OP_DELETE_COOKIE_ASYNC;
  op.url = url;
  op.cookie_name = cookie_name;

  GetCookieStoreForCookieOpAndLog(op)->DeleteCookieAsync(url, cookie_name,
                                                         callback);
}

void PrerenderCookieStore::DeleteAllCreatedBetweenAsync(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    const DeleteCallback& callback) {
  NOTREACHED();
}

void PrerenderCookieStore::DeleteAllCreatedBetweenForHostAsync(
    const base::Time delete_begin,
    const base::Time delete_end,
    const GURL& url,
    const DeleteCallback& callback) {
  NOTREACHED();
}

void PrerenderCookieStore::DeleteSessionCookiesAsync(const DeleteCallback&) {
  NOTREACHED();
}

net::CookieMonster* PrerenderCookieStore::GetCookieMonster() {
  NOTREACHED();
  return NULL;
}

net::CookieStore* PrerenderCookieStore::GetCookieStoreForCookieOpAndLog(
    const CookieOperation& op) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  std::string key = default_cookie_monster_->GetKey(op.url.host());
  bool is_read_only = (op.op == COOKIE_OP_GET_COOKIES_WITH_OPTIONS_ASYNC ||
                       op.op == COOKIE_OP_GET_ALL_COOKIES_FOR_URL_ASYNC);

  if (in_forwarding_mode_)
    return default_cookie_monster_.get();

  DCHECK(changes_cookie_monster_.get() != NULL);

  cookie_ops_.push_back(op);

  bool key_copied = ContainsKey(copied_keys_, key);

  if (key_copied)
    return changes_cookie_monster_.get();

  if (is_read_only) {
    // Insert this key into the set of read keys, if it doesn't exist yet.
    if (!ContainsKey(read_keys_, key))
      read_keys_.insert(key);
    return default_cookie_monster_.get();
  }

  // If this method hasn't returned yet, the key has not been copied yet,
  // and we must copy it due to the requested write operation.

  bool copy_success =
      default_cookie_monster_->CopyCookiesForKeyToOtherCookieMonster(
          key, changes_cookie_monster_.get());

  // The copy must succeed.
  DCHECK(copy_success);

  copied_keys_.insert(key);

  return changes_cookie_monster_.get();
}

void PrerenderCookieStore::ApplyChanges(std::vector<GURL>* cookie_change_urls) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (in_forwarding_mode_)
    return;

  // Apply all changes to the underlying cookie store.
  for (std::vector<CookieOperation>::const_iterator it = cookie_ops_.begin();
       it != cookie_ops_.end();
       ++it) {
    switch (it->op) {
      case COOKIE_OP_SET_COOKIE_WITH_OPTIONS_ASYNC:
        cookie_change_urls->push_back(it->url);
        default_cookie_monster_->SetCookieWithOptionsAsync(
            it->url, it->cookie_line, it->options, SetCookiesCallback());
        break;
      case COOKIE_OP_GET_COOKIES_WITH_OPTIONS_ASYNC:
        default_cookie_monster_->GetCookiesWithOptionsAsync(
            it->url, it->options, GetCookiesCallback());
        break;
      case COOKIE_OP_GET_ALL_COOKIES_FOR_URL_ASYNC:
        default_cookie_monster_->GetAllCookiesForURLAsync(
            it->url, GetCookieListCallback());
        break;
      case COOKIE_OP_DELETE_COOKIE_ASYNC:
        cookie_change_urls->push_back(it->url);
        default_cookie_monster_->DeleteCookieAsync(
            it->url, it->cookie_name, base::Closure());
        break;
      case COOKIE_OP_MAX:
        NOTREACHED();
    }
  }

  in_forwarding_mode_ = true;
  copied_keys_.clear();
  cookie_ops_.clear();
  changes_cookie_monster_ = NULL;
}

void PrerenderCookieStore::OnCookieChangedForURL(
    net::CookieMonster* cookie_monster,
    const GURL& url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // If the cookie was changed in a different cookie monster than the one
  // being decorated, there is nothing to do).
  if (cookie_monster != default_cookie_monster_.get())
    return;

  if (in_forwarding_mode_)
    return;

  // If we have encountered a conflict before, it has already been recorded
  // and the cb has been issued, so nothing to do.
  if (cookie_conflict_)
    return;

  std::string key = default_cookie_monster_->GetKey(url.host());

  // If the key for the cookie which was modified was neither read nor written,
  // nothing to do.
  if ((!ContainsKey(read_keys_, key)) && (!ContainsKey(copied_keys_, key)))
    return;

  // There was a conflict in cookies. Call the conflict callback, which should
  // cancel the prerender if necessary (i.e. if it hasn't already been
  // cancelled for some other reason).
  // Notice that there is a race here with swapping in the prerender, but this
  // is the same issue that occurs when two tabs modify cookies for the
  // same domain concurrently. Therefore, there is no need to do anything
  // special to prevent this race condition.
  cookie_conflict_ = true;
  if (!cookie_conflict_cb_.is_null()) {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        cookie_conflict_cb_);
  }
}

PrerenderCookieStore::CookieOperation::CookieOperation() {
}

PrerenderCookieStore::CookieOperation::~CookieOperation() {
}

}  // namespace prerender
