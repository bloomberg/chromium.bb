// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/automation_provider_observers.h"

#include <deque>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/file_util.h"
#include "base/json/json_writer.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/automation/automation_provider.h"
#include "chrome/browser/automation/automation_provider_json.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/history/top_sites.h"
#include "chrome/browser/infobars/confirm_infobar_delegate.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/metrics/metric_event_duration_details.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_host/chrome_render_message_filter.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/sessions/tab_restore_service.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_iterator.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/find_bar/find_notification_details.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/login/login_prompt.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/ntp/app_launcher_handler.h"
#include "chrome/browser/ui/webui/ntp/most_visited_handler.h"
#include "chrome/browser/ui/webui/ntp/new_tab_ui.h"
#include "chrome/browser/ui/webui/ntp/recently_closed_tabs_handler.h"
#include "chrome/common/automation_constants.h"
#include "chrome/common/automation_messages.h"
#include "chrome/common/content_settings_types.h"
#include "chrome/common/extensions/extension_constants.h"
#include "components/password_manager/core/browser/password_store_change.h"
#include "content/public/browser/dom_operation_notification_details.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/process_type.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/runtime_data.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest.h"
#include "extensions/common/view_type.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/rect.h"
#include "url/gurl.h"

#if !defined(OS_CHROMEOS)
#include "chrome/browser/notifications/balloon_notification_ui_manager.h"
#endif

using content::BrowserThread;
using content::DomOperationNotificationDetails;
using content::DownloadItem;
using content::DownloadManager;
using content::NavigationController;
using content::RenderViewHost;
using content::WebContents;

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
      crashed_tab_count_(0),
      outstanding_tab_count_(tab_count),
      init_time_(base::TimeTicks::Now()) {
  if (outstanding_tab_count_ > 0) {
    registrar_.Add(this, content::NOTIFICATION_LOAD_START,
                   content::NotificationService::AllSources());
    registrar_.Add(this, content::NOTIFICATION_LOAD_STOP,
                   content::NotificationService::AllSources());
    registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
                   content::NotificationService::AllSources());
  }
}

InitialLoadObserver::~InitialLoadObserver() {
}

void InitialLoadObserver::Observe(int type,
                                  const content::NotificationSource& source,
                                  const content::NotificationDetails& details) {
  if (type == content::NOTIFICATION_LOAD_START) {
    if (outstanding_tab_count_ > loading_tabs_.size())
      loading_tabs_.insert(TabTimeMap::value_type(
          source.map_key(),
          TabTime(base::TimeTicks::Now())));
  } else if (type == content::NOTIFICATION_LOAD_STOP) {
    if (outstanding_tab_count_ > finished_tabs_.size()) {
      TabTimeMap::iterator iter = loading_tabs_.find(source.map_key());
      if (iter != loading_tabs_.end()) {
        finished_tabs_.insert(source.map_key());
        iter->second.set_stop_time(base::TimeTicks::Now());
      }
    }
  } else if (type == content::NOTIFICATION_RENDERER_PROCESS_CLOSED) {
    base::TerminationStatus status =
        content::Details<content::RenderProcessHost::RendererClosedDetails>(
            details)->status;
    switch (status) {
      case base::TERMINATION_STATUS_NORMAL_TERMINATION:
        break;

      case base::TERMINATION_STATUS_ABNORMAL_TERMINATION:
      case base::TERMINATION_STATUS_PROCESS_WAS_KILLED:
      case base::TERMINATION_STATUS_PROCESS_CRASHED:
        crashed_tab_count_++;
        break;

      case base::TERMINATION_STATUS_STILL_RUNNING:
        LOG(ERROR) << "Got RENDERER_PROCESS_CLOSED notification, "
                   << "but the process is still running. We may miss further "
                   << "crash notification, resulting in hangs.";
        break;

      default:
        LOG(ERROR) << "Unhandled termination status " << status;
        NOTREACHED();
        break;
    }
  } else {
    NOTREACHED();
  }

  if (finished_tabs_.size() + crashed_tab_count_ >= outstanding_tab_count_)
    ConditionMet();
}

base::DictionaryValue* InitialLoadObserver::GetTimingInformation() const {
  base::ListValue* items = new base::ListValue;
  for (TabTimeMap::const_iterator it = loading_tabs_.begin();
       it != loading_tabs_.end();
       ++it) {
    base::DictionaryValue* item = new base::DictionaryValue;
    base::TimeDelta delta_start = it->second.start_time() - init_time_;

    item->SetDouble("load_start_ms", delta_start.InMillisecondsF());
    if (it->second.stop_time().is_null()) {
      item->Set("load_stop_ms", base::Value::CreateNullValue());
    } else {
      base::TimeDelta delta_stop = it->second.stop_time() - init_time_;
      item->SetDouble("load_stop_ms", delta_stop.InMillisecondsF());
    }
    items->Append(item);
  }
  base::DictionaryValue* return_value = new base::DictionaryValue;
  return_value->Set("tabs", items);
  return return_value;
}

void InitialLoadObserver::ConditionMet() {
  registrar_.RemoveAll();
  if (automation_.get())
    automation_->OnInitialTabLoadsComplete();
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
    registrar_.Add(this, content::NOTIFICATION_LOAD_STOP,
                   content::NotificationService::AllSources());
  }
}

NavigationControllerRestoredObserver::~NavigationControllerRestoredObserver() {
}

void NavigationControllerRestoredObserver::Observe(
    int type, const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (FinishedRestoring()) {
    registrar_.RemoveAll();
    SendDone();
  }
}

bool NavigationControllerRestoredObserver::FinishedRestoring() {
  return (!controller_->NeedsReload() && !controller_->GetPendingEntry() &&
          !controller_->GetWebContents()->IsLoading());
}

void NavigationControllerRestoredObserver::SendDone() {
  if (automation_.get()) {
    AutomationJSONReply(automation_.get(), reply_message_.release())
        .SendSuccess(NULL);
  }
  delete this;
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
  if (number_of_navigations == 0) {
    ConditionMet(AUTOMATION_MSG_NAVIGATION_SUCCESS);
    return;
  }
  DCHECK_LT(0, navigations_remaining_);
  content::Source<NavigationController> source(controller_);
  registrar_.Add(this, content::NOTIFICATION_NAV_ENTRY_COMMITTED, source);
  registrar_.Add(this, content::NOTIFICATION_LOAD_START, source);
  registrar_.Add(this, content::NOTIFICATION_LOAD_STOP, source);
  registrar_.Add(this, chrome::NOTIFICATION_AUTH_NEEDED, source);
  registrar_.Add(this, chrome::NOTIFICATION_AUTH_SUPPLIED, source);
  registrar_.Add(this, chrome::NOTIFICATION_AUTH_CANCELLED, source);
  registrar_.Add(this, chrome::NOTIFICATION_APP_MODAL_DIALOG_SHOWN,
                 content::NotificationService::AllSources());

  if (include_current_navigation && controller->GetWebContents()->IsLoading())
    navigation_started_ = true;
}

NavigationNotificationObserver::~NavigationNotificationObserver() {
}

void NavigationNotificationObserver::Observe(
    int type, const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (!automation_.get()) {
    delete this;
    return;
  }

  // We listen for 2 events to determine when the navigation started because:
  // - when this is used by the WaitForNavigation method, we might be invoked
  // afer the load has started (but not after the entry was committed, as
  // WaitForNavigation compares times of the last navigation).
  // - when this is used with a page requiring authentication, we will not get
  // a chrome::NAV_ENTRY_COMMITTED until after we authenticate, so
  // we need the chrome::LOAD_START.
  if (type == content::NOTIFICATION_NAV_ENTRY_COMMITTED ||
      type == content::NOTIFICATION_LOAD_START) {
    navigation_started_ = true;
  } else if (type == content::NOTIFICATION_LOAD_STOP) {
    if (navigation_started_) {
      navigation_started_ = false;
      if (--navigations_remaining_ == 0)
        ConditionMet(AUTOMATION_MSG_NAVIGATION_SUCCESS);
    }
  } else if (type == chrome::NOTIFICATION_AUTH_SUPPLIED ||
             type == chrome::NOTIFICATION_AUTH_CANCELLED) {
    // Treat this as if navigation started again, since load start/stop don't
    // occur while authentication is ongoing.
    navigation_started_ = true;
  } else if (type == chrome::NOTIFICATION_AUTH_NEEDED) {
    // Respond that authentication is needed.
    navigation_started_ = false;
    ConditionMet(AUTOMATION_MSG_NAVIGATION_AUTH_NEEDED);
  } else if (type == chrome::NOTIFICATION_APP_MODAL_DIALOG_SHOWN) {
    ConditionMet(AUTOMATION_MSG_NAVIGATION_BLOCKED_BY_MODAL_DIALOG);
  } else {
    NOTREACHED();
  }
}

