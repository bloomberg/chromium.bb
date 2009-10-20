// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOMATION_AUTOMATION_PROVIDER_OBSERVERS_H_
#define CHROME_BROWSER_AUTOMATION_AUTOMATION_PROVIDER_OBSERVERS_H_

#include <map>
#include <set>

#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_type.h"
#include "chrome/test/automation/automation_messages.h"

class AutomationProvider;
class Browser;
class NavigationController;
class TabContents;

namespace IPC {
class Message;
}

class InitialLoadObserver : public NotificationObserver {
 public:
  InitialLoadObserver(size_t tab_count, AutomationProvider* automation);
  ~InitialLoadObserver();

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  typedef std::set<uintptr_t> TabSet;

  void ConditionMet();

  NotificationRegistrar registrar_;

  AutomationProvider* automation_;
  size_t outstanding_tab_count_;
  TabSet loading_tabs_;
  TabSet finished_tabs_;

  DISALLOW_COPY_AND_ASSIGN(InitialLoadObserver);
};

// Watches for NewTabUI page loads for performance timing purposes.
class NewTabUILoadObserver : public NotificationObserver {
 public:
  explicit NewTabUILoadObserver(AutomationProvider* automation);
  ~NewTabUILoadObserver();

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  NotificationRegistrar registrar_;
  AutomationProvider* automation_;

  DISALLOW_COPY_AND_ASSIGN(NewTabUILoadObserver);
};

class NavigationControllerRestoredObserver : public NotificationObserver {
 public:
  NavigationControllerRestoredObserver(AutomationProvider* automation,
                                       NavigationController* controller,
                                       IPC::Message* reply_message);
  ~NavigationControllerRestoredObserver();

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  bool FinishedRestoring();
  void SendDone();

  NotificationRegistrar registrar_;
  AutomationProvider* automation_;
  NavigationController* controller_;
  IPC::Message* reply_message_;

  DISALLOW_COPY_AND_ASSIGN(NavigationControllerRestoredObserver);
};

class NavigationNotificationObserver : public NotificationObserver {
 public:
  NavigationNotificationObserver(NavigationController* controller,
                                 AutomationProvider* automation,
                                 IPC::Message* reply_message,
                                 int number_of_navigations);
  ~NavigationNotificationObserver();

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  void ConditionMet(AutomationMsg_NavigationResponseValues navigation_result);

  NotificationRegistrar registrar_;
  AutomationProvider* automation_;
  IPC::Message* reply_message_;
  NavigationController* controller_;
  int navigations_remaining_;
  bool navigation_started_;

  DISALLOW_COPY_AND_ASSIGN(NavigationNotificationObserver);
};

class TabStripNotificationObserver : public NotificationObserver {
 public:
  TabStripNotificationObserver(NotificationType notification,
                               AutomationProvider* automation);
  virtual ~TabStripNotificationObserver();

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  virtual void ObserveTab(NavigationController* controller) = 0;

 protected:
  NotificationRegistrar registrar_;
  AutomationProvider* automation_;
  NotificationType notification_;
};

class TabAppendedNotificationObserver : public TabStripNotificationObserver {
 public:
  TabAppendedNotificationObserver(Browser* parent,
                                  AutomationProvider* automation,
                                  IPC::Message* reply_message);

  virtual void ObserveTab(NavigationController* controller);

 protected:
  Browser* parent_;
  IPC::Message* reply_message_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TabAppendedNotificationObserver);
};

class TabClosedNotificationObserver : public TabStripNotificationObserver {
 public:
  TabClosedNotificationObserver(AutomationProvider* automation,
                                bool wait_until_closed,
                                IPC::Message* reply_message);

  virtual void ObserveTab(NavigationController* controller);

  void set_for_browser_command(bool for_browser_command);

 protected:
  IPC::Message* reply_message_;
  bool for_browser_command_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TabClosedNotificationObserver);
};

class ExtensionNotificationObserver : public NotificationObserver {
 public:
  ExtensionNotificationObserver(AutomationProvider* automation,
                                int id,
                                IPC::Message* reply_message);
  virtual ~ExtensionNotificationObserver();

  // Implementation of NotificationObserver.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  void SendResponse(AutomationMsg_ExtensionResponseValues response);

  NotificationRegistrar registrar_;
  scoped_refptr<AutomationProvider> automation_;
  int id_;
  IPC::Message* reply_message_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionNotificationObserver);
};

class BrowserOpenedNotificationObserver : public NotificationObserver {
 public:
  BrowserOpenedNotificationObserver(AutomationProvider* automation,
                                    IPC::Message* reply_message);
  ~BrowserOpenedNotificationObserver();

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  void set_for_browser_command(bool for_browser_command);

 private:
  NotificationRegistrar registrar_;
  AutomationProvider* automation_;
  IPC::Message* reply_message_;
  bool for_browser_command_;

  DISALLOW_COPY_AND_ASSIGN(BrowserOpenedNotificationObserver);
};

