// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/automation_provider_observers.h"

#include <deque>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/file_util.h"
#include "base/json/json_writer.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/automation/automation_provider.h"
#include "chrome/browser/automation/automation_provider_json.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/dom_operation_notification_details.h"
#include "chrome/browser/download/download_item.h"
#include "chrome/browser/download/save_package.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extension_tabs_module.h"
#include "chrome/browser/extensions/extension_updater.h"
#include "chrome/browser/history/top_sites.h"
#include "chrome/browser/metrics/metric_event_duration_details.h"
#include "chrome/browser/notifications/balloon.h"
#include "chrome/browser/notifications/balloon_collection.h"
#include "chrome/browser/notifications/balloon_host.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/printing/print_job.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/sessions/tab_restore_service.h"
#include "chrome/browser/tab_contents/thumbnail_generator.h"
#include "chrome/browser/translate/page_translated_details.h"
#include "chrome/browser/translate/translate_infobar_delegate.h"
#include "chrome/browser/translate/translate_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/find_bar/find_notification_details.h"
#include "chrome/browser/ui/login/login_prompt.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/webui/app_launcher_handler.h"
#include "chrome/browser/ui/webui/most_visited_handler.h"
#include "chrome/browser/ui/webui/new_tab_ui.h"
#include "chrome/common/automation_messages.h"
#include "chrome/common/extensions/extension.h"
#include "content/browser/renderer_host/render_process_host.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/navigation_controller.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/notification_service.h"
#include "googleurl/src/gurl.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/rect.h"

// Holds onto start and stop timestamps for a particular tab
class InitialLoadObserver::TabTime {
 public:
  explicit TabTime(base::TimeTicks started)
      : load_start_time_(started) {
  }
  void set_stop_time(base::TimeTicks stopped) {
    load_stop_time_ = stopped;
  }
  base::TimeTicks stop_time() const {
    return load_stop_time_;
  }
  base::TimeTicks start_time() const {
    return load_start_time_;
  }
 private:
  base::TimeTicks load_start_time_;
  base::TimeTicks load_stop_time_;
};

InitialLoadObserver::InitialLoadObserver(size_t tab_count,
                                         AutomationProvider* automation)
    : automation_(automation->AsWeakPtr()),
      outstanding_tab_count_(tab_count),
      init_time_(base::TimeTicks::Now()) {
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
      loading_tabs_.insert(TabTimeMap::value_type(
          source.map_key(),
          TabTime(base::TimeTicks::Now())));
  } else if (type == NotificationType::LOAD_STOP) {
    if (outstanding_tab_count_ > finished_tabs_.size()) {
      TabTimeMap::iterator iter = loading_tabs_.find(source.map_key());
      if (iter != loading_tabs_.end()) {
        finished_tabs_.insert(source.map_key());
        iter->second.set_stop_time(base::TimeTicks::Now());
      }
      if (outstanding_tab_count_ == finished_tabs_.size())
        ConditionMet();
    }
  } else {
    NOTREACHED();
  }
}

DictionaryValue* InitialLoadObserver::GetTimingInformation() const {
  ListValue* items = new ListValue;
  for (TabTimeMap::const_iterator it = loading_tabs_.begin();
       it != loading_tabs_.end();
       ++it) {
    DictionaryValue* item = new DictionaryValue;
    base::TimeDelta delta_start = it->second.start_time() - init_time_;

    item->SetDouble("load_start_ms", delta_start.InMillisecondsF());
    if (it->second.stop_time().is_null()) {
      item->Set("load_stop_ms", Value::CreateNullValue());
    } else {
      base::TimeDelta delta_stop = it->second.stop_time() - init_time_;
      item->SetDouble("load_stop_ms", delta_stop.InMillisecondsF());
    }
    items->Append(item);
  }
  DictionaryValue* return_value = new DictionaryValue;
  return_value->Set("tabs", items);
  return return_value;
}

void InitialLoadObserver::ConditionMet() {
  registrar_.RemoveAll();
  if (automation_)
    automation_->OnInitialTabLoadsComplete();
}

NewTabUILoadObserver::NewTabUILoadObserver(AutomationProvider* automation)
    : automation_(automation->AsWeakPtr()) {
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
    if (automation_) {
      automation_->Send(
          new AutomationMsg_InitialNewTabUILoadComplete(*load_time.ptr()));
    }
  } else {
    NOTREACHED();
  }
}

NavigationControllerRestoredObserver::NavigationControllerRestoredObserver(
    AutomationProvider* automation,
    NavigationController* controller,
    IPC::Message* reply_message)
    : automation_(automation->AsWeakPtr()),
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
  if (!automation_)
    return;

  AutomationMsg_WaitForTabToBeRestored::WriteReplyParams(reply_message_.get(),
                                                         true);
  automation_->Send(reply_message_.release());
}

NavigationNotificationObserver::NavigationNotificationObserver(
    NavigationController* controller,
    AutomationProvider* automation,
    IPC::Message* reply_message,
    int number_of_navigations,
    bool include_current_navigation,
    bool use_json_interface)
    : automation_(automation->AsWeakPtr()),
      reply_message_(reply_message),
      controller_(controller),
      navigations_remaining_(number_of_navigations),
      navigation_started_(false),
      use_json_interface_(use_json_interface) {
  DCHECK_LT(0, navigations_remaining_);
  Source<NavigationController> source(controller_);
  registrar_.Add(this, NotificationType::NAV_ENTRY_COMMITTED, source);
  registrar_.Add(this, NotificationType::LOAD_START, source);
  registrar_.Add(this, NotificationType::LOAD_STOP, source);
  registrar_.Add(this, NotificationType::AUTH_NEEDED, source);
  registrar_.Add(this, NotificationType::AUTH_SUPPLIED, source);
  registrar_.Add(this, NotificationType::AUTH_CANCELLED, source);

  if (include_current_navigation && controller->tab_contents()->is_loading())
    navigation_started_ = true;
}

NavigationNotificationObserver::~NavigationNotificationObserver() {
}

void NavigationNotificationObserver::Observe(
    NotificationType type, const NotificationSource& source,
    const NotificationDetails& details) {
  if (!automation_) {
    delete this;
    return;
  }

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
  } else if (type == NotificationType::AUTH_SUPPLIED ||
             type == NotificationType::AUTH_CANCELLED) {
    // The LoginHandler for this tab is no longer valid.
    automation_->RemoveLoginHandler(controller_);

    // Treat this as if navigation started again, since load start/stop don't
    // occur while authentication is ongoing.
    navigation_started_ = true;
  } else if (type == NotificationType::AUTH_NEEDED) {
    // Remember the login handler that wants authentication.
    // We do this in all cases (not just when navigation_started_ == true) so
    // tests can still wait for auth dialogs outside of navigation.
    LoginHandler* handler =
        Details<LoginNotificationDetails>(details)->handler();
    automation_->AddLoginHandler(controller_, handler);

    // Respond that authentication is needed.
    navigation_started_ = false;
    ConditionMet(AUTOMATION_MSG_NAVIGATION_AUTH_NEEDED);
  } else {
    NOTREACHED();
  }
}

void NavigationNotificationObserver::ConditionMet(
    AutomationMsg_NavigationResponseValues navigation_result) {
  if (automation_) {
    if (use_json_interface_) {
      DictionaryValue dict;
      dict.SetInteger("result", navigation_result);
      AutomationJSONReply(automation_, reply_message_.release())
          .SendSuccess(&dict);
    } else {
      IPC::ParamTraits<AutomationMsg_NavigationResponseValues>::Write(
          reply_message_.get(), navigation_result);
      automation_->Send(reply_message_.release());
    }
  }

  delete this;
}

TabStripNotificationObserver::TabStripNotificationObserver(
    NotificationType notification, AutomationProvider* automation)
    : automation_(automation->AsWeakPtr()),
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

TabAppendedNotificationObserver::~TabAppendedNotificationObserver() {}

