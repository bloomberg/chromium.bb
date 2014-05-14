// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INFOBARS_INFOBAR_SERVICE_H_
#define CHROME_BROWSER_INFOBARS_INFOBAR_SERVICE_H_

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "components/infobars/core/infobar_manager.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
struct LoadCommittedDetails;
class WebContents;
}

namespace infobars {
class InfoBar;
}

// Associates a Tab to a InfoBarManager and manages its lifetime.
// It manages the infobar notifications and responds to navigation events.
class InfoBarService : public infobars::InfoBarManager,
                       public content::WebContentsObserver,
                       public content::WebContentsUserData<InfoBarService> {
 public:
  static infobars::InfoBarDelegate::NavigationDetails
      NavigationDetailsFromLoadCommittedDetails(
          const content::LoadCommittedDetails& details);

  // This function must only be called on infobars that are owned by an
  // InfoBarService instance (or not owned at all, in which case this returns
  // NULL).
  static content::WebContents* WebContentsFromInfoBar(
      infobars::InfoBar* infobar);

  // Retrieve the WebContents for the tab this service is associated with.
  content::WebContents* web_contents() {
    return content::WebContentsObserver::web_contents();
  }

 private:
  friend class content::WebContentsUserData<InfoBarService>;

  explicit InfoBarService(content::WebContents* web_contents);
  virtual ~InfoBarService();

  // InfoBarManager:
  virtual int GetActiveEntryID() OVERRIDE;
  // TODO(droger): Remove these functions once infobar notifications are
  // removed. See http://crbug.com/354380
  virtual void NotifyInfoBarAdded(infobars::InfoBar* infobar) OVERRIDE;
  virtual void NotifyInfoBarRemoved(infobars::InfoBar* infobar,
                                    bool animate) OVERRIDE;

  // content::WebContentsObserver:
  virtual void RenderProcessGone(base::TerminationStatus status) OVERRIDE;
  virtual void NavigationEntryCommitted(
      const content::LoadCommittedDetails& load_details) OVERRIDE;
  virtual void WebContentsDestroyed() OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // Message handlers.
  void OnDidBlockDisplayingInsecureContent();
  void OnDidBlockRunningInsecureContent();


  DISALLOW_COPY_AND_ASSIGN(InfoBarService);
};

#endif  // CHROME_BROWSER_INFOBARS_INFOBAR_SERVICE_H_