void NavigationNotificationObserver::ConditionMet(
    AutomationMsg_NavigationResponseValues navigation_result) {
  if (automation_.get()) {
    if (use_json_interface_) {
      if (navigation_result == AUTOMATION_MSG_NAVIGATION_SUCCESS) {
        base::DictionaryValue dict;
        dict.SetInteger("result", navigation_result);
        AutomationJSONReply(automation_.get(), reply_message_.release())
            .SendSuccess(&dict);
      } else {
        AutomationJSONReply(automation_.get(), reply_message_.release())
            .SendError(base::StringPrintf(
                 "Navigation failed with error code=%d.", navigation_result));
      }
    } else {
      IPC::ParamTraits<int>::Write(
          reply_message_.get(), navigation_result);
      automation_->Send(reply_message_.release());
    }
  }

  delete this;
}

TabStripNotificationObserver::TabStripNotificationObserver(
    int notification, AutomationProvider* automation)
    : automation_(automation->AsWeakPtr()),
      notification_(notification) {
  registrar_.Add(this, notification_,
                 content::NotificationService::AllSources());
}

TabStripNotificationObserver::~TabStripNotificationObserver() {
}

void TabStripNotificationObserver::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(notification_, type);
  if (type == chrome::NOTIFICATION_TAB_PARENTED) {
    ObserveTab(&content::Source<content::WebContents>(source)->GetController());
  } else if (type == content::NOTIFICATION_WEB_CONTENTS_DESTROYED) {
    ObserveTab(&content::Source<content::WebContents>(source)->GetController());
  } else {
    ObserveTab(content::Source<NavigationController>(source).ptr());
  }
  delete this;
}

TabAppendedNotificationObserver::TabAppendedNotificationObserver(
    Browser* parent,
    AutomationProvider* automation,
    IPC::Message* reply_message,
    bool use_json_interface)
    : TabStripNotificationObserver(chrome::NOTIFICATION_TAB_PARENTED,
                                   automation),
      parent_(parent),
      reply_message_(reply_message),
      use_json_interface_(use_json_interface) {
}

TabAppendedNotificationObserver::~TabAppendedNotificationObserver() {}

void TabAppendedNotificationObserver::ObserveTab(
    NavigationController* controller) {
  if (!automation_.get() || !reply_message_.get())
    return;

  if (automation_->GetIndexForNavigationController(controller, parent_) ==
      TabStripModel::kNoTab) {
    // This tab notification doesn't belong to the parent_.
    return;
  }

  new NavigationNotificationObserver(controller,
                                     automation_.get(),
                                     reply_message_.release(),
                                     1,
                                     false,
                                     use_json_interface_);
}

IPC::Message* TabAppendedNotificationObserver::ReleaseReply() {
  return reply_message_.release();
}

TabClosedNotificationObserver::TabClosedNotificationObserver(
    AutomationProvider* automation,
    bool wait_until_closed,
    IPC::Message* reply_message,
    bool use_json_interface)
    : TabStripNotificationObserver((wait_until_closed ?
          static_cast<int>(content::NOTIFICATION_WEB_CONTENTS_DESTROYED) :
          static_cast<int>(chrome::NOTIFICATION_TAB_CLOSING)), automation),
      reply_message_(reply_message),
      use_json_interface_(use_json_interface),
      for_browser_command_(false) {
}

TabClosedNotificationObserver::~TabClosedNotificationObserver() {}

void TabClosedNotificationObserver::ObserveTab(
    NavigationController* controller) {
  if (!automation_.get())
    return;

  if (use_json_interface_) {
    AutomationJSONReply(automation_.get(), reply_message_.release())
        .SendSuccess(NULL);
  } else {
    if (for_browser_command_) {
      AutomationMsg_WindowExecuteCommand::WriteReplyParams(reply_message_.get(),
                                                           true);
    } else {
      AutomationMsg_CloseTab::WriteReplyParams(reply_message_.get(), true);
    }
    automation_->Send(reply_message_.release());
  }
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
      tab_strip_model_(browser->tab_strip_model()),
      target_tab_count_(target_tab_count) {
  tab_strip_model_->AddObserver(this);
  CheckTabCount();
}

TabCountChangeObserver::~TabCountChangeObserver() {
  tab_strip_model_->RemoveObserver(this);
}

void TabCountChangeObserver::TabInsertedAt(WebContents* contents,
                                           int index,
                                           bool foreground) {
  CheckTabCount();
}

void TabCountChangeObserver::TabDetachedAt(WebContents* contents,
                                           int index) {
  CheckTabCount();
}

void TabCountChangeObserver::TabStripModelDeleted() {
  if (automation_.get()) {
    AutomationMsg_WaitForTabCountToBecome::WriteReplyParams(
        reply_message_.get(), false);
    automation_->Send(reply_message_.release());
  }

  delete this;
}

void TabCountChangeObserver::CheckTabCount() {
  if (tab_strip_model_->count() != target_tab_count_)
    return;

  if (automation_.get()) {
    AutomationMsg_WaitForTabCountToBecome::WriteReplyParams(
        reply_message_.get(), true);
    automation_->Send(reply_message_.release());
  }

  delete this;
}

bool DidExtensionViewsStopLoading(extensions::ProcessManager* manager) {
  extensions::ProcessManager::ViewSet all_views = manager->GetAllViews();
  for (extensions::ProcessManager::ViewSet::const_iterator iter =
           all_views.begin();
       iter != all_views.end(); ++iter) {
    if ((*iter)->IsLoading())
      return false;
  }
  return true;
}

ExtensionUninstallObserver::ExtensionUninstallObserver(
    AutomationProvider* automation,
    IPC::Message* reply_message,
    const std::string& id)
    : automation_(automation->AsWeakPtr()),
      reply_message_(reply_message),
      id_(id) {
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNINSTALLED,
                 content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNINSTALL_NOT_ALLOWED,
                 content::NotificationService::AllSources());
}

ExtensionUninstallObserver::~ExtensionUninstallObserver() {
}

void ExtensionUninstallObserver::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (!automation_.get()) {
    delete this;
    return;
  }

  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_UNINSTALLED: {
      if (id_ == content::Details<extensions::Extension>(details).ptr()->id()) {
        scoped_ptr<base::DictionaryValue> return_value(
            new base::DictionaryValue);
        return_value->SetBoolean("success", true);
        AutomationJSONReply(automation_.get(), reply_message_.release())
            .SendSuccess(return_value.get());
        delete this;
        return;
      }
      break;
    }

    case chrome::NOTIFICATION_EXTENSION_UNINSTALL_NOT_ALLOWED: {
      const extensions::Extension* extension =
          content::Details<extensions::Extension>(details).ptr();
      if (id_ == extension->id()) {
        scoped_ptr<base::DictionaryValue> return_value(
            new base::DictionaryValue);
        return_value->SetBoolean("success", false);
        AutomationJSONReply(automation_.get(), reply_message_.release())
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
    extensions::ExtensionSystem* extension_system,
    AutomationProvider* automation,
    IPC::Message* reply_message)
    : extension_system_(extension_system),
      automation_(automation->AsWeakPtr()),
      reply_message_(reply_message),
      extension_(NULL) {
  Init();
}

ExtensionReadyNotificationObserver::~ExtensionReadyNotificationObserver() {
}

void ExtensionReadyNotificationObserver::Init() {
  registrar_.Add(this, content::NOTIFICATION_LOAD_STOP,
                 content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_LOADED,
                 content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_LOAD_ERROR,
                 content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_INSTALL_ERROR,
                 content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UPDATE_DISABLED,
                 content::NotificationService::AllSources());
}

void ExtensionReadyNotificationObserver::Observe(
    int type, const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (!automation_.get()) {
    delete this;
    return;
  }

  switch (type) {
    case content::NOTIFICATION_LOAD_STOP:
      // Only continue on with this method if our extension has been loaded
      // and all the extension views have stopped loading.
      if (!extension_ ||
          !DidExtensionViewsStopLoading(extension_system_->process_manager()))
        return;
      break;
    case chrome::NOTIFICATION_EXTENSION_LOADED: {
      const extensions::Extension* loaded_extension =
          content::Details<const extensions::Extension>(details).ptr();
      // Only track an internal or unpacked extension load.
      extensions::Manifest::Location location = loaded_extension->location();
      if (location != extensions::Manifest::INTERNAL &&
          !extensions::Manifest::IsUnpackedLocation(location))
        return;
      extension_ = loaded_extension;
      if (!DidExtensionViewsStopLoading(extension_system_->process_manager()))
        return;
      // For some reason, the background extension view is not yet
      // created at this point so just checking whether all extension views
      // are loaded is not sufficient. If background page is not ready,
      // we wait for NOTIFICATION_LOAD_STOP.
      if (!extension_system_->runtime_data()->IsBackgroundPageReady(extension_))
        return;
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_INSTALL_ERROR:
    case chrome::NOTIFICATION_EXTENSION_LOAD_ERROR:
    case chrome::NOTIFICATION_EXTENSION_UPDATE_DISABLED:
      break;
    default:
      NOTREACHED();
      break;
  }

  AutomationJSONReply reply(automation_.get(), reply_message_.release());
  if (extension_) {
    base::DictionaryValue dict;
    dict.SetString("id", extension_->id());
    reply.SendSuccess(&dict);
  } else {
    reply.SendError("Extension could not be installed");
  }
  delete this;
}

ExtensionUnloadNotificationObserver::ExtensionUnloadNotificationObserver()
    : did_receive_unload_notification_(false) {
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED_DEPRECATED,
                 content::NotificationService::AllSources());
}

ExtensionUnloadNotificationObserver::~ExtensionUnloadNotificationObserver() {
}

void ExtensionUnloadNotificationObserver::Observe(
    int type, const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_EXTENSION_UNLOADED_DEPRECATED) {
    did_receive_unload_notification_ = true;
  } else {
    NOTREACHED();
  }
}

