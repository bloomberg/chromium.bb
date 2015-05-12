// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BAD_MESSAGE_H_
#define CONTENT_BROWSER_BAD_MESSAGE_H_

namespace content {
class RenderProcessHost;

namespace bad_message {

// The browser process often chooses to terminate a renderer if it receives
// a bad IPC message. The reasons are tracked for metrics.
//
// Content embedders should implement their own bad message statistics but
// should use similar histogram names to make analysis easier.
//
// NOTE: Do not remove or reorder elements in this list. Add new entries at the
// end. Items may be renamed but do not change the values. We rely on the enum
// values in histograms. Also update histograms.xml with any new values.
enum BadMessageReason {
  NC_IN_PAGE_NAVIGATION = 0,
  RFH_CAN_COMMIT_URL_BLOCKED = 1,
  RFH_CAN_ACCESS_FILES_OF_PAGE_STATE = 2,
  RFH_SANDBOX_FLAGS = 3,
  RFH_NO_PROXY_TO_PARENT = 4,
  RPH_DESERIALIZATION_FAILED = 5,
  RVH_CAN_ACCESS_FILES_OF_PAGE_STATE = 6,
  RVH_FILE_CHOOSER_PATH = 7,
  RWH_SYNTHETIC_GESTURE = 8,
  RWH_FOCUS = 9,
  RWH_BLUR = 10,
  RWH_SHARED_BITMAP = 11,
  RWH_BAD_ACK_MESSAGE = 12,
  RWHVA_SHARED_MEMORY = 13,
  SERVICE_WORKER_BAD_URL = 14,
  WC_INVALID_FRAME_SOURCE = 15,
  RWHVM_UNEXPECTED_FRAME_TYPE = 16,
  RFPH_DETACH = 17,
  DFH_BAD_EMBEDDER_MESSAGE = 18,
  NC_AUTO_SUBFRAME = 19,
  // Please add new elements here. The naming convention is abbreviated class
  // name (e.g. RenderFrameHost becomes RFH) plus a unique description of the
  // reason.
  BAD_MESSAGE_MAX
};

// Called when the browser receives a bad IPC message from a renderer process.
// Logs the event, records a histogram metric for the |reason|, and terminates
// the process for |host|.
void ReceivedBadMessage(RenderProcessHost* host, BadMessageReason reason);

}  // namespace bad_message
}  // namespace content

#endif  // CONTENT_BROWSER_BAD_MESSAGE_H_
