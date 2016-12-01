// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/safe_browsing/threat_dom_details.h"

#include "base/compiler_specific.h"
#include "components/safe_browsing/common/safebrowsing_messages.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebElementCollection.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"

namespace safe_browsing {

namespace {

// Handler for the various HTML elements that we extract URLs from.
void HandleElement(
    const blink::WebElement& element,
    SafeBrowsingHostMsg_ThreatDOMDetails_Node* parent_node,
    std::vector<SafeBrowsingHostMsg_ThreatDOMDetails_Node>* resources) {
  if (!element.hasAttribute("src"))
    return;

  // Retrieve the link and resolve the link in case it's relative.
  blink::WebURL full_url =
      element.document().completeURL(element.getAttribute("src"));

  const GURL& child_url = GURL(full_url);

  // Add to the parent node.
  parent_node->children.push_back(child_url);

  // Create the child node.
  SafeBrowsingHostMsg_ThreatDOMDetails_Node child_node;
  child_node.url = child_url;
  child_node.tag_name = element.tagName().utf8();
  child_node.parent = parent_node->url;
  resources->push_back(child_node);
}

}  // namespace

// An upper limit on the number of nodes we collect.
uint32_t ThreatDOMDetails::kMaxNodes = 500;

// static
ThreatDOMDetails* ThreatDOMDetails::Create(content::RenderFrame* render_frame) {
  // Private constructor and public static Create() method to facilitate
  // stubbing out this class for binary-size reduction purposes.
  return new ThreatDOMDetails(render_frame);
}

ThreatDOMDetails::ThreatDOMDetails(content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame) {}

ThreatDOMDetails::~ThreatDOMDetails() {}

bool ThreatDOMDetails::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ThreatDOMDetails, message)
    IPC_MESSAGE_HANDLER(SafeBrowsingMsg_GetThreatDOMDetails,
                        OnGetThreatDOMDetails)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ThreatDOMDetails::OnGetThreatDOMDetails() {
  std::vector<SafeBrowsingHostMsg_ThreatDOMDetails_Node> resources;
  ExtractResources(&resources);
  // Notify the browser.
  Send(new SafeBrowsingHostMsg_ThreatDOMDetails(routing_id(), resources));
}

void ThreatDOMDetails::ExtractResources(
    std::vector<SafeBrowsingHostMsg_ThreatDOMDetails_Node>* resources) {
  blink::WebFrame* frame = render_frame()->GetWebFrame();
  if (!frame)
    return;
  SafeBrowsingHostMsg_ThreatDOMDetails_Node details_node;
  blink::WebDocument document = frame->document();
  details_node.url = GURL(document.url());
  if (document.isNull()) {
    // Nothing in this frame. Just report its URL.
    resources->push_back(details_node);
    return;
  }
  blink::WebElementCollection elements = document.all();
  blink::WebElement element = elements.firstItem();
  for (; !element.isNull(); element = elements.nextItem()) {
    if (element.hasHTMLTagName("iframe") || element.hasHTMLTagName("frame") ||
        element.hasHTMLTagName("embed") || element.hasHTMLTagName("script")) {
      HandleElement(element, &details_node, resources);
      if (resources->size() >= kMaxNodes) {
        // We have reached kMaxNodes, exit early.
        resources->push_back(details_node);
        return;
      }
    }
  }
  resources->push_back(details_node);
}

void ThreatDOMDetails::OnDestruct() {
  delete this;
}

}  // namespace safe_browsing
