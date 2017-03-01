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

#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "content/public/renderer/render_frame_observer.h"

struct SafeBrowsingHostMsg_ThreatDOMDetails_Node;

namespace safe_browsing {

extern const base::Feature kThreatDomDetailsTagAndAttributeFeature;
extern const char kTagAndAttributeParamName[];

// Represents the tag name of an HTML Element and its associated attributes.
// Used to determine which elements to collect. Populated from the param value
// of |kThreatDomDetailsTagAndAttributeFeature|.
class TagAndAttributesItem {
 public:
  TagAndAttributesItem();
  TagAndAttributesItem(const TagAndAttributesItem& item);
  ~TagAndAttributesItem();

  std::string tag_name;
  std::vector<std::string> attributes;
};

// There is one ThreatDOMDetails per RenderFrame.
class ThreatDOMDetails : public content::RenderFrameObserver {
 public:
  // An upper limit on the number of nodes we collect. Not const for the test.
  static uint32_t kMaxNodes;

  static ThreatDOMDetails* Create(content::RenderFrame* render_frame);
  ~ThreatDOMDetails() override;

  // Begins extracting resource urls for the page currently loaded in
  // this object's RenderFrame.
  // Exposed for testing.
  void ExtractResources(
      std::vector<SafeBrowsingHostMsg_ThreatDOMDetails_Node>* resources);

 private:
  // Creates a ThreatDOMDetails for the specified RenderFrame.
  // The ThreatDOMDetails should be destroyed prior to destroying
  // the RenderFrame.
  explicit ThreatDOMDetails(content::RenderFrame* render_frame);

  // RenderFrameObserver implementation.
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnDestruct() override;

  void OnGetThreatDOMDetails();

  // A list of tag names and associates attributes, used to determine which
  // elements need to be collected.
  std::vector<TagAndAttributesItem> tag_and_attributes_list_;

  DISALLOW_COPY_AND_ASSIGN(ThreatDOMDetails);
};

}  // namespace safe_browsing

#endif  // CHROME_RENDERER_SAFE_BROWSING_THREAT_DOM_DETAILS_H_
