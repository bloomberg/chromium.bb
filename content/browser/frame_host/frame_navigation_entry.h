// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_FRAME_NAVIGATION_ENTRY_H_
#define CONTENT_BROWSER_FRAME_HOST_FRAME_NAVIGATION_ENTRY_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "content/browser/site_instance_impl.h"
#include "content/public/common/referrer.h"

namespace content {

// Represents a session history item for a particular frame.
//
// This class is refcounted and can be shared across multiple NavigationEntries.
// For now, it is owned by a single NavigationEntry and only tracks the main
// frame.
//
// TODO(creis): In --site-per-process, fill in a tree of FrameNavigationEntries
// in each NavigationEntry, one per frame.  FrameNavigationEntries may be shared
// across NavigationEntries if the frame hasn't changed.
class CONTENT_EXPORT FrameNavigationEntry
    : public base::RefCounted<FrameNavigationEntry> {
 public:
  // TODO(creis): We should not use FTN IDs here, since they will change if you
  // leave a page and come back later.  We should evaluate whether Blink's
  // frame sequence numbers or unique names would work instead, similar to
  // HistoryNode.
  explicit FrameNavigationEntry(int64 frame_tree_node_id);
  FrameNavigationEntry(int64 frame_tree_node_id,
                       SiteInstanceImpl* site_instance,
                       const GURL& url,
                       const Referrer& referrer);

  // Creates a copy of this FrameNavigationEntry that can be modified
  // independently from the original.
  FrameNavigationEntry* Clone() const;

  // Updates all the members of this entry.
  void UpdateEntry(SiteInstanceImpl* site_instance,
                   const GURL& url,
                   const Referrer& referrer);

  // The ID of the FrameTreeNode this entry is for.  -1 for the main frame,
  // since we don't always know the FrameTreeNode ID when creating the overall
  // NavigationEntry.
  // TODO(creis): Replace with frame sequence number or unique name.
  int64 frame_tree_node_id() const { return frame_tree_node_id_; }

  // The SiteInstance responsible for rendering this frame.  All frames sharing
  // a SiteInstance must live in the same process.  This is a refcounted pointer
  // that keeps the SiteInstance (not necessarily the process) alive as long as
  // this object remains in the session history.
  void set_site_instance(SiteInstanceImpl* site_instance) {
    site_instance_ = site_instance;
  }
  SiteInstanceImpl* site_instance() const { return site_instance_.get(); }

  // The actual URL loaded in the frame.  This is in contrast to the virtual
  // URL, which is shown to the user.
  void set_url(const GURL& url) { url_ = url; }
  const GURL& url() const { return url_; }

  // The referring URL.  Can be empty.
  void set_referrer(const Referrer& referrer) { referrer_ = referrer; }
  const Referrer& referrer() const { return referrer_; }

 private:
  friend class base::RefCounted<FrameNavigationEntry>;
  virtual ~FrameNavigationEntry();

  // WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
  // For all new fields, update |Clone|.
  // TODO(creis): These fields have implications for session restore.  This is
  // currently managed by NavigationEntry, but the logic will move here.
  // WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING

  // See the accessors above for descriptions.
  int64 frame_tree_node_id_;
  scoped_refptr<SiteInstanceImpl> site_instance_;
  GURL url_;
  Referrer referrer_;

  DISALLOW_COPY_AND_ASSIGN(FrameNavigationEntry);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_FRAME_NAVIGATION_ENTRY_H_