ExtensionsUpdatedObserver::ExtensionsUpdatedObserver(
    extensions::ProcessManager* manager, AutomationProvider* automation,
    IPC::Message* reply_message)
    : manager_(manager), automation_(automation->AsWeakPtr()),
      reply_message_(reply_message), updater_finished_(false) {
  registrar_.Add(this, content::NOTIFICATION_LOAD_STOP,
                 content::NotificationService::AllSources());
}

ExtensionsUpdatedObserver::~ExtensionsUpdatedObserver() {
}

void ExtensionsUpdatedObserver::Observe(
    int type, const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (!automation_.get()) {
    delete this;
    return;
  }

  DCHECK(type == content::NOTIFICATION_LOAD_STOP);
  MaybeReply();
}

void ExtensionsUpdatedObserver::UpdateCheckFinished() {
  if (!automation_.get()) {
    delete this;
    return;
  }

  // Extension updater has completed updating all extensions.
  updater_finished_ = true;
  MaybeReply();
}

void ExtensionsUpdatedObserver::MaybeReply() {
  // Send the reply if (1) the extension updater has finished updating all
  // extensions; and (2) all extension views have stopped loading.
  if (updater_finished_ && DidExtensionViewsStopLoading(manager_)) {
    AutomationJSONReply reply(automation_.get(), reply_message_.release());
    reply.SendSuccess(NULL);
    delete this;
  }
}

BrowserOpenedNotificationObserver::BrowserOpenedNotificationObserver(
    AutomationProvider* automation,
    IPC::Message* reply_message,
    bool use_json_interface)
    : automation_(automation->AsWeakPtr()),
      reply_message_(reply_message),
      new_window_id_(extension_misc::kUnknownWindowId),
      use_json_interface_(use_json_interface),
      for_browser_command_(false) {
  registrar_.Add(this, chrome::NOTIFICATION_BROWSER_OPENED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, content::NOTIFICATION_LOAD_STOP,
                 content::NotificationService::AllBrowserContextsAndSources());
}

BrowserOpenedNotificationObserver::~BrowserOpenedNotificationObserver() {
}

void BrowserOpenedNotificationObserver::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (!automation_.get()) {
    delete this;
    return;
  }

  if (type == chrome::NOTIFICATION_BROWSER_OPENED) {
    // Store the new browser ID and continue waiting for a new tab within it
    // to stop loading.
    new_window_id_ = extensions::ExtensionTabUtil::GetWindowId(
        content::Source<Browser>(source).ptr());
  } else {
    DCHECK_EQ(content::NOTIFICATION_LOAD_STOP, type);
    // Only send the result if the loaded tab is in the new window.
    NavigationController* controller =
        content::Source<NavigationController>(source).ptr();
    SessionTabHelper* session_tab_helper =
        SessionTabHelper::FromWebContents(controller->GetWebContents());
    int window_id = session_tab_helper ? session_tab_helper->window_id().id()
                                       : -1;
    if (window_id == new_window_id_) {
      if (use_json_interface_) {
        AutomationJSONReply(automation_.get(), reply_message_.release())
            .SendSuccess(NULL);
      } else {
        if (for_browser_command_) {
          AutomationMsg_WindowExecuteCommand::WriteReplyParams(
              reply_message_.get(), true);
        }
        automation_->Send(reply_message_.release());
      }
      delete this;
      return;
    }
  }
}

void BrowserOpenedNotificationObserver::set_for_browser_command(
    bool for_browser_command) {
  for_browser_command_ = for_browser_command;
}

BrowserClosedNotificationObserver::BrowserClosedNotificationObserver(
    Browser* browser,
    AutomationProvider* automation,
    IPC::Message* reply_message,
    bool use_json_interface)
    : automation_(automation->AsWeakPtr()),
      reply_message_(reply_message),
      use_json_interface_(use_json_interface),
      for_browser_command_(false) {
  registrar_.Add(this, chrome::NOTIFICATION_BROWSER_CLOSED,
                 content::Source<Browser>(browser));
}

BrowserClosedNotificationObserver::~BrowserClosedNotificationObserver() {}

void BrowserClosedNotificationObserver::Observe(
    int type, const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_BROWSER_CLOSED, type);

  if (!automation_.get()) {
    delete this;
    return;
  }

  // The automation layer doesn't support non-native desktops.
  int browser_count = static_cast<int>(BrowserList::GetInstance(
                          chrome::HOST_DESKTOP_TYPE_NATIVE)->size());
  // We get the notification before the browser is removed from the BrowserList.
  bool app_closing = browser_count == 1;

  if (use_json_interface_) {
    AutomationJSONReply(automation_.get(), reply_message_.release())
        .SendSuccess(NULL);
  } else {
    if (for_browser_command_) {
      AutomationMsg_WindowExecuteCommand::WriteReplyParams(reply_message_.get(),
                                                           true);
    } else {
      AutomationMsg_CloseBrowser::WriteReplyParams(reply_message_.get(), true,
                                                   app_closing);
    }
    automation_->Send(reply_message_.release());
  }
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
  registrar_.Add(this, chrome::NOTIFICATION_BROWSER_OPENED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, chrome::NOTIFICATION_BROWSER_CLOSED,
                 content::NotificationService::AllBrowserContextsAndSources());
}

BrowserCountChangeNotificationObserver::
    ~BrowserCountChangeNotificationObserver() {}

void BrowserCountChangeNotificationObserver::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(type == chrome::NOTIFICATION_BROWSER_OPENED ||
         type == chrome::NOTIFICATION_BROWSER_CLOSED);

  // The automation layer doesn't support non-native desktops.
  int current_count = static_cast<int>(BrowserList::GetInstance(
                          chrome::HOST_DESKTOP_TYPE_NATIVE)->size());
  if (type == chrome::NOTIFICATION_BROWSER_CLOSED) {
    // At the time of the notification the browser being closed is not removed
    // from the list. The real count is one less than the reported count.
    DCHECK_LT(0, current_count);
    current_count--;
  }

  if (!automation_.get()) {
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

namespace {

// Define mapping from command to notification
struct CommandNotification {
  int command;
  int notification_type;
};

const struct CommandNotification command_notifications[] = {
  {IDC_DUPLICATE_TAB, chrome::NOTIFICATION_TAB_PARENTED},

  // Returns as soon as the restored tab is created. To further wait until
  // the content page is loaded, use WaitForTabToBeRestored.
  {IDC_RESTORE_TAB, chrome::NOTIFICATION_TAB_PARENTED},

  // For the following commands, we need to wait for a new tab to be created,
  // load to finish, and title to change.
  {IDC_MANAGE_EXTENSIONS, content::NOTIFICATION_WEB_CONTENTS_TITLE_UPDATED},
  {IDC_OPTIONS, content::NOTIFICATION_WEB_CONTENTS_TITLE_UPDATED},
  {IDC_PRINT, content::NOTIFICATION_WEB_CONTENTS_TITLE_UPDATED},
  {IDC_SHOW_DOWNLOADS, content::NOTIFICATION_WEB_CONTENTS_TITLE_UPDATED},
  {IDC_SHOW_HISTORY, content::NOTIFICATION_WEB_CONTENTS_TITLE_UPDATED},
};

}  // namespace

ExecuteBrowserCommandObserver::~ExecuteBrowserCommandObserver() {
}

// static
bool ExecuteBrowserCommandObserver::CreateAndRegisterObserver(
    AutomationProvider* automation,
    Browser* browser,
    int command,
    IPC::Message* reply_message,
    bool use_json_interface) {
  bool result = true;
  switch (command) {
    case IDC_NEW_TAB: {
      new NewTabObserver(automation, reply_message, use_json_interface);
      break;
    }
    case IDC_NEW_WINDOW:
    case IDC_NEW_INCOGNITO_WINDOW: {
      BrowserOpenedNotificationObserver* observer =
          new BrowserOpenedNotificationObserver(automation, reply_message,
                                                use_json_interface);
      observer->set_for_browser_command(true);
      break;
    }
    case IDC_CLOSE_WINDOW: {
      BrowserClosedNotificationObserver* observer =
          new BrowserClosedNotificationObserver(browser, automation,
                                                reply_message,
                                                use_json_interface);
      observer->set_for_browser_command(true);
      break;
    }
    case IDC_CLOSE_TAB: {
      TabClosedNotificationObserver* observer =
          new TabClosedNotificationObserver(automation, true, reply_message,
                                            use_json_interface);
      observer->set_for_browser_command(true);
      break;
    }
    case IDC_BACK:
    case IDC_FORWARD:
    case IDC_RELOAD: {
      new NavigationNotificationObserver(
          &browser->tab_strip_model()->GetActiveWebContents()->GetController(),
          automation, reply_message, 1, false, use_json_interface);
      break;
    }
    default: {
      ExecuteBrowserCommandObserver* observer =
          new ExecuteBrowserCommandObserver(automation, reply_message,
                                            use_json_interface);
      if (!observer->Register(command)) {
        observer->ReleaseReply();
        delete observer;
        result = false;
      }
      break;
    }
  }
  return result;
}

void ExecuteBrowserCommandObserver::Observe(
    int type, const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == notification_type_) {
    if (automation_.get()) {
      if (use_json_interface_) {
        AutomationJSONReply(automation_.get(), reply_message_.release())
            .SendSuccess(NULL);
      } else {
        AutomationMsg_WindowExecuteCommand::WriteReplyParams(
            reply_message_.get(), true);
        automation_->Send(reply_message_.release());
      }
    }
    delete this;
  } else {
    NOTREACHED();
  }
}

