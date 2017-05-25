// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message file, so no include guard.

#include <stdint.h>
#include <string>
#include <vector>

#include "components/safe_browsing/common/safebrowsing_types.h"
#include "ipc/ipc_message_macros.h"
#include "url/gurl.h"
#include "url/ipc/url_param_traits.h"

#define IPC_MESSAGE_START SafeBrowsingMsgStart

// A node is essentially a frame.
IPC_STRUCT_BEGIN(SafeBrowsingHostMsg_ThreatDOMDetails_Node)
  // A unique ID for this node, unique to the current Render Frame.
  IPC_STRUCT_MEMBER(int32_t, node_id)

  // URL of this resource. Can be empty.
  IPC_STRUCT_MEMBER(GURL, url)

  // If this resource was in the "src" attribute of a tag, this is the tagname
  // (eg "IFRAME"). Can be empty.
  IPC_STRUCT_MEMBER(std::string, tag_name)

  // URL of the parent node. Can be empty.
  IPC_STRUCT_MEMBER(GURL, parent)

  // The unique ID of the parent node. Can be 0 if this is the top node.
  IPC_STRUCT_MEMBER(int32_t, parent_node_id)

  // children of this node. Can be emtpy.
  IPC_STRUCT_MEMBER(std::vector<GURL>, children)

  // The unique IDs of the child nodes. Can be empty if there are no children.
  IPC_STRUCT_MEMBER(std::vector<int32_t>, child_node_ids)

  // The node's attributes, as a collection of name-value pairs.
  IPC_STRUCT_MEMBER(std::vector<safe_browsing::AttributeNameValue>, attributes)

  // If this node represents a frame or iframe, then this field is set to the
  // routing ID of the local or remote frame in this renderer process that is
  // responsible for rendering the contents of this frame (to handle OOPIFs).
  IPC_STRUCT_MEMBER(int32_t, child_frame_routing_id)
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
