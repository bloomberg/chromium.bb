// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOMATION_TESTING_AUTOMATION_PROVIDER_H_
#define CHROME_BROWSER_AUTOMATION_TESTING_AUTOMATION_PROVIDER_H_
#pragma once

#include "base/basictypes.h"
#include "chrome/browser/automation/automation_provider.h"
#include "chrome/browser/browser_list.h"
#include "chrome/common/notification_registrar.h"

// This is an automation provider containing testing calls.
class TestingAutomationProvider : public AutomationProvider,
                                  public BrowserList::Observer,
                                  public NotificationObserver {
 public:
  explicit TestingAutomationProvider(Profile* profile);

  // BrowserList::Observer implementation
  // Called immediately after a browser is added to the list
  virtual void OnBrowserAdded(const Browser* browser);
  // Called immediately before a browser is removed from the list
  virtual void OnBrowserRemoving(const Browser* browser);

  // IPC implementations
  virtual void OnMessageReceived(const IPC::Message& msg);
  virtual void OnChannelError();

 private:
  virtual ~TestingAutomationProvider();

  // IPC Message callbacks.
  void CloseBrowser(int handle, IPC::Message* reply_message);
  void CloseBrowserAsync(int browser_handle);
  void ActivateTab(int handle, int at_index, int* status);
  void AppendTab(int handle, const GURL& url, IPC::Message* reply_message);
  void GetActiveTabIndex(int handle, int* active_tab_index);
  void CloseTab(int tab_handle, bool wait_until_closed,
                IPC::Message* reply_message);
  void GetCookies(const GURL& url, int handle, int* value_size,
                  std::string* value);
  void SetCookie(const GURL& url,
                 const std::string value,
                 int handle,
                 int* response_value);
  void DeleteCookie(const GURL& url, const std::string& cookie_name,
                    int handle, bool* success);
  void ShowCollectedCookiesDialog(int handle, bool* success);
  void NavigateToURL(int handle, const GURL& url, IPC::Message* reply_message);
  void NavigateToURLBlockUntilNavigationsComplete(int handle, const GURL& url,
                                                  int number_of_navigations,
                                                  IPC::Message* reply_message);

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  void OnRemoveProvider();  // Called via PostTask

  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(TestingAutomationProvider);
};

#endif  // CHROME_BROWSER_AUTOMATION_TESTING_AUTOMATION_PROVIDER_H_