ExecuteBrowserCommandObserver::ExecuteBrowserCommandObserver(
    AutomationProvider* automation,
    IPC::Message* reply_message,
    bool use_json_interface)
    : automation_(automation->AsWeakPtr()),
      notification_type_(content::NOTIFICATION_ALL),
      reply_message_(reply_message),
      use_json_interface_(use_json_interface) {
}

bool ExecuteBrowserCommandObserver::Register(int command) {
  if (!Getint(command, &notification_type_))
    return false;
  registrar_.Add(this, notification_type_,
                 content::NotificationService::AllSources());
  return true;
}

bool ExecuteBrowserCommandObserver::Getint(
    int command, int* type) {
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

IPC::Message* ExecuteBrowserCommandObserver::ReleaseReply() {
  return reply_message_.release();
}

FindInPageNotificationObserver::FindInPageNotificationObserver(
    AutomationProvider* automation, WebContents* parent_tab,
    bool reply_with_json, IPC::Message* reply_message)
    : automation_(automation->AsWeakPtr()),
      active_match_ordinal_(-1),
      reply_with_json_(reply_with_json),
      reply_message_(reply_message) {
  registrar_.Add(this, chrome::NOTIFICATION_FIND_RESULT_AVAILABLE,
                 content::Source<WebContents>(parent_tab));
}

FindInPageNotificationObserver::~FindInPageNotificationObserver() {
}

void FindInPageNotificationObserver::Observe(
    int type, const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  content::Details<FindNotificationDetails> find_details(details);
  if (!(find_details->final_update() && reply_message_ != NULL)) {
    DVLOG(1) << "Ignoring, since we only care about the final message";
    return;
  }

  if (!automation_.get()) {
    delete this;
    return;
  }

  // We get multiple responses and one of those will contain the ordinal.
  // This message comes to us before the final update is sent.
  if (find_details->request_id() == kFindInPageRequestId) {
    if (reply_with_json_) {
      scoped_ptr<base::DictionaryValue> return_value(new base::DictionaryValue);
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
      AutomationJSONReply(automation_.get(), reply_message_.release())
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

DomOperationObserver::DomOperationObserver(int automation_id)
    : automation_id_(automation_id) {
  registrar_.Add(this, content::NOTIFICATION_DOM_OPERATION_RESPONSE,
                 content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_APP_MODAL_DIALOG_SHOWN,
                 content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_WEB_CONTENT_SETTINGS_CHANGED,
                 content::NotificationService::AllSources());
}

DomOperationObserver::~DomOperationObserver() {}

void DomOperationObserver::Observe(
    int type, const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == content::NOTIFICATION_DOM_OPERATION_RESPONSE) {
    content::Details<DomOperationNotificationDetails> dom_op_details(details);
    if (dom_op_details->automation_id == automation_id_)
      OnDomOperationCompleted(dom_op_details->json);
  } else if (type == chrome::NOTIFICATION_APP_MODAL_DIALOG_SHOWN) {
    OnJavascriptBlocked();
  } else {
    DCHECK_EQ(chrome::NOTIFICATION_WEB_CONTENT_SETTINGS_CHANGED, type);
    WebContents* web_contents = content::Source<WebContents>(source).ptr();
    if (web_contents) {
      TabSpecificContentSettings* tab_content_settings =
          TabSpecificContentSettings::FromWebContents(web_contents);
      if (tab_content_settings &&
          tab_content_settings->IsContentBlocked(
              CONTENT_SETTINGS_TYPE_JAVASCRIPT))
        OnJavascriptBlocked();
    }
  }
}

DomOperationMessageSender::DomOperationMessageSender(
    AutomationProvider* automation,
    IPC::Message* reply_message,
    bool use_json_interface)
    : DomOperationObserver(0),
      automation_(automation->AsWeakPtr()),
      reply_message_(reply_message),
      use_json_interface_(use_json_interface) {
}

DomOperationMessageSender::~DomOperationMessageSender() {}

void DomOperationMessageSender::OnDomOperationCompleted(
    const std::string& json) {
  if (automation_.get()) {
    if (use_json_interface_) {
      base::DictionaryValue dict;
      dict.SetString("result", json);
      AutomationJSONReply(automation_.get(), reply_message_.release())
          .SendSuccess(&dict);
    } else {
      AutomationMsg_DomOperation::WriteReplyParams(reply_message_.get(), json);
      automation_->Send(reply_message_.release());
    }
  }
  delete this;
}

void DomOperationMessageSender::OnJavascriptBlocked() {
  if (automation_.get() && use_json_interface_) {
    AutomationJSONReply(automation_.get(), reply_message_.release())
        .SendError("Javascript execution was blocked");
    delete this;
  }
}

MetricEventDurationObserver::MetricEventDurationObserver() {
  registrar_.Add(this, chrome::NOTIFICATION_METRIC_EVENT_DURATION,
                 content::NotificationService::AllSources());
}

MetricEventDurationObserver::~MetricEventDurationObserver() {}

int MetricEventDurationObserver::GetEventDurationMs(
    const std::string& event_name) {
  EventDurationMap::const_iterator it = durations_.find(event_name);
  if (it == durations_.end())
    return -1;
  return it->second;
}

void MetricEventDurationObserver::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type != chrome::NOTIFICATION_METRIC_EVENT_DURATION) {
    NOTREACHED();
    return;
  }
  MetricEventDurationDetails* metric_event_duration =
      content::Details<MetricEventDurationDetails>(details).ptr();
  durations_[metric_event_duration->event_name] =
      metric_event_duration->duration_ms;
}

InfoBarCountObserver::InfoBarCountObserver(AutomationProvider* automation,
                                           IPC::Message* reply_message,
                                           WebContents* web_contents,
                                           size_t target_count)
    : automation_(automation->AsWeakPtr()),
      reply_message_(reply_message),
      web_contents_(web_contents),
      target_count_(target_count) {
  content::Source<InfoBarService> source(
      InfoBarService::FromWebContents(web_contents));
  registrar_.Add(this, chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_ADDED,
                 source);
  registrar_.Add(this, chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_REMOVED,
                 source);
  CheckCount();
}

InfoBarCountObserver::~InfoBarCountObserver() {}

void InfoBarCountObserver::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(type == chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_ADDED ||
         type == chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_REMOVED);
  CheckCount();
}

