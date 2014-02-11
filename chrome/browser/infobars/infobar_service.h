// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INFOBARS_INFOBAR_SERVICE_H_
#define CHROME_BROWSER_INFOBARS_INFOBAR_SERVICE_H_

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class InfoBar;

// Provides access to creating, removing and enumerating info bars
// attached to a tab.
class InfoBarService : public content::WebContentsObserver,
                       public content::WebContentsUserData<InfoBarService> {
 public:
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

  // Returns the infobar at the given |index|.  The InfoBarService retains
  // ownership.
  //
  // Warning: Does not sanity check |index|.
  InfoBar* infobar_at(size_t index) { return infobars_[index]; }

  // Retrieve the WebContents for the tab this service is associated with.
  content::WebContents* web_contents() {
    return content::WebContentsObserver::web_contents();
  }

 private:
  friend class content::WebContentsUserData<InfoBarService>;

  // InfoBars associated with this InfoBarService.  We own these pointers.
  // However, this is not a ScopedVector, because we don't delete the infobars
  // directly once they've been added to this; instead, when we're done with an
  // infobar, we instruct it to delete itself and then orphan it.  See
  // RemoveInfoBarInternal().
  typedef std::vector<InfoBar*> InfoBars;

  explicit InfoBarService(content::WebContents* web_contents);
  virtual ~InfoBarService();

  // content::WebContentsObserver:
  virtual void RenderProcessGone(base::TerminationStatus status) OVERRIDE;
  virtual void NavigationEntryCommitted(
      const content::LoadCommittedDetails& load_details) OVERRIDE;
  virtual void WebContentsDestroyed(
      content::WebContents* web_contents) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  void RemoveInfoBarInternal(InfoBar* infobar, bool animate);
  void RemoveAllInfoBars(bool animate);

  // Message handlers.
  void OnDidBlockDisplayingInsecureContent();
  void OnDidBlockRunningInsecureContent();

  InfoBars infobars_;
  bool infobars_enabled_;

  DISALLOW_COPY_AND_ASSIGN(InfoBarService);
};

#endif  // CHROME_BROWSER_INFOBARS_INFOBAR_SERVICE_H_
