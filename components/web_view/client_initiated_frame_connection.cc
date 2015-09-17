// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_view/client_initiated_frame_connection.h"

namespace web_view {

ClientInitiatedFrameConnection::ClientInitiatedFrameConnection(
    FrameTreeClientPtr frame_tree_client)
    : frame_tree_client_(frame_tree_client.Pass()) {}

ClientInitiatedFrameConnection::~ClientInitiatedFrameConnection() {}

}  // namespace web_view
