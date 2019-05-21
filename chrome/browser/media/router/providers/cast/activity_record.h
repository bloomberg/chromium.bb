// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_ACTIVITY_RECORD_H_
#define CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_ACTIVITY_RECORD_H_

#include <string>

#include "chrome/common/media_router/media_route.h"

namespace media_router {

class ActivityRecord {
 public:
  ActivityRecord(const MediaRoute& route, const std::string& app_id);
  ActivityRecord(const ActivityRecord&) = delete;
  ActivityRecord& operator=(const ActivityRecord&) = delete;
  virtual ~ActivityRecord();

  const MediaRoute& route() const { return route_; }
  const std::string& app_id() const { return app_id_; }

 protected:
  MediaRoute route_;
  const std::string app_id_;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_ACTIVITY_RECORD_H_
