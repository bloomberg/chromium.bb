// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message file, so no include guard.

#include <stdint.h>
#include <string>
#include <vector>

#include "ipc/ipc_message_macros.h"
#include "url/gurl.h"
#include "url/ipc/url_param_traits.h"

#define IPC_MESSAGE_START SafeBrowsingMsgStart

// SafeBrowsing client-side detection messages sent from the browser to the
// renderer.

#if defined(FULL_SAFE_BROWSING)
// A classification model for client-side phishing detection.
// The string is an encoded safe_browsing::ClientSideModel protocol buffer, or
// empty to disable client-side phishing detection for this renderer.
IPC_MESSAGE_CONTROL1(SafeBrowsingMsg_SetPhishingModel,
                     std::string /* encoded ClientSideModel proto */)

#endif
