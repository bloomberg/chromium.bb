// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_MODEL_OBSERVER_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_MODEL_OBSERVER_H_

class TabStripModel;

namespace content {
class WebContents;
}

namespace ui {
class ListSelectionModel;
}

////////////////////////////////////////////////////////////////////////////////
//
// TabStripModelObserver
//
//  Objects implement this interface when they wish to be notified of changes
//  to the TabStripModel.
//
//  Two major implementers are the TabStrip, which uses notifications sent
//  via this interface to update the presentation of the strip, and the Browser
//  object, which updates bookkeeping and shows/hides individual WebContentses.
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

  enum ChangeReason {
    // Used to indicate that none of the reasons below are responsible for the
    // active tab change.
    CHANGE_REASON_NONE = 0,
    // The active tab changed because the tab's web contents was replaced.
    CHANGE_REASON_REPLACED = 1 << 0,
    // The active tab changed due to a user input event.
    CHANGE_REASON_USER_GESTURE = 1 << 1,
  };

  // A new WebContents was inserted into the TabStripModel at the
  // specified index. |foreground| is whether or not it was opened in the
  // foreground (selected).
  virtual void TabInsertedAt(content::WebContents* contents,
                             int index,
                             bool foreground);

  // The specified WebContents at |index| is being closed (and eventually
  // destroyed). |tab_strip_model| is the TabStripModel that contained the tab.
  virtual void TabClosingAt(TabStripModel* tab_strip_model,
                            content::WebContents* contents,
                            int index);

  // The specified WebContents at |index| is being detached, perhaps to
  // be inserted in another TabStripModel. The implementer should take whatever
  // action is necessary to deal with the WebContents no longer being
  // present.
  virtual void TabDetachedAt(content::WebContents* contents, int index);

  // The active WebContents is about to change from |old_contents|.
  // This gives observers a chance to prepare for an impending switch before it
  // happens.
  virtual void TabDeactivated(content::WebContents* contents);

  // Sent when the active tab changes. The previously active tab is identified
  // by |old_contents| and the newly active tab by |new_contents|. |index| is
  // the index of |new_contents|. If |reason| has CHANGE_REASON_REPLACED set
  // then the web contents was replaced (see TabChangedAt). If |reason| has
  // CHANGE_REASON_USER_GESTURE set then the web contents was changed due to a
  // user input event (e.g. clicking on a tab, keystroke).
  // Note: It is possible for the selection to change while the active tab
  // remains unchanged. For example, control-click may not change the active tab
  // but does change the selection. In this case |ActiveTabChanged| is not sent.
  // If you care about any changes to the selection, override
  // TabSelectionChanged.
  // Note: |old_contents| will be NULL if there was no contents previously
  // active.
  virtual void ActiveTabChanged(content::WebContents* old_contents,
                                content::WebContents* new_contents,
                                int index,
                                int reason);

  // Sent when the selection changes in |tab_strip_model|. More precisely when
  // selected tabs, anchor tab or active tab change. |old_model| is a snapshot
  // of the selection model before the change. See also ActiveTabChanged for
  // details.
  virtual void TabSelectionChanged(TabStripModel* tab_strip_model,
                                   const ui::ListSelectionModel& old_model);

  // The specified WebContents at |from_index| was moved to |to_index|.
  virtual void TabMoved(content::WebContents* contents,
                        int from_index,
                        int to_index);

  // The specified WebContents at |index| changed in some way. |contents|
  // may be an entirely different object and the old value is no longer
  // available by the time this message is delivered.
  //
  // See TabChangeType for a description of |change_type|.
  virtual void TabChangedAt(content::WebContents* contents,
                            int index,
                            TabChangeType change_type);

  // The WebContents was replaced at the specified index. This is invoked
  // when instant is enabled and the user navigates by way of instant or when
  // prerendering swaps in a prerendered WebContents.
  virtual void TabReplacedAt(TabStripModel* tab_strip_model,
                             content::WebContents* old_contents,
                             content::WebContents* new_contents,
                             int index);

  // Invoked when the pinned state of a tab changes. See note in
  // TabMiniStateChanged as to how this relates to TabMiniStateChanged.
  virtual void TabPinnedStateChanged(content::WebContents* contents, int index);

  // Invoked if the mini state of a tab changes.
  // NOTE: This is sent when the pinned state of a non-app tab changes and is
  // sent in addition to TabPinnedStateChanged. UI code typically need not care
  // about TabPinnedStateChanged, but instead this.
  virtual void TabMiniStateChanged(content::WebContents* contents, int index);

  // Invoked when the blocked state of a tab changes.
  // NOTE: This is invoked when a tab becomes blocked/unblocked by a tab modal
  // window.
  virtual void TabBlockedStateChanged(content::WebContents* contents,
                                      int index);

  // The TabStripModel now no longer has any tabs. The implementer may
  // use this as a trigger to try and close the window containing the
  // TabStripModel, for example...
  virtual void TabStripEmpty();

  // Sent when the tabstrip model is about to be deleted and any reference held
  // must be dropped.
  virtual void TabStripModelDeleted();

 protected:
  virtual ~TabStripModelObserver() {}
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_MODEL_OBSERVER_H_
