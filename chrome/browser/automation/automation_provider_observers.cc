// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/automation_provider_observers.h"

#include "base/basictypes.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/automation/automation_provider.h"
#include "chrome/browser/dom_operation_notification_details.h"
#include "chrome/browser/login_prompt.h"
#include "chrome/browser/metrics/metric_event_duration_details.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/notification_service.h"

#if defined(OS_WIN)
#include "chrome/browser/printing/print_job.h"
#endif  // defined(OS_WIN)

InitialLoadObserver::InitialLoadObserver(size_t tab_count,
                                         AutomationProvider* automation)
    : automation_(automation),
      outstanding_tab_count_(tab_count) {
  if (outstanding_tab_count_ > 0) {
    registrar_.Add(this, NotificationType::LOAD_START,
                   NotificationService::AllSources());
    registrar_.Add(this, NotificationType::LOAD_STOP,
                   NotificationService::AllSources());
  }
}

InitialLoadObserver::~InitialLoadObserver() {
}

void InitialLoadObserver::Observe(NotificationType type,
                                  const NotificationSource& source,
                                  const NotificationDetails& details) {
  if (type == NotificationType::LOAD_START) {
    if (outstanding_tab_count_ > loading_tabs_.size())
      loading_tabs_.insert(source.map_key());
  } else if (type == NotificationType::LOAD_STOP) {
    if (outstanding_tab_count_ > finished_tabs_.size()) {
      if (loading_tabs_.find(source.map_key()) != loading_tabs_.end())
        finished_tabs_.insert(source.map_key());
      if (outstanding_tab_count_ == finished_tabs_.size())
        ConditionMet();
    }
  } else {
    NOTREACHED();
  }
}

void InitialLoadObserver::ConditionMet() {
  registrar_.RemoveAll();
  automation_->Send(new AutomationMsg_InitialLoadsComplete(0));
}

NewTabUILoadObserver::NewTabUILoadObserver(AutomationProvider* automation)
    : automation_(automation) {
  registrar_.Add(this, NotificationType::INITIAL_NEW_TAB_UI_LOAD,
                 NotificationService::AllSources());
}

NewTabUILoadObserver::~NewTabUILoadObserver() {
}

void NewTabUILoadObserver::Observe(NotificationType type,
                                   const NotificationSource& source,
                                   const NotificationDetails& details) {
  if (type == NotificationType::INITIAL_NEW_TAB_UI_LOAD) {
    Details<int> load_time(details);
    automation_->Send(
        new AutomationMsg_InitialNewTabUILoadComplete(0, *load_time.ptr()));
  } else {
    NOTREACHED();
  }
}

NavigationControllerRestoredObserver::NavigationControllerRestoredObserver(
    AutomationProvider* automation,
    NavigationController* controller,
    IPC::Message* reply_message)
    : automation_(automation),
      controller_(controller),
      reply_message_(reply_message) {
  if (FinishedRestoring()) {
    SendDone();
  } else {
    registrar_.Add(this, NotificationType::LOAD_STOP,
                   NotificationService::AllSources());
  }
}

NavigationControllerRestoredObserver::~NavigationControllerRestoredObserver() {
}

void NavigationControllerRestoredObserver::Observe(
    NotificationType type, const NotificationSource& source,
    const NotificationDetails& details) {
  if (FinishedRestoring()) {
    SendDone();
    registrar_.RemoveAll();
  }
}

bool NavigationControllerRestoredObserver::FinishedRestoring() {
  return (!controller_->needs_reload() && !controller_->pending_entry() &&
          !controller_->tab_contents()->is_loading());
}

void NavigationControllerRestoredObserver::SendDone() {
  DCHECK(reply_message_ != NULL);
  automation_->Send(reply_message_);
}