void TabAppendedNotificationObserver::ObserveTab(
    NavigationController* controller) {
  if (!automation_)
    return;

  if (automation_->GetIndexForNavigationController(controller, parent_) ==
      TabStripModel::kNoTab) {
    // This tab notification doesn't belong to the parent_.
    return;
  }

  new NavigationNotificationObserver(controller, automation_,
                                     reply_message_.release(),
                                     1, false, false);
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

TabClosedNotificationObserver::~TabClosedNotificationObserver() {}

void TabClosedNotificationObserver::ObserveTab(
    NavigationController* controller) {
  if (!automation_)
    return;

  if (for_browser_command_) {
    AutomationMsg_WindowExecuteCommand::WriteReplyParams(reply_message_.get(),
                                                         true);
  } else {
    AutomationMsg_CloseTab::WriteReplyParams(reply_message_.get(), true);
  }
  automation_->Send(reply_message_.release());
}

void TabClosedNotificationObserver::set_for_browser_command(
    bool for_browser_command) {
  for_browser_command_ = for_browser_command;
}

TabCountChangeObserver::TabCountChangeObserver(AutomationProvider* automation,
                                               Browser* browser,
                                               IPC::Message* reply_message,
                                               int target_tab_count)
    : automation_(automation->AsWeakPtr()),
      reply_message_(reply_message),
      tab_strip_model_(browser->tabstrip_model()),
      target_tab_count_(target_tab_count) {
  tab_strip_model_->AddObserver(this);
  CheckTabCount();
}

TabCountChangeObserver::~TabCountChangeObserver() {
  tab_strip_model_->RemoveObserver(this);
}

void TabCountChangeObserver::TabInsertedAt(TabContentsWrapper* contents,
                                           int index,
                                           bool foreground) {
  CheckTabCount();
}

void TabCountChangeObserver::TabDetachedAt(TabContentsWrapper* contents,
                                           int index) {
  CheckTabCount();
}

void TabCountChangeObserver::TabStripModelDeleted() {
  if (automation_) {
    AutomationMsg_WaitForTabCountToBecome::WriteReplyParams(
        reply_message_.get(), false);
    automation_->Send(reply_message_.release());
  }

  delete this;
}

void TabCountChangeObserver::CheckTabCount() {
  if (tab_strip_model_->count() != target_tab_count_)
    return;

  if (automation_) {
    AutomationMsg_WaitForTabCountToBecome::WriteReplyParams(
        reply_message_.get(), true);
    automation_->Send(reply_message_.release());
  }

  delete this;
}

bool DidExtensionHostsStopLoading(ExtensionProcessManager* manager) {
  for (ExtensionProcessManager::const_iterator iter = manager->begin();
       iter != manager->end(); ++iter) {
    if (!(*iter)->did_stop_loading())
      return false;
  }
  return true;
}

ExtensionInstallNotificationObserver::ExtensionInstallNotificationObserver(
    AutomationProvider* automation, int id, IPC::Message* reply_message)
    : automation_(automation->AsWeakPtr()),
      id_(id),
      reply_message_(reply_message) {
  registrar_.Add(this, NotificationType::EXTENSION_LOADED,
                 NotificationService::AllSources());
  registrar_.Add(this, NotificationType::EXTENSION_INSTALL_ERROR,
                 NotificationService::AllSources());
  registrar_.Add(this, NotificationType::EXTENSION_UPDATE_DISABLED,
                 NotificationService::AllSources());
}

ExtensionInstallNotificationObserver::~ExtensionInstallNotificationObserver() {
}

void ExtensionInstallNotificationObserver::Observe(
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
    default:
      NOTREACHED();
      break;
  }

  delete this;
}

void ExtensionInstallNotificationObserver::SendResponse(
    AutomationMsg_ExtensionResponseValues response) {
  if (!automation_ || !reply_message_.get()) {
    delete this;
    return;
  }

  switch (id_) {
    case AutomationMsg_InstallExtension::ID:
      AutomationMsg_InstallExtension::WriteReplyParams(reply_message_.get(),
                                                       response);
      break;
    default:
      NOTREACHED();
      break;
  }

  automation_->Send(reply_message_.release());
}

ExtensionUninstallObserver::ExtensionUninstallObserver(
    AutomationProvider* automation,
    IPC::Message* reply_message,
    const std::string& id)
    : automation_(automation->AsWeakPtr()),
      reply_message_(reply_message),
      id_(id) {
  registrar_.Add(this, NotificationType::EXTENSION_UNINSTALLED,
                 NotificationService::AllSources());
  registrar_.Add(this, NotificationType::EXTENSION_UNINSTALL_NOT_ALLOWED,
                 NotificationService::AllSources());
}

ExtensionUninstallObserver::~ExtensionUninstallObserver() {
}

void ExtensionUninstallObserver::Observe(
    NotificationType type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  if (!automation_) {
    delete this;
    return;
  }

  switch (type.value) {
    case NotificationType::EXTENSION_UNINSTALLED: {
      UninstalledExtensionInfo* info =
          Details<UninstalledExtensionInfo>(details).ptr();
      if (id_ == info->extension_id) {
        scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
        return_value->SetBoolean("success", true);
        AutomationJSONReply(automation_, reply_message_.release())
            .SendSuccess(return_value.get());
        delete this;
        return;
      }
      break;
    }

    case NotificationType::EXTENSION_UNINSTALL_NOT_ALLOWED: {
      const Extension* extension = Details<Extension>(details).ptr();
      if (id_ == extension->id()) {
        scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
        return_value->SetBoolean("success", false);
        AutomationJSONReply(automation_, reply_message_.release())
            .SendSuccess(return_value.get());
        delete this;
        return;
      }
      break;
    }

    default:
      NOTREACHED();
  }
}

ExtensionReadyNotificationObserver::ExtensionReadyNotificationObserver(
    ExtensionProcessManager* manager, AutomationProvider* automation, int id,
    IPC::Message* reply_message)
    : manager_(manager),
      automation_(automation->AsWeakPtr()),
      id_(id),
      reply_message_(reply_message),
      extension_(NULL) {
  registrar_.Add(this, NotificationType::EXTENSION_HOST_DID_STOP_LOADING,
                 NotificationService::AllSources());
  registrar_.Add(this, NotificationType::EXTENSION_LOADED,
                 NotificationService::AllSources());
  registrar_.Add(this, NotificationType::EXTENSION_INSTALL_ERROR,
                 NotificationService::AllSources());
  registrar_.Add(this, NotificationType::EXTENSION_UPDATE_DISABLED,
                 NotificationService::AllSources());
}

ExtensionReadyNotificationObserver::~ExtensionReadyNotificationObserver() {
}

void ExtensionReadyNotificationObserver::Observe(
    NotificationType type, const NotificationSource& source,
    const NotificationDetails& details) {
  if (!automation_) {
    delete this;
    return;
  }

  bool success = false;
  switch (type.value) {
    case NotificationType::EXTENSION_HOST_DID_STOP_LOADING:
      // Only continue on with this method if our extension has been loaded
      // and all the extension hosts have stopped loading.
      if (!extension_ || !DidExtensionHostsStopLoading(manager_))
        return;
      success = true;
      break;
    case NotificationType::EXTENSION_LOADED:
      extension_ = Details<const Extension>(details).ptr();
      if (!DidExtensionHostsStopLoading(manager_))
        return;
      success = true;
      break;
    case NotificationType::EXTENSION_INSTALL_ERROR:
    case NotificationType::EXTENSION_UPDATE_DISABLED:
      success = false;
      break;
    default:
      NOTREACHED();
      break;
  }

  if (id_ == AutomationMsg_InstallExtensionAndGetHandle::ID) {
    // A handle of zero indicates an error.
    int extension_handle = 0;
    if (extension_)
      extension_handle = automation_->AddExtension(extension_);
    AutomationMsg_InstallExtensionAndGetHandle::WriteReplyParams(
        reply_message_.get(), extension_handle);
  } else if (id_ == AutomationMsg_EnableExtension::ID) {
    AutomationMsg_EnableExtension::WriteReplyParams(reply_message_.get(), true);
  } else {
    NOTREACHED();
    LOG(ERROR) << "Cannot write reply params for unknown message id.";
  }

  automation_->Send(reply_message_.release());
  delete this;
}

ExtensionUnloadNotificationObserver::ExtensionUnloadNotificationObserver()
    : did_receive_unload_notification_(false) {
  registrar_.Add(this, NotificationType::EXTENSION_UNLOADED,
                 NotificationService::AllSources());
}

ExtensionUnloadNotificationObserver::~ExtensionUnloadNotificationObserver() {
}

void ExtensionUnloadNotificationObserver::Observe(
    NotificationType type, const NotificationSource& source,
    const NotificationDetails& details) {
  if (type.value == NotificationType::EXTENSION_UNLOADED) {
    did_receive_unload_notification_ = true;
  } else {
    NOTREACHED();
  }
}

ExtensionsUpdatedObserver::ExtensionsUpdatedObserver(
    ExtensionProcessManager* manager, AutomationProvider* automation,
    IPC::Message* reply_message)
    : manager_(manager), automation_(automation->AsWeakPtr()),
      reply_message_(reply_message), updater_finished_(false) {
  registrar_.Add(this, NotificationType::EXTENSION_HOST_DID_STOP_LOADING,
                 NotificationService::AllSources());
  registrar_.Add(this, NotificationType::EXTENSION_INSTALL_ERROR,
                 NotificationService::AllSources());
  registrar_.Add(this, NotificationType::EXTENSION_INSTALL_NOT_ALLOWED,
                 NotificationService::AllSources());
  registrar_.Add(this, NotificationType::EXTENSION_LOADED,
                 NotificationService::AllSources());
  registrar_.Add(this, NotificationType::EXTENSION_UPDATE_DISABLED,
                 NotificationService::AllSources());
  registrar_.Add(this, NotificationType::EXTENSION_UPDATE_FOUND,
                 NotificationService::AllSources());
  registrar_.Add(this, NotificationType::EXTENSION_UPDATING_FINISHED,
                 NotificationService::AllSources());
}

