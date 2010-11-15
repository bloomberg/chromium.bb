// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Utilities shared by both Broker RPC client and server.

#ifndef CEEE_IE_BROKER_BROKER_RPC_UTILS_H_
#define CEEE_IE_BROKER_BROKER_RPC_UTILS_H_

#include <string>

// Protocol used for RPC comunication.
extern const wchar_t kRpcProtocol[];

// Returns RPC end point. Depends on current session ID and
// account used by chrome frame.
std::wstring GetRpcEndPointAddress();

#endif  // CEEE_IE_BROKER_BROKER_RPC_UTILS_H_
