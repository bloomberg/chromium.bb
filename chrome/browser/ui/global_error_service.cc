// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_error_service.h"

#include <algorithm>

#include "base/stl_util.h"
#include "chrome/browser/ui/global_error.h"

GlobalErrorService::GlobalErrorService() {
}

GlobalErrorService::~GlobalErrorService() {
  STLDeleteElements(&errors_);
}

void GlobalErrorService::AddGlobalError(GlobalError* error) {
  errors_.push_back(error);
}

void GlobalErrorService::RemoveGlobalError(GlobalError* error) {
  errors_.erase(std::find(errors_.begin(), errors_.end(), error));
}

GlobalError* GlobalErrorService::GetGlobalErrorByMenuItemCommandID(
    int command_id) const {
  for (std::vector<GlobalError*>::const_iterator
       it = errors_.begin(); it != errors_.end(); ++it) {
    GlobalError* error = *it;
    if (error->HasMenuItem() && command_id == error->MenuItemCommandID())
      return error;
  }
  return NULL;
}

int GlobalErrorService::GetFirstBadgeResourceID() {
  for (std::vector<GlobalError*>::const_iterator
       it = errors_.begin(); it != errors_.end(); ++it) {
    GlobalError* error = *it;
    if (error->HasBadge())
      return error->GetBadgeResourceID();
  }
  return 0;
}
