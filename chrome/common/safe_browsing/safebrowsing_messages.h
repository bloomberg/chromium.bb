// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message file, so no include guard.

#include "googleurl/src/gurl.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_platform_file.h"

#define IPC_MESSAGE_START SafeBrowsingMsgStart

// A node is essentially a frame.
IPC_STRUCT_BEGIN(SafeBrowsingHostMsg_MalwareDOMDetails_Node)
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

// Inform the browser that the URL in the given ClientPhishingRequest proto is
// phishing according to the client-side phishing detector.
// TODO(noelutz): we may want to create custom ParamTraits for MessageLite to
// have a generic way to send protocol messages over IPC.
IPC_MESSAGE_ROUTED1(SafeBrowsingHostMsg_DetectedPhishingSite,
                    std::string /* encoded ClientPhishingRequest proto */)

// Send part of the DOM to the browser, to be used in a malware report.
IPC_MESSAGE_ROUTED1(SafeBrowsingHostMsg_MalwareDOMDetails,
                    std::vector<SafeBrowsingHostMsg_MalwareDOMDetails_Node>)

// SafeBrowsing client-side detection messages sent from the browser to the
// renderer.

// A classification model for client-side phishing detection.
// The given file contains an encoded safe_browsing::ClientSideModel
// protocol buffer.
IPC_MESSAGE_CONTROL1(SafeBrowsingMsg_SetPhishingModel,
                     IPC::PlatformFileForTransit /* model_file */)

// Request a DOM tree when a malware interstitial is shown.
IPC_MESSAGE_ROUTED0(SafeBrowsingMsg_GetMalwareDOMDetails)

// Tells the renderer to begin phishing detection for the given toplevel URL
// which it has started loading.
IPC_MESSAGE_ROUTED1(SafeBrowsingMsg_StartPhishingDetection,
                    GURL)
