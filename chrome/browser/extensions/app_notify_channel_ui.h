// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_APP_NOTIFY_CHANNEL_UI_H_
#define CHROME_BROWSER_EXTENSIONS_APP_NOTIFY_CHANNEL_UI_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/sync/profile_sync_service_observer.h"

class Browser;
class TabContentsWrapper;

// An interface for prompting a user to sign in to sync so that we can create
// an app notification channel for one of their apps.
class AppNotifyChannelUI {
 public:
  virtual ~AppNotifyChannelUI() {}

  class Delegate {
   public:
    // A callback for whether the user successfully set up sync or not.
    virtual void OnSyncSetupResult(bool enabled) = 0;
  };

  // Shows a prompt for sync setup - |delegate| will be called back later when
  // setup is complete or cancelled. This should only be called once per
  // instance.
  virtual void PromptSyncSetup(Delegate* delegate) = 0;
};


class AppNotifyChannelUIImpl : public AppNotifyChannelUI,
                               public ProfileSyncServiceObserver {
 public:
  AppNotifyChannelUIImpl(Browser* browser,
                         TabContentsWrapper* wrapper,
                         const std::string& app_name);
  virtual ~AppNotifyChannelUIImpl();

  // AppNotifyChannelUI.
  virtual void PromptSyncSetup(AppNotifyChannelUI::Delegate* delegate) OVERRIDE;

 protected:
  // A private class we use to put up an infobar - its lifetime is managed by
  // |wrapper_|, so we don't have one as an instance variable.
  class InfoBar;
  friend class AppNotifyChannelUIImpl::InfoBar;

  // Called by our InfoBar when it's accepted or cancelled/closed.
  void OnInfoBarResult(bool accepted);

  // ProfileSyncServiceObserver.
  virtual void OnStateChanged() OVERRIDE;

 private:
  void StartObservingSync();
  void StopObservingSync();

  Browser* browser_;
  TabContentsWrapper* wrapper_;
  std::string app_name_;
  AppNotifyChannelUI::Delegate* delegate_;

  // Have we registered ourself as a ProfileSyncServiceObserver?
  bool observing_sync_;

  // This is for working around a bug that ProfileSyncService calls
  // ProfileSyncServiceObserver::OnStateChanged callback many times
  // after ShowLoginDialog is called and before the wizard is
  // actually visible to the user. So we record if the wizard was
  // shown to user and then wait for wizard to get dismissed.
  // See crbug.com/101842.
  bool wizard_shown_to_user_;

  DISALLOW_COPY_AND_ASSIGN(AppNotifyChannelUIImpl);
};

#endif  // CHROME_BROWSER_EXTENSIONS_APP_NOTIFY_CHANNEL_UI_H_
