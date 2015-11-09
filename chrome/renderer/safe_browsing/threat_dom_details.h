// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ThreatDOMDetails iterates over a document's frames and gathers
// interesting URLs such as those of scripts and frames. When done, it sends
// them to the ThreatDetails that requested them.

#ifndef CHROME_RENDERER_SAFE_BROWSING_THREAT_DOM_DETAILS_H_
#define CHROME_RENDERER_SAFE_BROWSING_THREAT_DOM_DETAILS_H_

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "content/public/renderer/render_view_observer.h"

struct SafeBrowsingHostMsg_ThreatDOMDetails_Node;

namespace safe_browsing {

// There is one ThreatDOMDetails per RenderView.
class ThreatDOMDetails : public content::RenderViewObserver {
 public:
  // An upper limit on the number of nodes we collect. Not const for the test.
  static uint32 kMaxNodes;

  static ThreatDOMDetails* Create(content::RenderView* render_view);
  ~ThreatDOMDetails() override;

  // Begins extracting resource urls for the page currently loaded in
  // this object's RenderView.
  // Exposed for testing.
  void ExtractResources(
      std::vector<SafeBrowsingHostMsg_ThreatDOMDetails_Node>* resources);

 private:
  // Creates a ThreatDOMDetails for the specified RenderView.
  // The ThreatDOMDetails should be destroyed prior to destroying
  // the RenderView.
  explicit ThreatDOMDetails(content::RenderView* render_view);

  // RenderViewObserver implementation.
  bool OnMessageReceived(const IPC::Message& message) override;

  void OnGetThreatDOMDetails();

  DISALLOW_COPY_AND_ASSIGN(ThreatDOMDetails);
};

}  // namespace safe_browsing

#endif  // CHROME_RENDERER_SAFE_BROWSING_THREAT_DOM_DETAILS_H_