ExtensionsUpdatedObserver::~ExtensionsUpdatedObserver() {
}

void ExtensionsUpdatedObserver::Observe(
    NotificationType type, const NotificationSource& source,
    const NotificationDetails& details) {
  if (!automation_) {
    delete this;
    return;
  }

  // We expect the following sequence of events.  First, the ExtensionUpdater
  // service notifies of each extension that needs to be updated.  Once the
  // ExtensionUpdater has finished searching for extensions to update, it
  // notifies that it is finished.  Meanwhile, the extensions are updated
  // asynchronously: either they will be updated and loaded, or else they will
  // not load due to (1) not being allowed; (2) having updating disabled; or
  // (3) encountering an error.  Finally, notifications are also sent whenever
  // an extension host stops loading.  Updating is not considered complete if
  // any extension hosts are still loading.
  switch (type.value) {
    case NotificationType::EXTENSION_UPDATE_FOUND:
      // Extension updater has identified an extension that needs to be updated.
      in_progress_updates_.insert(*(Details<const std::string>(details).ptr()));
      break;

    case NotificationType::EXTENSION_UPDATING_FINISHED:
      // Extension updater has completed notifying all extensions to update
      // themselves.
      updater_finished_ = true;
      break;

    case NotificationType::EXTENSION_LOADED:
    case NotificationType::EXTENSION_INSTALL_NOT_ALLOWED:
    case NotificationType::EXTENSION_UPDATE_DISABLED: {
      // An extension has either completed update installation and is now
      // loaded, or else the install has been skipped because it is
      // either not allowed or else has been disabled.
      const Extension* extension = Details<Extension>(details).ptr();
      in_progress_updates_.erase(extension->id());
      break;
    }

    case NotificationType::EXTENSION_INSTALL_ERROR: {
      // An extension had an error on update installation.
      CrxInstaller* installer = Source<CrxInstaller>(source).ptr();
      in_progress_updates_.erase(installer->expected_id());
      break;
    }

    case NotificationType::EXTENSION_HOST_DID_STOP_LOADING:
      // Break out to the conditional check below to see if all extension hosts
      // have stopped loading.
      break;

    default:
      NOTREACHED();
      break;
  }

  // Send the reply if (1) the extension updater has finished notifying all
  // extensions to update themselves; (2) all extensions that need to be updated
  // have completed installation and are now loaded; and (3) all extension hosts
  // have stopped loading.
  if (updater_finished_ && in_progress_updates_.empty() &&
      DidExtensionHostsStopLoading(manager_)) {
    AutomationJSONReply reply(automation_, reply_message_.release());
    reply.SendSuccess(NULL);
    delete this;
  }
}

ExtensionTestResultNotificationObserver::
    ExtensionTestResultNotificationObserver(AutomationProvider* automation)
        : automation_(automation->AsWeakPtr()) {
  registrar_.Add(this, NotificationType::EXTENSION_TEST_PASSED,
                 NotificationService::AllSources());
  registrar_.Add(this, NotificationType::EXTENSION_TEST_FAILED,
                 NotificationService::AllSources());
}

ExtensionTestResultNotificationObserver::
    ~ExtensionTestResultNotificationObserver() {
}

void ExtensionTestResultNotificationObserver::Observe(
    NotificationType type, const NotificationSource& source,
    const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::EXTENSION_TEST_PASSED:
      results_.push_back(true);
      messages_.push_back("");
      break;

    case NotificationType::EXTENSION_TEST_FAILED:
      results_.push_back(false);
      messages_.push_back(*(Details<std::string>(details).ptr()));
      break;

    default:
      NOTREACHED();
  }
  // There may be a reply message waiting for this event, so check.
  MaybeSendResult();
}

void ExtensionTestResultNotificationObserver::MaybeSendResult() {
  if (!automation_)
    return;

  if (!results_.empty()) {
    // This release method should return the automation's current
    // reply message, or NULL if there is no current one. If it is not
    // NULL, we are stating that we will handle this reply message.
    IPC::Message* reply_message = automation_->reply_message_release();
    // Send the result back if we have a reply message.
    if (reply_message) {
      AutomationMsg_WaitForExtensionTestResult::WriteReplyParams(
          reply_message, results_.front(), messages_.front());
      results_.pop_front();
      messages_.pop_front();
      automation_->Send(reply_message);
    }
  }
}

BrowserOpenedNotificationObserver::BrowserOpenedNotificationObserver(
    AutomationProvider* automation,
    IPC::Message* reply_message)
    : automation_(automation->AsWeakPtr()),
      reply_message_(reply_message),
      new_window_id_(extension_misc::kUnknownWindowId),
      for_browser_command_(false) {
  registrar_.Add(this, NotificationType::BROWSER_OPENED,
                 NotificationService::AllSources());
  registrar_.Add(this, NotificationType::LOAD_STOP,
                 NotificationService::AllSources());
}

BrowserOpenedNotificationObserver::~BrowserOpenedNotificationObserver() {
}

void BrowserOpenedNotificationObserver::Observe(
    NotificationType type, const NotificationSource& source,
    const NotificationDetails& details) {
  if (!automation_) {
    delete this;
    return;
  }

  if (type.value == NotificationType::BROWSER_OPENED) {
    // Store the new browser ID and continue waiting for a new tab within it
    // to stop loading.
    new_window_id_ = ExtensionTabUtil::GetWindowId(
        Source<Browser>(source).ptr());
  } else if (type.value == NotificationType::LOAD_STOP) {
    // Only send the result if the loaded tab is in the new window.
    int window_id = Source<NavigationController>(source)->window_id().id();
    if (window_id == new_window_id_) {
      if (for_browser_command_) {
        AutomationMsg_WindowExecuteCommand::WriteReplyParams(
            reply_message_.get(), true);
      }
      automation_->Send(reply_message_.release());
      delete this;
      return;
    }
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
    : automation_(automation->AsWeakPtr()),
      reply_message_(reply_message),
      for_browser_command_(false) {
  registrar_.Add(this, NotificationType::BROWSER_CLOSED,
                 Source<Browser>(browser));
}

BrowserClosedNotificationObserver::~BrowserClosedNotificationObserver() {}

void BrowserClosedNotificationObserver::Observe(
    NotificationType type, const NotificationSource& source,
    const NotificationDetails& details) {
  DCHECK(type == NotificationType::BROWSER_CLOSED);

  if (!automation_) {
    delete this;
    return;
  }

  Details<bool> close_app(details);

  if (for_browser_command_) {
    AutomationMsg_WindowExecuteCommand::WriteReplyParams(reply_message_.get(),
                                                         true);
  } else {
    AutomationMsg_CloseBrowser::WriteReplyParams(reply_message_.get(), true,
                                                 *(close_app.ptr()));
  }
  automation_->Send(reply_message_.release());
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
      automation_(automation->AsWeakPtr()),
      reply_message_(reply_message) {
  registrar_.Add(this, NotificationType::BROWSER_OPENED,
                 NotificationService::AllSources());
  registrar_.Add(this, NotificationType::BROWSER_CLOSED,
                 NotificationService::AllSources());
}

BrowserCountChangeNotificationObserver::
    ~BrowserCountChangeNotificationObserver() {}

void BrowserCountChangeNotificationObserver::Observe(
    NotificationType type,
    const NotificationSource& source,
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

  if (!automation_) {
    delete this;
    return;
  }

  if (current_count == target_count_) {
    AutomationMsg_WaitForBrowserWindowCountToBecome::WriteReplyParams(
        reply_message_.get(), true);
    automation_->Send(reply_message_.release());
    delete this;
  }
}

AppModalDialogShownObserver::AppModalDialogShownObserver(
    AutomationProvider* automation, IPC::Message* reply_message)
    : automation_(automation->AsWeakPtr()),
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

  if (automation_) {
    AutomationMsg_WaitForAppModalDialogToBeShown::WriteReplyParams(
        reply_message_.get(), true);
    automation_->Send(reply_message_.release());
  }
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

  // Returns as soon as the restored tab is created. To further wait until
  // the content page is loaded, use WaitForTabToBeRestored.
  {IDC_RESTORE_TAB, NotificationType::TAB_PARENTED},

  // For the following commands, we need to wait for a new tab to be created,
  // load to finish, and title to change.
  {IDC_MANAGE_EXTENSIONS, NotificationType::TAB_CONTENTS_TITLE_UPDATED},
  {IDC_OPTIONS, NotificationType::TAB_CONTENTS_TITLE_UPDATED},
  {IDC_PRINT, NotificationType::TAB_CONTENTS_TITLE_UPDATED},
  {IDC_SHOW_DOWNLOADS, NotificationType::TAB_CONTENTS_TITLE_UPDATED},
  {IDC_SHOW_HISTORY, NotificationType::TAB_CONTENTS_TITLE_UPDATED},
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
    case IDC_NEW_TAB: {
      new NewTabObserver(automation, reply_message);
      break;
    }
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
      new NavigationNotificationObserver(
          &browser->GetSelectedTabContents()->controller(),
          automation, reply_message, 1, false, false);
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
    if (automation_) {
      AutomationMsg_WindowExecuteCommand::WriteReplyParams(reply_message_.get(),
                                                           true);
      automation_->Send(reply_message_.release());
    }
    delete this;
  } else {
    NOTREACHED();
  }
}

ExecuteBrowserCommandObserver::ExecuteBrowserCommandObserver(
    AutomationProvider* automation, IPC::Message* reply_message)
    : automation_(automation->AsWeakPtr()),
      notification_type_(NotificationType::ALL),
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
    bool reply_with_json, IPC::Message* reply_message)
    : automation_(automation->AsWeakPtr()),
      active_match_ordinal_(-1),
      reply_with_json_(reply_with_json),
      reply_message_(reply_message) {
  registrar_.Add(this, NotificationType::FIND_RESULT_AVAILABLE,
                 Source<TabContents>(parent_tab));
}

