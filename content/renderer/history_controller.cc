// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
 * Copyright (C) 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved.
 *     (http://www.torchmobile.com/)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "content/renderer/history_controller.h"

#include <deque>

#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_view_impl.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/web/WebFrame.h"

using blink::WebFrame;
using blink::WebHistoryCommitType;
using blink::WebHistoryItem;
using blink::WebURLRequest;
using blink::WebVector;

namespace content {

HistoryController::HistoryController(RenderViewImpl* render_view)
    : render_view_(render_view) {
}

HistoryController::~HistoryController() {
}

void HistoryController::GoToEntry(HistoryEntry* target_entry,
                                  WebURLRequest::CachePolicy cache_policy) {
  HistoryFrameLoadVector same_document_loads;
  HistoryFrameLoadVector different_document_loads;

  provisional_entry_.reset(target_entry);

  WebFrame* main_frame = render_view_->main_render_frame()->GetWebFrame();
  if (current_entry_) {
    RecursiveGoToEntry(
        main_frame, same_document_loads, different_document_loads);
  } else {
    different_document_loads.push_back(
        std::make_pair(main_frame, provisional_entry_->root()));
  }

  if (same_document_loads.empty() && different_document_loads.empty()) {
    same_document_loads.push_back(
        std::make_pair(main_frame, provisional_entry_->root()));
  }

  if (different_document_loads.empty()) {
    previous_entry_.reset(current_entry_.release());
    current_entry_.reset(provisional_entry_.release());
  }

  for (size_t i = 0; i < same_document_loads.size(); ++i) {
    WebFrame* frame = same_document_loads[i].first;
    if (!RenderFrameImpl::FromWebFrame(frame))
      continue;
    frame->loadHistoryItem(same_document_loads[i].second,
                           blink::WebHistorySameDocumentLoad,
                           cache_policy);
  }
  for (size_t i = 0; i < different_document_loads.size(); ++i) {
    WebFrame* frame = different_document_loads[i].first;
    if (!RenderFrameImpl::FromWebFrame(frame))
      continue;
    frame->loadHistoryItem(different_document_loads[i].second,
                           blink::WebHistoryDifferentDocumentLoad,
                           cache_policy);
  }
}

void HistoryController::RecursiveGoToEntry(
    WebFrame* frame,
    HistoryFrameLoadVector& same_document_loads,
    HistoryFrameLoadVector& different_document_loads) {
  DCHECK(provisional_entry_);
  DCHECK(current_entry_);
  RenderFrameImpl* render_frame = RenderFrameImpl::FromWebFrame(frame);
  const WebHistoryItem& new_item =
      provisional_entry_->GetItemForFrame(render_frame);
  const WebHistoryItem& old_item =
      current_entry_->GetItemForFrame(render_frame);
  if (new_item.isNull())
    return;

  if (old_item.isNull() ||
      new_item.itemSequenceNumber() != old_item.itemSequenceNumber()) {
    if (!old_item.isNull() &&
        new_item.documentSequenceNumber() == old_item.documentSequenceNumber())
      same_document_loads.push_back(std::make_pair(frame, new_item));
    else
      different_document_loads.push_back(std::make_pair(frame, new_item));
    return;
  }

  for (WebFrame* child = frame->firstChild(); child;
       child = child->nextSibling()) {
    RecursiveGoToEntry(child, same_document_loads, different_document_loads);
  }
}

void HistoryController::GoToItem(const WebHistoryItem& target_item,
                                 WebURLRequest::CachePolicy cache_policy) {
  // We don't have enough information to set a correct frame id here. This
  // might be a restore from disk, and the frame ids might not match up if the
  // state was saved from a different process. Ensure the HistoryEntry's main
  // frame id matches the actual main frame id. Its subframe ids are invalid to
  // ensure they don't accidentally match a potentially random frame.
  HistoryEntry* new_entry = new HistoryEntry(
      target_item, render_view_->main_render_frame()->GetRoutingID());
  std::deque<HistoryEntry::HistoryNode*> history_nodes;
  history_nodes.push_back(new_entry->root_history_node());
  while (!history_nodes.empty()) {
    // For each item, read the children (if any) off the WebHistoryItem,
    // create a new HistoryNode for each child and attach it,
    // then clear the children on the WebHistoryItem.
    HistoryEntry::HistoryNode* history_node = history_nodes.front();
    history_nodes.pop_front();

    WebVector<WebHistoryItem> children = history_node->item().children();
    for (size_t i = 0; i < children.size(); i++) {
      HistoryEntry::HistoryNode* child_history_node =
          history_node->AddChild(children[i], kInvalidFrameRoutingID);
      history_nodes.push_back(child_history_node);
    }
    history_node->item().setChildren(WebVector<WebHistoryItem>());
  }
  GoToEntry(new_entry, cache_policy);
}

void HistoryController::UpdateForInitialLoadInChildFrame(
    RenderFrameImpl* frame,
    const WebHistoryItem& item) {
  DCHECK_NE(frame->GetWebFrame()->top(), frame->GetWebFrame());
  if (!current_entry_)
    return;
  if (HistoryEntry::HistoryNode* existing_node =
          current_entry_->GetHistoryNodeForFrame(frame)) {
    existing_node->set_item(item);
    return;
  }
  RenderFrameImpl* parent =
      RenderFrameImpl::FromWebFrame(frame->GetWebFrame()->parent());
  if (HistoryEntry::HistoryNode* parent_history_node =
          current_entry_->GetHistoryNodeForFrame(parent)) {
    parent_history_node->AddChild(item, frame->GetRoutingID());
  }
}

void HistoryController::UpdateForCommit(RenderFrameImpl* frame,
                                        const WebHistoryItem& item,
                                        WebHistoryCommitType commit_type,
                                        bool navigation_within_page) {
  if (commit_type == blink::WebBackForwardCommit) {
    if (!provisional_entry_)
      return;
    previous_entry_.reset(current_entry_.release());
    current_entry_.reset(provisional_entry_.release());
  } else if (commit_type == blink::WebStandardCommit) {
    CreateNewBackForwardItem(frame, item, navigation_within_page);
  } else if (commit_type == blink::WebInitialCommitInChildFrame) {
    UpdateForInitialLoadInChildFrame(frame, item);
  }
}

static WebHistoryItem ItemForExport(HistoryEntry::HistoryNode* history_node) {
  DCHECK(history_node);
  WebHistoryItem item = history_node->item();
  item.setChildren(WebVector<WebHistoryItem>());
  std::vector<HistoryEntry::HistoryNode*>& child_nodes =
      history_node->children();
  for (size_t i = 0; i < child_nodes.size(); i++)
    item.appendToChildren(ItemForExport(child_nodes.at(i)));
  return item;
}

WebHistoryItem HistoryController::GetCurrentItemForExport() {
  if (!current_entry_)
    return WebHistoryItem();
  return ItemForExport(current_entry_->root_history_node());
}

WebHistoryItem HistoryController::GetPreviousItemForExport() {
  if (!previous_entry_)
    return WebHistoryItem();
  return ItemForExport(previous_entry_->root_history_node());
}

WebHistoryItem HistoryController::GetItemForNewChildFrame(
    RenderFrameImpl* frame) const {
  if (!current_entry_)
    return WebHistoryItem();
  return current_entry_->GetItemForFrame(frame);
}

void HistoryController::RemoveChildrenForRedirect(RenderFrameImpl* frame) {
  if (!provisional_entry_)
    return;
  if (HistoryEntry::HistoryNode* node =
          provisional_entry_->GetHistoryNodeForFrame(frame))
    node->RemoveChildren();
}

void HistoryController::CreateNewBackForwardItem(
    RenderFrameImpl* target_frame,
    const WebHistoryItem& new_item,
    bool clone_children_of_target) {
  if (!current_entry_) {
    current_entry_.reset(
        new HistoryEntry(new_item, target_frame->GetRoutingID()));
  } else {
    previous_entry_.reset(current_entry_.release());
    current_entry_.reset(previous_entry_->CloneAndReplace(
        new_item, clone_children_of_target, target_frame, render_view_));
  }
}

}  // namespace content
