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
// This class is currently owned by a single NavigationEntry and only tracks the
// main frame.
//
// TODO(creis): Keep a tree of FrameNavigationEntries in each NavigationEntry,
// one per frame.  FrameNavigationEntries may be shared across NavigationEntries
// if the frame hasn't changed.
class CONTENT_EXPORT FrameNavigationEntry {
 public:
  FrameNavigationEntry();
  FrameNavigationEntry(SiteInstanceImpl* site_instance,
                       const GURL& url,
                       const Referrer& referrer);
  virtual ~FrameNavigationEntry();

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
  // TODO(creis): Move virtual URL and related members to FrameNavigationEntry.
  void set_url(const GURL& url) { url_ = url; }
  const GURL& url() const { return url_; }

  // The referring URL.  Can be empty.
  void set_referrer(const Referrer& referrer) { referrer_ = referrer; }
  const Referrer& referrer() const { return referrer_; }

 private:
  // TODO(creis): These fields have implications for session restore.  This is
  // currently managed by NavigationEntry, but the logic will move here.

  // See the accessors above for descriptions.
  scoped_refptr<SiteInstanceImpl> site_instance_;
  GURL url_;
  Referrer referrer_;

  // Copy and assignment is explicitly allowed for this class.
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_FRAME_NAVIGATION_ENTRY_H_