FindInPageNotificationObserver::~FindInPageNotificationObserver() {
}

void FindInPageNotificationObserver::Observe(
    NotificationType type, const NotificationSource& source,
    const NotificationDetails& details) {
  Details<FindNotificationDetails> find_details(details);
  if (!(find_details->final_update() && reply_message_ != NULL)) {
    DVLOG(1) << "Ignoring, since we only care about the final message";
    return;
  }

  if (!automation_) {
    delete this;
    return;
  }

  // We get multiple responses and one of those will contain the ordinal.
  // This message comes to us before the final update is sent.
  if (find_details->request_id() == kFindInPageRequestId) {
    if (reply_with_json_) {
      scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
      return_value->SetInteger("match_count",
          find_details->number_of_matches());
      gfx::Rect rect = find_details->selection_rect();
      // If MatchCount is > 0, then rect should not be Empty.
      // We dont guard it here because we want to let the test
      // code catch this invalid case if needed.
      if (!rect.IsEmpty()) {
        return_value->SetInteger("match_left", rect.x());
        return_value->SetInteger("match_top", rect.y());
        return_value->SetInteger("match_right", rect.right());
        return_value->SetInteger("match_bottom", rect.bottom());
      }
      AutomationJSONReply(automation_, reply_message_.release())
          .SendSuccess(return_value.get());
      delete this;
    } else {
      if (find_details->active_match_ordinal() > -1) {
        active_match_ordinal_ = find_details->active_match_ordinal();
        AutomationMsg_Find::WriteReplyParams(reply_message_.get(),
            active_match_ordinal_, find_details->number_of_matches());
        automation_->Send(reply_message_.release());
      }
    }
  }
}

// static
const int FindInPageNotificationObserver::kFindInPageRequestId = -1;

DomOperationObserver::DomOperationObserver() {
  registrar_.Add(this, NotificationType::DOM_OPERATION_RESPONSE,
                 NotificationService::AllSources());
}

DomOperationObserver::~DomOperationObserver() {}

void DomOperationObserver::Observe(
    NotificationType type, const NotificationSource& source,
    const NotificationDetails& details) {
  if (NotificationType::DOM_OPERATION_RESPONSE == type) {
    Details<DomOperationNotificationDetails> dom_op_details(details);
    OnDomOperationCompleted(dom_op_details->json());
  }
}

DomOperationMessageSender::DomOperationMessageSender(
    AutomationProvider* automation,
    IPC::Message* reply_message,
    bool use_json_interface)
    : automation_(automation->AsWeakPtr()),
      reply_message_(reply_message),
      use_json_interface_(use_json_interface) {
}

DomOperationMessageSender::~DomOperationMessageSender() {}

void DomOperationMessageSender::OnDomOperationCompleted(
    const std::string& json) {
  if (automation_) {
    if (use_json_interface_) {
      DictionaryValue dict;
      dict.SetString("result", json);
      AutomationJSONReply(automation_, reply_message_.release())
          .SendSuccess(&dict);
    } else {
      AutomationMsg_DomOperation::WriteReplyParams(reply_message_.get(), json);
      automation_->Send(reply_message_.release());
    }
  }
  delete this;
}

DocumentPrintedNotificationObserver::DocumentPrintedNotificationObserver(
    AutomationProvider* automation, IPC::Message* reply_message)
    : automation_(automation->AsWeakPtr()),
      success_(false),
      reply_message_(reply_message) {
  registrar_.Add(this, NotificationType::PRINT_JOB_EVENT,
                 NotificationService::AllSources());
}