NavigationNotificationObserver::NavigationNotificationObserver(
    NavigationController* controller,
    AutomationProvider* automation,
    IPC::Message* reply_message,
    int number_of_navigations)
  : automation_(automation),
    reply_message_(reply_message),
    controller_(controller),
    navigations_remaining_(number_of_navigations),
    navigation_started_(false) {
  DCHECK_LT(0, navigations_remaining_);
  Source<NavigationController> source(controller_);
  registrar_.Add(this, NotificationType::NAV_ENTRY_COMMITTED, source);
  registrar_.Add(this, NotificationType::LOAD_START, source);
  registrar_.Add(this, NotificationType::LOAD_STOP, source);
  registrar_.Add(this, NotificationType::AUTH_NEEDED, source);
  registrar_.Add(this, NotificationType::AUTH_SUPPLIED, source);
}

NavigationNotificationObserver::~NavigationNotificationObserver() {
  if (reply_message_) {
    // This means we did not receive a notification for this navigation.
    // Send over a failed navigation status back to the caller to ensure that
    // the caller does not hang waiting for the response.
    IPC::ParamTraits<AutomationMsg_NavigationResponseValues>::Write(
        reply_message_, AUTOMATION_MSG_NAVIGATION_ERROR);
    automation_->Send(reply_message_);
    reply_message_ = NULL;
  }

  automation_->RemoveNavigationStatusListener(this);
}

void NavigationNotificationObserver::Observe(
    NotificationType type, const NotificationSource& source,
    const NotificationDetails& details) {
  // We listen for 2 events to determine when the navigation started because:
  // - when this is used by the WaitForNavigation method, we might be invoked
  // afer the load has started (but not after the entry was committed, as
  // WaitForNavigation compares times of the last navigation).
  // - when this is used with a page requiring authentication, we will not get
  // a NotificationType::NAV_ENTRY_COMMITTED until after we authenticate, so
  // we need the NotificationType::LOAD_START.
  if (type == NotificationType::NAV_ENTRY_COMMITTED ||
      type == NotificationType::LOAD_START) {
    navigation_started_ = true;
  } else if (type == NotificationType::LOAD_STOP) {
    if (navigation_started_) {
      navigation_started_ = false;
      if (--navigations_remaining_ == 0)
        ConditionMet(AUTOMATION_MSG_NAVIGATION_SUCCESS);
    }
  } else if (type == NotificationType::AUTH_SUPPLIED) {
    // The LoginHandler for this tab is no longer valid.
    automation_->RemoveLoginHandler(controller_);

    // Treat this as if navigation started again, since load start/stop don't
    // occur while authentication is ongoing.
    navigation_started_ = true;
  } else if (type == NotificationType::AUTH_NEEDED) {
#if defined(OS_WIN)
    if (navigation_started_) {
      // Remember the login handler that wants authentication.
      LoginHandler* handler =
          Details<LoginNotificationDetails>(details)->handler();
      automation_->AddLoginHandler(controller_, handler);

      // Respond that authentication is needed.
      navigation_started_ = false;
      ConditionMet(AUTOMATION_MSG_NAVIGATION_AUTH_NEEDED);
    } else {
      NOTREACHED();
    }
#else
    // TODO(port): Enable when we have LoginNotificationDetails etc.
    NOTIMPLEMENTED();
#endif
  } else {
    NOTREACHED();
  }
}

void NavigationNotificationObserver::ConditionMet(
    AutomationMsg_NavigationResponseValues navigation_result) {
  DCHECK(reply_message_ != NULL);

  IPC::ParamTraits<AutomationMsg_NavigationResponseValues>::Write(
      reply_message_, navigation_result);
  automation_->Send(reply_message_);
  reply_message_ = NULL;

  delete this;
}

TabStripNotificationObserver::TabStripNotificationObserver(
    NotificationType notification, AutomationProvider* automation)
    : automation_(automation),
      notification_(notification) {
  registrar_.Add(this, notification_, NotificationService::AllSources());
}

TabStripNotificationObserver::~TabStripNotificationObserver() {
}

void TabStripNotificationObserver::Observe(NotificationType type,
                                           const NotificationSource& source,
                                           const NotificationDetails& details) {
  if (type == notification_) {
    ObserveTab(Source<NavigationController>(source).ptr());

    // If verified, no need to observe anymore
    automation_->RemoveTabStripObserver(this);
    delete this;
  } else {
    NOTREACHED();
  }
}

