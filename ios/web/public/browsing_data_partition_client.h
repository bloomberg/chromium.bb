// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_BROWSING_DATA_PARTITION_CLIENT_H_
#define IOS_WEB_PUBLIC_BROWSING_DATA_PARTITION_CLIENT_H_

#include "base/macros.h"

namespace web {

class BrowsingDataPartitionClient;

// Setter and getter for the client.
void SetBrowsingDataPartitionClient(BrowsingDataPartitionClient* client);
BrowsingDataPartitionClient* GetBrowsingDataPartitionClient();

// Interface that the embedder of the web layer implements. Informs embedders
// when the BrowsingDataPartition is synchronized.
class BrowsingDataPartitionClient {
 public:
  BrowsingDataPartitionClient();
  virtual ~BrowsingDataPartitionClient();

  // Called when web::BrowsingDataPartition becomes synchronized.
  virtual void DidBecomeSynchronized() const {}

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowsingDataPartitionClient);
};

}  // namespace web

#endif  // IOS_NET_COOKIES_COOKIE_STORE_IOS_CLIENT_H_
