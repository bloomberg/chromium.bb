// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TABS_TAB_STRIP_MODEL_OBSERVER_H_
#define CHROME_BROWSER_TABS_TAB_STRIP_MODEL_OBSERVER_H_
#pragma once

class TabContents;

////////////////////////////////////////////////////////////////////////////////
//
// TabStripModelObserver
//
//  Objects implement this interface when they wish to be notified of changes
//  to the TabStripModel.
//
//  Two major implementers are the TabStrip, which uses notifications sent
//  via this interface to update the presentation of the strip, and the Browser
//  object, which updates bookkeeping and shows/hides individual TabContentses.
//
//  Register your TabStripModelObserver with the TabStripModel using its
//  Add/RemoveObserver methods.
//
////////////////////////////////////////////////////////////////////////////////
class TabStripModelObserver {
 public:
  // Enumeration of the possible values supplied to TabChangedAt.
  enum TabChangeType {
    // Only the loading state changed.
    LOADING_ONLY,

    // Only the title changed and page isn't loading.
    TITLE_NOT_LOADING,

    // Change not characterized by LOADING_ONLY or TITLE_NOT_LOADING.
    ALL
  };

  // A new TabContents was inserted into the TabStripModel at the specified
  // index. |foreground| is whether or not it was opened in the foreground
  // (selected).
  virtual void TabInsertedAt(TabContents* contents,
                             int index,
                             bool foreground);

  // The specified TabContents at |index| is being closed (and eventually
  // destroyed).
  virtual void TabClosingAt(TabContents* contents, int index);

  // The specified TabContents at |index| is being detached, perhaps to be
  // inserted in another TabStripModel. The implementer should take whatever
  // action is necessary to deal with the TabContents no longer being present.
  virtual void TabDetachedAt(TabContents* contents, int index);

  // The selected TabContents is about to change from |old_contents| at |index|.
  // This gives observers a chance to prepare for an impending switch before it
  // happens.
  virtual void TabDeselectedAt(TabContents* contents, int index);

  // The selected TabContents changed from |old_contents| to |new_contents| at
  // |index|. |user_gesture| specifies whether or not this was done by a user
  // input event (e.g. clicking on a tab, keystroke) or as a side-effect of
  // some other function.
  virtual void TabSelectedAt(TabContents* old_contents,
                             TabContents* new_contents,
                             int index,
                             bool user_gesture);

  // The specified TabContents at |from_index| was moved to |to_index|.
  virtual void TabMoved(TabContents* contents,
                        int from_index,
                        int to_index);

  // The specified TabContents at |index| changed in some way. |contents| may
  // be an entirely different object and the old value is no longer available
  // by the time this message is delivered.
  //
  // See TabChangeType for a description of |change_type|.
  virtual void TabChangedAt(TabContents* contents,
                            int index,
                            TabChangeType change_type);

  // The tab contents was replaced at the specified index. This is invoked when
  // a tab becomes phantom. See description of phantom tabs in class description
  // of TabStripModel for details.
  // TODO(sky): nuke this in favor of the 4 arg variant.
  virtual void TabReplacedAt(TabContents* old_contents,
                             TabContents* new_contents,
                             int index);

  // Invoked when the pinned state of a tab changes. This is not invoked if the
  // tab ends up moving as a result of the mini state changing.
  // See note in TabMiniStateChanged as to how this relates to
  // TabMiniStateChanged.
  virtual void TabPinnedStateChanged(TabContents* contents, int index);

  // Invoked if the mini state of a tab changes.  This is not invoked if the
  // tab ends up moving as a result of the mini state changing.
  // NOTE: this is sent when the pinned state of a non-app tab changes and is
  // sent in addition to TabPinnedStateChanged. UI code typically need not care
  // about TabPinnedStateChanged, but instead this.
  virtual void TabMiniStateChanged(TabContents* contents, int index);

  // Invoked when the blocked state of a tab changes.
  // NOTE: This is invoked when a tab becomes blocked/unblocked by a tab modal
  // window.
  virtual void TabBlockedStateChanged(TabContents* contents, int index);

  // The TabStripModel now no longer has any phantom tabs. The implementer may
  // use this as a trigger to try and close the window containing the
  // TabStripModel, for example...
  virtual void TabStripEmpty();

  // Sent when the tabstrip model is about to be deleted and any reference held
  // must be dropped.
  virtual void TabStripModelDeleted();

 protected:
  virtual ~TabStripModelObserver() {}
};

#endif  // CHROME_BROWSER_TABS_TAB_STRIP_MODEL_OBSERVER_H_
