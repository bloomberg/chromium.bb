// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INFOBARS_INFOBAR_TAB_HELPER_H_
#define CHROME_BROWSER_INFOBARS_INFOBAR_TAB_HELPER_H_

#include <vector>

#include "base/basictypes.h"
#include "chrome/browser/api/infobars/infobar_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class InfoBarDelegate;

// Per-tab info bar manager.
class InfoBarTabHelper : public InfoBarService,
                         public content::WebContentsObserver,
                         public content::NotificationObserver,
                         public content::WebContentsUserData<InfoBarTabHelper> {
 private:
  friend class content::WebContentsUserData<InfoBarTabHelper>;

  typedef std::vector<InfoBarDelegate*> InfoBars;

  explicit InfoBarTabHelper(content::WebContents* web_contents);
  virtual ~InfoBarTabHelper();

  // InfoBarService:
  virtual void SetInfoBarsEnabled(bool enabled) OVERRIDE;
  virtual InfoBarDelegate* AddInfoBar(
      scoped_ptr<InfoBarDelegate> delegate) OVERRIDE;
  virtual void RemoveInfoBar(InfoBarDelegate* delegate) OVERRIDE;
  virtual InfoBarDelegate* ReplaceInfoBar(
      InfoBarDelegate* old_delegate,
      scoped_ptr<InfoBarDelegate> new_delegate) OVERRIDE;
  virtual size_t GetInfoBarCount() const OVERRIDE;
  virtual InfoBarDelegate* GetInfoBarDelegateAt(size_t index) OVERRIDE;
  virtual content::WebContents* GetWebContents() OVERRIDE;

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

  // Delegates for InfoBars associated with this InfoBarTabHelper.
  InfoBars infobars_;
  bool infobars_enabled_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(InfoBarTabHelper);
};

#endif  // CHROME_BROWSER_INFOBARS_INFOBAR_TAB_HELPER_H_