TabAppendedNotificationObserver::TabAppendedNotificationObserver(
    Browser* parent, AutomationProvider* automation,
    IPC::Message* reply_message)
    : TabStripNotificationObserver(NotificationType::TAB_PARENTED, automation),
      parent_(parent),
      reply_message_(reply_message) {
}

void TabAppendedNotificationObserver::ObserveTab(
    NavigationController* controller) {
  if (automation_->GetIndexForNavigationController(controller, parent_) ==
      TabStripModel::kNoTab) {
    // This tab notification doesn't belong to the parent_.
    return;
  }

  automation_->AddNavigationStatusListener(controller, reply_message_, 1);
}

TabClosedNotificationObserver::TabClosedNotificationObserver(
    AutomationProvider* automation, bool wait_until_closed,
    IPC::Message* reply_message)
    : TabStripNotificationObserver(wait_until_closed ?
          NotificationType::TAB_CLOSED : NotificationType::TAB_CLOSING,
          automation),
      reply_message_(reply_message),
      for_browser_command_(false) {
}

void TabClosedNotificationObserver::ObserveTab(
    NavigationController* controller) {
  if (for_browser_command_) {
    AutomationMsg_WindowExecuteCommand::WriteReplyParams(reply_message_,
                                                         true);
  } else {
    AutomationMsg_CloseTab::WriteReplyParams(reply_message_, true);
  }
  automation_->Send(reply_message_);
}

void TabClosedNotificationObserver::set_for_browser_command(
    bool for_browser_command) {
  for_browser_command_ = for_browser_command;
}

ExtensionNotificationObserver::ExtensionNotificationObserver(
    AutomationProvider* automation, int id, IPC::Message* reply_message)
    : automation_(automation),
      id_(id),
      reply_message_(reply_message) {
  registrar_.Add(this, NotificationType::EXTENSION_LOADED,
                 NotificationService::AllSources());
  registrar_.Add(this, NotificationType::EXTENSION_INSTALL_ERROR,
                 NotificationService::AllSources());
  registrar_.Add(this, NotificationType::EXTENSION_OVERINSTALL_ERROR,
                 NotificationService::AllSources());
  registrar_.Add(this, NotificationType::EXTENSION_UPDATE_DISABLED,
                 NotificationService::AllSources());
}

ExtensionNotificationObserver::~ExtensionNotificationObserver() {
}

void ExtensionNotificationObserver::Observe(
    NotificationType type, const NotificationSource& source,
    const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::EXTENSION_LOADED:
      SendResponse(AUTOMATION_MSG_EXTENSION_INSTALL_SUCCEEDED);
      break;
    case NotificationType::EXTENSION_INSTALL_ERROR:
    case NotificationType::EXTENSION_UPDATE_DISABLED:
      SendResponse(AUTOMATION_MSG_EXTENSION_INSTALL_FAILED);
      break;
    case NotificationType::EXTENSION_OVERINSTALL_ERROR:
      SendResponse(AUTOMATION_MSG_EXTENSION_ALREADY_INSTALLED);
      break;
    default:
      NOTREACHED();
      break;
  }

  delete this;
}

void ExtensionNotificationObserver::SendResponse(
    AutomationMsg_ExtensionResponseValues response) {
  if (reply_message_ != NULL) {
    switch (id_) {
      case AutomationMsg_InstallExtension::ID:
        AutomationMsg_InstallExtension::WriteReplyParams(reply_message_,
                                                         response);
        break;
      case AutomationMsg_LoadExpandedExtension::ID:
        AutomationMsg_LoadExpandedExtension::WriteReplyParams(reply_message_,
                                                              response);
        break;
      default:
        NOTREACHED();
        break;
    }

    automation_->Send(reply_message_);
    reply_message_ = NULL;
  }
}

BrowserOpenedNotificationObserver::BrowserOpenedNotificationObserver(
    AutomationProvider* automation, IPC::Message* reply_message)
    : automation_(automation),
      reply_message_(reply_message),
      for_browser_command_(false) {
  registrar_.Add(this, NotificationType::BROWSER_OPENED,
                 NotificationService::AllSources());
}

BrowserOpenedNotificationObserver::~BrowserOpenedNotificationObserver() {
}

