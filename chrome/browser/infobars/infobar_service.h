// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INFOBARS_INFOBAR_SERVICE_H_
#define CHROME_BROWSER_INFOBARS_INFOBAR_SERVICE_H_

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}

class InfoBarDelegate;

// Provides access to creating, removing and enumerating info bars
// attached to a tab.
class InfoBarService : public content::WebContentsObserver,
                       public content::NotificationObserver,
                       public content::WebContentsUserData<InfoBarService> {
 public:
  // Changes whether infobars are enabled.  The default is true.
  void set_infobars_enabled(bool enabled) { infobars_enabled_ = enabled; }

  // Adds an InfoBar for the specified |delegate|.
  //
  // If infobars are disabled for this tab or the tab already has a delegate
  // which returns true for InfoBarDelegate::EqualsDelegate(delegate),
  // |delegate| is closed immediately without being added.
  //
  // Returns the delegate if it was successfully added.
  InfoBarDelegate* AddInfoBar(scoped_ptr<InfoBarDelegate> delegate);

  // Removes the InfoBar for the specified |delegate|.
  //
  // If infobars are disabled for this tab, this will do nothing, on the
  // assumption that the matching AddInfoBar() call will have already closed the
  // delegate (see above).
  void RemoveInfoBar(InfoBarDelegate* delegate);

  // Replaces one infobar with another, without any animation in between.
  //
  // If infobars are disabled for this tab, |new_delegate| is closed immediately
  // without being added, and nothing else happens.
  //
  // Returns the new delegate if it was successfully added.
  //
  // NOTE: This does not perform any EqualsDelegate() checks like AddInfoBar().
  InfoBarDelegate* ReplaceInfoBar(InfoBarDelegate* old_delegate,
                                  scoped_ptr<InfoBarDelegate> new_delegate);

  // Returns the number of infobars for this tab.
  size_t infobar_count() const { return infobars_.size(); }

  // Returns the infobar delegate at the given |index|.  The InfoBarService
  // retains ownership.
  //
  // Warning: Does not sanity check |index|.
  InfoBarDelegate* infobar_at(size_t index) { return infobars_[index]; }

  // Retrieve the WebContents for the tab this service is associated with.
  content::WebContents* web_contents() {
    return content::WebContentsObserver::web_contents();
  }

 private:
  friend class content::WebContentsUserData<InfoBarService>;

  typedef std::vector<InfoBarDelegate*> InfoBars;

  explicit InfoBarService(content::WebContents* web_contents);
  virtual ~InfoBarService();

  // content::WebContentsObserver:
  virtual void RenderViewGone(base::TerminationStatus status) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  void RemoveInfoBarInternal(InfoBarDelegate* delegate, bool animate);
  void RemoveAllInfoBars(bool animate);

  // Message handlers.
  void OnDidBlockDisplayingInsecureContent();
  void OnDidBlockRunningInsecureContent();

  // Delegates for InfoBars associated with this InfoBarService.
  InfoBars infobars_;
  bool infobars_enabled_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(InfoBarService);
};

#endif  // CHROME_BROWSER_INFOBARS_INFOBAR_SERVICE_H_
