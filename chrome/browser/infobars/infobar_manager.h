// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INFOBARS_INFOBAR_MANAGER_H_
#define CHROME_BROWSER_INFOBARS_INFOBAR_MANAGER_H_

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/infobars/infobar_delegate.h"

namespace content {
class WebContents;
}

class InfoBar;

// Provides access to creating, removing and enumerating info bars
// attached to a tab.
class InfoBarManager {
 public:
  // Observer class for infobar events.
  class Observer {
   public:
    virtual void OnInfoBarAdded(InfoBar* infobar) = 0;
    virtual void OnInfoBarRemoved(InfoBar* infobar, bool animate) = 0;
    virtual void OnInfoBarReplaced(InfoBar* old_infobar,
                                   InfoBar* new_infobar) = 0;
    virtual void OnManagerShuttingDown(InfoBarManager* manager) = 0;
  };

  explicit InfoBarManager(content::WebContents* web_contents);
  ~InfoBarManager();

  // Adds the specified |infobar|, which already owns a delegate.
  //
  // If infobars are disabled for this tab or the tab already has an infobar
  // whose delegate returns true for
  // InfoBarDelegate::EqualsDelegate(infobar->delegate()), |infobar| is deleted
  // immediately without being added.
  //
  // Returns the infobar if it was successfully added.
  virtual InfoBar* AddInfoBar(scoped_ptr<InfoBar> infobar);

  // Removes the specified |infobar|.  This in turn may close immediately or
  // animate closed; at the end the infobar will delete itself.
  //
  // If infobars are disabled for this tab, this will do nothing, on the
  // assumption that the matching AddInfoBar() call will have already deleted
  // the infobar (see above).
  void RemoveInfoBar(InfoBar* infobar);

  // Removes all the infobars.
  void RemoveAllInfoBars(bool animate);

  // Replaces one infobar with another, without any animation in between.  This
  // will result in |old_infobar| being synchronously deleted.
  //
  // If infobars are disabled for this tab, |new_infobar| is deleted immediately
  // without being added, and nothing else happens.
  //
  // Returns the new infobar if it was successfully added.
  //
  // NOTE: This does not perform any EqualsDelegate() checks like AddInfoBar().
  InfoBar* ReplaceInfoBar(InfoBar* old_infobar,
                          scoped_ptr<InfoBar> new_infobar);

  // Returns the number of infobars for this tab.
  size_t infobar_count() const { return infobars_.size(); }

  // Returns the infobar at the given |index|.  The InfoBarManager retains
  // ownership.
  //
  // Warning: Does not sanity check |index|.
  InfoBar* infobar_at(size_t index) { return infobars_[index]; }

  // Retrieve the WebContents for the tab this service is associated with.
  // Do not add new call sites for this.
  // TODO(droger): remove this method. See http://crbug.com/354379.
  content::WebContents* web_contents() { return web_contents_; }

  // Must be called when a navigation happens.
  void OnNavigation(const InfoBarDelegate::NavigationDetails& details);

  // Called when the associated WebContents is being destroyed.
  void OnWebContentsDestroyed();

  void AddObserver(Observer* obs);
  void RemoveObserver(Observer* obs);

 private:
  // InfoBars associated with this InfoBarManager.  We own these pointers.
  // However, this is not a ScopedVector, because we don't delete the infobars
  // directly once they've been added to this; instead, when we're done with an
  // infobar, we instruct it to delete itself and then orphan it.  See
  // RemoveInfoBarInternal().
  typedef std::vector<InfoBar*> InfoBars;

  void RemoveInfoBarInternal(InfoBar* infobar, bool animate);

  InfoBars infobars_;
  bool infobars_enabled_;

  // TODO(droger): remove this field. See http://crbug.com/354379.
  content::WebContents* web_contents_;

  ObserverList<Observer, true> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(InfoBarManager);
};

#endif  // CHROME_BROWSER_INFOBARS_INFOBAR_MANAGER_H_