void BrowserOpenedNotificationObserver::Observe(
    NotificationType type, const NotificationSource& source,
    const NotificationDetails& details) {
  if (type == NotificationType::BROWSER_OPENED) {
    if (for_browser_command_) {
      AutomationMsg_WindowExecuteCommand::WriteReplyParams(reply_message_,
                                                           true);
    }
    automation_->Send(reply_message_);
    delete this;
  } else {
    NOTREACHED();
  }
}

void BrowserOpenedNotificationObserver::set_for_browser_command(
    bool for_browser_command) {
  for_browser_command_ = for_browser_command;
}

BrowserClosedNotificationObserver::BrowserClosedNotificationObserver(
    Browser* browser,
    AutomationProvider* automation,
    IPC::Message* reply_message)
    : automation_(automation),
      reply_message_(reply_message),
      for_browser_command_(false) {
  registrar_.Add(this, NotificationType::BROWSER_CLOSED,
                 Source<Browser>(browser));
}

void BrowserClosedNotificationObserver::Observe(
    NotificationType type, const NotificationSource& source,
    const NotificationDetails& details) {
  DCHECK(type == NotificationType::BROWSER_CLOSED);
  Details<bool> close_app(details);
  DCHECK(reply_message_ != NULL);
  if (for_browser_command_) {
    AutomationMsg_WindowExecuteCommand::WriteReplyParams(reply_message_,
                                                         true);
  } else {
    AutomationMsg_CloseBrowser::WriteReplyParams(reply_message_, true,
                                                 *(close_app.ptr()));
  }
  automation_->Send(reply_message_);
  reply_message_ = NULL;
  delete this;
}

void BrowserClosedNotificationObserver::set_for_browser_command(
    bool for_browser_command) {
  for_browser_command_ = for_browser_command;
}

BrowserCountChangeNotificationObserver::BrowserCountChangeNotificationObserver(
    int target_count,
    AutomationProvider* automation,
    IPC::Message* reply_message)
    : target_count_(target_count),
      automation_(automation),
      reply_message_(reply_message) {
  registrar_.Add(this, NotificationType::BROWSER_OPENED,
                 NotificationService::AllSources());
  registrar_.Add(this, NotificationType::BROWSER_CLOSED,
                 NotificationService::AllSources());
}

void BrowserCountChangeNotificationObserver::Observe(
    NotificationType type, const NotificationSource& source,
    const NotificationDetails& details) {
  DCHECK(type == NotificationType::BROWSER_OPENED ||
         type == NotificationType::BROWSER_CLOSED);
  int current_count = static_cast<int>(BrowserList::size());
  if (type == NotificationType::BROWSER_CLOSED) {
    // At the time of the notification the browser being closed is not removed
    // from the list. The real count is one less than the reported count.
    DCHECK_LT(0, current_count);
    current_count--;
  }
  if (current_count == target_count_) {
    AutomationMsg_WaitForBrowserWindowCountToBecome::WriteReplyParams(
        reply_message_, true);
    automation_->Send(reply_message_);
    reply_message_ = NULL;
    delete this;
  }
}

AppModalDialogShownObserver::AppModalDialogShownObserver(
    AutomationProvider* automation, IPC::Message* reply_message)
    : automation_(automation),
      reply_message_(reply_message) {
  registrar_.Add(this, NotificationType::APP_MODAL_DIALOG_SHOWN,
                 NotificationService::AllSources());
}

AppModalDialogShownObserver::~AppModalDialogShownObserver() {
}

void AppModalDialogShownObserver::Observe(
    NotificationType type, const NotificationSource& source,
    const NotificationDetails& details) {
  DCHECK(type == NotificationType::APP_MODAL_DIALOG_SHOWN);
  AutomationMsg_WaitForAppModalDialogToBeShown::WriteReplyParams(
      reply_message_, true);
  automation_->Send(reply_message_);
  reply_message_ = NULL;
  delete this;
}

