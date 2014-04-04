// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INFOBARS_INFOBAR_SERVICE_H_
#define CHROME_BROWSER_INFOBARS_INFOBAR_SERVICE_H_

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/infobars/infobar_manager.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
struct LoadCommittedDetails;
class WebContents;
}

class InfoBar;

// Associates a Tab to a InfoBarManager and manages its lifetime.
// It manages the infobar notifications and responds to navigation events.
class InfoBarService : public content::WebContentsObserver,
                       public content::WebContentsUserData<InfoBarService>,
                       public InfoBarManager::Observer {
 public:
  // Helper function to get the InfoBarManager attached to |web_contents|.
  static InfoBarManager* InfoBarManagerFromWebContents(
      content::WebContents* web_contents);

  static InfoBarDelegate::NavigationDetails
      NavigationDetailsFromLoadCommittedDetails(
          const content::LoadCommittedDetails& details);

  // These methods are simple pass-throughs to InfoBarManager, and are here to
  // prepare for the componentization of Infobars, see http://crbug.com/354379.
  InfoBar* AddInfoBar(scoped_ptr<InfoBar> infobar);
  InfoBar* ReplaceInfoBar(InfoBar* old_infobar,
                          scoped_ptr<InfoBar> new_infobar);

  // Retrieve the WebContents for the tab this service is associated with.
  content::WebContents* web_contents() {
    return content::WebContentsObserver::web_contents();
  }

  InfoBarManager* infobar_manager() { return &infobar_manager_; }

 private:
  friend class content::WebContentsUserData<InfoBarService>;

  explicit InfoBarService(content::WebContents* web_contents);
  virtual ~InfoBarService();

  // content::WebContentsObserver:
  virtual void RenderProcessGone(base::TerminationStatus status) OVERRIDE;
  virtual void NavigationEntryCommitted(
      const content::LoadCommittedDetails& load_details) OVERRIDE;
  virtual void WebContentsDestroyed(
      content::WebContents* web_contents) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // InfoBarManager::Observer:
  virtual void OnInfoBarAdded(InfoBar* infobar) OVERRIDE;
  virtual void OnInfoBarReplaced(InfoBar* old_infobar,
                                 InfoBar* new_infobar) OVERRIDE;
  virtual void OnInfoBarRemoved(InfoBar* infobar, bool animate) OVERRIDE;
  virtual void OnManagerShuttingDown(InfoBarManager* manager) OVERRIDE;

  // Message handlers.
  void OnDidBlockDisplayingInsecureContent();
  void OnDidBlockRunningInsecureContent();

  InfoBarManager infobar_manager_;

  DISALLOW_COPY_AND_ASSIGN(InfoBarService);
};

#endif  // CHROME_BROWSER_INFOBARS_INFOBAR_SERVICE_H_
