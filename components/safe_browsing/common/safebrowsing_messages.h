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

#if defined(FULL_SAFE_BROWSING)
// Inform the browser that the client-side phishing detector running in the
// renderer is done classifying the current URL.  If the URL is phishing
// the request proto will have |is_phishing()| set to true.
// TODO(noelutz): we may want to create custom ParamTraits for MessageLite to
// have a generic way to send protocol messages over IPC.
IPC_MESSAGE_ROUTED1(SafeBrowsingHostMsg_PhishingDetectionDone,
                    std::string /* encoded ClientPhishingRequest proto */)
#endif

// SafeBrowsing client-side detection messages sent from the browser to the
// renderer.

#if defined(FULL_SAFE_BROWSING)
// A classification model for client-side phishing detection.
// The string is an encoded safe_browsing::ClientSideModel protocol buffer, or
// empty to disable client-side phishing detection for this renderer.
IPC_MESSAGE_CONTROL1(SafeBrowsingMsg_SetPhishingModel,
                     std::string /* encoded ClientSideModel proto */)

// Tells the renderer to begin phishing detection for the given toplevel URL
// which it has started loading.
IPC_MESSAGE_ROUTED1(SafeBrowsingMsg_StartPhishingDetection,
                    GURL)
#endif