void InfoBarCountObserver::CheckCount() {
  if (InfoBarService::FromWebContents(web_contents_)->infobar_count() !=
      target_count_)
    return;

  if (automation_.get()) {
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
    BookmarkModel* model,
    bool use_json_interface)
    : automation_provider_(provider->AsWeakPtr()),
      reply_message_(reply_message),
      model_(model),
      use_json_interface_(use_json_interface) {
  model_->AddObserver(this);
}

AutomationProviderBookmarkModelObserver::
    ~AutomationProviderBookmarkModelObserver() {
  model_->RemoveObserver(this);
}

void AutomationProviderBookmarkModelObserver::BookmarkModelLoaded(
    BookmarkModel* model,
    bool ids_reassigned) {
  ReplyAndDelete(true);
}

void AutomationProviderBookmarkModelObserver::BookmarkModelBeingDeleted(
    BookmarkModel* model) {
  ReplyAndDelete(false);
}

IPC::Message* AutomationProviderBookmarkModelObserver::ReleaseReply() {
  return reply_message_.release();
}

void AutomationProviderBookmarkModelObserver::ReplyAndDelete(bool success) {
  if (automation_provider_.get()) {
    if (use_json_interface_) {
      AutomationJSONReply(automation_provider_.get(), reply_message_.release())
          .SendSuccess(NULL);
    } else {
      AutomationMsg_WaitForBookmarkModelToLoad::WriteReplyParams(
          reply_message_.get(), success);
      automation_provider_->Send(reply_message_.release());
    }
  }
  delete this;
}

AutomationProviderDownloadUpdatedObserver::
AutomationProviderDownloadUpdatedObserver(
    AutomationProvider* provider,
    IPC::Message* reply_message,
    bool wait_for_open,
    bool incognito)
    : provider_(provider->AsWeakPtr()),
      reply_message_(reply_message),
      wait_for_open_(wait_for_open),
      incognito_(incognito) {
}

AutomationProviderDownloadUpdatedObserver::
    ~AutomationProviderDownloadUpdatedObserver() {}

void AutomationProviderDownloadUpdatedObserver::OnDownloadUpdated(
    DownloadItem* download) {
  // If this observer is watching for open, only send the reply if the download
  // has been auto-opened.
  if (wait_for_open_ && !download->GetAutoOpened())
    return;

  download->RemoveObserver(this);

  if (provider_.get()) {
    scoped_ptr<base::DictionaryValue> return_value(
        provider_->GetDictionaryFromDownloadItem(download, incognito_));
    AutomationJSONReply(provider_.get(), reply_message_.release())
        .SendSuccess(return_value.get());
  }
  delete this;
}

void AutomationProviderDownloadUpdatedObserver::OnDownloadOpened(
    DownloadItem* download) {
  download->RemoveObserver(this);

  if (provider_.get()) {
    scoped_ptr<base::DictionaryValue> return_value(
        provider_->GetDictionaryFromDownloadItem(download, incognito_));
    AutomationJSONReply(provider_.get(), reply_message_.release())
        .SendSuccess(return_value.get());
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
      notifier_(download_manager, this) {
}

AutomationProviderDownloadModelChangedObserver::
    ~AutomationProviderDownloadModelChangedObserver() {}

void AutomationProviderDownloadModelChangedObserver::ModelChanged() {
  if (provider_.get())
    AutomationJSONReply(provider_.get(), reply_message_.release())
        .SendSuccess(NULL);
  delete this;
}

void AutomationProviderDownloadModelChangedObserver::OnDownloadCreated(
    DownloadManager* manager, DownloadItem* item) {
  ModelChanged();
}

void AutomationProviderDownloadModelChangedObserver::OnDownloadRemoved(
    DownloadManager* manager, DownloadItem* item) {
  ModelChanged();
}

AllDownloadsCompleteObserver::AllDownloadsCompleteObserver(
    AutomationProvider* provider,
    IPC::Message* reply_message,
    DownloadManager* download_manager,
    base::ListValue* pre_download_ids)
    : provider_(provider->AsWeakPtr()),
      reply_message_(reply_message),
      download_manager_(download_manager) {
  for (base::ListValue::iterator it = pre_download_ids->begin();
       it != pre_download_ids->end(); ++it) {
    int val = 0;
    if ((*it)->GetAsInteger(&val)) {
      pre_download_ids_.insert(val);
    } else {
      AutomationJSONReply(provider_.get(), reply_message_.release())
          .SendError("Cannot convert ID of prior download to integer.");
      delete this;
      return;
    }
  }
  download_manager_->AddObserver(this);
  DownloadManager::DownloadVector all_items;
  download_manager->GetAllDownloads(&all_items);
  for (DownloadManager::DownloadVector::const_iterator
       it = all_items.begin(); it != all_items.end(); ++it) {
    OnDownloadCreated(download_manager_, *it);
  }
  ReplyIfNecessary();
}

AllDownloadsCompleteObserver::~AllDownloadsCompleteObserver() {
  if (download_manager_) {
    download_manager_->RemoveObserver(this);
    download_manager_ = NULL;
  }
  for (std::set<DownloadItem*>::const_iterator it = pending_downloads_.begin();
       it != pending_downloads_.end(); ++it) {
    (*it)->RemoveObserver(this);
  }
  pending_downloads_.clear();
}

void AllDownloadsCompleteObserver::ManagerGoingDown(DownloadManager* manager) {
  DCHECK_EQ(manager, download_manager_);
  download_manager_->RemoveObserver(this);
  download_manager_ = NULL;
}

void AllDownloadsCompleteObserver::OnDownloadCreated(
    DownloadManager* manager, DownloadItem* item) {
  // This method is also called in the c-tor for previously existing items.
  if (pre_download_ids_.find(item->GetId()) == pre_download_ids_.end() &&
      item->GetState() == DownloadItem::IN_PROGRESS) {
    item->AddObserver(this);
    pending_downloads_.insert(item);
  }
}

void AllDownloadsCompleteObserver::OnDownloadUpdated(DownloadItem* download) {
  // If the current download's status has changed to a final state (not state
  // "in progress"), remove it from the pending list.
  if (download->GetState() != DownloadItem::IN_PROGRESS) {
    download->RemoveObserver(this);
    pending_downloads_.erase(download);
    ReplyIfNecessary();
  }
}

void AllDownloadsCompleteObserver::ReplyIfNecessary() {
  if (!pending_downloads_.empty())
    return;

  download_manager_->RemoveObserver(this);
  if (provider_.get())
    AutomationJSONReply(provider_.get(), reply_message_.release())
        .SendSuccess(NULL);
  delete this;
}

AutomationProviderSearchEngineObserver::AutomationProviderSearchEngineObserver(
    AutomationProvider* provider,
    Profile* profile,
    IPC::Message* reply_message)
    : provider_(provider->AsWeakPtr()),
      profile_(profile),
      reply_message_(reply_message) {
}

AutomationProviderSearchEngineObserver::
    ~AutomationProviderSearchEngineObserver() {}

void AutomationProviderSearchEngineObserver::OnTemplateURLServiceChanged() {
  if (provider_.get()) {
    TemplateURLService* url_service =
        TemplateURLServiceFactory::GetForProfile(profile_);
    url_service->RemoveObserver(this);
    AutomationJSONReply(provider_.get(), reply_message_.release())
        .SendSuccess(NULL);
  }
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
  if (!provider_.get()) {
    delete this;
    return;
  }

  scoped_ptr<base::DictionaryValue> return_value(new base::DictionaryValue);

  base::ListValue* history_list = new base::ListValue;
  for (size_t i = 0; i < results->size(); ++i) {
    base::DictionaryValue* page_value = new base::DictionaryValue;
    history::URLResult const &page = (*results)[i];
    page_value->SetString("title", page.title());
    page_value->SetString("url", page.url().spec());
    page_value->SetDouble("time",
                          static_cast<double>(page.visit_time().ToDoubleT()));
    page_value->SetString("snippet", page.snippet().text());
    page_value->SetBoolean(
        "starred",
        BookmarkModelFactory::GetForProfile(
            provider_->profile())->IsBookmarked(page.url()));
    history_list->Append(page_value);
  }

  return_value->Set("history", history_list);
  // Return history info.
  AutomationJSONReply reply(provider_.get(), reply_message_.release());
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
  if (provider_.get())
    AutomationJSONReply(provider_.get(), reply_message_.release())
        .SendSuccess(NULL);
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

void AutomationProviderGetPasswordsObserver::OnGetPasswordStoreResults(
    const std::vector<autofill::PasswordForm*>& results) {
  if (!provider_.get()) {
    delete this;
    return;
  }

  scoped_ptr<base::DictionaryValue> return_value(new base::DictionaryValue);

  base::ListValue* passwords = new base::ListValue;
  for (std::vector<autofill::PasswordForm*>::const_iterator it =
           results.begin(); it != results.end(); ++it) {
    base::DictionaryValue* password_val = new base::DictionaryValue;
    autofill::PasswordForm* password_form = *it;
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
    password_val->SetString("submit_element", password_form->submit_element);
    password_val->SetString("action_target", password_form->action.spec());
    password_val->SetBoolean("blacklist", password_form->blacklisted_by_user);
    passwords->Append(password_val);
  }

  return_value->Set("passwords", passwords);
  AutomationJSONReply(provider_.get(), reply_message_.release())
      .SendSuccess(return_value.get());
  delete this;
}

OmniboxAcceptNotificationObserver::OmniboxAcceptNotificationObserver(
    NavigationController* controller,
    AutomationProvider* automation,
    IPC::Message* reply_message)
    : automation_(automation->AsWeakPtr()),
      reply_message_(reply_message),
      controller_(controller) {
  content::Source<NavigationController> source(controller_);
  registrar_.Add(this, content::NOTIFICATION_LOAD_STOP, source);
  // Pages requiring auth don't send LOAD_STOP.
  registrar_.Add(this, chrome::NOTIFICATION_AUTH_NEEDED, source);
}

OmniboxAcceptNotificationObserver::~OmniboxAcceptNotificationObserver() {
}

void OmniboxAcceptNotificationObserver::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == content::NOTIFICATION_LOAD_STOP ||
      type == chrome::NOTIFICATION_AUTH_NEEDED) {
    if (automation_.get()) {
      AutomationJSONReply(automation_.get(), reply_message_.release())
          .SendSuccess(NULL);
    }
    delete this;
  } else {
    NOTREACHED();
  }
}

SavePackageNotificationObserver::SavePackageNotificationObserver(
    content::DownloadManager* download_manager,
    AutomationProvider* automation,
    IPC::Message* reply_message)
    : download_manager_(download_manager),
      automation_(automation->AsWeakPtr()),
      reply_message_(reply_message) {
  download_manager_->AddObserver(this);
}

SavePackageNotificationObserver::~SavePackageNotificationObserver() {
  download_manager_->RemoveObserver(this);
}

void SavePackageNotificationObserver::OnSavePackageSuccessfullyFinished(
    content::DownloadManager* manager, content::DownloadItem* item) {
  if (automation_.get()) {
    AutomationJSONReply(automation_.get(), reply_message_.release())
        .SendSuccess(NULL);
  }
  delete this;
}

void SavePackageNotificationObserver::ManagerGoingDown(
    content::DownloadManager* manager) {
  delete this;
}

namespace {

// Returns a vector of dictionaries containing information about installed apps,
// as identified from a given list of extensions.  The caller takes ownership
// of the created vector.
std::vector<base::DictionaryValue*>* GetAppInfoFromExtensions(
    const extensions::ExtensionSet* extensions,
    ExtensionService* ext_service) {
  std::vector<base::DictionaryValue*>* apps_list =
      new std::vector<base::DictionaryValue*>();
  for (extensions::ExtensionSet::const_iterator ext = extensions->begin();
       ext != extensions->end(); ++ext) {
    // Only return information about extensions that are actually apps.
    if ((*ext)->is_app()) {
      base::DictionaryValue* app_info = new base::DictionaryValue();
      AppLauncherHandler::CreateAppInfo(ext->get(), ext_service, app_info);
      app_info->SetBoolean(
          "is_component_extension",
          (*ext)->location() == extensions::Manifest::COMPONENT);

      // Convert the launch_type integer into a more descriptive string.
      int launch_type;
      const char* kLaunchType = "launch_type";
      if (!app_info->GetInteger(kLaunchType, &launch_type)) {
        NOTREACHED() << "Can't get integer from key " << kLaunchType;
        continue;
      }
      if (launch_type == extensions::LAUNCH_TYPE_PINNED) {
        app_info->SetString(kLaunchType, "pinned");
      } else if (launch_type == extensions::LAUNCH_TYPE_REGULAR) {
        app_info->SetString(kLaunchType, "regular");
      } else if (launch_type == extensions::LAUNCH_TYPE_FULLSCREEN) {
        app_info->SetString(kLaunchType, "fullscreen");
      } else if (launch_type == extensions::LAUNCH_TYPE_WINDOW) {
        app_info->SetString(kLaunchType, "window");
      } else {
        app_info->SetString(kLaunchType, "unknown");
      }

      apps_list->push_back(app_info);
    }
  }
  return apps_list;
}

}  // namespace

NTPInfoObserver::NTPInfoObserver(AutomationProvider* automation,
                                 IPC::Message* reply_message)
    : automation_(automation->AsWeakPtr()),
      reply_message_(reply_message),
      request_(0),
      ntp_info_(new base::DictionaryValue) {
  top_sites_ = automation_->profile()->GetTopSites();
  if (!top_sites_) {
    AutomationJSONReply(automation_.get(), reply_message_.release())
        .SendError("Profile does not have service for querying the top sites.");
    return;
  }
  TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(automation_->profile());
  if (!service) {
    AutomationJSONReply(automation_.get(), reply_message_.release())
        .SendError("No TabRestoreService.");
    return;
  }

  // Collect information about the apps in the new tab page.
  ExtensionService* ext_service = extensions::ExtensionSystem::Get(
      automation_->profile())->extension_service();
  if (!ext_service) {
    AutomationJSONReply(automation_.get(), reply_message_.release())
        .SendError("No ExtensionService.");
    return;
  }
  // Process enabled extensions.
  extensions::ExtensionRegistry* extension_registry =
      extensions::ExtensionRegistry::Get(automation_->profile());
  base::ListValue* apps_list = new base::ListValue();
  const extensions::ExtensionSet& enabled_extensions =
      extension_registry->enabled_extensions();
  std::vector<base::DictionaryValue*>* enabled_apps = GetAppInfoFromExtensions(
      &enabled_extensions, ext_service);
  for (std::vector<base::DictionaryValue*>::const_iterator app =
       enabled_apps->begin(); app != enabled_apps->end(); ++app) {
    (*app)->SetBoolean("is_disabled", false);
    apps_list->Append(*app);
  }
  delete enabled_apps;
  // Process disabled extensions.
  const extensions::ExtensionSet& disabled_extensions =
      extension_registry->disabled_extensions();
  std::vector<base::DictionaryValue*>* disabled_apps = GetAppInfoFromExtensions(
      &disabled_extensions, ext_service);
  for (std::vector<base::DictionaryValue*>::const_iterator app =
       disabled_apps->begin(); app != disabled_apps->end(); ++app) {
    (*app)->SetBoolean("is_disabled", true);
    apps_list->Append(*app);
  }
  delete disabled_apps;
  // Process terminated extensions.
  const extensions::ExtensionSet& terminated_extensions =
      extension_registry->terminated_extensions();
  std::vector<base::DictionaryValue*>* terminated_apps =
      GetAppInfoFromExtensions(&terminated_extensions, ext_service);
  for (std::vector<base::DictionaryValue*>::const_iterator app =
       terminated_apps->begin(); app != terminated_apps->end(); ++app) {
    (*app)->SetBoolean("is_disabled", true);
    apps_list->Append(*app);
  }
  delete terminated_apps;
  ntp_info_->Set("apps", apps_list);

  // Get the info that would be displayed in the recently closed section.
  base::ListValue* recently_closed_list = new base::ListValue;
  RecentlyClosedTabsHandler::CreateRecentlyClosedValues(service->entries(),
                                                        recently_closed_list);
  ntp_info_->Set("recently_closed", recently_closed_list);

  // Add default site URLs.
  base::ListValue* default_sites_list = new base::ListValue;
  history::MostVisitedURLList urls = top_sites_->GetPrepopulatePages();
  for (size_t i = 0; i < urls.size(); ++i) {
    default_sites_list->Append(base::Value::CreateStringValue(
        urls[i].url.possibly_invalid_spec()));
  }
  ntp_info_->Set("default_sites", default_sites_list);

  registrar_.Add(this, chrome::NOTIFICATION_TOP_SITES_UPDATED,
                 content::Source<history::TopSites>(top_sites_));
  if (top_sites_->loaded()) {
    OnTopSitesLoaded();
  } else {
    registrar_.Add(this, chrome::NOTIFICATION_TOP_SITES_LOADED,
                   content::Source<Profile>(automation_->profile()));
  }
}

NTPInfoObserver::~NTPInfoObserver() {}

void NTPInfoObserver::Observe(int type,
                              const content::NotificationSource& source,
                              const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_TOP_SITES_LOADED) {
    OnTopSitesLoaded();
  } else if (type == chrome::NOTIFICATION_TOP_SITES_UPDATED) {
    content::Details<CancelableRequestProvider::Handle> request_details(
          details);
    if (request_ == *request_details.ptr()) {
      top_sites_->GetMostVisitedURLs(
          base::Bind(&NTPInfoObserver::OnTopSitesReceived,
                     base::Unretained(this)), false);
    }
  }
}

void NTPInfoObserver::OnTopSitesLoaded() {
  request_ = top_sites_->StartQueryForMostVisited();
}

void NTPInfoObserver::OnTopSitesReceived(
    const history::MostVisitedURLList& visited_list) {
  if (!automation_.get()) {
    delete this;
    return;
  }

  base::ListValue* list_value = new base::ListValue;
  for (size_t i = 0; i < visited_list.size(); ++i) {
    const history::MostVisitedURL& visited = visited_list[i];
    if (visited.url.spec().empty())
      break;  // This is the signal that there are no more real visited sites.
    base::DictionaryValue* dict = new base::DictionaryValue;
    dict->SetString("url", visited.url.spec());
    dict->SetString("title", visited.title);
    list_value->Append(dict);
  }
  ntp_info_->Set("most_visited", list_value);
  AutomationJSONReply(automation_.get(), reply_message_.release())
      .SendSuccess(ntp_info_.get());
  delete this;
}

AppLaunchObserver::AppLaunchObserver(
    NavigationController* controller,
    AutomationProvider* automation,
    IPC::Message* reply_message,
    extensions::LaunchContainer launch_container)
    : controller_(controller),
      automation_(automation->AsWeakPtr()),
      reply_message_(reply_message),
      launch_container_(launch_container),
      new_window_id_(extension_misc::kUnknownWindowId) {
  if (launch_container_ == extensions::LAUNCH_CONTAINER_TAB) {
    // Need to wait for the currently-active tab to reload.
    content::Source<NavigationController> source(controller_);
    registrar_.Add(this, content::NOTIFICATION_LOAD_STOP, source);
  } else {
    // Need to wait for a new tab in a new window to load.
    registrar_.Add(this, content::NOTIFICATION_LOAD_STOP,
                   content::NotificationService::AllSources());
    registrar_.Add(this, chrome::NOTIFICATION_BROWSER_WINDOW_READY,
                   content::NotificationService::AllSources());
  }
}

AppLaunchObserver::~AppLaunchObserver() {}

void AppLaunchObserver::Observe(int type,
                                const content::NotificationSource& source,
                                const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_BROWSER_WINDOW_READY) {
    new_window_id_ = extensions::ExtensionTabUtil::GetWindowId(
        content::Source<Browser>(source).ptr());
    return;
  }

  DCHECK_EQ(content::NOTIFICATION_LOAD_STOP, type);
  SessionTabHelper* session_tab_helper = SessionTabHelper::FromWebContents(
      content::Source<NavigationController>(source)->GetWebContents());
  if ((launch_container_ == extensions::LAUNCH_CONTAINER_TAB) ||
      (session_tab_helper &&
          (session_tab_helper->window_id().id() == new_window_id_))) {
    if (automation_.get()) {
      AutomationJSONReply(automation_.get(), reply_message_.release())
          .SendSuccess(NULL);
    }
    delete this;
  }
}

