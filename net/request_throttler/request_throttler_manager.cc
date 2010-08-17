// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/request_throttler/request_throttler_manager.h"

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/rand_util.h"
#include "base/string_util.h"

const unsigned int RequestThrottlerManager::kMaximumNumberOfEntries = 1500;
const unsigned int RequestThrottlerManager::kRequestsBetweenCollecting = 200;

scoped_refptr<RequestThrottlerEntryInterface>
    RequestThrottlerManager::RegisterRequestUrl(
        const GURL &url) {
  DCHECK_EQ(MessageLoop::current()->type(), MessageLoop::TYPE_IO);

  // Periodically garbage collect old entries.
  requests_since_last_gc_++;
  if (requests_since_last_gc_ >= kRequestsBetweenCollecting) {
    GarbageCollectEntries();
    requests_since_last_gc_ = 0;
  }

  // Normalize the url
  std::string url_id = GetIdFromUrl(url);

  // Finds the entry in the map or creates it.
  scoped_refptr<RequestThrottlerEntry>& entry = url_entries_[url_id];
  if (entry == NULL)
    entry = new RequestThrottlerEntry();

  return entry;
}

RequestThrottlerManager::RequestThrottlerManager()
    : current_loop_(MessageLoop::current()),
      requests_since_last_gc_(0) {
}

RequestThrottlerManager::~RequestThrottlerManager() {
  // Deletes all entries
  url_entries_.clear();
}

std::string RequestThrottlerManager::GetIdFromUrl(const GURL& url) {
  std::string url_id;
  url_id += url.scheme();
  url_id += "://";
  url_id += url.host();
  url_id += url.path();

  return StringToLowerASCII(url_id);
}

void RequestThrottlerManager::GarbageCollectEntries() {
  DCHECK_EQ(MessageLoop::current()->type(), MessageLoop::TYPE_IO);
  UrlEntryMap::iterator i = url_entries_.begin();

  while (i != url_entries_.end()) {
    if ((i->second)->IsEntryOutdated()) {
      url_entries_.erase(i++);
    } else {
      ++i;
    }
  }

  // In case something broke we want to make sure not to grow indefinitely.
  while (url_entries_.size() > kMaximumNumberOfEntries) {
    url_entries_.erase(url_entries_.begin());
  }
}

void RequestThrottlerManager::NotifyRequestBodyWasMalformed(const GURL& url) {
  DCHECK_EQ(MessageLoop::current()->type(), MessageLoop::TYPE_IO);
  // Normalize the url
  std::string url_id = GetIdFromUrl(url);
  UrlEntryMap::iterator i = url_entries_.find(url_id);

  if (i != url_entries_.end()) {
    i->second->ReceivedContentWasMalformed();
  }
}
