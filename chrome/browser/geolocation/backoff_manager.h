// Copyright 2008, Google Inc.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//  1. Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//  2. Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//  3. Neither the name of Google Inc. nor the names of its contributors may be
//     used to endorse or promote products derived from this software without
//     specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
// WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
// EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
// OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// The BackoffManager class is used to implement exponential back-off for
// network requests in case of sever errors. Users report to the BackoffManager
// class when they make a request to or receive a response from a given url. The
// BackoffManager class provides the earliest time at which subsequent requests
// should be made.

#ifndef GEARS_GEOLOCATION_BACKOFF_MANAGER_H__
#define GEARS_GEOLOCATION_BACKOFF_MANAGER_H__

// TODO(joth): port to chromium
#if 0

#include "gears/base/common/common.h"
#include "gears/base/common/mutex.h"
#include "gears/base/common/stopwatch.h"  // For GetCurrentTimeMillis
#include <map>

class BackoffManager {
 public:
  static void ReportRequest(const std::string16 &url);
  static int64 ReportResponse(const std::string16 &url, bool server_error);

 private:
  // A map from server URL to a pair of integers representing the last request
  // time and the current minimum interval between requests, both in
  // milliseconds.
  typedef std::map<std::string16, std::pair<int64, int64> > ServerMap;
  static ServerMap servers_;

  // The mutex used to protect the map.
  static Mutex servers_mutex_;

  DISALLOW_EVIL_CONSTRUCTORS(BackoffManager);
};

#endif  // if 0

#endif  // GEARS_GEOLOCATION_BACKOFF_MANAGER_H__
