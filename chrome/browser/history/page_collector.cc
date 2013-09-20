// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/page_collector.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/history/history_publisher.h"
#include "url/gurl.h"

namespace {

// Page info older than this will be published even if we haven't
// gotten a title and/or body.
const int kExpirationSeconds = 20;

}  // namespace

namespace history {

// PageCollector::PageInfo -----------------------------------------------

PageCollector::PageInfo::PageInfo(base::Time visit_time)
    : visit_time_(visit_time),
      added_time_(base::TimeTicks::Now()) {
}

PageCollector::PageInfo::~PageInfo() {}

// NOTE(shess): Per the comment on has_title() and has_body(), this
// code maps empty strings to single space to differentiate set title
// and body from empty.  This approach is held over from the original
// TextDatabaseManager version.
void PageCollector::PageInfo::set_title(const string16& ttl) {
  if (ttl.empty())
    title_ = ASCIIToUTF16(" ");
  else
    title_ = ttl;
}

void PageCollector::PageInfo::set_body(const string16& bdy) {
  if (bdy.empty())
    body_ = ASCIIToUTF16(" ");
  else
    body_ = bdy;
}

bool PageCollector::PageInfo::Expired(base::TimeTicks now) const {
  return now - added_time_ > base::TimeDelta::FromSeconds(kExpirationSeconds);
}

PageCollector::PageCollector()
    : recent_changes_(RecentChangeList::NO_AUTO_EVICT),
      weak_factory_(this) {
}

PageCollector::~PageCollector() {
}

void PageCollector::Init(const HistoryPublisher* history_publisher) {
  history_publisher_ = history_publisher;
}

void PageCollector::AddPageURL(const GURL& url, base::Time time) {
  // Don't collect data which cannot be published.
  if (!history_publisher_)
    return;

  // Just save this info for later (evicting any previous data). We
  // will delete it when it expires or when all the data is complete.
  recent_changes_.Put(url, PageInfo(time));

  // Schedule flush if not already scheduled.
  if (!weak_factory_.HasWeakPtrs())
    ScheduleFlushCollected();
}

void PageCollector::AddPageTitle(const GURL& url, const string16& title) {
  if (!history_publisher_)
    return;

  // If the title comes in after the page has aged out, drop it.
  // Older code would manufacture information from the database.
  RecentChangeList::iterator found = recent_changes_.Peek(url);
  if (found == recent_changes_.end())
    return;

  // Publish the info if complete.
  if (found->second.has_body()) {
    history_publisher_->PublishPageContent(
        found->second.visit_time(), url, title, found->second.body());
    recent_changes_.Erase(found);
  } else {
    found->second.set_title(title);
  }
}

void PageCollector::AddPageContents(const GURL& url,
                                    const string16& body) {
  if (!history_publisher_)
    return;

  // If the body comes in after the page has aged out, drop it.
  // Older code would manufacture information from the database.
  RecentChangeList::iterator found = recent_changes_.Peek(url);
  if (found == recent_changes_.end())
    return;

  // Publish the info if complete.
  if (found->second.has_title()) {
    history_publisher_->PublishPageContent(
        found->second.visit_time(), url, found->second.title(), body);
    recent_changes_.Erase(found);
  } else {
    found->second.set_body(body);
  }
}

void PageCollector::AddPageData(const GURL& url,
                                base::Time visit_time,
                                const string16& title,
                                const string16& body) {
  if (!history_publisher_)
    return;

  // Publish the item.
  history_publisher_->PublishPageContent(visit_time, url, title, body);
}

void PageCollector::ScheduleFlushCollected() {
  weak_factory_.InvalidateWeakPtrs();
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&PageCollector::FlushCollected,
                 weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromSeconds(kExpirationSeconds));
}

void PageCollector::FlushCollected() {
  base::TimeTicks now = base::TimeTicks::Now();

  // Iterate from oldest to newest publishing items which expire while
  // waiting for title or body.
  RecentChangeList::reverse_iterator iter = recent_changes_.rbegin();
  while (iter != recent_changes_.rend() && iter->second.Expired(now)) {
    AddPageData(iter->first, iter->second.visit_time(),
                iter->second.title(), iter->second.body());
    iter = recent_changes_.Erase(iter);
  }

  if (!recent_changes_.empty())
    ScheduleFlushCollected();
}

}  // namespace history
