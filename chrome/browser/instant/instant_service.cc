// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/instant/instant_service.h"

InstantService::InstantService() {
}

InstantService::~InstantService() {
}

void InstantService::AddInstantProcess(int process_id) {
  process_ids_.insert(process_id);
}

void InstantService::RemoveInstantProcess(int process_id) {
  process_ids_.erase(process_id);
}

bool InstantService::IsInstantProcess(int process_id) const {
  return process_ids_.count(process_id) != 0;
}

int InstantService::GetInstantProcessCount() const {
  return process_ids_.size();
}

void InstantService::Shutdown() {
  process_ids_.clear();
}
