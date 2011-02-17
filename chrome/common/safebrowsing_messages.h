// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_SAFEBROWSING_MESSAGES_H_
#define CHROME_COMMON_SAFEBROWSING_MESSAGES_H_
#pragma once

#include "googleurl/src/gurl.h"
#include "ipc/ipc_message_macros.h"

#define IPC_MESSAGE_START SafeBrowsingMsgStart

// SafeBrowsing client-side detection messages sent from the renderer to the
// browser.

// Inform the browser that the current URL is phishing according to the
// client-side phishing detector.
IPC_MESSAGE_ROUTED2(SafeBrowsingDetectionHostMsg_DetectedPhishingSite,
                    GURL /* phishing_url */,
                    double /* phishing_score */)

// SafeBrowsing client-side detection messages sent from the browser to the
// renderer.  TODO(noelutz): move other IPC messages here.

#endif  // CHROME_COMMON_SAFEBROWSING_MESSAGES_H_
