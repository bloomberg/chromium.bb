// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/cdm_promise_adapter.h"

#include "media/base/media_keys.h"

namespace media {

CdmPromiseAdapter::CdmPromiseAdapter() : next_promise_id_(1) {
}

CdmPromiseAdapter::~CdmPromiseAdapter() {
}

uint32_t CdmPromiseAdapter::SavePromise(scoped_ptr<CdmPromise> promise) {
  uint32_t promise_id = next_promise_id_++;
  promises_.add(promise_id, promise.Pass());
  return promise_id;
}

scoped_ptr<CdmPromise> CdmPromiseAdapter::TakePromise(uint32_t promise_id) {
  PromiseMap::iterator it = promises_.find(promise_id);
  if (it == promises_.end())
    return nullptr;
  return promises_.take_and_erase(it);
}

void CdmPromiseAdapter::Clear() {
  // Reject all outstanding promises.
  for (auto& promise : promises_)
    promise.second->reject(MediaKeys::UNKNOWN_ERROR, 0, "Operation aborted.");
  promises_.clear();
}

}  // namespace media
