// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INFOBARS_INFOBAR_TAB_HELPER_H_
#define CHROME_BROWSER_INFOBARS_INFOBAR_TAB_HELPER_H_
#pragma once

#include "base/basictypes.h"
#include "content/browser/tab_contents/tab_contents_observer.h"
#include "content/common/notification_registrar.h"

class InfoBarDelegate;
class TabContents;

// Per-tab info bar manager.
class InfoBarTabHelper : public TabContentsObserver,
                         public NotificationObserver {
 public:
  explicit InfoBarTabHelper(TabContents* tab_contents);
  virtual ~InfoBarTabHelper();

  // Adds an InfoBar for the specified |delegate|.
  //
  // If infobars are disabled for this tab or the tab already has a delegate
  // which returns true for InfoBarDelegate::EqualsDelegate(delegate),
  // |delegate| is closed immediately without being added.
  void AddInfoBar(InfoBarDelegate* delegate);

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
  // NOTE: This does not perform any EqualsDelegate() checks like AddInfoBar().
  void ReplaceInfoBar(InfoBarDelegate* old_delegate,
                      InfoBarDelegate* new_delegate);

  // Enumeration and access functions.
  size_t infobar_count() const { return infobars_.size(); }
  // WARNING: This does not sanity-check |index|!
  InfoBarDelegate* GetInfoBarDelegateAt(size_t index);
  void set_infobars_enabled(bool value) { infobars_enabled_ = value; }

  // TabContentsObserver overrides:
  virtual void RenderViewGone() OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // NotificationObserver overrides:
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

  // Helper functions for infobars:
  TabContents* tab_contents() {
    return TabContentsObserver::tab_contents();
  }

 private:
  void RemoveInfoBarInternal(InfoBarDelegate* delegate, bool animate);
  void RemoveAllInfoBars(bool animate);

  // Message handlers.
  void OnDidBlockDisplayingInsecureContent();
  void OnDidBlockRunningInsecureContent();

  // Delegates for InfoBars associated with this InfoBarTabHelper.
  std::vector<InfoBarDelegate*> infobars_;
  bool infobars_enabled_;

  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(InfoBarTabHelper);
};

#endif  // CHROME_BROWSER_INFOBARS_INFOBAR_TAB_HELPER_H_
