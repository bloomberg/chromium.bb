// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/browsing_data_partition_client.h"

namespace web {

namespace {

static BrowsingDataPartitionClient* g_client;

}  // namespace

void SetBrowsingDataPartitionClient(BrowsingDataPartitionClient* client) {
  g_client = client;
}

BrowsingDataPartitionClient* GetBrowsingDataPartitionClient() {
  return g_client;
}

BrowsingDataPartitionClient::BrowsingDataPartitionClient() {}
BrowsingDataPartitionClient::~BrowsingDataPartitionClient() {}

}  // namespace web