RendererProcessClosedObserver::RendererProcessClosedObserver(
    AutomationProvider* automation,
    IPC::Message* reply_message)
    : automation_(automation->AsWeakPtr()),
      reply_message_(reply_message) {
  registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
                 content::NotificationService::AllSources());
}

RendererProcessClosedObserver::~RendererProcessClosedObserver() {}

void RendererProcessClosedObserver::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (automation_.get()) {
    AutomationJSONReply(automation_.get(), reply_message_.release())
        .SendSuccess(NULL);
  }
  delete this;
}

InputEventAckNotificationObserver::InputEventAckNotificationObserver(
    AutomationProvider* automation,
    IPC::Message* reply_message,
    int event_type,
    int count)
    : automation_(automation->AsWeakPtr()),
      reply_message_(reply_message),
      event_type_(event_type),
      count_(count) {
  DCHECK(1 <= count);
  registrar_.Add(
      this,
      content::NOTIFICATION_RENDER_WIDGET_HOST_DID_RECEIVE_INPUT_EVENT_ACK,
      content::NotificationService::AllSources());
  registrar_.Add(
      this,
      chrome::NOTIFICATION_APP_MODAL_DIALOG_SHOWN,
      content::NotificationService::AllSources());
}

InputEventAckNotificationObserver::~InputEventAckNotificationObserver() {}

