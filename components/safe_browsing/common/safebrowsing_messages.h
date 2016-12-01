// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message file, so no include guard.

#include <string>
#include <vector>

#include "ipc/ipc_message_macros.h"
#include "url/gurl.h"
#include "url/ipc/url_param_traits.h"

#define IPC_MESSAGE_START SafeBrowsingMsgStart

// A node is essentially a frame.
IPC_STRUCT_BEGIN(SafeBrowsingHostMsg_ThreatDOMDetails_Node)
  // URL of this resource. Can be empty.
  IPC_STRUCT_MEMBER(GURL, url)

  // If this resource was in the "src" attribute of a tag, this is the tagname
  // (eg "IFRAME"). Can be empty.
  IPC_STRUCT_MEMBER(std::string, tag_name)

  // URL of the parent node. Can be empty.
  IPC_STRUCT_MEMBER(GURL, parent)

  // children of this node. Can be emtpy.
  IPC_STRUCT_MEMBER(std::vector<GURL>, children)
IPC_STRUCT_END()

// SafeBrowsing client-side detection messages sent from the renderer to the
// browser.

// Send part of the DOM to the browser, to be used in a threat report.
IPC_MESSAGE_ROUTED1(SafeBrowsingHostMsg_ThreatDOMDetails,
                    std::vector<SafeBrowsingHostMsg_ThreatDOMDetails_Node>)

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

// Request a DOM tree when a safe browsing interstitial is shown.
IPC_MESSAGE_ROUTED0(SafeBrowsingMsg_GetThreatDOMDetails)

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