namespace {

// Define mapping from command to notification
struct CommandNotification {
  int command;
  NotificationType::Type notification_type;
};

const struct CommandNotification command_notifications[] = {
  {IDC_DUPLICATE_TAB, NotificationType::TAB_PARENTED},
  {IDC_NEW_TAB, NotificationType::INITIAL_NEW_TAB_UI_LOAD},
  // Returns as soon as the restored tab is created. To further wait until
  // the content page is loaded, use WaitForTabToBeRestored.
  {IDC_RESTORE_TAB, NotificationType::TAB_PARENTED}
};

}  // namespace

ExecuteBrowserCommandObserver::~ExecuteBrowserCommandObserver() {
}

// static
bool ExecuteBrowserCommandObserver::CreateAndRegisterObserver(
    AutomationProvider* automation, Browser* browser, int command,
    IPC::Message* reply_message) {
  bool result = true;
  switch (command) {
    case IDC_NEW_WINDOW:
    case IDC_NEW_INCOGNITO_WINDOW: {
      BrowserOpenedNotificationObserver* observer =
          new BrowserOpenedNotificationObserver(automation, reply_message);
      observer->set_for_browser_command(true);
      break;
    }
    case IDC_CLOSE_WINDOW: {
      BrowserClosedNotificationObserver* observer =
          new BrowserClosedNotificationObserver(browser, automation,
                                                reply_message);
      observer->set_for_browser_command(true);
      break;
    }
    case IDC_CLOSE_TAB: {
      TabClosedNotificationObserver* observer =
          new TabClosedNotificationObserver(automation, true, reply_message);
      observer->set_for_browser_command(true);
      break;
    }
    case IDC_BACK:
    case IDC_FORWARD:
    case IDC_RELOAD: {
      automation->AddNavigationStatusListener(
          &browser->GetSelectedTabContents()->controller(),
          reply_message, 1);
      break;
    }
    default: {
      ExecuteBrowserCommandObserver* observer =
          new ExecuteBrowserCommandObserver(automation, reply_message);
      if (!observer->Register(command)) {
        delete observer;
        result = false;
      }
      break;
    }
  }
  return result;
}

void ExecuteBrowserCommandObserver::Observe(
    NotificationType type, const NotificationSource& source,
    const NotificationDetails& details) {
  if (type == notification_type_) {
    AutomationMsg_WindowExecuteCommand::WriteReplyParams(reply_message_,
                                                         true);
    automation_->Send(reply_message_);
    delete this;
  } else {
    NOTREACHED();
  }
}

ExecuteBrowserCommandObserver::ExecuteBrowserCommandObserver(
    AutomationProvider* automation, IPC::Message* reply_message)
    : automation_(automation),
      reply_message_(reply_message) {
}

bool ExecuteBrowserCommandObserver::Register(int command) {
  if (!GetNotificationType(command, &notification_type_))
    return false;
  registrar_.Add(this, notification_type_, NotificationService::AllSources());
  return true;
}

bool ExecuteBrowserCommandObserver::GetNotificationType(
    int command, NotificationType::Type* type) {
  if (!type)
    return false;
  bool found = false;
  for (unsigned int i = 0; i < arraysize(command_notifications); i++) {
    if (command_notifications[i].command == command) {
      *type = command_notifications[i].notification_type;
      found = true;
      break;
    }
  }
  return found;
}

FindInPageNotificationObserver::FindInPageNotificationObserver(
    AutomationProvider* automation, TabContents* parent_tab,
    IPC::Message* reply_message)
    : automation_(automation),
      active_match_ordinal_(-1),
      reply_message_(reply_message) {
  registrar_.Add(this, NotificationType::FIND_RESULT_AVAILABLE,
                 Source<TabContents>(parent_tab));
}

FindInPageNotificationObserver::~FindInPageNotificationObserver() {
}

void FindInPageNotificationObserver::Observe(
    NotificationType type, const NotificationSource& source,
    const NotificationDetails& details) {
  if (type == NotificationType::FIND_RESULT_AVAILABLE) {
    Details<FindNotificationDetails> find_details(details);
    if (find_details->request_id() == kFindInPageRequestId) {
      // We get multiple responses and one of those will contain the ordinal.
      // This message comes to us before the final update is sent.
      if (find_details->active_match_ordinal() > -1)
        active_match_ordinal_ = find_details->active_match_ordinal();
      if (find_details->final_update()) {
        if (reply_message_ != NULL) {
          AutomationMsg_FindInPage::WriteReplyParams(reply_message_,
              active_match_ordinal_, find_details->number_of_matches());
          automation_->Send(reply_message_);
          reply_message_ = NULL;
        } else {
          DLOG(WARNING) << "Multiple final Find messages observed.";
        }
      } else {
        DLOG(INFO) << "Ignoring, since we only care about the final message";
      }
    }
  } else {
    NOTREACHED();
  }
}