void InputEventAckNotificationObserver::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_APP_MODAL_DIALOG_SHOWN) {
    AutomationJSONReply(automation_.get(), reply_message_.release())
        .SendSuccess(NULL);
    delete this;
    return;
  }

  content::Details<int> request_details(details);
  // If the event type matches for |count_| times, replies with a JSON message.
  if (event_type_ == *request_details.ptr()) {
    if (--count_ == 0 && automation_.get()) {
      AutomationJSONReply(automation_.get(), reply_message_.release())
          .SendSuccess(NULL);
      delete this;
    }
  } else {
    LOG(WARNING) << "Ignoring unexpected event type: "
                 << *request_details.ptr() << " (expected: " << event_type_
                 << ")";
  }
}

NewTabObserver::NewTabObserver(AutomationProvider* automation,
                               IPC::Message* reply_message,
                               bool use_json_interface)
    : automation_(automation->AsWeakPtr()),
      reply_message_(reply_message),
      use_json_interface_(use_json_interface) {
  // Use TAB_PARENTED to detect the new tab.
  registrar_.Add(this,
                 chrome::NOTIFICATION_TAB_PARENTED,
                 content::NotificationService::AllSources());
}

void NewTabObserver::Observe(int type,
                             const content::NotificationSource& source,
                             const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_TAB_PARENTED, type);
  NavigationController* controller =
      &(content::Source<content::WebContents>(source).ptr()->GetController());
  if (automation_.get()) {
    // TODO(phajdan.jr): Clean up this hack. We write the correct return type
    // here, but don't send the message. NavigationNotificationObserver
    // will wait properly for the load to finish, and send the message,
    // but it will also append its own return value at the end of the reply.
    if (!use_json_interface_)
      AutomationMsg_WindowExecuteCommand::WriteReplyParams(reply_message_.get(),
                                                           true);
    new NavigationNotificationObserver(controller,
                                       automation_.get(),
                                       reply_message_.release(),
                                       1,
                                       false,
                                       use_json_interface_);
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
      base::Bind(
          &WaitForProcessLauncherThreadToGoIdleObserver::
              RunOnProcessLauncherThread,
          this));
}

WaitForProcessLauncherThreadToGoIdleObserver::
    ~WaitForProcessLauncherThreadToGoIdleObserver() {
}

void WaitForProcessLauncherThreadToGoIdleObserver::
RunOnProcessLauncherThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::PROCESS_LAUNCHER));
  BrowserThread::PostTask(
      BrowserThread::PROCESS_LAUNCHER, FROM_HERE,
      base::Bind(
          &WaitForProcessLauncherThreadToGoIdleObserver::
              RunOnProcessLauncherThread2,
          this));
}

void WaitForProcessLauncherThreadToGoIdleObserver::
RunOnProcessLauncherThread2() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::PROCESS_LAUNCHER));
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&WaitForProcessLauncherThreadToGoIdleObserver::RunOnUIThread,
                 this));
}

void WaitForProcessLauncherThreadToGoIdleObserver::RunOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (automation_.get())
    automation_->Send(reply_message_.release());
  Release();
}

DragTargetDropAckNotificationObserver::DragTargetDropAckNotificationObserver(
    AutomationProvider* automation,
    IPC::Message* reply_message)
    : automation_(automation->AsWeakPtr()),
      reply_message_(reply_message) {
  registrar_.Add(
      this,
      content::NOTIFICATION_RENDER_VIEW_HOST_DID_RECEIVE_DRAG_TARGET_DROP_ACK,
      content::NotificationService::AllSources());
  registrar_.Add(
      this,
      chrome::NOTIFICATION_APP_MODAL_DIALOG_SHOWN,
      content::NotificationService::AllSources());
}

DragTargetDropAckNotificationObserver::
    ~DragTargetDropAckNotificationObserver() {}

void DragTargetDropAckNotificationObserver::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (automation_.get()) {
    AutomationJSONReply(automation_.get(), reply_message_.release())
        .SendSuccess(NULL);
  }
  delete this;
}

ProcessInfoObserver::ProcessInfoObserver(
    AutomationProvider* automation,
    IPC::Message* reply_message)
    : automation_(automation->AsWeakPtr()),
      reply_message_(reply_message) {}

ProcessInfoObserver::~ProcessInfoObserver() {}

void ProcessInfoObserver::OnDetailsAvailable() {
  scoped_ptr<base::DictionaryValue> return_value(new base::DictionaryValue);
  base::ListValue* browser_proc_list = new base::ListValue();
  const std::vector<ProcessData>& all_processes = processes();
  for (size_t index = 0; index < all_processes.size(); ++index) {
    base::DictionaryValue* browser_data = new base::DictionaryValue();
    browser_data->SetString("name", all_processes[index].name);
    browser_data->SetString("process_name", all_processes[index].process_name);

    base::ListValue* proc_list = new base::ListValue();
    for (ProcessMemoryInformationList::const_iterator iterator =
             all_processes[index].processes.begin();
         iterator != all_processes[index].processes.end(); ++iterator) {
      base::DictionaryValue* proc_data = new base::DictionaryValue();

      proc_data->SetInteger("pid", iterator->pid);

      // Working set (resident) memory usage, in KBytes.
      base::DictionaryValue* working_set = new base::DictionaryValue();
      working_set->SetInteger("priv", iterator->working_set.priv);
      working_set->SetInteger("shareable", iterator->working_set.shareable);
      working_set->SetInteger("shared", iterator->working_set.shared);
      proc_data->Set("working_set_mem", working_set);

      // Committed (resident + paged) memory usage, in KBytes.
      base::DictionaryValue* committed = new base::DictionaryValue();
      committed->SetInteger("priv", iterator->committed.priv);
      committed->SetInteger("mapped", iterator->committed.mapped);
      committed->SetInteger("image", iterator->committed.image);
      proc_data->Set("committed_mem", committed);

      proc_data->SetString("version", iterator->version);
      proc_data->SetString("product_name", iterator->product_name);
      proc_data->SetInteger("num_processes", iterator->num_processes);
      proc_data->SetBoolean("is_diagnostics", iterator->is_diagnostics);

      // Process type, if this is a child process of Chrome (e.g., 'plugin').
      std::string process_type = "Unknown";
      // The following condition avoids a DCHECK in debug builds when the
      // process type passed to |GetTypeNameInEnglish| is unknown.
      if (iterator->process_type != content::PROCESS_TYPE_UNKNOWN) {
        process_type =
            content::GetProcessTypeNameInEnglish(iterator->process_type);
      }
      proc_data->SetString("child_process_type", process_type);

      // Renderer type, if this is a renderer process.
      std::string renderer_type = "Unknown";
      if (iterator->renderer_type !=
          ProcessMemoryInformation::RENDERER_UNKNOWN) {
        renderer_type = ProcessMemoryInformation::GetRendererTypeNameInEnglish(
            iterator->renderer_type);
      }
      proc_data->SetString("renderer_type", renderer_type);

      // Titles associated with this process.
      base::ListValue* titles = new base::ListValue();
      for (size_t title_index = 0; title_index < iterator->titles.size();
           ++title_index) {
        titles->Append(
            base::Value::CreateStringValue(iterator->titles[title_index]));
      }
      proc_data->Set("titles", titles);

      proc_list->Append(proc_data);
    }
    browser_data->Set("processes", proc_list);

    browser_proc_list->Append(browser_data);
  }
  return_value->Set("browsers", browser_proc_list);

  if (automation_.get()) {
    AutomationJSONReply(automation_.get(), reply_message_.release())
        .SendSuccess(return_value.get());
  }
}

