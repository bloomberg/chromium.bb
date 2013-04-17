// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity/identity_mint_queue.h"

#include "base/logging.h"
#include "base/stl_util.h"

namespace extensions {

IdentityMintRequestQueue::IdentityMintRequestQueue() {
}

IdentityMintRequestQueue::~IdentityMintRequestQueue() {
  std::map<RequestKey, RequestList>::const_iterator it;
  for (it = request_queue_.begin(); it != request_queue_.end(); ++it)
    DCHECK_EQ(it->second.size(), 0lu);
}

IdentityMintRequestQueue::RequestKey::RequestKey(
    IdentityMintRequestQueue::MintType type,
    const std::string& extension_id,
    const std::set<std::string> scopes) : type(type),
                                          extension_id(extension_id),
                                          scopes(scopes) {
}

IdentityMintRequestQueue::RequestKey::~RequestKey() {
}

bool IdentityMintRequestQueue::RequestKey::operator<(
    const RequestKey& rhs) const {
  if (type < rhs.type)
    return true;
  else if (rhs.type < type)
    return false;

  if (extension_id < rhs.extension_id)
    return true;
  else if (rhs.extension_id < extension_id)
    return false;

  return scopes < rhs.scopes;
}

void IdentityMintRequestQueue::RequestStart(
    IdentityMintRequestQueue::MintType type,
    const std::string& extension_id,
    const std::set<std::string> scopes,
    IdentityMintRequestQueue::Request* request) {
  RequestKey key(type, extension_id, scopes);
  request_queue_[key].push_back(request);
  // If this is the first request, start it now. RequestComplete will start
  // all other requests.
  if (request_queue_[key].size() == 1)
    request_queue_[key].front()->StartMintToken(type);
}

void IdentityMintRequestQueue::RequestComplete(
    IdentityMintRequestQueue::MintType type,
    const std::string& extension_id,
    const std::set<std::string> scopes,
    IdentityMintRequestQueue::Request* request) {
  RequestKey key(type, extension_id, scopes);
  CHECK(request_queue_[key].front() == request);
  request_queue_[key].pop_front();
  if (request_queue_[key].size() > 0)
    request_queue_[key].front()->StartMintToken(type);
}

bool IdentityMintRequestQueue::empty(IdentityMintRequestQueue::MintType type,
                                     const std::string& extension_id,
                                     const std::set<std::string> scopes) const {
  RequestKey key(type, extension_id, scopes);
  return !ContainsKey(request_queue_, key) ||
      (request_queue_.find(key))->second.empty();
}

}  // namespace extensions