// static
const int FindInPageNotificationObserver::kFindInPageRequestId = -1;

DomOperationNotificationObserver::DomOperationNotificationObserver(
    AutomationProvider* automation)
    : automation_(automation) {
  registrar_.Add(this, NotificationType::DOM_OPERATION_RESPONSE,
                 NotificationService::AllSources());
}

DomOperationNotificationObserver::~DomOperationNotificationObserver() {
}

void DomOperationNotificationObserver::Observe(
    NotificationType type, const NotificationSource& source,
    const NotificationDetails& details) {
  if (NotificationType::DOM_OPERATION_RESPONSE == type) {
    Details<DomOperationNotificationDetails> dom_op_details(details);

    IPC::Message* reply_message = automation_->reply_message_release();
    if (reply_message) {
      AutomationMsg_DomOperation::WriteReplyParams(reply_message,
                                                   dom_op_details->json());
      automation_->Send(reply_message);
    }
  }
}

#if defined(OS_WIN)
// TODO(port): Enable when printing is ported.
DocumentPrintedNotificationObserver::DocumentPrintedNotificationObserver(
    AutomationProvider* automation, IPC::Message* reply_message)
    : automation_(automation),
      success_(false),
      reply_message_(reply_message) {
  registrar_.Add(this, NotificationType::PRINT_JOB_EVENT,
                 NotificationService::AllSources());
}

DocumentPrintedNotificationObserver::~DocumentPrintedNotificationObserver() {
  DCHECK(reply_message_ != NULL);
  AutomationMsg_PrintNow::WriteReplyParams(reply_message_, success_);
  automation_->Send(reply_message_);
  automation_->RemoveNavigationStatusListener(this);
}

void DocumentPrintedNotificationObserver::Observe(
    NotificationType type, const NotificationSource& source,
    const NotificationDetails& details) {
  using namespace printing;
  DCHECK(type == NotificationType::PRINT_JOB_EVENT);
  switch (Details<JobEventDetails>(details)->type()) {
    case JobEventDetails::JOB_DONE: {
      // Succeeded.
      success_ = true;
      delete this;
      break;
    }
    case JobEventDetails::USER_INIT_CANCELED:
    case JobEventDetails::FAILED: {
      // Failed.
      delete this;
      break;
    }
    case JobEventDetails::NEW_DOC:
    case JobEventDetails::USER_INIT_DONE:
    case JobEventDetails::DEFAULT_INIT_DONE:
    case JobEventDetails::NEW_PAGE:
    case JobEventDetails::PAGE_DONE:
    case JobEventDetails::DOC_DONE:
    case JobEventDetails::ALL_PAGES_REQUESTED: {
      // Don't care.
      break;
    }
    default: {
      NOTREACHED();
      break;
    }
  }
}
#endif  // defined(OS_WIN)

MetricEventDurationObserver::MetricEventDurationObserver() {
  registrar_.Add(this, NotificationType::METRIC_EVENT_DURATION,
                 NotificationService::AllSources());
}

int MetricEventDurationObserver::GetEventDurationMs(
    const std::string& event_name) {
  EventDurationMap::const_iterator it = durations_.find(event_name);
  if (it == durations_.end())
    return -1;
  return it->second;
}

void MetricEventDurationObserver::Observe(NotificationType type,
    const NotificationSource& source, const NotificationDetails& details) {
  if (type != NotificationType::METRIC_EVENT_DURATION) {
    NOTREACHED();
    return;
  }
  MetricEventDurationDetails* metric_event_duration =
      Details<MetricEventDurationDetails>(details).ptr();
  durations_[metric_event_duration->event_name] =
      metric_event_duration->duration_ms;
}
