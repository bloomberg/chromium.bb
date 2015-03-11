// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/route_id_manager.h"

#include "base/memory/singleton.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/media/router/media_route.h"

namespace media_router {

const char kLocalRoutePrefix[] = "route-local-";

RouteIDManager::RouteIDManager() : next_local_id_(0) {}

std::string RouteIDManager::ForLocalRoute() {
  DCHECK(CalledOnValidThread());
  return kLocalRoutePrefix + base::Uint64ToString(next_local_id_++);
}

// static
RouteIDManager* RouteIDManager::GetInstance() {
  return Singleton<RouteIDManager>::get();
}

}  // namespace media_router