V8HeapStatsObserver::V8HeapStatsObserver(
    AutomationProvider* automation,
    IPC::Message* reply_message,
    base::ProcessId renderer_id)
    : automation_(automation->AsWeakPtr()),
      reply_message_(reply_message),
      renderer_id_(renderer_id) {
  registrar_.Add(
      this,
      chrome::NOTIFICATION_RENDERER_V8_HEAP_STATS_COMPUTED,
      content::NotificationService::AllSources());
}

V8HeapStatsObserver::~V8HeapStatsObserver() {}

void V8HeapStatsObserver::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(type == chrome::NOTIFICATION_RENDERER_V8_HEAP_STATS_COMPUTED);

  base::ProcessId updated_renderer_id =
      *(content::Source<base::ProcessId>(source).ptr());
  // Only return information for the renderer ID we're interested in.
  if (renderer_id_ != updated_renderer_id)
    return;

  ChromeRenderMessageFilter::V8HeapStatsDetails* v8_heap_details =
      content::Details<ChromeRenderMessageFilter::V8HeapStatsDetails>(details)
          .ptr();
  scoped_ptr<base::DictionaryValue> return_value(new base::DictionaryValue);
  return_value->SetInteger("renderer_id", updated_renderer_id);
  return_value->SetInteger("v8_memory_allocated",
                           v8_heap_details->v8_memory_allocated());
  return_value->SetInteger("v8_memory_used",
                           v8_heap_details->v8_memory_used());

  if (automation_.get()) {
    AutomationJSONReply(automation_.get(), reply_message_.release())
        .SendSuccess(return_value.get());
  }
  delete this;
}

FPSObserver::FPSObserver(
    AutomationProvider* automation,
    IPC::Message* reply_message,
    base::ProcessId renderer_id,
    int routing_id)
    : automation_(automation->AsWeakPtr()),
      reply_message_(reply_message),
      renderer_id_(renderer_id),
      routing_id_(routing_id) {
  registrar_.Add(
      this,
      chrome::NOTIFICATION_RENDERER_FPS_COMPUTED,
      content::NotificationService::AllSources());
}

FPSObserver::~FPSObserver() {}

void FPSObserver::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(type == chrome::NOTIFICATION_RENDERER_FPS_COMPUTED);

  base::ProcessId updated_renderer_id =
      *(content::Source<base::ProcessId>(source).ptr());
  // Only return information for the renderer ID we're interested in.
  if (renderer_id_ != updated_renderer_id)
    return;

  ChromeRenderMessageFilter::FPSDetails* fps_details =
      content::Details<ChromeRenderMessageFilter::FPSDetails>(details).ptr();
  // Only return information for the routing id of the host render view we're
  // interested in.
  if (routing_id_ != fps_details->routing_id())
    return;

  scoped_ptr<base::DictionaryValue> return_value(new base::DictionaryValue);
  return_value->SetInteger("renderer_id", updated_renderer_id);
  return_value->SetInteger("routing_id", fps_details->routing_id());
  return_value->SetDouble("fps", fps_details->fps());
  if (automation_.get()) {
    AutomationJSONReply(automation_.get(), reply_message_.release())
        .SendSuccess(return_value.get());
  }
  delete this;
}

BrowserOpenedWithNewProfileNotificationObserver::
    BrowserOpenedWithNewProfileNotificationObserver(
        AutomationProvider* automation,
        IPC::Message* reply_message)
        : automation_(automation->AsWeakPtr()),
          reply_message_(reply_message),
          new_window_id_(extension_misc::kUnknownWindowId) {
  registrar_.Add(this, chrome::NOTIFICATION_PROFILE_CREATED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, chrome::NOTIFICATION_BROWSER_OPENED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, content::NOTIFICATION_LOAD_STOP,
                 content::NotificationService::AllBrowserContextsAndSources());
}

BrowserOpenedWithNewProfileNotificationObserver::
    ~BrowserOpenedWithNewProfileNotificationObserver() {
}

void BrowserOpenedWithNewProfileNotificationObserver::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (!automation_.get()) {
    delete this;
    return;
  }

  if (type == chrome::NOTIFICATION_PROFILE_CREATED) {
    // As part of multi-profile creation, a new browser window will
    // automatically be opened.
    Profile* profile = content::Source<Profile>(source).ptr();
    if (!profile) {
      AutomationJSONReply(automation_.get(), reply_message_.release())
          .SendError("Profile could not be created.");
      return;
    }
  } else if (type == chrome::NOTIFICATION_BROWSER_OPENED) {
    // Store the new browser ID and continue waiting for a new tab within it
    // to stop loading.
    new_window_id_ = extensions::ExtensionTabUtil::GetWindowId(
        content::Source<Browser>(source).ptr());
  } else {
    DCHECK_EQ(content::NOTIFICATION_LOAD_STOP, type);
    // Only send the result if the loaded tab is in the new window.
    NavigationController* controller =
        content::Source<NavigationController>(source).ptr();
    SessionTabHelper* session_tab_helper =
        SessionTabHelper::FromWebContents(controller->GetWebContents());
    int window_id = session_tab_helper ? session_tab_helper->window_id().id()
                                       : -1;
    if (window_id == new_window_id_) {
      if (automation_.get()) {
        AutomationJSONReply(automation_.get(), reply_message_.release())
            .SendSuccess(NULL);
      }
      delete this;
    }
  }
}

ExtensionPopupObserver::ExtensionPopupObserver(
    AutomationProvider* automation,
    IPC::Message* reply_message,
    const std::string& extension_id)
    : automation_(automation->AsWeakPtr()),
      reply_message_(reply_message),
      extension_id_(extension_id) {
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_HOST_DID_STOP_LOADING,
                 content::NotificationService::AllSources());
}

ExtensionPopupObserver::~ExtensionPopupObserver() {
}

void ExtensionPopupObserver::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (!automation_.get()) {
    delete this;
    return;
  }

  extensions::ExtensionHost* host =
      content::Details<extensions::ExtensionHost>(details).ptr();
  if (host->extension_id() == extension_id_ &&
      host->extension_host_type() == extensions::VIEW_TYPE_EXTENSION_POPUP) {
    AutomationJSONReply(automation_.get(), reply_message_.release())
        .SendSuccess(NULL);
    delete this;
  }
}

#if defined(OS_LINUX)
WindowMaximizedObserver::WindowMaximizedObserver(
    AutomationProvider* automation,
    IPC::Message* reply_message)
    : automation_(automation->AsWeakPtr()),
      reply_message_(reply_message) {
  registrar_.Add(this, chrome::NOTIFICATION_BROWSER_WINDOW_MAXIMIZED,
                 content::NotificationService::AllSources());
}

WindowMaximizedObserver::~WindowMaximizedObserver() {}

void WindowMaximizedObserver::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_BROWSER_WINDOW_MAXIMIZED, type);

  if (automation_.get()) {
    AutomationJSONReply(automation_.get(), reply_message_.release())
        .SendSuccess(NULL);
  }
  delete this;
}
#endif  // defined(OS_LINUX)

BrowserOpenedWithExistingProfileNotificationObserver::
    BrowserOpenedWithExistingProfileNotificationObserver(
        AutomationProvider* automation,
        IPC::Message* reply_message,
        int num_loads)
        : automation_(automation->AsWeakPtr()),
          reply_message_(reply_message),
          new_window_id_(extension_misc::kUnknownWindowId),
          num_loads_(num_loads) {
  registrar_.Add(this, chrome::NOTIFICATION_BROWSER_OPENED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, content::NOTIFICATION_LOAD_STOP,
                 content::NotificationService::AllBrowserContextsAndSources());
}

BrowserOpenedWithExistingProfileNotificationObserver::
    ~BrowserOpenedWithExistingProfileNotificationObserver() {
}

void BrowserOpenedWithExistingProfileNotificationObserver::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (!automation_.get()) {
    delete this;
    return;
  }

  if (type == chrome::NOTIFICATION_BROWSER_OPENED) {
    // Store the new browser ID and continue waiting for NOTIFICATION_LOAD_STOP.
    new_window_id_ = extensions::ExtensionTabUtil::GetWindowId(
        content::Source<Browser>(source).ptr());
  } else if (type == content::NOTIFICATION_LOAD_STOP) {
    // Only consider if the loaded tab is in the new window.
    NavigationController* controller =
        content::Source<NavigationController>(source).ptr();
    SessionTabHelper* session_tab_helper =
        SessionTabHelper::FromWebContents(controller->GetWebContents());
    int window_id = session_tab_helper ? session_tab_helper->window_id().id()
                                       : -1;
    if (window_id == new_window_id_ && --num_loads_ == 0) {
      if (automation_.get()) {
        AutomationJSONReply(automation_.get(), reply_message_.release())
            .SendSuccess(NULL);
      }
      delete this;
    }
  } else {
    NOTREACHED();
  }
}