DocumentPrintedNotificationObserver::~DocumentPrintedNotificationObserver() {
  if (automation_) {
    AutomationMsg_PrintNow::WriteReplyParams(reply_message_.get(), success_);
    automation_->Send(reply_message_.release());
  }
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

MetricEventDurationObserver::MetricEventDurationObserver() {
  registrar_.Add(this, NotificationType::METRIC_EVENT_DURATION,
                 NotificationService::AllSources());
}

MetricEventDurationObserver::~MetricEventDurationObserver() {}

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

PageTranslatedObserver::PageTranslatedObserver(AutomationProvider* automation,
                                               IPC::Message* reply_message,
                                               TabContents* tab_contents)
  : automation_(automation->AsWeakPtr()),
    reply_message_(reply_message) {
  registrar_.Add(this, NotificationType::PAGE_TRANSLATED,
                 Source<TabContents>(tab_contents));
}

PageTranslatedObserver::~PageTranslatedObserver() {}

void PageTranslatedObserver::Observe(NotificationType type,
                                     const NotificationSource& source,
                                     const NotificationDetails& details) {
  if (!automation_) {
    delete this;
    return;
  }

  DCHECK(type == NotificationType::PAGE_TRANSLATED);
  AutomationJSONReply reply(automation_, reply_message_.release());

  PageTranslatedDetails* translated_details =
      Details<PageTranslatedDetails>(details).ptr();
  scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
  return_value->SetBoolean(
      "translation_success",
      translated_details->error_type == TranslateErrors::NONE);
  reply.SendSuccess(return_value.get());
  delete this;
}

TabLanguageDeterminedObserver::TabLanguageDeterminedObserver(
    AutomationProvider* automation, IPC::Message* reply_message,
    TabContents* tab_contents, TranslateInfoBarDelegate* translate_bar)
    : automation_(automation->AsWeakPtr()),
      reply_message_(reply_message),
      tab_contents_(tab_contents),
      translate_bar_(translate_bar) {
  registrar_.Add(this, NotificationType::TAB_LANGUAGE_DETERMINED,
                 Source<TabContents>(tab_contents));
}

TabLanguageDeterminedObserver::~TabLanguageDeterminedObserver() {}

void TabLanguageDeterminedObserver::Observe(
    NotificationType type, const NotificationSource& source,
    const NotificationDetails& details) {
  DCHECK(type == NotificationType::TAB_LANGUAGE_DETERMINED);

  if (!automation_) {
    delete this;
    return;
  }

  TranslateTabHelper* helper = TabContentsWrapper::GetCurrentWrapperForContents(
      tab_contents_)->translate_tab_helper();
  scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
  return_value->SetBoolean("page_translated",
                           helper->language_state().IsPageTranslated());
  return_value->SetBoolean(
      "can_translate_page", TranslatePrefs::CanTranslate(
          automation_->profile()->GetPrefs(),
          helper->language_state().original_language(),
          tab_contents_->GetURL()));
  return_value->SetString("original_language",
                          helper->language_state().original_language());
  if (translate_bar_) {
    DictionaryValue* bar_info = new DictionaryValue;
    std::map<TranslateInfoBarDelegate::Type, std::string> type_to_string;
    type_to_string[TranslateInfoBarDelegate::BEFORE_TRANSLATE] =
        "BEFORE_TRANSLATE";
    type_to_string[TranslateInfoBarDelegate::TRANSLATING] =
        "TRANSLATING";
    type_to_string[TranslateInfoBarDelegate::AFTER_TRANSLATE] =
        "AFTER_TRANSLATE";
    type_to_string[TranslateInfoBarDelegate::TRANSLATION_ERROR] =
        "TRANSLATION_ERROR";

    bar_info->SetBoolean("always_translate_lang_button_showing",
                         translate_bar_->ShouldShowAlwaysTranslateButton());
    bar_info->SetBoolean("never_translate_lang_button_showing",
                         translate_bar_->ShouldShowNeverTranslateButton());
    bar_info->SetString("bar_state", type_to_string[translate_bar_->type()]);
    bar_info->SetString("target_lang_code",
                        translate_bar_->GetTargetLanguageCode());
    bar_info->SetString("original_lang_code",
                        translate_bar_->GetOriginalLanguageCode());
    return_value->Set("translate_bar", bar_info);
  }
  AutomationJSONReply(automation_, reply_message_.release())
      .SendSuccess(return_value.get());
  delete this;
}

InfoBarCountObserver::InfoBarCountObserver(AutomationProvider* automation,
                                           IPC::Message* reply_message,
                                           TabContents* tab_contents,
                                           size_t target_count)
    : automation_(automation->AsWeakPtr()),
      reply_message_(reply_message),
      tab_contents_(tab_contents),
      target_count_(target_count) {
  Source<TabContents> source(tab_contents);
  registrar_.Add(this, NotificationType::TAB_CONTENTS_INFOBAR_ADDED, source);
  registrar_.Add(this, NotificationType::TAB_CONTENTS_INFOBAR_REMOVED, source);
  CheckCount();
}

InfoBarCountObserver::~InfoBarCountObserver() {}

void InfoBarCountObserver::Observe(NotificationType type,
                                   const NotificationSource& source,
                                   const NotificationDetails& details) {
  DCHECK(type == NotificationType::TAB_CONTENTS_INFOBAR_ADDED ||
         type == NotificationType::TAB_CONTENTS_INFOBAR_REMOVED);
  CheckCount();
}

void InfoBarCountObserver::CheckCount() {
  if (tab_contents_->infobar_count() != target_count_)
    return;

  if (automation_) {
    AutomationMsg_WaitForInfoBarCount::WriteReplyParams(reply_message_.get(),
                                                        true);
    automation_->Send(reply_message_.release());
  }
  delete this;
}

AutomationProviderBookmarkModelObserver::
AutomationProviderBookmarkModelObserver(
    AutomationProvider* provider,
    IPC::Message* reply_message,
    BookmarkModel* model)
    : automation_provider_(provider->AsWeakPtr()),
      reply_message_(reply_message),
      model_(model) {
  model_->AddObserver(this);
}

AutomationProviderBookmarkModelObserver::
    ~AutomationProviderBookmarkModelObserver() {
  model_->RemoveObserver(this);
}

void AutomationProviderBookmarkModelObserver::Loaded(BookmarkModel* model) {
  ReplyAndDelete(true);
}

void AutomationProviderBookmarkModelObserver::BookmarkModelBeingDeleted(
    BookmarkModel* model) {
  ReplyAndDelete(false);
}

void AutomationProviderBookmarkModelObserver::ReplyAndDelete(bool success) {
  if (automation_provider_) {
    AutomationMsg_WaitForBookmarkModelToLoad::WriteReplyParams(
        reply_message_.get(), success);
    automation_provider_->Send(reply_message_.release());
  }
  delete this;
}

AutomationProviderDownloadItemObserver::AutomationProviderDownloadItemObserver(
    AutomationProvider* provider,
    IPC::Message* reply_message,
    int downloads)
    : provider_(provider->AsWeakPtr()),
      reply_message_(reply_message),
      downloads_(downloads),
      interrupted_(false) {
}

AutomationProviderDownloadItemObserver::
    ~AutomationProviderDownloadItemObserver() {}

void AutomationProviderDownloadItemObserver::OnDownloadUpdated(
    DownloadItem* download) {
  interrupted_ |= download->IsInterrupted();
  // If any download was interrupted, on the next update each outstanding
  // download is cancelled.
  if (interrupted_) {
    // |Cancel()| does nothing if |download| is already interrupted.
    download->Cancel(true);
    RemoveAndCleanupOnLastEntry(download);
  }

  if (download->IsComplete())
    RemoveAndCleanupOnLastEntry(download);
}

// We don't want to send multiple messages, as the behavior is undefined.
// Set |interrupted_| on error, and on the last download completed/
// interrupted, send either an error or a success message.
void AutomationProviderDownloadItemObserver::RemoveAndCleanupOnLastEntry(
    DownloadItem* download) {
  // Forget about the download.
  download->RemoveObserver(this);
  if (--downloads_ == 0) {
    if (provider_) {
      if (interrupted_) {
        AutomationJSONReply(provider_, reply_message_.release()).SendError(
            "Download Interrupted");
      } else {
        AutomationJSONReply(provider_, reply_message_.release()).SendSuccess(
            NULL);
      }
    }
    delete this;
  }
}

void AutomationProviderDownloadItemObserver::OnDownloadOpened(
    DownloadItem* download) {
}

AutomationProviderDownloadUpdatedObserver::
AutomationProviderDownloadUpdatedObserver(
    AutomationProvider* provider,
    IPC::Message* reply_message,
    bool wait_for_open)
    : provider_(provider->AsWeakPtr()),
      reply_message_(reply_message),
      wait_for_open_(wait_for_open) {
}

AutomationProviderDownloadUpdatedObserver::
    ~AutomationProviderDownloadUpdatedObserver() {}

void AutomationProviderDownloadUpdatedObserver::OnDownloadUpdated(
    DownloadItem* download) {
  // If this observer is watching for open, only send the reply if the download
  // has been auto-opened.
  if (wait_for_open_ && !download->auto_opened())
    return;

  download->RemoveObserver(this);
  scoped_ptr<DictionaryValue> return_value(
      provider_->GetDictionaryFromDownloadItem(download));

  if (provider_) {
    AutomationJSONReply(provider_, reply_message_.release()).SendSuccess(
        return_value.get());
  }
  delete this;
}

void AutomationProviderDownloadUpdatedObserver::OnDownloadOpened(
    DownloadItem* download) {
  download->RemoveObserver(this);
  scoped_ptr<DictionaryValue> return_value(
      provider_->GetDictionaryFromDownloadItem(download));

  if (provider_) {
    AutomationJSONReply(provider_, reply_message_.release()).SendSuccess(
        return_value.get());
  }
  delete this;
}

AutomationProviderDownloadModelChangedObserver::
AutomationProviderDownloadModelChangedObserver(
    AutomationProvider* provider,
    IPC::Message* reply_message,
    DownloadManager* download_manager)
    : provider_(provider->AsWeakPtr()),
      reply_message_(reply_message),
      download_manager_(download_manager) {
}

AutomationProviderDownloadModelChangedObserver::
    ~AutomationProviderDownloadModelChangedObserver() {}

void AutomationProviderDownloadModelChangedObserver::ModelChanged() {
  download_manager_->RemoveObserver(this);

  if (provider_)
    AutomationJSONReply(provider_, reply_message_.release()).SendSuccess(NULL);
  delete this;
}

AutomationProviderSearchEngineObserver::AutomationProviderSearchEngineObserver(
    AutomationProvider* provider,
    IPC::Message* reply_message)
    : provider_(provider->AsWeakPtr()),
      reply_message_(reply_message) {
}

AutomationProviderSearchEngineObserver::
    ~AutomationProviderSearchEngineObserver() {}

void AutomationProviderSearchEngineObserver::OnTemplateURLModelChanged() {
  TemplateURLModel* url_model = provider_->profile()->GetTemplateURLModel();
  url_model->RemoveObserver(this);

  if (provider_)
    AutomationJSONReply(provider_, reply_message_.release()).SendSuccess(NULL);
  delete this;
}

AutomationProviderHistoryObserver::AutomationProviderHistoryObserver(
    AutomationProvider* provider,
    IPC::Message* reply_message)
    : provider_(provider->AsWeakPtr()),
      reply_message_(reply_message) {
}

AutomationProviderHistoryObserver::~AutomationProviderHistoryObserver() {}

void AutomationProviderHistoryObserver::HistoryQueryComplete(
    HistoryService::Handle request_handle,
    history::QueryResults* results) {
  if (!provider_) {
    delete this;
    return;
  }

  scoped_ptr<DictionaryValue> return_value(new DictionaryValue);

  ListValue* history_list = new ListValue;
  for (size_t i = 0; i < results->size(); ++i) {
    DictionaryValue* page_value = new DictionaryValue;
    history::URLResult const &page = (*results)[i];
    page_value->SetString("title", page.title());
    page_value->SetString("url", page.url().spec());
    page_value->SetDouble("time",
                          static_cast<double>(page.visit_time().ToDoubleT()));
    page_value->SetString("snippet", page.snippet().text());
    page_value->SetBoolean(
        "starred",
        provider_->profile()->GetBookmarkModel()->IsBookmarked(page.url()));
    history_list->Append(page_value);
  }

  return_value->Set("history", history_list);
  // Return history info.
  AutomationJSONReply reply(provider_, reply_message_.release());
  reply.SendSuccess(return_value.get());
  delete this;
}

AutomationProviderImportSettingsObserver::
AutomationProviderImportSettingsObserver(
    AutomationProvider* provider,
    IPC::Message* reply_message)
    : provider_(provider->AsWeakPtr()),
      reply_message_(reply_message) {
}

AutomationProviderImportSettingsObserver::
    ~AutomationProviderImportSettingsObserver() {}

void AutomationProviderImportSettingsObserver::ImportStarted() {
}

void AutomationProviderImportSettingsObserver::ImportItemStarted(
    importer::ImportItem item) {
}

void AutomationProviderImportSettingsObserver::ImportItemEnded(
    importer::ImportItem item) {
}

void AutomationProviderImportSettingsObserver::ImportEnded() {
  if (provider_)
    AutomationJSONReply(provider_, reply_message_.release()).SendSuccess(NULL);
  delete this;
}

AutomationProviderGetPasswordsObserver::AutomationProviderGetPasswordsObserver(
    AutomationProvider* provider,
    IPC::Message* reply_message)
    : provider_(provider->AsWeakPtr()),
      reply_message_(reply_message) {
}

AutomationProviderGetPasswordsObserver::
    ~AutomationProviderGetPasswordsObserver() {}

void AutomationProviderGetPasswordsObserver::OnPasswordStoreRequestDone(
    CancelableRequestProvider::Handle handle,
    const std::vector<webkit_glue::PasswordForm*>& result) {
  if (!provider_) {
    delete this;
    return;
  }

  scoped_ptr<DictionaryValue> return_value(new DictionaryValue);

  ListValue* passwords = new ListValue;
  for (std::vector<webkit_glue::PasswordForm*>::const_iterator it =
          result.begin(); it != result.end(); ++it) {
    DictionaryValue* password_val = new DictionaryValue;
    webkit_glue::PasswordForm* password_form = *it;
    password_val->SetString("username_value", password_form->username_value);
    password_val->SetString("password_value", password_form->password_value);
    password_val->SetString("signon_realm", password_form->signon_realm);
    password_val->SetDouble(
        "time", static_cast<double>(password_form->date_created.ToDoubleT()));
    password_val->SetString("origin_url", password_form->origin.spec());
    password_val->SetString("username_element",
                            password_form->username_element);
    password_val->SetString("password_element",
                            password_form->password_element);
    password_val->SetString("submit_element",
                                     password_form->submit_element);
    password_val->SetString("action_target", password_form->action.spec());
    password_val->SetBoolean("blacklist", password_form->blacklisted_by_user);
    passwords->Append(password_val);
  }

  return_value->Set("passwords", passwords);
  AutomationJSONReply(provider_, reply_message_.release()).SendSuccess(
      return_value.get());
  delete this;
}

AutomationProviderBrowsingDataObserver::AutomationProviderBrowsingDataObserver(
    AutomationProvider* provider,
    IPC::Message* reply_message)
    : provider_(provider->AsWeakPtr()),
      reply_message_(reply_message) {
}

AutomationProviderBrowsingDataObserver::
    ~AutomationProviderBrowsingDataObserver() {}

void AutomationProviderBrowsingDataObserver::OnBrowsingDataRemoverDone() {
  if (provider_)
    AutomationJSONReply(provider_, reply_message_.release()).SendSuccess(NULL);
  delete this;
}

OmniboxAcceptNotificationObserver::OmniboxAcceptNotificationObserver(
    NavigationController* controller,
    AutomationProvider* automation,
    IPC::Message* reply_message)
    : automation_(automation->AsWeakPtr()),
      reply_message_(reply_message),
      controller_(controller) {
  Source<NavigationController> source(controller_);
  registrar_.Add(this, NotificationType::LOAD_STOP, source);
  // Pages requiring auth don't send LOAD_STOP.
  registrar_.Add(this, NotificationType::AUTH_NEEDED, source);
}

OmniboxAcceptNotificationObserver::~OmniboxAcceptNotificationObserver() {
}

void OmniboxAcceptNotificationObserver::Observe(
    NotificationType type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  if (type == NotificationType::LOAD_STOP ||
      type == NotificationType::AUTH_NEEDED) {
    if (automation_) {
      AutomationJSONReply(automation_,
                          reply_message_.release()).SendSuccess(NULL);
    }
    delete this;
  } else {
    NOTREACHED();
  }
}

SavePackageNotificationObserver::SavePackageNotificationObserver(
    SavePackage* save_package,
    AutomationProvider* automation,
    IPC::Message* reply_message)
    : automation_(automation->AsWeakPtr()),
      reply_message_(reply_message) {
  Source<SavePackage> source(save_package);
  registrar_.Add(this, NotificationType::SAVE_PACKAGE_SUCCESSFULLY_FINISHED,
                 source);
}

SavePackageNotificationObserver::~SavePackageNotificationObserver() {}

void SavePackageNotificationObserver::Observe(
    NotificationType type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  if (type == NotificationType::SAVE_PACKAGE_SUCCESSFULLY_FINISHED) {
    if (automation_) {
      AutomationJSONReply(automation_,
                          reply_message_.release()).SendSuccess(NULL);
    }
    delete this;
  } else {
    NOTREACHED();
  }
}

PageSnapshotTaker::PageSnapshotTaker(AutomationProvider* automation,
                                     IPC::Message* reply_message,
                                     RenderViewHost* render_view,
                                     const FilePath& path)
    : automation_(automation->AsWeakPtr()),
      reply_message_(reply_message),
      render_view_(render_view),
      image_path_(path),
      received_width_(false) {}

PageSnapshotTaker::~PageSnapshotTaker() {}

void PageSnapshotTaker::Start() {
  ExecuteScript(L"window.domAutomationController.send(document.width);");
}

void PageSnapshotTaker::OnDomOperationCompleted(const std::string& json) {
  int dimension;
  if (!base::StringToInt(json, &dimension)) {
    LOG(ERROR) << "Could not parse received dimensions: " << json;
    SendMessage(false);
  } else if (!received_width_) {
    received_width_ = true;
    entire_page_size_.set_width(dimension);

    ExecuteScript(L"window.domAutomationController.send(document.height);");
  } else {
    entire_page_size_.set_height(dimension);

    ThumbnailGenerator* generator =
        g_browser_process->GetThumbnailGenerator();
    ThumbnailGenerator::ThumbnailReadyCallback* callback =
        NewCallback(this, &PageSnapshotTaker::OnSnapshotTaken);
    // Don't actually start the thumbnail generator, this leads to crashes on
    // Mac, crbug.com/62986. Instead, just hook the generator to the
    // RenderViewHost manually.

    generator->MonitorRenderer(render_view_, true);
    generator->AskForSnapshot(render_view_, false, callback,
                              entire_page_size_, entire_page_size_);
  }
}

void PageSnapshotTaker::OnSnapshotTaken(const SkBitmap& bitmap) {
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  std::vector<unsigned char> png_data;
  gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, true, &png_data);
  int bytes_written = file_util::WriteFile(image_path_,
      reinterpret_cast<char*>(&png_data[0]), png_data.size());
  SendMessage(bytes_written == static_cast<int>(png_data.size()));
}

