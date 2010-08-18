// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/testing_automation_provider.h"

#include "base/command_line.h"
#include "chrome/browser/automation/automation_browser_tracker.h"
#include "chrome/browser/automation/automation_provider_list.h"
#include "chrome/browser/automation/automation_provider_observers.h"
#include "chrome/browser/browser_window.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/notification_service.h"
#include "chrome/test/automation/automation_messages.h"

TestingAutomationProvider::TestingAutomationProvider(Profile* profile)
    : AutomationProvider(profile) {
  BrowserList::AddObserver(this);
  registrar_.Add(this, NotificationType::SESSION_END,
                 NotificationService::AllSources());
}

TestingAutomationProvider::~TestingAutomationProvider() {
  BrowserList::RemoveObserver(this);
}

void TestingAutomationProvider::OnMessageReceived(
    const IPC::Message& message) {
  IPC_BEGIN_MESSAGE_MAP(TestingAutomationProvider, message)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_CloseBrowser, CloseBrowser)
    IPC_MESSAGE_HANDLER(AutomationMsg_CloseBrowserRequestAsync,
                        CloseBrowserAsync)
    IPC_MESSAGE_HANDLER(AutomationMsg_ActivateTab, ActivateTab)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_AppendTab, AppendTab)

    IPC_MESSAGE_UNHANDLED(AutomationProvider::OnMessageReceived(message));
  IPC_END_MESSAGE_MAP()
}

void TestingAutomationProvider::OnChannelError() {
  BrowserList::CloseAllBrowsersAndExit();
  AutomationProvider::OnChannelError();
}

void TestingAutomationProvider::CloseBrowser(int browser_handle,
                                             IPC::Message* reply_message) {
  if (browser_tracker_->ContainsHandle(browser_handle)) {
    Browser* browser = browser_tracker_->GetResource(browser_handle);
    new BrowserClosedNotificationObserver(browser, this,
                                          reply_message);
    browser->window()->Close();
  } else {
    NOTREACHED();
  }
}

void TestingAutomationProvider::CloseBrowserAsync(int browser_handle) {
  if (browser_tracker_->ContainsHandle(browser_handle)) {
    Browser* browser = browser_tracker_->GetResource(browser_handle);
    browser->window()->Close();
  } else {
    NOTREACHED();
  }
}

void TestingAutomationProvider::ActivateTab(int handle,
                                            int at_index,
                                            int* status) {
  *status = -1;
  if (browser_tracker_->ContainsHandle(handle) && at_index > -1) {
    Browser* browser = browser_tracker_->GetResource(handle);
    if (at_index >= 0 && at_index < browser->tab_count()) {
      browser->SelectTabContentsAt(at_index, true);
      *status = 0;
    }
  }
}

void TestingAutomationProvider::AppendTab(int handle, const GURL& url,
                                          IPC::Message* reply_message) {
  int append_tab_response = -1;  // -1 is the error code
  NotificationObserver* observer = NULL;

  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    observer = AddTabStripObserver(browser, reply_message);
    TabContents* tab_contents = browser->AddTabWithURL(
        url, GURL(), PageTransition::TYPED, -1, TabStripModel::ADD_SELECTED,
        NULL, std::string(), &browser);
    if (tab_contents) {
      append_tab_response =
          GetIndexForNavigationController(&tab_contents->controller(), browser);
    }
  }

  if (append_tab_response < 0) {
    // The append tab failed. Remove the TabStripObserver
    if (observer) {
      RemoveTabStripObserver(observer);
      delete observer;
    }

    AutomationMsg_AppendTab::WriteReplyParams(reply_message,
                                              append_tab_response);
    Send(reply_message);
  }
}

void TestingAutomationProvider::OnBrowserAdded(const Browser* browser) {
}

void TestingAutomationProvider::OnBrowserRemoving(const Browser* browser) {
  // For backwards compatibility with the testing automation interface, we
  // want the automation provider (and hence the process) to go away when the
  // last browser goes away.
  if (BrowserList::size() == 1 && !CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kKeepAliveForTest)) {
    // If you change this, update Observer for NotificationType::SESSION_END
    // below.
    MessageLoop::current()->PostTask(FROM_HERE,
        NewRunnableMethod(this, &TestingAutomationProvider::OnRemoveProvider));
  }
}

void TestingAutomationProvider::Observe(NotificationType type,
                                        const NotificationSource& source,
                                        const NotificationDetails& details) {
  DCHECK(type == NotificationType::SESSION_END);
  // OnBrowserRemoving does a ReleaseLater. When session end is received we exit
  // before the task runs resulting in this object not being deleted. This
  // Release balance out the Release scheduled by OnBrowserRemoving.
  Release();
}

void TestingAutomationProvider::OnRemoveProvider() {
  AutomationProviderList::GetInstance()->RemoveProvider(this);
}
