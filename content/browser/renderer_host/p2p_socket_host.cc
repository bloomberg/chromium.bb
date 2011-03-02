// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/p2p_socket_host.h"

P2PSocketHost::P2PSocketHost(P2PSocketsHost* host, int routing_id, int id)
    : host_(host), routing_id_(routing_id), id_(id) {
}

P2PSocketHost::~P2PSocketHost() { }