class BrowserClosedNotificationObserver : public NotificationObserver {
 public:
  BrowserClosedNotificationObserver(Browser* browser,
                                    AutomationProvider* automation,
                                    IPC::Message* reply_message);

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  void set_for_browser_command(bool for_browser_command);

 private:
  NotificationRegistrar registrar_;
  AutomationProvider* automation_;
  IPC::Message* reply_message_;
  bool for_browser_command_;

  DISALLOW_COPY_AND_ASSIGN(BrowserClosedNotificationObserver);
};

class BrowserCountChangeNotificationObserver : public NotificationObserver {
 public:
  BrowserCountChangeNotificationObserver(int target_count,
                                         AutomationProvider* automation,
                                         IPC::Message* reply_message);

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  int target_count_;
  NotificationRegistrar registrar_;
  AutomationProvider* automation_;
  IPC::Message* reply_message_;

  DISALLOW_COPY_AND_ASSIGN(BrowserCountChangeNotificationObserver);
};

class AppModalDialogShownObserver : public NotificationObserver {
 public:
  AppModalDialogShownObserver(AutomationProvider* automation,
                              IPC::Message* reply_message);
  ~AppModalDialogShownObserver();

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  NotificationRegistrar registrar_;
  AutomationProvider* automation_;
  IPC::Message* reply_message_;

  DISALLOW_COPY_AND_ASSIGN(AppModalDialogShownObserver);
};

class ExecuteBrowserCommandObserver : public NotificationObserver {
 public:
  ~ExecuteBrowserCommandObserver();

  static bool CreateAndRegisterObserver(AutomationProvider* automation,
                                        Browser* browser,
                                        int command,
                                        IPC::Message* reply_message);

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  ExecuteBrowserCommandObserver(AutomationProvider* automation,
                                IPC::Message* reply_message);

  bool Register(int command);

  bool GetNotificationType(int command, NotificationType::Type* type);

  NotificationRegistrar registrar_;
  AutomationProvider* automation_;
  NotificationType::Type notification_type_;
  IPC::Message* reply_message_;

  DISALLOW_COPY_AND_ASSIGN(ExecuteBrowserCommandObserver);
};

class FindInPageNotificationObserver : public NotificationObserver {
 public:
  FindInPageNotificationObserver(AutomationProvider* automation,
                                 TabContents* parent_tab,
                                 IPC::Message* reply_message);
  ~FindInPageNotificationObserver();

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // The Find mechanism is over asynchronous IPC, so a search is kicked off and
  // we wait for notification to find out what the results are. As the user is
  // typing, new search requests can be issued and the Request ID helps us make
  // sense of whether this is the current request or an old one. The unit tests,
  // however, which uses this constant issues only one search at a time, so we
  // don't need a rolling id to identify each search. But, we still need to
  // specify one, so we just use a fixed one - its value does not matter.
  static const int kFindInPageRequestId;

 private:
  NotificationRegistrar registrar_;
  AutomationProvider* automation_;
  // We will at some point (before final update) be notified of the ordinal and
  // we need to preserve it so we can send it later.
  int active_match_ordinal_;
  IPC::Message* reply_message_;

  DISALLOW_COPY_AND_ASSIGN(FindInPageNotificationObserver);
};

class DomOperationNotificationObserver : public NotificationObserver {
 public:
  explicit DomOperationNotificationObserver(AutomationProvider* automation);
  ~DomOperationNotificationObserver();

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  NotificationRegistrar registrar_;
  AutomationProvider* automation_;

  DISALLOW_COPY_AND_ASSIGN(DomOperationNotificationObserver);
};

#if defined(OS_WIN)
// TODO(port): Enable when printing is ported.
class DocumentPrintedNotificationObserver : public NotificationObserver {
 public:
  DocumentPrintedNotificationObserver(AutomationProvider* automation,
                                      IPC::Message* reply_message);
  ~DocumentPrintedNotificationObserver();

  virtual void Observe(NotificationType type, const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  NotificationRegistrar registrar_;
  scoped_refptr<AutomationProvider> automation_;
  bool success_;
  IPC::Message* reply_message_;

  DISALLOW_COPY_AND_ASSIGN(DocumentPrintedNotificationObserver);
};
#endif  // defined(OS_WIN)

// Collects METRIC_EVENT_DURATION notifications and keep track of the times.
class MetricEventDurationObserver : public NotificationObserver {
 public:
  MetricEventDurationObserver();

  // Get the duration of an event.  Returns -1 if we haven't seen the event.
  int GetEventDurationMs(const std::string& event_name);

  // NotificationObserver interface.
  virtual void Observe(NotificationType type, const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  NotificationRegistrar registrar_;

  typedef std::map<std::string, int> EventDurationMap;
  EventDurationMap durations_;

  DISALLOW_COPY_AND_ASSIGN(MetricEventDurationObserver);
};

#endif  // CHROME_BROWSER_AUTOMATION_AUTOMATION_PROVIDER_OBSERVERS_H_
