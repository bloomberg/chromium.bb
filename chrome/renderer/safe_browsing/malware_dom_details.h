// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// MalwareDOMDetails iterates over a document's frames and gathers
// interesting URLs such as those of scripts and frames. When done, it sends
// them to the MalwareDetails that requested them.

#ifndef CHROME_RENDERER_SAFE_BROWSING_MALWARE_DOM_DETAILS_H_
#define CHROME_RENDERER_SAFE_BROWSING_MALWARE_DOM_DETAILS_H_

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "content/public/renderer/render_view_observer.h"

struct SafeBrowsingHostMsg_MalwareDOMDetails_Node;

namespace safe_browsing {

// There is one MalwareDOMDetails per RenderView.
class MalwareDOMDetails : public content::RenderViewObserver {
 public:
  // An upper limit on the number of nodes we collect. Not const for the test.
  static uint32 kMaxNodes;

  static MalwareDOMDetails* Create(content::RenderView* render_view);
  virtual ~MalwareDOMDetails();

  // Begins extracting resource urls for the page currently loaded in
  // this object's RenderView.
  // Exposed for testing.
  void ExtractResources(
      std::vector<SafeBrowsingHostMsg_MalwareDOMDetails_Node>* resources);

 private:
  // Creates a MalwareDOMDetails for the specified RenderView.
  // The MalwareDOMDetails should be destroyed prior to destroying
  // the RenderView.
  explicit MalwareDOMDetails(content::RenderView* render_view);

  // RenderViewObserver implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  void OnGetMalwareDOMDetails();

  DISALLOW_COPY_AND_ASSIGN(MalwareDOMDetails);
};

}  // namespace safe_browsing

#endif  // CHROME_RENDERER_SAFE_BROWSING_MALWARE_DOM_DETAILS_H_