void PageSnapshotTaker::ExecuteScript(const std::wstring& javascript) {
  std::wstring set_automation_id;
  base::SStringPrintf(
      &set_automation_id,
      L"window.domAutomationController.setAutomationId(%d);",
      reply_message_->routing_id());

  render_view_->ExecuteJavascriptInWebFrame(string16(),
                                            WideToUTF16Hack(set_automation_id));
  render_view_->ExecuteJavascriptInWebFrame(string16(),
                                            WideToUTF16Hack(javascript));
}

void PageSnapshotTaker::SendMessage(bool success) {
  if (automation_) {
    if (success) {
      AutomationJSONReply(automation_, reply_message_.release())
          .SendSuccess(NULL);
    } else {
      AutomationJSONReply(automation_, reply_message_.release())
          .SendError("Failed to take snapshot of page");
    }
  }
  delete this;
}

namespace {

// Returns a vector of dictionaries containing information about installed apps,
// as identified from a given list of extensions.  The caller takes ownership
// of the created vector.
std::vector<DictionaryValue*>* GetAppInfoFromExtensions(
    const ExtensionList* extensions,
    ExtensionPrefs* ext_prefs) {
  std::vector<DictionaryValue*>* apps_list =
      new std::vector<DictionaryValue*>();
  for (ExtensionList::const_iterator ext = extensions->begin();
       ext != extensions->end(); ++ext) {
    // Only return information about extensions that are actually apps.
    if ((*ext)->is_app()) {
      DictionaryValue* app_info = new DictionaryValue();
      AppLauncherHandler::CreateAppInfo(*ext, ext_prefs, app_info);
      app_info->SetBoolean("is_component_extension",
                           (*ext)->location() == Extension::COMPONENT);

      // Convert the launch_type integer into a more descriptive string.
      int launch_type;
      app_info->GetInteger("launch_type", &launch_type);
      if (launch_type == ExtensionPrefs::LAUNCH_PINNED) {
        app_info->SetString("launch_type", "pinned");
      } else if (launch_type == ExtensionPrefs::LAUNCH_REGULAR) {
        app_info->SetString("launch_type", "regular");
      } else if (launch_type == ExtensionPrefs::LAUNCH_FULLSCREEN) {
        app_info->SetString("launch_type", "fullscreen");
      } else if (launch_type == ExtensionPrefs::LAUNCH_WINDOW) {
        app_info->SetString("launch_type", "window");
      } else {
        app_info->SetString("launch_type", "unknown");
      }

      apps_list->push_back(app_info);
    }
  }
  return apps_list;
}

}  // namespace

