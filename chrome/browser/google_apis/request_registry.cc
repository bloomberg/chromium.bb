// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google_apis/request_registry.h"

namespace google_apis {

RequestRegistry::Request::Request(RequestRegistry* registry)
    : registry_(registry), id_(-1) {
}

RequestRegistry::Request::~Request() {
}

void RequestRegistry::Request::NotifyStart() {
  registry_->OnRequestStart(this, &id_);
}

void RequestRegistry::Request::NotifyFinish() {
  registry_->OnRequestFinish(id_);
}

RequestRegistry::RequestRegistry() {
  in_flight_requests_.set_check_on_null_data(true);
}

RequestRegistry::~RequestRegistry() {
}

void RequestRegistry::OnRequestStart(
    RequestRegistry::Request* request,
    RequestID* id) {
  *id = in_flight_requests_.Add(request);
}

void RequestRegistry::OnRequestFinish(RequestID id) {
  if (in_flight_requests_.Lookup(id))
    in_flight_requests_.Remove(id);
}

}  // namespace google_apis
