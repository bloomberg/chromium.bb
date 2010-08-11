// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_MATCH_PREVIEW_H_
#define CHROME_BROWSER_TAB_CONTENTS_MATCH_PREVIEW_H_

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "googleurl/src/gurl.h"

class TabContents;

// MatchPreview maintains a TabContents that is intended to give a preview of
// a URL. MatchPreview is owned by TabContents.
//
// As the user types in the omnibox the LocationBar updates MatchPreview by
// way of 'Update'. If the user does a gesture on the preview, say clicks a
// link, the TabContentsDelegate of the hosting TabContents is notified with
// CommitMatchPreview and the TabContents maintained by MatchPreview replaces
// the current TabContents in the TabStripModel.
//
// At any time the TabContents maintained by MatchPreview may be destroyed by
// way of 'DestroyPreviewContents'. Consumers of MatchPreview can detect the
// preview TabContents was destroyed by observing TAB_CONTENTS_DESTROYED.
//
// Consumers of MatchPreview can detect a new TabContents was created by
// MatchPreview by listening for MATCH_PREVIEW_TAB_CONTENTS_CREATED.
class MatchPreview {
 public:
  explicit MatchPreview(TabContents* host);
  ~MatchPreview();

  // Is MatchPreview enabled?
  static bool IsEnabled();

  // Invoked as the user types in the omnibox with the url to navigate to.  If
  // the url is empty and there is a preview TabContents it is destroyed. If url
  // is non-empty and the preview TabContents has not been created it is
  // created.
  void Update(const GURL& url);

  // Destroys the preview TabContents. Does nothing if the preview TabContents
  // has not been created.
  void DestroyPreviewContents();

  // Invoked when the user does some gesture that should trigger making the
  // current previewed page the permanent page.
  void CommitCurrentPreview();

  // Releases the preview TabContents passing ownership to the caller. This is
  // intended to be called when the preview TabContents is committed.
  TabContents* ReleasePreviewContents();

  // The TabContents we're maintaining the preview for.
  TabContents* host() { return host_; }

  // The preview TabContents; may be null.
  TabContents* preview_contents() { return preview_contents_.get(); }

 private:
  class TabContentsDelegateImpl;

  // The url we're displaying.
  GURL url_;

  // The TabContents we're providing the preview for.
  TabContents* host_;

  // Delegate of the preview TabContents. Used to detect when the user does some
  // gesture on the TabContents and the preview needs to be activated.
  scoped_ptr<TabContentsDelegateImpl> delegate_;

  // The preview TabContents; may be null.
  scoped_ptr<TabContents> preview_contents_;

  DISALLOW_COPY_AND_ASSIGN(MatchPreview);
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_MATCH_PREVIEW_H_