NTPInfoObserver::NTPInfoObserver(
    AutomationProvider* automation,
    IPC::Message* reply_message,
    CancelableRequestConsumer* consumer)
    : automation_(automation->AsWeakPtr()),
      reply_message_(reply_message),
      consumer_(consumer),
      request_(0),
      ntp_info_(new DictionaryValue) {
  top_sites_ = automation_->profile()->GetTopSites();
  if (!top_sites_) {
    AutomationJSONReply(automation_, reply_message_.release())
        .SendError("Profile does not have service for querying the top sites.");
    return;
  }
  TabRestoreService* service = automation_->profile()->GetTabRestoreService();
  if (!service) {
    AutomationJSONReply(automation_, reply_message_.release())
        .SendError("No TabRestoreService.");
    return;
  }

  // Collect information about the apps in the new tab page.
  ExtensionService* ext_service = automation_->profile()->GetExtensionService();
  if (!ext_service) {
    AutomationJSONReply(automation_, reply_message_.release())
        .SendError("No ExtensionService.");
    return;
  }
  // Process enabled extensions.
  ExtensionPrefs* ext_prefs = ext_service->extension_prefs();
  ListValue* apps_list = new ListValue();
  const ExtensionList* extensions = ext_service->extensions();
  std::vector<DictionaryValue*>* enabled_apps = GetAppInfoFromExtensions(
      extensions, ext_prefs);
  for (std::vector<DictionaryValue*>::const_iterator app =
       enabled_apps->begin(); app != enabled_apps->end(); ++app) {
    (*app)->SetBoolean("is_disabled", false);
    apps_list->Append(*app);
  }
  delete enabled_apps;
  // Process disabled extensions.
  const ExtensionList* disabled_extensions = ext_service->disabled_extensions();
  std::vector<DictionaryValue*>* disabled_apps = GetAppInfoFromExtensions(
      disabled_extensions, ext_prefs);
  for (std::vector<DictionaryValue*>::const_iterator app =
       disabled_apps->begin(); app != disabled_apps->end(); ++app) {
    (*app)->SetBoolean("is_disabled", true);
    apps_list->Append(*app);
  }
  delete disabled_apps;
  ntp_info_->Set("apps", apps_list);

  // Get the info that would be displayed in the recently closed section.
  ListValue* recently_closed_list = new ListValue;
  NewTabUI::AddRecentlyClosedEntries(service->entries(),
                                     recently_closed_list);
  ntp_info_->Set("recently_closed", recently_closed_list);

  // Add default site URLs.
  ListValue* default_sites_list = new ListValue;
  std::vector<GURL> urls = MostVisitedHandler::GetPrePopulatedUrls();
  for (size_t i = 0; i < urls.size(); ++i) {
    default_sites_list->Append(Value::CreateStringValue(
        urls[i].possibly_invalid_spec()));
  }
  ntp_info_->Set("default_sites", default_sites_list);

  registrar_.Add(this, NotificationType::TOP_SITES_UPDATED,
                 Source<history::TopSites>(top_sites_));
  if (top_sites_->loaded()) {
    OnTopSitesLoaded();
  } else {
    registrar_.Add(this, NotificationType::TOP_SITES_LOADED,
                   Source<Profile>(automation_->profile()));
  }
}

NTPInfoObserver::~NTPInfoObserver() {}

void NTPInfoObserver::Observe(NotificationType type,
                              const NotificationSource& source,
                              const NotificationDetails& details) {
  if (type == NotificationType::TOP_SITES_LOADED) {
    OnTopSitesLoaded();
  } else if (type == NotificationType::TOP_SITES_UPDATED) {
    Details<CancelableRequestProvider::Handle> request_details(details);
    if (request_ == *request_details.ptr()) {
      top_sites_->GetMostVisitedURLs(
          consumer_,
          NewCallback(this, &NTPInfoObserver::OnTopSitesReceived));
    }
  }
}

void NTPInfoObserver::OnTopSitesLoaded() {
  request_ = top_sites_->StartQueryForMostVisited();
}

void NTPInfoObserver::OnTopSitesReceived(
    const history::MostVisitedURLList& visited_list) {
  if (!automation_) {
    delete this;
    return;
  }

  ListValue* list_value = new ListValue;
  for (size_t i = 0; i < visited_list.size(); ++i) {
    const history::MostVisitedURL& visited = visited_list[i];
    if (visited.url.spec().empty())
      break;  // This is the signal that there are no more real visited sites.
    DictionaryValue* dict = new DictionaryValue;
    dict->SetString("url", visited.url.spec());
    dict->SetString("title", visited.title);
    dict->SetBoolean("is_pinned", top_sites_->IsURLPinned(visited.url));
    list_value->Append(dict);
  }
  ntp_info_->Set("most_visited", list_value);
  AutomationJSONReply(automation_,
                      reply_message_.release()).SendSuccess(ntp_info_.get());
  delete this;
}

AppLaunchObserver::AppLaunchObserver(
    NavigationController* controller,
    AutomationProvider* automation,
    IPC::Message* reply_message,
    extension_misc::LaunchContainer launch_container)
    : controller_(controller),
      automation_(automation->AsWeakPtr()),
      reply_message_(reply_message),
      launch_container_(launch_container),
      new_window_id_(extension_misc::kUnknownWindowId) {
  if (launch_container_ == extension_misc::LAUNCH_TAB) {
    // Need to wait for the currently-active tab to reload.
    Source<NavigationController> source(controller_);
    registrar_.Add(this, NotificationType::LOAD_STOP, source);
  } else {
    // Need to wait for a new tab in a new window to load.
    registrar_.Add(this, NotificationType::LOAD_STOP,
                   NotificationService::AllSources());
    registrar_.Add(this, NotificationType::BROWSER_WINDOW_READY,
                   NotificationService::AllSources());
  }
}

AppLaunchObserver::~AppLaunchObserver() {}

void AppLaunchObserver::Observe(NotificationType type,
                                const NotificationSource& source,
                                const NotificationDetails& details) {
  if (type.value == NotificationType::LOAD_STOP) {
    if (launch_container_ == extension_misc::LAUNCH_TAB) {
      // The app has been launched in the new tab.
      if (automation_) {
        AutomationJSONReply(automation_,
                            reply_message_.release()).SendSuccess(NULL);
      }
      delete this;
      return;
    } else {
      // The app has launched only if the loaded tab is in the new window.
      int window_id = Source<NavigationController>(source)->window_id().id();
      if (window_id == new_window_id_) {
        if (automation_) {
          AutomationJSONReply(automation_,
                              reply_message_.release()).SendSuccess(NULL);
        }
        delete this;
        return;
      }
    }
  } else if (type.value == NotificationType::BROWSER_WINDOW_READY) {
    new_window_id_ = ExtensionTabUtil::GetWindowId(
        Source<Browser>(source).ptr());
  } else {
    NOTREACHED();
  }
}

AutocompleteEditFocusedObserver::AutocompleteEditFocusedObserver(
    AutomationProvider* automation,
    AutocompleteEditModel* autocomplete_edit,
    IPC::Message* reply_message)
    : automation_(automation->AsWeakPtr()),
      reply_message_(reply_message),
      autocomplete_edit_model_(autocomplete_edit) {
  Source<AutocompleteEditModel> source(autocomplete_edit);
  registrar_.Add(this, NotificationType::AUTOCOMPLETE_EDIT_FOCUSED, source);
}

AutocompleteEditFocusedObserver::~AutocompleteEditFocusedObserver() {}

void AutocompleteEditFocusedObserver::Observe(
    NotificationType type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  DCHECK(type == NotificationType::AUTOCOMPLETE_EDIT_FOCUSED);
  if (automation_) {
    AutomationMsg_WaitForAutocompleteEditFocus::WriteReplyParams(
        reply_message_.get(), true);
    automation_->Send(reply_message_.release());
  }
  delete this;
}

namespace {

// Returns whether the notification's host has a non-null process handle.
bool IsNotificationProcessReady(Balloon* balloon) {
  return balloon->view() &&
         balloon->view()->GetHost() &&
         balloon->view()->GetHost()->render_view_host() &&
         balloon->view()->GetHost()->render_view_host()->process()->GetHandle();
}

// Returns whether all active notifications have an associated process ID.
bool AreActiveNotificationProcessesReady() {
  NotificationUIManager* manager = g_browser_process->notification_ui_manager();
  const BalloonCollection::Balloons& balloons =
      manager->balloon_collection()->GetActiveBalloons();
  BalloonCollection::Balloons::const_iterator iter;
  for (iter = balloons.begin(); iter != balloons.end(); ++iter) {
    if (!IsNotificationProcessReady(*iter))
      return false;
  }
  return true;
}

}  // namespace

