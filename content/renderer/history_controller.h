// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
 * Copyright (C) 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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

#ifndef CONTENT_RENDERER_HISTORY_CONTROLLER_H_
#define CONTENT_RENDERER_HISTORY_CONTROLLER_H_

#include "base/containers/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/web/WebHistoryCommitType.h"
#include "third_party/WebKit/public/web/WebHistoryItem.h"

namespace blink {
class WebFrame;
}

namespace content {
class HistoryEntry;
class RenderFrameImpl;
class RenderViewImpl;

// A guide to history state in the renderer:
//
// HistoryController: Owned by RenderView, is the entry point for interacting
//     with history. Handles most of the operations to modify history state,
//     navigate to an existing back/forward entry, etc.
//
// HistoryEntry: Represents a single entry in the back/forward list,
//     encapsulating all frames in the page it represents. It provides access
//     to each frame's state via lookups by frame id or frame name.
// HistoryNode: Represents a single frame in a HistoryEntry. Owned by a
//     HistoryEntry. HistoryNodes form a tree that mirrors the frame tree in
//     the corresponding page.
// HistoryNodes represent the structure of the page, but don't hold any
// per-frame state except a list of child frames.
// WebHistoryItem (lives in blink): The state for a given frame. Can persist
//     across navigations. WebHistoryItem is reference counted, and each
//     HistoryNode holds a reference to its single corresponding
//     WebHistoryItem. Can be referenced by multiple HistoryNodes and can
//     therefore exist in multiple HistoryEntry instances.
//
// Suppose we have the following page, foo.com, which embeds foo.com/a in an
// iframe:
//
// HistoryEntry 0:
//     HistoryNode 0_0 (WebHistoryItem A (url: foo.com))
//         HistoryNode 0_1: (WebHistoryItem B (url: foo.com/a))
//
// Now we navigate the top frame to bar.com, which embeds bar.com/b and
// bar.com/c in iframes, and bar.com/b in turn embeds bar.com/d. We will
// create a new HistoryEntry with a tree containing 4 new HistoryNodes. The
// state will be:
//
// HistoryEntry 1:
//     HistoryNode 1_0 (WebHistoryItem C (url: bar.com))
//         HistoryNode 1_1: (WebHistoryItem D (url: bar.com/b))
//             HistoryNode 1_3: (WebHistoryItem F (url: bar.com/d))
//         HistoryNode 1_2: (WebHistoryItem E (url: bar.com/c))
//
//
// Finally, we navigate the first subframe from bar.com/b to bar.com/e, which
// embeds bar.com/f. We will create a new HistoryEntry and new HistoryNode for
// each frame. Any frame that navigates (bar.com/e and its child, bar.com/f)
// will receive a new WebHistoryItem. However, 2 frames were not navigated
// (bar.com and bar.com/c), so those two frames will reuse the existing
// WebHistoryItem:
//
// HistoryEntry 2:
//     HistoryNode 2_0 (WebHistoryItem C (url: bar.com))  *REUSED*
//         HistoryNode 2_1: (WebHistoryItem G (url: bar.com/e))
//            HistoryNode 2_3: (WebHistoryItem H (url: bar.com/f))
//         HistoryNode 2_2: (WebHistoryItem E (url: bar.com/c)) *REUSED*
//

class HistoryNode {
 public:
  HistoryNode(HistoryEntry* entry,
              const blink::WebHistoryItem& item,
              int64_t frame_id);
  ~HistoryNode();

  HistoryNode* AddChild(const blink::WebHistoryItem& item, int64_t frame_id);
  HistoryNode* CloneAndReplace(HistoryEntry* new_entry,
                               const blink::WebHistoryItem& new_item,
                               bool clone_children_of_target,
                               RenderFrameImpl* target_frame,
                               RenderFrameImpl* current_frame);
  blink::WebHistoryItem& item() { return item_; }
  void set_item(const blink::WebHistoryItem& item) { item_ = item; }
  std::vector<HistoryNode*>& children() const { return children_->get(); }
  void RemoveChildren();

 private:
  HistoryEntry* entry_;
  scoped_ptr<ScopedVector<HistoryNode> > children_;
  blink::WebHistoryItem item_;
};

class HistoryEntry {
 public:
  HistoryEntry(const blink::WebHistoryItem& root, int64_t frame_id);
  ~HistoryEntry();

  HistoryEntry* CloneAndReplace(const blink::WebHistoryItem& newItem,
                                bool clone_children_of_target,
                                RenderFrameImpl* target_frame,
                                RenderViewImpl* render_view);

  HistoryNode* GetHistoryNodeForFrame(RenderFrameImpl* frame);
  blink::WebHistoryItem GetItemForFrame(RenderFrameImpl* frame);
  const blink::WebHistoryItem& root() const { return root_->item(); }
  HistoryNode* root_history_node() const { return root_.get(); }

 private:
  friend class HistoryNode;

  HistoryEntry();

  scoped_ptr<HistoryNode> root_;

  typedef base::hash_map<uint64_t, HistoryNode*> FramesToItems;
  FramesToItems frames_to_items_;

  typedef base::hash_map<std::string, HistoryNode*> UniqueNamesToItems;
  UniqueNamesToItems unique_names_to_items_;
};

class CONTENT_EXPORT HistoryController {
 public:
  explicit HistoryController(RenderViewImpl* render_view);
  ~HistoryController();

  void GoToItem(const blink::WebHistoryItem& item,
                blink::WebURLRequest::CachePolicy cache_policy);

  void UpdateForCommit(RenderFrameImpl* frame,
                       const blink::WebHistoryItem& item,
                       blink::WebHistoryCommitType commit_type,
                       bool navigation_within_page);

  blink::WebHistoryItem GetCurrentItemForExport();
  blink::WebHistoryItem GetPreviousItemForExport();
  blink::WebHistoryItem GetItemForNewChildFrame(RenderFrameImpl* frame) const;
  void RemoveChildrenForRedirect(RenderFrameImpl* frame);

 private:
  void GoToEntry(HistoryEntry* entry,
                 blink::WebURLRequest::CachePolicy cache_policy);

  typedef std::vector<std::pair<blink::WebFrame*, blink::WebHistoryItem> >
      HistoryFrameLoadVector;
  void RecursiveGoToEntry(blink::WebFrame* frame,
                          HistoryFrameLoadVector& sameDocumentLoads,
                          HistoryFrameLoadVector& differentDocumentLoads);

  void UpdateForInitialLoadInChildFrame(RenderFrameImpl* frame,
                                        const blink::WebHistoryItem& item);
  void CreateNewBackForwardItem(RenderFrameImpl* frame,
                                const blink::WebHistoryItem& item,
                                bool clone_children_of_target);

  RenderViewImpl* render_view_;

  scoped_ptr<HistoryEntry> current_entry_;
  scoped_ptr<HistoryEntry> previous_entry_;
  scoped_ptr<HistoryEntry> provisional_entry_;

  DISALLOW_COPY_AND_ASSIGN(HistoryController);
};

}  // namespace content

#endif  // CONTENT_RENDERER_HISTORY_CONTROLLER_H_
