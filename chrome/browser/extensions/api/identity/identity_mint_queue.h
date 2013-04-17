// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_MINT_QUEUE_H_
#define CHROME_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_MINT_QUEUE_H_

#include <list>
#include <map>
#include <set>
#include <string>

namespace extensions {

// getAuthToken requests are serialized to avoid excessive traffic to
// GAIA and to consolidate UI pop-ups. IdentityMintRequestQueue
// maitains a set of queues, one for each RequestKey.
//
// The queue calls StartMintToken on each Request when it reaches the
// head of the line.
//
// The queue does not own Requests. Request pointers must be valid
// until they are removed from the queue with RequestComplete.
class IdentityMintRequestQueue {
 public:
  enum MintType {
    MINT_TYPE_NONINTERACTIVE,
    MINT_TYPE_INTERACTIVE
  };

  IdentityMintRequestQueue();
  virtual ~IdentityMintRequestQueue();

  class Request {
   public:
    virtual ~Request() {}
    virtual void StartMintToken(IdentityMintRequestQueue::MintType type) = 0;
  };

  struct RequestKey {
    RequestKey(IdentityMintRequestQueue::MintType type,
               const std::string& extension_id,
               const std::set<std::string> scopes);
    ~RequestKey();
    bool operator<(const RequestKey& rhs) const;
    IdentityMintRequestQueue::MintType type;
    std::string extension_id;
    std::set<std::string> scopes;
  };

  // Adds a request to the queue specified by the id and scopes.
  void RequestStart(IdentityMintRequestQueue::MintType type,
                    const std::string& extension_id,
                    const std::set<std::string> scopes,
                    IdentityMintRequestQueue::Request* request);
  // Removes a request from the queue specified by the id and scopes.
  void RequestComplete(IdentityMintRequestQueue::MintType type,
                       const std::string& extension_id,
                       const std::set<std::string> scopes,
                       IdentityMintRequestQueue::Request* request);
  bool empty(IdentityMintRequestQueue::MintType type,
             const std::string& extension_id,
             const std::set<std::string> scopes) const;

 private:
  typedef std::list<IdentityMintRequestQueue::Request*> RequestList;
  std::map<RequestKey, RequestList> request_queue_;
};


}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_MINT_QUEUE_H_
