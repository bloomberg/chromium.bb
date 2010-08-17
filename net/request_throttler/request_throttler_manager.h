// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_REQUEST_THROTTLER_REQUEST_THROTTLER_MANAGER_H_
#define NET_REQUEST_THROTTLER_REQUEST_THROTTLER_MANAGER_H_

#include <map>
#include <string>

#include "base/lock.h"
#include "base/non_thread_safe.h"
#include "base/singleton.h"
#include "base/scoped_ptr.h"
#include "base/time.h"
#include "googleurl/src/gurl.h"
#include "net/request_throttler/request_throttler_entry.h"

class MessageLoop;

// Class that registers a request throttler entry for each URL being accessed in
// order to supervise traffic. URL Http Request should register their URL in
// this request throttler manager on each request.
class RequestThrottlerManager  {
 public:
  // Must be called for every request, returns the request throttler entry
  // associated with the URL. The caller must inform this entry of some events.
  // Please refer to request_throttler_entry.h for further informations.
  scoped_refptr<RequestThrottlerEntryInterface> RegisterRequestUrl(
      const GURL& url);

  // This method is used by higher level modules which can notify this class if
  // the response they received was malformed. It is useful because
  // in the case where the response header returned 200 response code but had
  // a malformed content body we will categorize it as a success and so we
  // defer this decision to the module that had requested the resource.
  void NotifyRequestBodyWasMalformed(const GURL& url);

 protected:
  RequestThrottlerManager();
  virtual ~RequestThrottlerManager();

  // From each URL we generate an ID composed of the host and path
  // that allows us to uniquely map an entry to it.
  typedef std::map<std::string, scoped_refptr<RequestThrottlerEntry> >
      UrlEntryMap;

  friend struct DefaultSingletonTraits<RequestThrottlerManager>;

  // Map that contains a list of URL ID and their matching
  // RequestThrottlerEntry.
  UrlEntryMap url_entries_;

  // Method that allows us to transform an URL into an ID that is useful and
  // can be used in our map. Resulting IDs will be lowercase and be missing both
  // the query string and fragment.
  std::string GetIdFromUrl(const GURL& url);

  // Method that ensures the map gets cleaned from time to time. The period at
  // which this GarbageCollection happens is adjustable with the
  // kRequestBetweenCollecting constant.
  void GarbageCollectEntries();

 private:
  MessageLoop* current_loop_;

  // Maximum number of entries that we are willing to collect in our map.
  static const unsigned int kMaximumNumberOfEntries;
  // Number of requests that will be made between garbage collection.
  static const unsigned int kRequestsBetweenCollecting;

  // This keeps track of how many request have been made. Used with
  // GarbageCollectEntries.
  unsigned int requests_since_last_gc_;

  DISALLOW_COPY_AND_ASSIGN(RequestThrottlerManager);
};

#endif  // NET_REQUEST_THROTTLER_REQUEST_THROTTLER_MANAGER_H_