GetActiveNotificationsObserver::GetActiveNotificationsObserver(
    AutomationProvider* automation,
    IPC::Message* reply_message)
    : reply_(automation, reply_message) {
  if (AreActiveNotificationProcessesReady()) {
    SendMessage();
  } else {
    registrar_.Add(this, NotificationType::RENDERER_PROCESS_CREATED,
                   NotificationService::AllSources());
  }
}

GetActiveNotificationsObserver::~GetActiveNotificationsObserver() {}

void GetActiveNotificationsObserver::Observe(
    NotificationType type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  if (AreActiveNotificationProcessesReady())
    SendMessage();
}

void GetActiveNotificationsObserver::SendMessage() {
  NotificationUIManager* manager =
      g_browser_process->notification_ui_manager();
  const BalloonCollection::Balloons& balloons =
      manager->balloon_collection()->GetActiveBalloons();
  scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
  ListValue* list = new ListValue;
  return_value->Set("notifications", list);
  BalloonCollection::Balloons::const_iterator iter;
  for (iter = balloons.begin(); iter != balloons.end(); ++iter) {
    const Notification& notification = (*iter)->notification();
    DictionaryValue* balloon = new DictionaryValue;
    balloon->SetString("content_url", notification.content_url().spec());
    balloon->SetString("origin_url", notification.origin_url().spec());
    balloon->SetString("display_source", notification.display_source());
    BalloonView* view = (*iter)->view();
    balloon->SetInteger("pid", base::GetProcId(
        view->GetHost()->render_view_host()->process()->GetHandle()));
    list->Append(balloon);
  }
  reply_.SendSuccess(return_value.get());
  delete this;
}

OnNotificationBalloonCountObserver::OnNotificationBalloonCountObserver(
    AutomationProvider* provider,
    IPC::Message* reply_message,
    BalloonCollection* collection,
    int count)
    : reply_(provider, reply_message),
      collection_(collection),
      count_(count) {
  collection->set_on_collection_changed_callback(NewCallback(
      this, &OnNotificationBalloonCountObserver::OnBalloonCollectionChanged));
}

void OnNotificationBalloonCountObserver::OnBalloonCollectionChanged() {
  if (static_cast<int>(collection_->GetActiveBalloons().size()) == count_) {
    collection_->set_on_collection_changed_callback(NULL);
    reply_.SendSuccess(NULL);
    delete this;
  }
}

RendererProcessClosedObserver::RendererProcessClosedObserver(
    AutomationProvider* automation,
    IPC::Message* reply_message)
    : automation_(automation->AsWeakPtr()),
      reply_message_(reply_message) {
  registrar_.Add(this, NotificationType::RENDERER_PROCESS_CLOSED,
                 NotificationService::AllSources());
}

RendererProcessClosedObserver::~RendererProcessClosedObserver() {}

void RendererProcessClosedObserver::Observe(
    NotificationType type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  if (automation_) {
    AutomationJSONReply(automation_,
                        reply_message_.release()).SendSuccess(NULL);
  }
  delete this;
}

InputEventAckNotificationObserver::InputEventAckNotificationObserver(
    AutomationProvider* automation,
    IPC::Message* reply_message,
    int event_type)
    : automation_(automation->AsWeakPtr()),
      reply_message_(reply_message),
      event_type_(event_type) {
  registrar_.Add(
      this, NotificationType::RENDER_WIDGET_HOST_DID_RECEIVE_INPUT_EVENT_ACK,
      NotificationService::AllSources());
}

InputEventAckNotificationObserver::~InputEventAckNotificationObserver() {}

void InputEventAckNotificationObserver::Observe(
    NotificationType type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  Details<int> request_details(details);
  if (event_type_ == *request_details.ptr()) {
    if (automation_) {
      AutomationJSONReply(automation_,
                          reply_message_.release()).SendSuccess(NULL);
    }
    delete this;
  } else {
    LOG(WARNING) << "Ignoring unexpected event types.";
  }
}

AllTabsStoppedLoadingObserver::AllTabsStoppedLoadingObserver(
    AutomationProvider* automation,
    IPC::Message* reply_message)
    : automation_(automation->AsWeakPtr()),
      reply_message_(reply_message) {
  for (BrowserList::const_iterator iter = BrowserList::begin();
       iter != BrowserList::end();
       ++iter) {
    Browser* browser = *iter;
    for (int i = 0; i < browser->tab_count(); ++i) {
      TabContentsWrapper* contents_wrapper =
          browser->GetTabContentsWrapperAt(i);
      StartObserving(contents_wrapper->automation_tab_helper());
      if (contents_wrapper->automation_tab_helper()->has_pending_loads())
        pending_tabs_.insert(contents_wrapper->tab_contents());
    }
  }
  CheckIfNoMorePendingLoads();
}

AllTabsStoppedLoadingObserver::~AllTabsStoppedLoadingObserver() {
}

void AllTabsStoppedLoadingObserver::OnFirstPendingLoad(
    TabContents* tab_contents) {
  pending_tabs_.insert(tab_contents);
}

void AllTabsStoppedLoadingObserver::OnNoMorePendingLoads(
    TabContents* tab_contents) {
  if (!automation_) {
    delete this;
    return;
  }

  TabSet::iterator iter = pending_tabs_.find(tab_contents);
  if (iter == pending_tabs_.end()) {
    LOG(ERROR) << "Received OnNoMorePendingLoads for tab without "
               << "OnFirstPendingLoad.";
    return;
  }
  pending_tabs_.erase(iter);
  CheckIfNoMorePendingLoads();
}

void AllTabsStoppedLoadingObserver::CheckIfNoMorePendingLoads() {
  if (!automation_) {
    delete this;
    return;
  }

  if (pending_tabs_.empty()) {
    AutomationJSONReply(automation_,
                        reply_message_.release()).SendSuccess(NULL);
    delete this;
  }
}

NewTabObserver::NewTabObserver(AutomationProvider* automation,
                               IPC::Message* reply_message)
    : automation_(automation->AsWeakPtr()),
      reply_message_(reply_message) {
  // Use TAB_PARENTED to detect the new tab.
  registrar_.Add(this,
                 NotificationType::TAB_PARENTED,
                 NotificationService::AllSources());
}

void NewTabObserver::Observe(NotificationType type,
                             const NotificationSource& source,
                             const NotificationDetails& details) {
  DCHECK_EQ(NotificationType::TAB_PARENTED, type.value);
  NavigationController* controller = Source<NavigationController>(source).ptr();
  if (automation_) {
    // TODO(phajdan.jr): Clean up this hack. We write the correct return type
    // here, but don't send the message. NavigationNotificationObserver
    // will wait properly for the load to finish, and send the message,
    // but it will also append its own return value at the end of the reply.
    AutomationMsg_WindowExecuteCommand::WriteReplyParams(reply_message_.get(),
                                                         true);
    new NavigationNotificationObserver(controller, automation_,
                                       reply_message_.release(),
                                       1, false, false);
  }
  delete this;
}

NewTabObserver::~NewTabObserver() {
}

WaitForProcessLauncherThreadToGoIdleObserver::
WaitForProcessLauncherThreadToGoIdleObserver(
    AutomationProvider* automation, IPC::Message* reply_message)
    : automation_(automation->AsWeakPtr()),
      reply_message_(reply_message) {
  // Balanced in RunOnUIThread.
  AddRef();
  BrowserThread::PostTask(
      BrowserThread::PROCESS_LAUNCHER, FROM_HERE,
      NewRunnableMethod(
          this,
          &WaitForProcessLauncherThreadToGoIdleObserver::
              RunOnProcessLauncherThread));
}

WaitForProcessLauncherThreadToGoIdleObserver::
    ~WaitForProcessLauncherThreadToGoIdleObserver() {
}

void WaitForProcessLauncherThreadToGoIdleObserver::
RunOnProcessLauncherThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::PROCESS_LAUNCHER));
  BrowserThread::PostTask(
      BrowserThread::PROCESS_LAUNCHER, FROM_HERE,
      NewRunnableMethod(
          this,
          &WaitForProcessLauncherThreadToGoIdleObserver::
          RunOnProcessLauncherThread2));
}

void WaitForProcessLauncherThreadToGoIdleObserver::
RunOnProcessLauncherThread2() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::PROCESS_LAUNCHER));
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(
          this,
          &WaitForProcessLauncherThreadToGoIdleObserver::RunOnUIThread));
}

void WaitForProcessLauncherThreadToGoIdleObserver::RunOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (automation_)
    automation_->Send(reply_message_.release());
  Release();
}
