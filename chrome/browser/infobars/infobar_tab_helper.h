// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INFOBARS_INFOBAR_TAB_HELPER_H_
#define CHROME_BROWSER_INFOBARS_INFOBAR_TAB_HELPER_H_
#pragma once

#include "base/basictypes.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_observer.h"

class InfoBarDelegate;

// Per-tab info bar manager.
class InfoBarTabHelper : public content::WebContentsObserver,
                         public content::NotificationObserver {
 public:
  explicit InfoBarTabHelper(content::WebContents* web_contents);
  virtual ~InfoBarTabHelper();

  // Adds an InfoBar for the specified |delegate|.
  //
  // If infobars are disabled for this tab or the tab already has a delegate
  // which returns true for InfoBarDelegate::EqualsDelegate(delegate),
  // |delegate| is closed immediately without being added.
  //
  // Returns whether |delegate| was successfully added.
  bool AddInfoBar(InfoBarDelegate* delegate);

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
  // Returns whether |new_delegate| was successfully added.
  //
  // NOTE: This does not perform any EqualsDelegate() checks like AddInfoBar().
  bool ReplaceInfoBar(InfoBarDelegate* old_delegate,
                      InfoBarDelegate* new_delegate);

  // Enumeration and access functions.
  size_t infobar_count() const { return infobars_.size(); }
  // WARNING: This does not sanity-check |index|!
  InfoBarDelegate* GetInfoBarDelegateAt(size_t index);
  void set_infobars_enabled(bool value) { infobars_enabled_ = value; }

  // content::WebContentsObserver overrides:
  virtual void RenderViewGone(base::TerminationStatus status) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // content::NotificationObserver overrides:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Helper functions for infobars:
  content::WebContents* web_contents() {
    return content::WebContentsObserver::web_contents();
  }

 private:
  typedef std::vector<InfoBarDelegate*> InfoBars;

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
