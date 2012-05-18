// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/testing_automation_provider.h"

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/json/string_escape.h"
#include "base/path_service.h"
#include "base/process.h"
#include "base/process_util.h"
#include "base/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/autocomplete/autocomplete.h"
#include "chrome/browser/autocomplete/autocomplete_edit.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/autofill/autofill_manager.h"
#include "chrome/browser/autofill/credit_card.h"
#include "chrome/browser/autofill/personal_data_manager.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/automation/automation_browser_tracker.h"
#include "chrome/browser/automation/automation_provider_json.h"
#include "chrome/browser/automation/automation_provider_list.h"
#include "chrome/browser/automation/automation_provider_observers.h"
#include "chrome/browser/automation/automation_tab_tracker.h"
#include "chrome/browser/automation/automation_util.h"
#include "chrome/browser/automation/automation_window_tracker.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_storage.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/debugger/devtools_window.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/download/download_service.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/download/download_shelf.h"
#include "chrome/browser/download/save_package_file_picker.h"
#include "chrome/browser/extensions/browser_action_test_util.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/extensions/updater/extension_updater.h"
#include "chrome/browser/history/top_sites.h"
#include "chrome/browser/importer/importer_host.h"
#include "chrome/browser/importer/importer_list.h"
#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "chrome/browser/instant/instant_controller.h"
#include "chrome/browser/notifications/balloon.h"
#include "chrome/browser/notifications/balloon_collection.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/password_manager/password_store.h"
#include "chrome/browser/password_manager/password_store_change.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/plugin_prefs.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/printing/print_preview_tab_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/protector/protector_service.h"
#include "chrome/browser/protector/protector_service_factory.h"
#include "chrome/browser/protector/protector_utils.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/sessions/restore_tab_helper.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/tab_contents/confirm_infobar_delegate.h"
#include "chrome/browser/tab_contents/link_infobar_delegate.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/translate/translate_infobar_delegate.h"
#include "chrome/browser/translate/translate_tab_helper.h"
#include "chrome/browser/ui/app_modal_dialogs/app_modal_dialog.h"
#include "chrome/browser/ui/app_modal_dialogs/app_modal_dialog_queue.h"
#include "chrome/browser/ui/app_modal_dialogs/js_modal_dialog.h"
#include "chrome/browser/ui/app_modal_dialogs/native_app_modal_dialog.h"
#include "chrome/browser/ui/blocked_content/blocked_content_tab_helper.h"
#include "chrome/browser/ui/bookmarks/bookmark_bar.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/find_bar/find_bar.h"
#include "chrome/browser/ui/fullscreen_controller.h"
#include "chrome/browser/ui/fullscreen_exit_bubble_type.h"
#include "chrome/browser/ui/login/login_prompt.h"
#include "chrome/browser/ui/media_stream_infobar_delegate.h"
#include "chrome/browser/ui/omnibox/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/search_engines/keyword_editor_controller.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/automation_id.h"
#include "chrome/common/automation_messages.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_view_type.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_action.h"
#include "chrome/common/extensions/url_pattern.h"
#include "chrome/common/extensions/url_pattern_set.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_child_process_host_iterator.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/geolocation.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/interstitial_page_delegate.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_view_host_delegate.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/common_param_traits.h"
#include "content/public/common/geoposition.h"
#include "content/public/common/ssl_status.h"
#include "net/cookies/cookie_store.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
#include "ui/base/events.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/base/ui_base_types.h"
#include "ui/ui_controls/ui_controls.h"
#include "webkit/glue/webdropdata.h"
#include "webkit/plugins/webplugininfo.h"

#if defined(ENABLE_CONFIGURATION_POLICY)
#include "chrome/browser/policy/policy_service.h"
#include "policy/policy_constants.h"
#endif

#if defined(OS_CHROMEOS)
#include "chromeos/dbus/dbus_thread_manager.h"
#endif

#if defined(OS_MACOSX)
#include "base/mach_ipc_mac.h"
#endif

#if !defined(NO_TCMALLOC) && (defined(OS_LINUX) || defined(OS_CHROMEOS))
#include "third_party/tcmalloc/chromium/src/gperftools/heap-profiler.h"
#endif  // !defined(NO_TCMALLOC) && (defined(OS_LINUX) || defined(OS_CHROMEOS))

using automation::Error;
using automation::ErrorCode;
using automation_util::SendErrorIfModalDialogActive;
using content::BrowserChildProcessHostIterator;
using content::BrowserThread;
using content::ChildProcessHost;
using content::DownloadItem;
using content::DownloadManager;
using content::InterstitialPage;
using content::NavigationController;
using content::NavigationEntry;
using content::PluginService;
using content::OpenURLParams;
using content::Referrer;
using content::RenderViewHost;
using content::SSLStatus;
using content::WebContents;

namespace {

// Helper to reply asynchronously if |automation| is still valid.
void SendSuccessReply(base::WeakPtr<AutomationProvider> automation,
                      IPC::Message* reply_message) {
  if (automation)
    AutomationJSONReply(automation.get(), reply_message).SendSuccess(NULL);
}

// Helper to resolve the overloading of PostTask.
void PostTask(BrowserThread::ID id, const base::Closure& callback) {
  BrowserThread::PostTask(id, FROM_HERE, callback);
}

void SendMouseClick(int flags) {
  ui_controls::MouseButton button = ui_controls::LEFT;
  if ((flags & ui::EF_LEFT_MOUSE_BUTTON) ==
      ui::EF_LEFT_MOUSE_BUTTON) {
    button = ui_controls::LEFT;
  } else if ((flags & ui::EF_RIGHT_MOUSE_BUTTON) ==
             ui::EF_RIGHT_MOUSE_BUTTON) {
    button = ui_controls::RIGHT;
  } else if ((flags & ui::EF_MIDDLE_MOUSE_BUTTON) ==
             ui::EF_MIDDLE_MOUSE_BUTTON) {
    button = ui_controls::MIDDLE;
  } else {
    NOTREACHED();
  }

  ui_controls::SendMouseClick(button);
}

class AutomationInterstitialPage : public content::InterstitialPageDelegate {
 public:
  AutomationInterstitialPage(WebContents* tab,
                             const GURL& url,
                             const std::string& contents)
      : contents_(contents) {
    interstitial_page_ = InterstitialPage::Create(tab, true, url, this);
    interstitial_page_->Show();
  }

  virtual std::string GetHTMLContents() OVERRIDE { return contents_; }

 private:
  const std::string contents_;
  InterstitialPage* interstitial_page_;  // Owns us.

  DISALLOW_COPY_AND_ASSIGN(AutomationInterstitialPage);
};

}  // namespace

TestingAutomationProvider::TestingAutomationProvider(Profile* profile)
    : AutomationProvider(profile)
#if defined(OS_CHROMEOS)
      , power_manager_observer_(NULL)
#endif
      {
  BrowserList::AddObserver(this);
  registrar_.Add(this, chrome::NOTIFICATION_SESSION_END,
                 content::NotificationService::AllSources());
#if defined(OS_CHROMEOS)
  AddChromeosObservers();
#endif
}

TestingAutomationProvider::~TestingAutomationProvider() {
#if defined(OS_CHROMEOS)
  RemoveChromeosObservers();
#endif
  BrowserList::RemoveObserver(this);
}

IPC::Channel::Mode TestingAutomationProvider::GetChannelMode(
    bool use_named_interface) {
  if (use_named_interface)
#if defined(OS_POSIX)
    return IPC::Channel::MODE_OPEN_NAMED_SERVER;
#else
    return IPC::Channel::MODE_NAMED_SERVER;
#endif
  else
    return IPC::Channel::MODE_CLIENT;
}

void TestingAutomationProvider::OnBrowserAdded(const Browser* browser) {
}

void TestingAutomationProvider::OnBrowserRemoved(const Browser* browser) {
#if !defined(OS_CHROMEOS) && !defined(OS_MACOSX)
  // For backwards compatibility with the testing automation interface, we
  // want the automation provider (and hence the process) to go away when the
  // last browser goes away.
  if (BrowserList::empty() && !CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kKeepAliveForTest)) {
    // If you change this, update Observer for chrome::SESSION_END
    // below.
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&TestingAutomationProvider::OnRemoveProvider, this));
  }
#endif  // !defined(OS_CHROMEOS) && !defined(OS_MACOSX)
}

void TestingAutomationProvider::OnSourceProfilesLoaded() {
  DCHECK_NE(static_cast<ImporterList*>(NULL), importer_list_.get());

  // Get the correct profile based on the browser that the user provided.
  importer::SourceProfile source_profile;
  size_t i = 0;
  size_t importers_count = importer_list_->count();
  for ( ; i < importers_count; ++i) {
    importer::SourceProfile profile = importer_list_->GetSourceProfileAt(i);
    if (profile.importer_name == import_settings_data_.browser_name) {
      source_profile = profile;
      break;
    }
  }
  // If we made it to the end of the loop, then the input was bad.
  if (i == importers_count) {
    AutomationJSONReply(this, import_settings_data_.reply_message)
        .SendError("Invalid browser name string found.");
    return;
  }

  scoped_refptr<ImporterHost> importer_host(new ImporterHost);
  importer_host->SetObserver(
      new AutomationProviderImportSettingsObserver(
          this, import_settings_data_.reply_message));

  Profile* target_profile = import_settings_data_.browser->profile();
  importer_host->StartImportSettings(source_profile,
                                     target_profile,
                                     import_settings_data_.import_items,
                                     new ProfileWriter(target_profile),
                                     import_settings_data_.first_run);
}

void TestingAutomationProvider::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(type == chrome::NOTIFICATION_SESSION_END);
  // OnBrowserRemoved does a ReleaseLater. When session end is received we exit
  // before the task runs resulting in this object not being deleted. This
  // Release balance out the Release scheduled by OnBrowserRemoved.
  Release();
}

bool TestingAutomationProvider::OnMessageReceived(
    const IPC::Message& message) {
  base::ThreadRestrictions::ScopedAllowWait allow_wait;
  bool handled = true;
  bool deserialize_success = true;
  IPC_BEGIN_MESSAGE_MAP_EX(TestingAutomationProvider,
                           message,
                           deserialize_success)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_CloseBrowser, CloseBrowser)
    IPC_MESSAGE_HANDLER(AutomationMsg_ActivateTab, ActivateTab)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_AppendTab, AppendTab)
    IPC_MESSAGE_HANDLER(AutomationMsg_GetMachPortCount, GetMachPortCount)
    IPC_MESSAGE_HANDLER(AutomationMsg_ActiveTabIndex, GetActiveTabIndex)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_CloseTab, CloseTab)
    IPC_MESSAGE_HANDLER(AutomationMsg_GetCookies, GetCookies)
    IPC_MESSAGE_HANDLER(AutomationMsg_SetCookie, SetCookie)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(
        AutomationMsg_NavigateToURLBlockUntilNavigationsComplete,
        NavigateToURLBlockUntilNavigationsComplete)
    IPC_MESSAGE_HANDLER(AutomationMsg_NavigationAsync, NavigationAsync)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_Reload, Reload)
    IPC_MESSAGE_HANDLER(AutomationMsg_BrowserWindowCount, GetBrowserWindowCount)
    IPC_MESSAGE_HANDLER(AutomationMsg_NormalBrowserWindowCount,
                        GetNormalBrowserWindowCount)
    IPC_MESSAGE_HANDLER(AutomationMsg_BrowserWindow, GetBrowserWindow)
    IPC_MESSAGE_HANDLER(AutomationMsg_ActiveWindow, GetActiveWindow)
    IPC_MESSAGE_HANDLER(AutomationMsg_FindTabbedBrowserWindow,
                        FindTabbedBrowserWindow)
    IPC_MESSAGE_HANDLER(AutomationMsg_WindowExecuteCommandAsync,
                        ExecuteBrowserCommandAsync)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_WindowExecuteCommand,
                        ExecuteBrowserCommand)
    IPC_MESSAGE_HANDLER(AutomationMsg_TerminateSession, TerminateSession)
    IPC_MESSAGE_HANDLER(AutomationMsg_WindowViewBounds, WindowGetViewBounds)
    IPC_MESSAGE_HANDLER(AutomationMsg_SetWindowBounds, SetWindowBounds)
    IPC_MESSAGE_HANDLER(AutomationMsg_WindowMouseMove, WindowSimulateMouseMove)
    IPC_MESSAGE_HANDLER(AutomationMsg_WindowKeyPress, WindowSimulateKeyPress)
    IPC_MESSAGE_HANDLER(AutomationMsg_TabCount, GetTabCount)
    IPC_MESSAGE_HANDLER(AutomationMsg_Type, GetType)
    IPC_MESSAGE_HANDLER(AutomationMsg_Tab, GetTab)
    IPC_MESSAGE_HANDLER(AutomationMsg_TabTitle, GetTabTitle)
    IPC_MESSAGE_HANDLER(AutomationMsg_TabIndex, GetTabIndex)
    IPC_MESSAGE_HANDLER(AutomationMsg_TabURL, GetTabURL)
    IPC_MESSAGE_HANDLER(AutomationMsg_ShelfVisibility, GetShelfVisibility)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_DomOperation,
                                    ExecuteJavascript)
    IPC_MESSAGE_HANDLER(AutomationMsg_DownloadDirectory, GetDownloadDirectory)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_OpenNewBrowserWindowOfType,
                                    OpenNewBrowserWindowOfType)
    IPC_MESSAGE_HANDLER(AutomationMsg_WindowForBrowser, GetWindowForBrowser)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_WaitForTabToBeRestored,
                                    WaitForTabToBeRestored)
    IPC_MESSAGE_HANDLER(AutomationMsg_GetSecurityState, GetSecurityState)
    IPC_MESSAGE_HANDLER(AutomationMsg_GetPageType, GetPageType)
    IPC_MESSAGE_HANDLER(AutomationMsg_GetMetricEventDuration,
                        GetMetricEventDuration)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_ActionOnSSLBlockingPage,
                                    ActionOnSSLBlockingPage)
    IPC_MESSAGE_HANDLER(AutomationMsg_BringBrowserToFront, BringBrowserToFront)
    IPC_MESSAGE_HANDLER(AutomationMsg_IsMenuCommandEnabled,
                        IsMenuCommandEnabled)
    IPC_MESSAGE_HANDLER(AutomationMsg_OpenFindInPage,
                        HandleOpenFindInPageRequest)
    IPC_MESSAGE_HANDLER(AutomationMsg_FindWindowVisibility,
                        GetFindWindowVisibility)
    IPC_MESSAGE_HANDLER(AutomationMsg_BookmarkBarVisibility,
                        GetBookmarkBarVisibility)
    IPC_MESSAGE_HANDLER(AutomationMsg_GetBookmarksAsJSON,
                        GetBookmarksAsJSON)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_WaitForBookmarkModelToLoad,
                                    WaitForBookmarkModelToLoad)
    IPC_MESSAGE_HANDLER(AutomationMsg_AddBookmarkGroup,
                        AddBookmarkGroup)
    IPC_MESSAGE_HANDLER(AutomationMsg_AddBookmarkURL,
                        AddBookmarkURL)
    IPC_MESSAGE_HANDLER(AutomationMsg_ReparentBookmark,
                        ReparentBookmark)
    IPC_MESSAGE_HANDLER(AutomationMsg_SetBookmarkTitle,
                        SetBookmarkTitle)
    IPC_MESSAGE_HANDLER(AutomationMsg_SetBookmarkURL,
                        SetBookmarkURL)
    IPC_MESSAGE_HANDLER(AutomationMsg_RemoveBookmark,
                        RemoveBookmark)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(
        AutomationMsg_WaitForBrowserWindowCountToBecome,
        WaitForBrowserWindowCountToBecome)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(
        AutomationMsg_GoBackBlockUntilNavigationsComplete,
        GoBackBlockUntilNavigationsComplete)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(
        AutomationMsg_GoForwardBlockUntilNavigationsComplete,
        GoForwardBlockUntilNavigationsComplete)
    IPC_MESSAGE_HANDLER(AutomationMsg_SetShelfVisibility, SetShelfVisibility)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_SendJSONRequest,
                                    SendJSONRequest)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_WaitForTabCountToBecome,
                                    WaitForTabCountToBecome)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_WaitForInfoBarCount,
                                    WaitForInfoBarCount)
    IPC_MESSAGE_HANDLER(AutomationMsg_ResetToDefaultTheme, ResetToDefaultTheme)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(
        AutomationMsg_WaitForProcessLauncherThreadToGoIdle,
        WaitForProcessLauncherThreadToGoIdle)

    IPC_MESSAGE_UNHANDLED(
        handled = AutomationProvider::OnMessageReceived(message))
  IPC_END_MESSAGE_MAP_EX()
  if (!deserialize_success)
    OnMessageDeserializationFailure();
  return handled;
}

void TestingAutomationProvider::OnChannelError() {
  if (!reinitialize_on_channel_error_ &&
      browser_shutdown::GetShutdownType() == browser_shutdown::NOT_VALID)
    BrowserList::AttemptExit();
  AutomationProvider::OnChannelError();
}

void TestingAutomationProvider::CloseBrowser(int browser_handle,
                                             IPC::Message* reply_message) {
  if (!browser_tracker_->ContainsHandle(browser_handle))
    return;

  Browser* browser = browser_tracker_->GetResource(browser_handle);
  new BrowserClosedNotificationObserver(browser, this, reply_message);
  browser->window()->Close();
}

void TestingAutomationProvider::ActivateTab(int handle,
                                            int at_index,
                                            int* status) {
  *status = -1;
  if (browser_tracker_->ContainsHandle(handle) && at_index > -1) {
    Browser* browser = browser_tracker_->GetResource(handle);
    if (at_index >= 0 && at_index < browser->tab_count()) {
      browser->ActivateTabAt(at_index, true);
      *status = 0;
    }
  }
}

void TestingAutomationProvider::AppendTab(int handle,
                                          const GURL& url,
                                          IPC::Message* reply_message) {
  int append_tab_response = -1;  // -1 is the error code
  content::NotificationObserver* observer = NULL;

  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    observer = new TabAppendedNotificationObserver(browser, this,
                                                   reply_message);
    TabContentsWrapper* contents =
        browser->AddSelectedTabWithURL(url, content::PAGE_TRANSITION_TYPED);
    if (contents) {
      append_tab_response = GetIndexForNavigationController(
          &contents->web_contents()->GetController(), browser);
    }
  }

  if (append_tab_response < 0) {
    // Appending tab failed. Clean up and send failure response.

    if (observer)
      delete observer;

    AutomationMsg_AppendTab::WriteReplyParams(reply_message,
                                              append_tab_response);
    Send(reply_message);
  }
}

void TestingAutomationProvider::GetMachPortCount(int* port_count) {
#if defined(OS_MACOSX)
  base::mac::GetNumberOfMachPorts(mach_task_self(), port_count);
#else
  *port_count = 0;
#endif
}

void TestingAutomationProvider::GetActiveTabIndex(int handle,
                                                  int* active_tab_index) {
  *active_tab_index = -1;  // -1 is the error code
  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    *active_tab_index = browser->active_index();
  }
}

void TestingAutomationProvider::CloseTab(int tab_handle,
                                         bool wait_until_closed,
                                         IPC::Message* reply_message) {
  if (tab_tracker_->ContainsHandle(tab_handle)) {
    NavigationController* controller = tab_tracker_->GetResource(tab_handle);
    int index;
    Browser* browser = browser::FindBrowserForController(controller, &index);
    DCHECK(browser);
    new TabClosedNotificationObserver(this, wait_until_closed, reply_message);
    browser->CloseTabContents(controller->GetWebContents());
    return;
  }

  AutomationMsg_CloseTab::WriteReplyParams(reply_message, false);
  Send(reply_message);
}

void TestingAutomationProvider::GetCookies(const GURL& url, int handle,
                                           int* value_size,
                                           std::string* value) {
  WebContents* contents = tab_tracker_->ContainsHandle(handle) ?
      tab_tracker_->GetResource(handle)->GetWebContents() : NULL;
  automation_util::GetCookies(url, contents, value_size, value);
}

void TestingAutomationProvider::SetCookie(const GURL& url,
                                          const std::string& value,
                                          int handle,
                                          int* response_value) {
  WebContents* contents = tab_tracker_->ContainsHandle(handle) ?
      tab_tracker_->GetResource(handle)->GetWebContents() : NULL;
  automation_util::SetCookie(url, value, contents, response_value);
}

void TestingAutomationProvider::NavigateToURLBlockUntilNavigationsComplete(
    int handle, const GURL& url, int number_of_navigations,
    IPC::Message* reply_message) {
  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* tab = tab_tracker_->GetResource(handle);

    // Simulate what a user would do. Activate the tab and then navigate.
    // We could allow navigating in a background tab in future.
    Browser* browser = FindAndActivateTab(tab);

    if (browser) {
      new NavigationNotificationObserver(tab, this, reply_message,
                                         number_of_navigations, false, false);

      // TODO(darin): avoid conversion to GURL.
      OpenURLParams params(
          url, Referrer(), CURRENT_TAB, content::PAGE_TRANSITION_TYPED, false);
      browser->OpenURL(params);
      return;
    }
  }

  AutomationMsg_NavigateToURLBlockUntilNavigationsComplete::WriteReplyParams(
      reply_message, AUTOMATION_MSG_NAVIGATION_ERROR);
  Send(reply_message);
}

void TestingAutomationProvider::NavigationAsync(int handle,
                                                const GURL& url,
                                                bool* status) {
  *status = false;

  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* tab = tab_tracker_->GetResource(handle);

    // Simulate what a user would do. Activate the tab and then navigate.
    // We could allow navigating in a background tab in future.
    Browser* browser = FindAndActivateTab(tab);

    if (browser) {
      // Don't add any listener unless a callback mechanism is desired.
      // TODO(vibhor): Do this if such a requirement arises in future.
      OpenURLParams params(
          url, Referrer(), CURRENT_TAB, content::PAGE_TRANSITION_TYPED, false);
      browser->OpenURL(params);
      *status = true;
    }
  }
}

void TestingAutomationProvider::Reload(int handle,
                                       IPC::Message* reply_message) {
  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* tab = tab_tracker_->GetResource(handle);
    Browser* browser = FindAndActivateTab(tab);
    if (browser && browser->command_updater()->IsCommandEnabled(IDC_RELOAD)) {
      new NavigationNotificationObserver(
          tab, this, reply_message, 1, false, false);
      browser->Reload(CURRENT_TAB);
      return;
    }
  }

  AutomationMsg_Reload::WriteReplyParams(
      reply_message, AUTOMATION_MSG_NAVIGATION_ERROR);
  Send(reply_message);
}

void TestingAutomationProvider::GetBrowserWindowCount(int* window_count) {
  *window_count = static_cast<int>(BrowserList::size());
}

void TestingAutomationProvider::GetNormalBrowserWindowCount(int* window_count) {
  *window_count = static_cast<int>(browser::GetTabbedBrowserCount(profile_));
}

void TestingAutomationProvider::GetBrowserWindow(int index, int* handle) {
  *handle = 0;
  Browser* browser = automation_util::GetBrowserAt(index);
  if (browser)
    *handle = browser_tracker_->Add(browser);
}

void TestingAutomationProvider::FindTabbedBrowserWindow(int* handle) {
  *handle = 0;
  Browser* browser = browser::FindTabbedBrowser(profile_, false);
  if (browser)
    *handle = browser_tracker_->Add(browser);
}

void TestingAutomationProvider::GetActiveWindow(int* handle) {
  *handle = 0;
  Browser* browser = BrowserList::GetLastActive();
  if (browser) {
    gfx::NativeWindow window = browser->window()->GetNativeHandle();
    *handle = window_tracker_->Add(window);
  }
}

void TestingAutomationProvider::ExecuteBrowserCommandAsync(int handle,
                                                           int command,
                                                           bool* success) {
  *success = false;
  if (!browser_tracker_->ContainsHandle(handle)) {
    LOG(WARNING) << "Browser tracker does not contain handle: " << handle;
    return;
  }
  Browser* browser = browser_tracker_->GetResource(handle);
  if (!browser->command_updater()->SupportsCommand(command)) {
    LOG(WARNING) << "Browser does not support command: " << command;
    return;
  }
  if (!browser->command_updater()->IsCommandEnabled(command)) {
    LOG(WARNING) << "Browser command not enabled: " << command;
    return;
  }
  browser->ExecuteCommand(command);
  *success = true;
}

void TestingAutomationProvider::ExecuteBrowserCommand(
    int handle, int command, IPC::Message* reply_message) {
  // List of commands which just finish synchronously and don't require
  // setting up an observer.
  static const int kSynchronousCommands[] = {
    IDC_HOME,
    IDC_SELECT_NEXT_TAB,
    IDC_SELECT_PREVIOUS_TAB,
    IDC_SHOW_BOOKMARK_MANAGER,
  };
  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    if (browser->command_updater()->SupportsCommand(command) &&
        browser->command_updater()->IsCommandEnabled(command)) {
      // First check if we can handle the command without using an observer.
      for (size_t i = 0; i < arraysize(kSynchronousCommands); i++) {
        if (command == kSynchronousCommands[i]) {
          browser->ExecuteCommand(command);
          AutomationMsg_WindowExecuteCommand::WriteReplyParams(reply_message,
                                                               true);
          Send(reply_message);
          return;
        }
      }

      // Use an observer if we have one, otherwise fail.
      if (ExecuteBrowserCommandObserver::CreateAndRegisterObserver(
          this, browser, command, reply_message)) {
        browser->ExecuteCommand(command);
        return;
      }
    }
  }
  AutomationMsg_WindowExecuteCommand::WriteReplyParams(reply_message, false);
  Send(reply_message);
}

void TestingAutomationProvider::WindowSimulateMouseMove(
    const IPC::Message& message,
    int handle,
    const gfx::Point& location) {
  if (window_tracker_->ContainsHandle(handle))
    ui_controls::SendMouseMove(location.x(), location.y());
}

void TestingAutomationProvider::WindowSimulateKeyPress(
    const IPC::Message& message,
    int handle,
    int key,
    int flags) {
  if (!window_tracker_->ContainsHandle(handle))
    return;

  gfx::NativeWindow window = window_tracker_->GetResource(handle);
  // The key event is sent to whatever window is active.
  ui_controls::SendKeyPress(window, static_cast<ui::KeyboardCode>(key),
                            ((flags & ui::EF_CONTROL_DOWN) ==
                             ui::EF_CONTROL_DOWN),
                            ((flags & ui::EF_SHIFT_DOWN) ==
                             ui::EF_SHIFT_DOWN),
                            ((flags & ui::EF_ALT_DOWN) ==
                             ui::EF_ALT_DOWN),
                            ((flags & ui::EF_COMMAND_DOWN) ==
                             ui::EF_COMMAND_DOWN));
}

void TestingAutomationProvider::WebkitMouseClick(DictionaryValue* args,
                                                 IPC::Message* reply_message) {
  if (SendErrorIfModalDialogActive(this, reply_message))
    return;

  RenderViewHost* view;
  std::string error;
  if (!GetRenderViewFromJSONArgs(args, profile(), &view, &error)) {
    AutomationJSONReply(this, reply_message).SendError(error);
    return;
  }

  WebKit::WebMouseEvent mouse_event;
  if (!args->GetInteger("x", &mouse_event.x) ||
      !args->GetInteger("y", &mouse_event.y)) {
    AutomationJSONReply(this, reply_message)
        .SendError("(X,Y) coordinates missing or invalid");
    return;
  }

  int button;
  if (!args->GetInteger("button", &button)) {
    AutomationJSONReply(this, reply_message)
        .SendError("Mouse button missing or invalid");
    return;
  }
  if (button == automation::kLeftButton) {
    mouse_event.button = WebKit::WebMouseEvent::ButtonLeft;
  } else if (button == automation::kRightButton) {
    mouse_event.button = WebKit::WebMouseEvent::ButtonRight;
  } else if (button == automation::kMiddleButton) {
    mouse_event.button = WebKit::WebMouseEvent::ButtonMiddle;
  } else {
    AutomationJSONReply(this, reply_message)
        .SendError("Invalid button press requested");
    return;
  }

  mouse_event.type = WebKit::WebInputEvent::MouseDown;
  mouse_event.clickCount = 1;

  view->ForwardMouseEvent(mouse_event);

  mouse_event.type = WebKit::WebInputEvent::MouseUp;
  new InputEventAckNotificationObserver(this, reply_message, mouse_event.type,
                                        1);
  view->ForwardMouseEvent(mouse_event);
}

void TestingAutomationProvider::WebkitMouseMove(
    DictionaryValue* args, IPC::Message* reply_message) {
  if (SendErrorIfModalDialogActive(this, reply_message))
    return;

  RenderViewHost* view;
  std::string error;
  if (!GetRenderViewFromJSONArgs(args, profile(), &view, &error)) {
    AutomationJSONReply(this, reply_message).SendError(error);
    return;
  }

  WebKit::WebMouseEvent mouse_event;
  if (!args->GetInteger("x", &mouse_event.x) ||
      !args->GetInteger("y", &mouse_event.y)) {
    AutomationJSONReply(this, reply_message)
        .SendError("(X,Y) coordinates missing or invalid");
    return;
  }

  mouse_event.type = WebKit::WebInputEvent::MouseMove;
  new InputEventAckNotificationObserver(this, reply_message, mouse_event.type,
                                        1);
  view->ForwardMouseEvent(mouse_event);
}

void TestingAutomationProvider::WebkitMouseDrag(DictionaryValue* args,
                                                IPC::Message* reply_message) {
  if (SendErrorIfModalDialogActive(this, reply_message))
    return;

  RenderViewHost* view;
  std::string error;
  if (!GetRenderViewFromJSONArgs(args, profile(), &view, &error)) {
    AutomationJSONReply(this, reply_message).SendError(error);
    return;
  }

  WebKit::WebMouseEvent mouse_event;
  int start_x, start_y, end_x, end_y;
  if (!args->GetInteger("start_x", &start_x) ||
      !args->GetInteger("start_y", &start_y) ||
      !args->GetInteger("end_x", &end_x) ||
      !args->GetInteger("end_y", &end_y)) {
    AutomationJSONReply(this, reply_message)
        .SendError("Invalid start/end positions");
    return;
  }

  mouse_event.type = WebKit::WebInputEvent::MouseMove;
  // Step 1- Move the mouse to the start position.
  mouse_event.x = start_x;
  mouse_event.y = start_y;
  view->ForwardMouseEvent(mouse_event);

  // Step 2- Left click mouse down, the mouse button is fixed.
  mouse_event.type = WebKit::WebInputEvent::MouseDown;
  mouse_event.button = WebKit::WebMouseEvent::ButtonLeft;
  mouse_event.clickCount = 1;
  view->ForwardMouseEvent(mouse_event);

  // Step 3 - Move the mouse to the end position.
  mouse_event.type = WebKit::WebInputEvent::MouseMove;
  mouse_event.x = end_x;
  mouse_event.y = end_y;
  mouse_event.clickCount = 0;
  view->ForwardMouseEvent(mouse_event);

  // Step 4 - Release the left mouse button.
  mouse_event.type = WebKit::WebInputEvent::MouseUp;
  mouse_event.clickCount = 1;
  new InputEventAckNotificationObserver(this, reply_message, mouse_event.type,
                                        1);
  view->ForwardMouseEvent(mouse_event);
}

void TestingAutomationProvider::WebkitMouseButtonDown(
    DictionaryValue* args, IPC::Message* reply_message) {
  if (SendErrorIfModalDialogActive(this, reply_message))
    return;

  RenderViewHost* view;
  std::string error;
  if (!GetRenderViewFromJSONArgs(args, profile(), &view, &error)) {
    AutomationJSONReply(this, reply_message).SendError(error);
    return;
  }

  WebKit::WebMouseEvent mouse_event;
  if (!args->GetInteger("x", &mouse_event.x) ||
      !args->GetInteger("y", &mouse_event.y)) {
    AutomationJSONReply(this, reply_message)
        .SendError("(X,Y) coordinates missing or invalid");
    return;
  }

  mouse_event.type = WebKit::WebInputEvent::MouseDown;
  mouse_event.button = WebKit::WebMouseEvent::ButtonLeft;
  mouse_event.clickCount = 1;
  new InputEventAckNotificationObserver(this, reply_message, mouse_event.type,
                                        1);
  view->ForwardMouseEvent(mouse_event);
}

void TestingAutomationProvider::WebkitMouseButtonUp(
    DictionaryValue* args, IPC::Message* reply_message) {
  if (SendErrorIfModalDialogActive(this, reply_message))
    return;

  RenderViewHost* view;
  std::string error;
  if (!GetRenderViewFromJSONArgs(args, profile(), &view, &error)) {
    AutomationJSONReply(this, reply_message).SendError(error);
    return;
  }

  WebKit::WebMouseEvent mouse_event;
  if (!args->GetInteger("x", &mouse_event.x) ||
      !args->GetInteger("y", &mouse_event.y)) {
    AutomationJSONReply(this, reply_message)
        .SendError("(X,Y) coordinates missing or invalid");
    return;
  }

  mouse_event.type = WebKit::WebInputEvent::MouseUp;
  mouse_event.button = WebKit::WebMouseEvent::ButtonLeft;
  mouse_event.clickCount = 1;
  new InputEventAckNotificationObserver(this, reply_message, mouse_event.type,
                                        1);
  view->ForwardMouseEvent(mouse_event);
}

void TestingAutomationProvider::WebkitMouseDoubleClick(
    DictionaryValue* args, IPC::Message* reply_message) {
  if (SendErrorIfModalDialogActive(this, reply_message))
    return;

  RenderViewHost* view;
  std::string error;
  if (!GetRenderViewFromJSONArgs(args, profile(), &view, &error)) {
    AutomationJSONReply(this, reply_message).SendError(error);
    return;
  }

  WebKit::WebMouseEvent mouse_event;
  if (!args->GetInteger("x", &mouse_event.x) ||
      !args->GetInteger("y", &mouse_event.y)) {
    AutomationJSONReply(this, reply_message)
        .SendError("(X,Y) coordinates missing or invalid");
    return;
  }

  mouse_event.type = WebKit::WebInputEvent::MouseDown;
  mouse_event.button = WebKit::WebMouseEvent::ButtonLeft;
  mouse_event.clickCount = 1;
  view->ForwardMouseEvent(mouse_event);

  mouse_event.type = WebKit::WebInputEvent::MouseUp;
  new InputEventAckNotificationObserver(this, reply_message, mouse_event.type,
                                        2);
  view->ForwardMouseEvent(mouse_event);

  mouse_event.type = WebKit::WebInputEvent::MouseDown;
  mouse_event.clickCount = 2;
  view->ForwardMouseEvent(mouse_event);

  mouse_event.type = WebKit::WebInputEvent::MouseUp;
  view->ForwardMouseEvent(mouse_event);
}

void TestingAutomationProvider::DragAndDropFilePaths(
    DictionaryValue* args, IPC::Message* reply_message) {
  if (SendErrorIfModalDialogActive(this, reply_message))
    return;

  RenderViewHost* view;
  std::string error;
  if (!GetRenderViewFromJSONArgs(args, profile(), &view, &error)) {
    AutomationJSONReply(this, reply_message).SendError(error);
    return;
  }

  int x, y;
  if (!args->GetInteger("x", &x) || !args->GetInteger("y", &y)) {
    AutomationJSONReply(this, reply_message)
        .SendError("(X,Y) coordinates missing or invalid");
    return;
  }

  ListValue* paths = NULL;
  if (!args->GetList("paths", &paths)) {
    AutomationJSONReply(this, reply_message)
        .SendError("'paths' missing or invalid");
    return;
  }

  // Emulate drag and drop to set the file paths to the file upload control.
  WebDropData drop_data;
  for (size_t path_index = 0; path_index < paths->GetSize(); ++path_index) {
    string16 path;
    if (!paths->GetString(path_index, &path)) {
      AutomationJSONReply(this, reply_message)
          .SendError("'paths' contains a non-string type");
      return;
    }

    drop_data.filenames.push_back(
        WebDropData::FileInfo(path, string16()));
  }

  const gfx::Point client(x, y);
  // We don't set any values in screen variable because DragTarget*** ignore the
  // screen argument.
  const gfx::Point screen;

  int operations = 0;
  operations |= WebKit::WebDragOperationCopy;
  operations |= WebKit::WebDragOperationLink;
  operations |= WebKit::WebDragOperationMove;

  view->DragTargetDragEnter(
      drop_data, client, screen,
      static_cast<WebKit::WebDragOperationsMask>(operations), 0);
  new DragTargetDropAckNotificationObserver(this, reply_message);
  view->DragTargetDrop(client, screen, 0);
}

void TestingAutomationProvider::GetTabCount(int handle, int* tab_count) {
  *tab_count = -1;  // -1 is the error code

  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    *tab_count = browser->tab_count();
  }
}

void TestingAutomationProvider::GetType(int handle, int* type_as_int) {
  *type_as_int = -1;  // -1 is the error code

  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    *type_as_int = static_cast<int>(browser->type());
  }
}

void TestingAutomationProvider::GetTab(int win_handle,
                                       int tab_index,
                                       int* tab_handle) {
  *tab_handle = 0;
  if (browser_tracker_->ContainsHandle(win_handle) && (tab_index >= 0)) {
    Browser* browser = browser_tracker_->GetResource(win_handle);
    if (tab_index < browser->tab_count()) {
      WebContents* web_contents = browser->GetWebContentsAt(tab_index);
      *tab_handle = tab_tracker_->Add(&web_contents->GetController());
    }
  }
}

void TestingAutomationProvider::GetTabTitle(int handle,
                                            int* title_string_size,
                                            std::wstring* title) {
  *title_string_size = -1;  // -1 is the error code
  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* tab = tab_tracker_->GetResource(handle);
    NavigationEntry* entry = tab->GetActiveEntry();
    if (entry != NULL) {
      *title = UTF16ToWideHack(entry->GetTitleForDisplay(""));
    } else {
      *title = std::wstring();
    }
    *title_string_size = static_cast<int>(title->size());
  }
}

void TestingAutomationProvider::GetTabIndex(int handle, int* tabstrip_index) {
  *tabstrip_index = -1;  // -1 is the error code

  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* tab = tab_tracker_->GetResource(handle);
    Browser* browser = browser::FindBrowserForController(tab, NULL);
    *tabstrip_index = browser->GetIndexOfController(tab);
  }
}

void TestingAutomationProvider::GetTabURL(int handle,
                                          bool* success,
                                          GURL* url) {
  *success = false;
  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* tab = tab_tracker_->GetResource(handle);
    // Return what the user would see in the location bar.
    *url = tab->GetActiveEntry()->GetVirtualURL();
    *success = true;
  }
}

void TestingAutomationProvider::GetShelfVisibility(int handle, bool* visible) {
  *visible = false;

  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    if (browser) {
      *visible = browser->window()->IsDownloadShelfVisible();
    }
  }
}

void TestingAutomationProvider::ExecuteJavascriptInRenderViewFrame(
    const string16& frame_xpath,
    const string16& script,
    IPC::Message* reply_message,
    RenderViewHost* render_view_host) {
  // Set the routing id of this message with the controller.
  // This routing id needs to be remembered for the reverse
  // communication while sending back the response of
  // this javascript execution.
  render_view_host->ExecuteJavascriptInWebFrame(
      frame_xpath,
      UTF8ToUTF16("window.domAutomationController.setAutomationId(0);"));
  render_view_host->ExecuteJavascriptInWebFrame(
      frame_xpath, script);
}

void TestingAutomationProvider::ExecuteJavascript(
    int handle,
    const std::wstring& frame_xpath,
    const std::wstring& script,
    IPC::Message* reply_message) {
  WebContents* web_contents = GetWebContentsForHandle(handle, NULL);
  if (!web_contents) {
    AutomationMsg_DomOperation::WriteReplyParams(reply_message, std::string());
    Send(reply_message);
    return;
  }

  new DomOperationMessageSender(this, reply_message, false);
  ExecuteJavascriptInRenderViewFrame(WideToUTF16Hack(frame_xpath),
                                     WideToUTF16Hack(script), reply_message,
                                     web_contents->GetRenderViewHost());
}

void TestingAutomationProvider::GetDownloadDirectory(
    int handle, FilePath* download_directory) {
  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* tab = tab_tracker_->GetResource(handle);
    DownloadManager* dlm = tab->GetBrowserContext()->GetDownloadManager();
    *download_directory =
        DownloadPrefs::FromDownloadManager(dlm)->download_path();
  }
}

// Sample json input: { "command": "OpenNewBrowserWindowWithNewProfile" }
// Sample output: {}
void TestingAutomationProvider::OpenNewBrowserWindowWithNewProfile(
    base::DictionaryValue* args, IPC::Message* reply_message) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  new BrowserOpenedWithNewProfileNotificationObserver(this, reply_message);
  profile_manager->CreateMultiProfileAsync();
}

// Sample json input: { "command": "GetMultiProfileInfo" }
// See GetMultiProfileInfo() in pyauto.py for sample output.
void TestingAutomationProvider::GetMultiProfileInfo(
    base::DictionaryValue* args, IPC::Message* reply_message) {
  scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  const ProfileInfoCache& profile_info_cache =
      profile_manager->GetProfileInfoCache();
  return_value->SetBoolean("enabled",
      profile_manager->IsMultipleProfilesEnabled());

  ListValue* profiles = new ListValue;
  for (size_t index = 0; index < profile_info_cache.GetNumberOfProfiles();
       ++index) {
    DictionaryValue* item = new DictionaryValue;
    item->SetString("name", profile_info_cache.GetNameOfProfileAtIndex(index));
    item->SetString("path",
                    profile_info_cache.GetPathOfProfileAtIndex(index).value());
    profiles->Append(item);
  }
  return_value->Set("profiles", profiles);
  AutomationJSONReply(this, reply_message).SendSuccess(return_value.get());
}

void TestingAutomationProvider::OpenNewBrowserWindowOfType(
    int type, bool show, IPC::Message* reply_message) {
  new BrowserOpenedNotificationObserver(this, reply_message);
  // We may have no current browser windows open so don't rely on
  // asking an existing browser to execute the IDC_NEWWINDOW command
  Browser* browser = new Browser(static_cast<Browser::Type>(type), profile_);
  browser->InitBrowserWindow();
  browser->AddBlankTab(true);
  if (show)
    browser->window()->Show();
}

void TestingAutomationProvider::GetWindowForBrowser(int browser_handle,
                                                    bool* success,
                                                    int* handle) {
  *success = false;
  *handle = 0;

  if (browser_tracker_->ContainsHandle(browser_handle)) {
    Browser* browser = browser_tracker_->GetResource(browser_handle);
    gfx::NativeWindow win = browser->window()->GetNativeHandle();
    // Add() returns the existing handle for the resource if any.
    *handle = window_tracker_->Add(win);
    *success = true;
  }
}

void TestingAutomationProvider::WaitForTabToBeRestored(
    int tab_handle,
    IPC::Message* reply_message) {
  if (tab_tracker_->ContainsHandle(tab_handle)) {
    NavigationController* tab = tab_tracker_->GetResource(tab_handle);
    restore_tracker_.reset(
        new NavigationControllerRestoredObserver(this, tab, reply_message));
  } else {
    AutomationMsg_WaitForTabToBeRestored::WriteReplyParams(
        reply_message, false);
    Send(reply_message);
  }
}

void TestingAutomationProvider::GetSecurityState(
    int handle,
    bool* success,
    content::SecurityStyle* security_style,
    net::CertStatus* ssl_cert_status,
    int* insecure_content_status) {
  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* tab = tab_tracker_->GetResource(handle);
    NavigationEntry* entry = tab->GetActiveEntry();
    *success = true;
    *security_style = entry->GetSSL().security_style;
    *ssl_cert_status = entry->GetSSL().cert_status;
    *insecure_content_status = entry->GetSSL().content_status;
  } else {
    *success = false;
    *security_style = content::SECURITY_STYLE_UNKNOWN;
    *ssl_cert_status = 0;
    *insecure_content_status = 0;
  }
}

void TestingAutomationProvider::GetPageType(
    int handle,
    bool* success,
    content::PageType* page_type) {
  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* tab = tab_tracker_->GetResource(handle);
    NavigationEntry* entry = tab->GetActiveEntry();
    *page_type = entry->GetPageType();
    *success = true;
    // In order to return the proper result when an interstitial is shown and
    // no navigation entry were created for it we need to ask the WebContents.
    if (*page_type == content::PAGE_TYPE_NORMAL &&
        tab->GetWebContents()->ShowingInterstitialPage())
      *page_type = content::PAGE_TYPE_INTERSTITIAL;
  } else {
    *success = false;
    *page_type = content::PAGE_TYPE_NORMAL;
  }
}

void TestingAutomationProvider::GetMetricEventDuration(
    const std::string& event_name,
    int* duration_ms) {
  *duration_ms = metric_event_duration_observer_->GetEventDurationMs(
      event_name);
}

void TestingAutomationProvider::ActionOnSSLBlockingPage(
    int handle,
    bool proceed,
    IPC::Message* reply_message) {
  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* tab = tab_tracker_->GetResource(handle);
    NavigationEntry* entry = tab->GetActiveEntry();
    if (entry->GetPageType() == content::PAGE_TYPE_INTERSTITIAL) {
      WebContents* web_contents = tab->GetWebContents();
      InterstitialPage* ssl_blocking_page =
          InterstitialPage::GetInterstitialPage(web_contents);
      if (ssl_blocking_page) {
        if (proceed) {
          new NavigationNotificationObserver(tab, this, reply_message, 1,
                                             false, false);
          ssl_blocking_page->Proceed();
          return;
        }
        ssl_blocking_page->DontProceed();
        AutomationMsg_ActionOnSSLBlockingPage::WriteReplyParams(
            reply_message, AUTOMATION_MSG_NAVIGATION_SUCCESS);
        Send(reply_message);
        return;
      }
    }
  }
  // We failed.
  AutomationMsg_ActionOnSSLBlockingPage::WriteReplyParams(
      reply_message, AUTOMATION_MSG_NAVIGATION_ERROR);
  Send(reply_message);
}

void TestingAutomationProvider::BringBrowserToFront(int browser_handle,
                                                    bool* success) {
  *success = false;
  if (browser_tracker_->ContainsHandle(browser_handle)) {
    Browser* browser = browser_tracker_->GetResource(browser_handle);
    browser->window()->Activate();
    *success = true;
  }
}

void TestingAutomationProvider::IsMenuCommandEnabled(int browser_handle,
                                                     int message_num,
                                                     bool* menu_item_enabled) {
  *menu_item_enabled = false;
  if (browser_tracker_->ContainsHandle(browser_handle)) {
    Browser* browser = browser_tracker_->GetResource(browser_handle);
    *menu_item_enabled =
        browser->command_updater()->IsCommandEnabled(message_num);
  }
}

void TestingAutomationProvider::HandleOpenFindInPageRequest(
    const IPC::Message& message, int handle) {
  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    browser->FindInPage(false, false);
  }
}

void TestingAutomationProvider::GetFindWindowVisibility(int handle,
                                                        bool* visible) {
  *visible = false;
  Browser* browser = browser_tracker_->GetResource(handle);
  if (browser) {
    FindBarTesting* find_bar =
        browser->GetFindBarController()->find_bar()->GetFindBarTesting();
    find_bar->GetFindBarWindowInfo(NULL, visible);
  }
}

// Bookmark bar visibility is based on the pref (e.g. is it in the toolbar).
// Presence in the NTP is signalled in |detached|.
void TestingAutomationProvider::GetBookmarkBarVisibility(int handle,
                                                         bool* visible,
                                                         bool* animating,
                                                         bool* detached) {
  *visible = false;
  *animating = false;

  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    if (browser) {
      // browser->window()->IsBookmarkBarVisible() is not consistent across
      // platforms. bookmark_bar_state() also follows prefs::kShowBookmarkBar
      // and has a shared implementation on all platforms.
      *visible = browser->bookmark_bar_state() == BookmarkBar::SHOW;
      *animating = browser->window()->IsBookmarkBarAnimating();
      *detached = browser->bookmark_bar_state() == BookmarkBar::DETACHED;
    }
  }
}

void TestingAutomationProvider::GetBookmarksAsJSON(
    int handle,
    std::string* bookmarks_as_json,
    bool* success) {
  *success = false;
  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    if (browser) {
      if (!browser->profile()->GetBookmarkModel()->IsLoaded()) {
        return;
      }
      scoped_refptr<BookmarkStorage> storage(new BookmarkStorage(
          browser->profile(),
          browser->profile()->GetBookmarkModel()));
      *success = storage->SerializeData(bookmarks_as_json);
    }
  }
}

void TestingAutomationProvider::WaitForBookmarkModelToLoad(
    int handle,
    IPC::Message* reply_message) {
  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    BookmarkModel* model = browser->profile()->GetBookmarkModel();
    if (model->IsLoaded()) {
      AutomationMsg_WaitForBookmarkModelToLoad::WriteReplyParams(
          reply_message, true);
      Send(reply_message);
    } else {
      // The observer will delete itself when done.
      new AutomationProviderBookmarkModelObserver(this, reply_message,
                                                  model);
    }
  }
}

void TestingAutomationProvider::AddBookmarkGroup(int handle,
                                                 int64 parent_id,
                                                 int index,
                                                 std::wstring title,
                                                 bool* success) {
  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    if (browser) {
      BookmarkModel* model = browser->profile()->GetBookmarkModel();
      if (!model->IsLoaded()) {
        *success = false;
        return;
      }
      const BookmarkNode* parent = model->GetNodeByID(parent_id);
      DCHECK(parent);
      if (parent) {
        const BookmarkNode* child = model->AddFolder(parent, index,
                                                     WideToUTF16Hack(title));
        DCHECK(child);
        if (child)
          *success = true;
      }
    }
  }
  *success = false;
}

void TestingAutomationProvider::AddBookmarkURL(int handle,
                                               int64 parent_id,
                                               int index,
                                               std::wstring title,
                                               const GURL& url,
                                               bool* success) {
  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    if (browser) {
      BookmarkModel* model = browser->profile()->GetBookmarkModel();
      if (!model->IsLoaded()) {
        *success = false;
        return;
      }
      const BookmarkNode* parent = model->GetNodeByID(parent_id);
      DCHECK(parent);
      if (parent) {
        const BookmarkNode* child = model->AddURL(parent, index,
                                                  WideToUTF16Hack(title), url);
        DCHECK(child);
        if (child)
          *success = true;
      }
    }
  }
  *success = false;
}

void TestingAutomationProvider::ReparentBookmark(int handle,
                                                 int64 id,
                                                 int64 new_parent_id,
                                                 int index,
                                                 bool* success) {
  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    if (browser) {
      BookmarkModel* model = browser->profile()->GetBookmarkModel();
      if (!model->IsLoaded()) {
        *success = false;
        return;
      }
      const BookmarkNode* node = model->GetNodeByID(id);
      DCHECK(node);
      const BookmarkNode* new_parent = model->GetNodeByID(new_parent_id);
      DCHECK(new_parent);
      if (node && new_parent) {
        model->Move(node, new_parent, index);
        *success = true;
      }
    }
  }
  *success = false;
}

void TestingAutomationProvider::SetBookmarkTitle(int handle,
                                                 int64 id,
                                                 std::wstring title,
                                                 bool* success) {
  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    if (browser) {
      BookmarkModel* model = browser->profile()->GetBookmarkModel();
      if (!model->IsLoaded()) {
        *success = false;
        return;
      }
      const BookmarkNode* node = model->GetNodeByID(id);
      DCHECK(node);
      if (node) {
        model->SetTitle(node, WideToUTF16Hack(title));
        *success = true;
      }
    }
  }
  *success = false;
}

void TestingAutomationProvider::SetBookmarkURL(int handle,
                                               int64 id,
                                               const GURL& url,
                                               bool* success) {
  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    if (browser) {
      BookmarkModel* model = browser->profile()->GetBookmarkModel();
      if (!model->IsLoaded()) {
        *success = false;
        return;
      }
      const BookmarkNode* node = model->GetNodeByID(id);
      DCHECK(node);
      if (node) {
        model->SetURL(node, url);
        *success = true;
      }
    }
  }
  *success = false;
}

void TestingAutomationProvider::RemoveBookmark(int handle,
                                               int64 id,
                                               bool* success) {
  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    if (browser) {
      BookmarkModel* model = browser->profile()->GetBookmarkModel();
      if (!model->IsLoaded()) {
        *success = false;
        return;
      }
      const BookmarkNode* node = model->GetNodeByID(id);
      DCHECK(node);
      if (node) {
        const BookmarkNode* parent = node->parent();
        DCHECK(parent);
        model->Remove(parent, parent->GetIndexOf(node));
        *success = true;
      }
    }
  }
  *success = false;
}

void TestingAutomationProvider::WaitForBrowserWindowCountToBecome(
    int target_count,
    IPC::Message* reply_message) {
  if (static_cast<int>(BrowserList::size()) == target_count) {
    AutomationMsg_WaitForBrowserWindowCountToBecome::WriteReplyParams(
        reply_message, true);
    Send(reply_message);
    return;
  }

  // Set up an observer (it will delete itself).
  new BrowserCountChangeNotificationObserver(target_count, this, reply_message);
}

void TestingAutomationProvider::GoBackBlockUntilNavigationsComplete(
    int handle, int number_of_navigations, IPC::Message* reply_message) {
  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* tab = tab_tracker_->GetResource(handle);
    Browser* browser = FindAndActivateTab(tab);
    if (browser && browser->command_updater()->IsCommandEnabled(IDC_BACK)) {
      new NavigationNotificationObserver(tab, this, reply_message,
                                         number_of_navigations, false, false);
      browser->GoBack(CURRENT_TAB);
      return;
    }
  }

  AutomationMsg_GoBackBlockUntilNavigationsComplete::WriteReplyParams(
      reply_message, AUTOMATION_MSG_NAVIGATION_ERROR);
  Send(reply_message);
}

void TestingAutomationProvider::GoForwardBlockUntilNavigationsComplete(
    int handle, int number_of_navigations, IPC::Message* reply_message) {
  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* tab = tab_tracker_->GetResource(handle);
    Browser* browser = FindAndActivateTab(tab);
    if (browser && browser->command_updater()->IsCommandEnabled(IDC_FORWARD)) {
      new NavigationNotificationObserver(tab, this, reply_message,
                                         number_of_navigations, false, false);
      browser->GoForward(CURRENT_TAB);
      return;
    }
  }

  AutomationMsg_GoForwardBlockUntilNavigationsComplete::WriteReplyParams(
      reply_message, AUTOMATION_MSG_NAVIGATION_ERROR);
  Send(reply_message);
}

void TestingAutomationProvider::SetShelfVisibility(int handle, bool visible) {
  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    if (browser) {
      if (visible)
        browser->window()->GetDownloadShelf()->Show();
      else
        browser->window()->GetDownloadShelf()->Close();
    }
  }
}

void TestingAutomationProvider::SendJSONRequest(int handle,
                                                const std::string& json_request,
                                                IPC::Message* reply_message) {
  std::string error;
  scoped_ptr<Value> values(base::JSONReader::ReadAndReturnError(json_request,
      base::JSON_ALLOW_TRAILING_COMMAS, NULL, &error));
  if (!error.empty()) {
    AutomationJSONReply(this, reply_message).SendError(error);
    return;
  }

  // Make sure input is a dict with a string command.
  std::string command;
  DictionaryValue* dict_value = NULL;
  if (values->GetType() != Value::TYPE_DICTIONARY) {
    AutomationJSONReply(this, reply_message).SendError("not a dict");
    return;
  }
  // Ownership remains with "values" variable.
  dict_value = static_cast<DictionaryValue*>(values.get());
  if (!dict_value->GetStringASCII(std::string("command"), &command)) {
    AutomationJSONReply(this, reply_message)
        .SendError("no command key in dict or not a string command");
    return;
  }

  // Map json commands to their handlers.
  std::map<std::string, JsonHandler> handler_map;
  handler_map["WaitForAllTabsToStopLoading"] =
      &TestingAutomationProvider::WaitForAllViewsToStopLoading;
  handler_map["GetIndicesFromTab"] =
      &TestingAutomationProvider::GetIndicesFromTab;
  handler_map["NavigateToURL"] =
      &TestingAutomationProvider::NavigateToURL;
  handler_map["WaitUntilNavigationCompletes"] =
      &TestingAutomationProvider::WaitUntilNavigationCompletes;
  handler_map["GetLocalStatePrefsInfo"] =
      &TestingAutomationProvider::GetLocalStatePrefsInfo;
  handler_map["SetLocalStatePrefs"] =
      &TestingAutomationProvider::SetLocalStatePrefs;
  handler_map["GetPrefsInfo"] = &TestingAutomationProvider::GetPrefsInfo;
  handler_map["SetPrefs"] = &TestingAutomationProvider::SetPrefs;
  handler_map["ExecuteJavascript"] =
      &TestingAutomationProvider::ExecuteJavascriptJSON;
  handler_map["AddDomEventObserver"] =
      &TestingAutomationProvider::AddDomEventObserver;
  handler_map["RemoveEventObserver"] =
      &TestingAutomationProvider::RemoveEventObserver;
  handler_map["GetNextEvent"] =
      &TestingAutomationProvider::GetNextEvent;
  handler_map["ClearEventQueue"] =
      &TestingAutomationProvider::ClearEventQueue;
  handler_map["ExecuteJavascriptInRenderView"] =
      &TestingAutomationProvider::ExecuteJavascriptInRenderView;
  handler_map["GoForward"] =
      &TestingAutomationProvider::GoForward;
  handler_map["GoBack"] =
      &TestingAutomationProvider::GoBack;
  handler_map["Reload"] =
      &TestingAutomationProvider::ReloadJSON;
  handler_map["CaptureEntirePage"] =
      &TestingAutomationProvider::CaptureEntirePageJSON;
  handler_map["GetCookies"] =
      &TestingAutomationProvider::GetCookiesJSON;
  handler_map["DeleteCookie"] =
      &TestingAutomationProvider::DeleteCookieJSON;
  handler_map["SetCookie"] =
      &TestingAutomationProvider::SetCookieJSON;
  handler_map["GetTabIds"] =
      &TestingAutomationProvider::GetTabIds;
  handler_map["GetViews"] =
      &TestingAutomationProvider::GetViews;
  handler_map["IsTabIdValid"] =
      &TestingAutomationProvider::IsTabIdValid;
  handler_map["DoesAutomationObjectExist"] =
      &TestingAutomationProvider::DoesAutomationObjectExist;
  handler_map["CloseTab"] =
      &TestingAutomationProvider::CloseTabJSON;
  handler_map["SetViewBounds"] =
      &TestingAutomationProvider::SetViewBounds;
  handler_map["WebkitMouseMove"] =
      &TestingAutomationProvider::WebkitMouseMove;
  handler_map["WebkitMouseClick"] =
      &TestingAutomationProvider::WebkitMouseClick;
  handler_map["WebkitMouseDrag"] =
      &TestingAutomationProvider::WebkitMouseDrag;
  handler_map["WebkitMouseButtonUp"] =
      &TestingAutomationProvider::WebkitMouseButtonUp;
  handler_map["WebkitMouseButtonDown"] =
      &TestingAutomationProvider::WebkitMouseButtonDown;
  handler_map["WebkitMouseDoubleClick"] =
      &TestingAutomationProvider::WebkitMouseDoubleClick;
  handler_map["DragAndDropFilePaths"] =
      &TestingAutomationProvider::DragAndDropFilePaths;
  handler_map["SendWebkitKeyEvent"] =
      &TestingAutomationProvider::SendWebkitKeyEvent;
  handler_map["SendOSLevelKeyEventToTab"] =
      &TestingAutomationProvider::SendOSLevelKeyEventToTab;
  handler_map["ProcessWebMouseEvent"] =
      &TestingAutomationProvider::ProcessWebMouseEvent;
  handler_map["ActivateTab"] =
      &TestingAutomationProvider::ActivateTabJSON;
  handler_map["GetAppModalDialogMessage"] =
      &TestingAutomationProvider::GetAppModalDialogMessage;
  handler_map["AcceptOrDismissAppModalDialog"] =
      &TestingAutomationProvider::AcceptOrDismissAppModalDialog;
  handler_map["GetChromeDriverAutomationVersion"] =
      &TestingAutomationProvider::GetChromeDriverAutomationVersion;
  handler_map["IsPageActionVisible"] =
      &TestingAutomationProvider::IsPageActionVisible;
  handler_map["CreateNewAutomationProvider"] =
      &TestingAutomationProvider::CreateNewAutomationProvider;
  handler_map["GetBrowserInfo"] =
      &TestingAutomationProvider::GetBrowserInfo;
  handler_map["OpenNewBrowserWindowWithNewProfile"] =
      &TestingAutomationProvider::OpenNewBrowserWindowWithNewProfile;
  handler_map["GetMultiProfileInfo"] =
      &TestingAutomationProvider::GetMultiProfileInfo;
  handler_map["GetProcessInfo"] =
      &TestingAutomationProvider::GetProcessInfo;
  handler_map["GetPolicyDefinitionList"] =
      &TestingAutomationProvider::GetPolicyDefinitionList;
  handler_map["RefreshPolicies"] =
      &TestingAutomationProvider::RefreshPolicies;
  handler_map["InstallExtension"] =
      &TestingAutomationProvider::InstallExtension;
  handler_map["GetExtensionsInfo"] =
      &TestingAutomationProvider::GetExtensionsInfo;
  handler_map["UninstallExtensionById"] =
      &TestingAutomationProvider::UninstallExtensionById;
  handler_map["SetExtensionStateById"] =
      &TestingAutomationProvider::SetExtensionStateById;
  handler_map["TriggerPageActionById"] =
      &TestingAutomationProvider::TriggerPageActionById;
  handler_map["TriggerBrowserActionById"] =
      &TestingAutomationProvider::TriggerBrowserActionById;
  handler_map["UpdateExtensionsNow"] =
      &TestingAutomationProvider::UpdateExtensionsNow;
#if !defined(NO_TCMALLOC) && (defined(OS_LINUX) || defined(OS_CHROMEOS))
  handler_map["HeapProfilerDump"] =
      &TestingAutomationProvider::HeapProfilerDump;
#endif  // !defined(NO_TCMALLOC) && (defined(OS_LINUX) || defined(OS_CHROMEOS))
  handler_map["OverrideGeoposition"] =
      &TestingAutomationProvider::OverrideGeoposition;
  handler_map["AppendSwitchASCIIToCommandLine"] =
      &TestingAutomationProvider::AppendSwitchASCIIToCommandLine;
  handler_map["SimulateAsanMemoryBug"] =
      &TestingAutomationProvider::SimulateAsanMemoryBug;

#if defined(OS_CHROMEOS)
  handler_map["GetLoginInfo"] = &TestingAutomationProvider::GetLoginInfo;
  handler_map["ShowCreateAccountUI"] =
      &TestingAutomationProvider::ShowCreateAccountUI;
  handler_map["ExecuteJavascriptInOOBEWebUI"] =
      &TestingAutomationProvider::ExecuteJavascriptInOOBEWebUI;
  handler_map["LoginAsGuest"] = &TestingAutomationProvider::LoginAsGuest;
  handler_map["AddLoginEventObserver"] =
      &TestingAutomationProvider::AddLoginEventObserver;
  handler_map["SignOut"] = &TestingAutomationProvider::SignOut;

  handler_map["LockScreen"] = &TestingAutomationProvider::LockScreen;
  handler_map["UnlockScreen"] = &TestingAutomationProvider::UnlockScreen;
  handler_map["SignoutInScreenLocker"] =
      &TestingAutomationProvider::SignoutInScreenLocker;

  handler_map["GetBatteryInfo"] = &TestingAutomationProvider::GetBatteryInfo;

  handler_map["GetNetworkInfo"] = &TestingAutomationProvider::GetNetworkInfo;
  handler_map["NetworkScan"] = &TestingAutomationProvider::NetworkScan;
  handler_map["ToggleNetworkDevice"] =
      &TestingAutomationProvider::ToggleNetworkDevice;
  handler_map["ConnectToCellularNetwork"] =
      &TestingAutomationProvider::ConnectToCellularNetwork;
  handler_map["DisconnectFromCellularNetwork"] =
      &TestingAutomationProvider::DisconnectFromCellularNetwork;
  handler_map["ConnectToWifiNetwork"] =
      &TestingAutomationProvider::ConnectToWifiNetwork;
  handler_map["ConnectToHiddenWifiNetwork"] =
      &TestingAutomationProvider::ConnectToHiddenWifiNetwork;
  handler_map["DisconnectFromWifiNetwork"] =
      &TestingAutomationProvider::DisconnectFromWifiNetwork;
  handler_map["ForgetWifiNetwork"] =
      &TestingAutomationProvider::ForgetWifiNetwork;

  handler_map["AddPrivateNetwork"] =
      &TestingAutomationProvider::AddPrivateNetwork;
  handler_map["GetPrivateNetworkInfo"] =
      &TestingAutomationProvider::GetPrivateNetworkInfo;
  handler_map["ConnectToPrivateNetwork"] =
      &TestingAutomationProvider::ConnectToPrivateNetwork;
  handler_map["DisconnectFromPrivateNetwork"] =
      &TestingAutomationProvider::DisconnectFromPrivateNetwork;

  handler_map["IsEnterpriseDevice"] =
      &TestingAutomationProvider::IsEnterpriseDevice;
  handler_map["GetEnterprisePolicyInfo"] =
      &TestingAutomationProvider::GetEnterprisePolicyInfo;
  handler_map["EnrollEnterpriseDevice"] =
      &TestingAutomationProvider::EnrollEnterpriseDevice;

  handler_map["EnableSpokenFeedback"] =
      &TestingAutomationProvider::EnableSpokenFeedback;
  handler_map["IsSpokenFeedbackEnabled"] =
      &TestingAutomationProvider::IsSpokenFeedbackEnabled;

  handler_map["GetTimeInfo"] = &TestingAutomationProvider::GetTimeInfo;
  handler_map["SetTimezone"] = &TestingAutomationProvider::SetTimezone;

  handler_map["GetUpdateInfo"] = &TestingAutomationProvider::GetUpdateInfo;
  handler_map["UpdateCheck"] = &TestingAutomationProvider::UpdateCheck;
  handler_map["SetReleaseTrack"] = &TestingAutomationProvider::SetReleaseTrack;

  handler_map["GetVolumeInfo"] = &TestingAutomationProvider::GetVolumeInfo;
  handler_map["SetVolume"] = &TestingAutomationProvider::SetVolume;
  handler_map["SetMute"] = &TestingAutomationProvider::SetMute;

#endif  // defined(OS_CHROMEOS)

  std::map<std::string, BrowserJsonHandler> browser_handler_map;
  browser_handler_map["DisablePlugin"] =
      &TestingAutomationProvider::DisablePlugin;
  browser_handler_map["EnablePlugin"] =
      &TestingAutomationProvider::EnablePlugin;
  browser_handler_map["GetPluginsInfo"] =
      &TestingAutomationProvider::GetPluginsInfo;

  browser_handler_map["GetNavigationInfo"] =
      &TestingAutomationProvider::GetNavigationInfo;

  browser_handler_map["PerformActionOnInfobar"] =
      &TestingAutomationProvider::PerformActionOnInfobar;

  browser_handler_map["GetHistoryInfo"] =
      &TestingAutomationProvider::GetHistoryInfo;
  browser_handler_map["AddHistoryItem"] =
      &TestingAutomationProvider::AddHistoryItem;

  browser_handler_map["GetOmniboxInfo"] =
      &TestingAutomationProvider::GetOmniboxInfo;
  browser_handler_map["SetOmniboxText"] =
      &TestingAutomationProvider::SetOmniboxText;
  browser_handler_map["OmniboxAcceptInput"] =
      &TestingAutomationProvider::OmniboxAcceptInput;
  browser_handler_map["OmniboxMovePopupSelection"] =
      &TestingAutomationProvider::OmniboxMovePopupSelection;

  browser_handler_map["GetInstantInfo"] =
      &TestingAutomationProvider::GetInstantInfo;

  browser_handler_map["LoadSearchEngineInfo"] =
      &TestingAutomationProvider::LoadSearchEngineInfo;
  browser_handler_map["GetSearchEngineInfo"] =
      &TestingAutomationProvider::GetSearchEngineInfo;
  browser_handler_map["AddOrEditSearchEngine"] =
      &TestingAutomationProvider::AddOrEditSearchEngine;
  browser_handler_map["PerformActionOnSearchEngine"] =
      &TestingAutomationProvider::PerformActionOnSearchEngine;

#if defined(ENABLE_PROTECTOR_SERVICE)
  browser_handler_map["GetProtectorState"] =
      &TestingAutomationProvider::GetProtectorState;
  browser_handler_map["PerformProtectorAction"] =
      &TestingAutomationProvider::PerformProtectorAction;
#endif

  browser_handler_map["SetWindowDimensions"] =
      &TestingAutomationProvider::SetWindowDimensions;

  browser_handler_map["GetDownloadsInfo"] =
      &TestingAutomationProvider::GetDownloadsInfo;
  browser_handler_map["WaitForAllDownloadsToComplete"] =
      &TestingAutomationProvider::WaitForAllDownloadsToComplete;
  browser_handler_map["PerformActionOnDownload"] =
      &TestingAutomationProvider::PerformActionOnDownload;

  browser_handler_map["GetInitialLoadTimes"] =
      &TestingAutomationProvider::GetInitialLoadTimes;

  browser_handler_map["SaveTabContents"] =
      &TestingAutomationProvider::SaveTabContents;

  browser_handler_map["ImportSettings"] =
      &TestingAutomationProvider::ImportSettings;

  browser_handler_map["AddSavedPassword"] =
      &TestingAutomationProvider::AddSavedPassword;
  browser_handler_map["RemoveSavedPassword"] =
      &TestingAutomationProvider::RemoveSavedPassword;
  browser_handler_map["GetSavedPasswords"] =
      &TestingAutomationProvider::GetSavedPasswords;

  browser_handler_map["ClearBrowsingData"] =
      &TestingAutomationProvider::ClearBrowsingData;

  browser_handler_map["GetBlockedPopupsInfo"] =
      &TestingAutomationProvider::GetBlockedPopupsInfo;
  browser_handler_map["UnblockAndLaunchBlockedPopup"] =
      &TestingAutomationProvider::UnblockAndLaunchBlockedPopup;

  // SetTheme() implemented using InstallExtension().
  browser_handler_map["GetThemeInfo"] =
      &TestingAutomationProvider::GetThemeInfo;

  browser_handler_map["FindInPage"] = &TestingAutomationProvider::FindInPage;

  browser_handler_map["SelectTranslateOption"] =
      &TestingAutomationProvider::SelectTranslateOption;
  browser_handler_map["GetTranslateInfo"] =
      &TestingAutomationProvider::GetTranslateInfo;

  browser_handler_map["GetAutofillProfile"] =
      &TestingAutomationProvider::GetAutofillProfile;
  browser_handler_map["FillAutofillProfile"] =
      &TestingAutomationProvider::FillAutofillProfile;
  browser_handler_map["SubmitAutofillForm"] =
      &TestingAutomationProvider::SubmitAutofillForm;
  browser_handler_map["AutofillTriggerSuggestions"] =
      &TestingAutomationProvider::AutofillTriggerSuggestions;
  browser_handler_map["AutofillHighlightSuggestion"] =
      &TestingAutomationProvider::AutofillHighlightSuggestion;
  browser_handler_map["AutofillAcceptSelection"] =
      &TestingAutomationProvider::AutofillAcceptSelection;

  browser_handler_map["GetAllNotifications"] =
      &TestingAutomationProvider::GetAllNotifications;
  browser_handler_map["CloseNotification"] =
      &TestingAutomationProvider::CloseNotification;
  browser_handler_map["WaitForNotificationCount"] =
      &TestingAutomationProvider::WaitForNotificationCount;

  browser_handler_map["SignInToSync"] =
      &TestingAutomationProvider::SignInToSync;
  browser_handler_map["GetSyncInfo"] = &TestingAutomationProvider::GetSyncInfo;
  browser_handler_map["AwaitFullSyncCompletion"] =
      &TestingAutomationProvider::AwaitFullSyncCompletion;
  browser_handler_map["AwaitSyncRestart"] =
      &TestingAutomationProvider::AwaitSyncRestart;
  browser_handler_map["EnableSyncForDatatypes"] =
      &TestingAutomationProvider::EnableSyncForDatatypes;
  browser_handler_map["DisableSyncForDatatypes"] =
      &TestingAutomationProvider::DisableSyncForDatatypes;

  browser_handler_map["GetNTPInfo"] =
      &TestingAutomationProvider::GetNTPInfo;
  browser_handler_map["RemoveNTPMostVisitedThumbnail"] =
      &TestingAutomationProvider::RemoveNTPMostVisitedThumbnail;
  browser_handler_map["RestoreAllNTPMostVisitedThumbnails"] =
      &TestingAutomationProvider::RestoreAllNTPMostVisitedThumbnails;

  browser_handler_map["KillRendererProcess"] =
      &TestingAutomationProvider::KillRendererProcess;

  browser_handler_map["LaunchApp"] = &TestingAutomationProvider::LaunchApp;
  browser_handler_map["SetAppLaunchType"] =
      &TestingAutomationProvider::SetAppLaunchType;

  browser_handler_map["GetV8HeapStats"] =
      &TestingAutomationProvider::GetV8HeapStats;
  browser_handler_map["GetFPS"] =
      &TestingAutomationProvider::GetFPS;

  browser_handler_map["IsFullscreenForBrowser"] =
      &TestingAutomationProvider::IsFullscreenForBrowser;
  browser_handler_map["IsFullscreenForTab"] =
      &TestingAutomationProvider::IsFullscreenForTab;
  browser_handler_map["IsMouseLocked"] =
      &TestingAutomationProvider::IsMouseLocked;
  browser_handler_map["IsMouseLockPermissionRequested"] =
      &TestingAutomationProvider::IsMouseLockPermissionRequested;
  browser_handler_map["IsFullscreenPermissionRequested"] =
      &TestingAutomationProvider::IsFullscreenPermissionRequested;
  browser_handler_map["IsFullscreenBubbleDisplayed"] =
      &TestingAutomationProvider::IsFullscreenBubbleDisplayed;
  browser_handler_map["IsFullscreenBubbleDisplayingButtons"] =
      &TestingAutomationProvider::IsFullscreenBubbleDisplayingButtons;
  browser_handler_map["AcceptCurrentFullscreenOrMouseLockRequest"] =
      &TestingAutomationProvider::AcceptCurrentFullscreenOrMouseLockRequest;
  browser_handler_map["DenyCurrentFullscreenOrMouseLockRequest"] =
      &TestingAutomationProvider::DenyCurrentFullscreenOrMouseLockRequest;

#if defined(OS_CHROMEOS)
  browser_handler_map["CaptureProfilePhoto"] =
      &TestingAutomationProvider::CaptureProfilePhoto;
  browser_handler_map["GetTimeInfo"] = &TestingAutomationProvider::GetTimeInfo;
  browser_handler_map["GetProxySettings"] =
      &TestingAutomationProvider::GetProxySettings;
  browser_handler_map["SetProxySettings"] =
      &TestingAutomationProvider::SetProxySettings;
#endif  // defined(OS_CHROMEOS)

  // Look for command in handlers that take a Browser handle.
  if (browser_handler_map.find(std::string(command)) !=
             browser_handler_map.end()) {
    Browser* browser = NULL;
    // Get Browser object associated with handle.
    if (!browser_tracker_->ContainsHandle(handle) ||
        !(browser = browser_tracker_->GetResource(handle))) {
      // Browser not found; attempt to fallback to non-Browser handlers.
      if (handler_map.find(std::string(command)) != handler_map.end())
        (this->*handler_map[command])(dict_value, reply_message);
      else
        AutomationJSONReply(this, reply_message).SendError(
            "No browser object.");
    } else {
      (this->*browser_handler_map[command])(browser, dict_value, reply_message);
    }
  // Look for command in handlers that don't take a Browser handle.
  } else if (handler_map.find(std::string(command)) != handler_map.end()) {
    (this->*handler_map[command])(dict_value, reply_message);
  // Command has no handlers for it.
  } else {
    std::string error_string = "Unknown command. Options: ";
    for (std::map<std::string, JsonHandler>::const_iterator it =
         handler_map.begin(); it != handler_map.end(); ++it) {
      error_string += it->first + ", ";
    }
    for (std::map<std::string, BrowserJsonHandler>::const_iterator it =
         browser_handler_map.begin(); it != browser_handler_map.end(); ++it) {
      error_string += it->first + ", ";
    }
    AutomationJSONReply(this, reply_message).SendError(error_string);
  }
}

// Sample json input: { "command": "SetWindowDimensions",
//                      "x": 20,         # optional
//                      "y": 20,         # optional
//                      "width": 800,    # optional
//                      "height": 600 }  # optional
void TestingAutomationProvider::SetWindowDimensions(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  gfx::Rect rect = browser->window()->GetRestoredBounds();
  int x, y, width, height;
  if (args->GetInteger("x", &x))
    rect.set_x(x);
  if (args->GetInteger("y", &y))
    rect.set_y(y);
  if (args->GetInteger("width", &width))
    rect.set_width(width);
  if (args->GetInteger("height", &height))
    rect.set_height(height);
  browser->window()->SetBounds(rect);
  AutomationJSONReply(this, reply_message).SendSuccess(NULL);
}

ListValue* TestingAutomationProvider::GetInfobarsInfo(WebContents* wc) {
  // Each infobar may have different properties depending on the type.
  ListValue* infobars = new ListValue;
  InfoBarTabHelper* infobar_helper =
      TabContentsWrapper::GetCurrentWrapperForContents(wc)->
          infobar_tab_helper();
  for (size_t i = 0; i < infobar_helper->infobar_count(); ++i) {
    DictionaryValue* infobar_item = new DictionaryValue;
    InfoBarDelegate* infobar = infobar_helper->GetInfoBarDelegateAt(i);
    switch (infobar->GetInfoBarAutomationType()) {
      case InfoBarDelegate::CONFIRM_INFOBAR:
        infobar_item->SetString("type", "confirm_infobar");
        break;
      case InfoBarDelegate::ONE_CLICK_LOGIN_INFOBAR:
        infobar_item->SetString("type", "oneclicklogin_infobar");
        break;
      case InfoBarDelegate::PASSWORD_INFOBAR:
        infobar_item->SetString("type", "password_infobar");
        break;
      case InfoBarDelegate::RPH_INFOBAR:
        infobar_item->SetString("type", "rph_infobar");
        break;
      case InfoBarDelegate::UNKNOWN_INFOBAR:
        infobar_item->SetString("type", "unknown_infobar");
        break;
    }
    if (infobar->AsConfirmInfoBarDelegate()) {
      // Also covers ThemeInstalledInfoBarDelegate.
      ConfirmInfoBarDelegate* confirm_infobar =
        infobar->AsConfirmInfoBarDelegate();
      infobar_item->SetString("text", confirm_infobar->GetMessageText());
      infobar_item->SetString("link_text", confirm_infobar->GetLinkText());
      ListValue* buttons_list = new ListValue;
      int buttons = confirm_infobar->GetButtons();
      if (buttons & ConfirmInfoBarDelegate::BUTTON_OK) {
        StringValue* button_label = new StringValue(
            confirm_infobar->GetButtonLabel(
              ConfirmInfoBarDelegate::BUTTON_OK));
        buttons_list->Append(button_label);
      }
      if (buttons & ConfirmInfoBarDelegate::BUTTON_CANCEL) {
        StringValue* button_label = new StringValue(
            confirm_infobar->GetButtonLabel(
              ConfirmInfoBarDelegate::BUTTON_CANCEL));
        buttons_list->Append(button_label);
      }
      infobar_item->Set("buttons", buttons_list);
    } else if (infobar->AsLinkInfoBarDelegate()) {
      infobar_item->SetString("type", "link_infobar");
      LinkInfoBarDelegate* link_infobar = infobar->AsLinkInfoBarDelegate();
      infobar_item->SetString("link_text", link_infobar->GetLinkText());
    } else if (infobar->AsTranslateInfoBarDelegate()) {
      infobar_item->SetString("type", "translate_infobar");
      TranslateInfoBarDelegate* translate_infobar =
          infobar->AsTranslateInfoBarDelegate();
      infobar_item->SetString("original_lang_code",
                              translate_infobar->GetOriginalLanguageCode());
      infobar_item->SetString("target_lang_code",
                              translate_infobar->GetTargetLanguageCode());
    } else if (infobar->AsExtensionInfoBarDelegate()) {
      infobar_item->SetString("type", "extension_infobar");
    } else {
      infobar_item->SetString("type", "unknown_infobar");
    }
    infobars->Append(infobar_item);
  }
  return infobars;
}

// Sample json input: { "command": "PerformActionOnInfobar",
//                      "action": "dismiss",
//                      "infobar_index": 0,
//                      "tab_index": 0 }
// Sample output: {}
void TestingAutomationProvider::PerformActionOnInfobar(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);
  int tab_index;
  int infobar_index_int;
  std::string action;
  if (!args->GetInteger("tab_index", &tab_index) ||
      !args->GetInteger("infobar_index", &infobar_index_int) ||
      !args->GetString("action", &action)) {
    reply.SendError("Invalid or missing args");
    return;
  }

  TabContentsWrapper* tab_contents =
      browser->GetTabContentsWrapperAt(tab_index);
  if (!tab_contents) {
    reply.SendError(StringPrintf("No such tab at index %d", tab_index));
    return;
  }
  InfoBarTabHelper* infobar_helper = tab_contents->infobar_tab_helper();

  InfoBarDelegate* infobar = NULL;
  size_t infobar_index = static_cast<size_t>(infobar_index_int);
  if (infobar_index >= infobar_helper->infobar_count()) {
    reply.SendError(StringPrintf("No such infobar at index %" PRIuS,
                                 infobar_index));
    return;
  }
  infobar = infobar_helper->GetInfoBarDelegateAt(infobar_index);

  if ("dismiss" == action) {
    infobar->InfoBarDismissed();
    infobar_helper->RemoveInfoBar(infobar);
    reply.SendSuccess(NULL);
    return;
  }
  if ("accept" == action || "cancel" == action) {
    ConfirmInfoBarDelegate* confirm_infobar;
    if (!(confirm_infobar = infobar->AsConfirmInfoBarDelegate())) {
      reply.SendError("Not a confirm infobar");
      return;
    }
    if ("accept" == action) {
      if (confirm_infobar->Accept())
        infobar_helper->RemoveInfoBar(infobar);
    } else if ("cancel" == action) {
      if (confirm_infobar->Cancel())
        infobar_helper->RemoveInfoBar(infobar);
    }
    reply.SendSuccess(NULL);
    return;
  }
  if ("allow" == action || "deny" == action) {
    MediaStreamInfoBarDelegate* media_stream_infobar;
    if (!(media_stream_infobar = infobar->AsMediaStreamInfoBarDelegate())) {
      reply.SendError("Not a media stream infobar.");
      return;
    }
    if ("allow" == action) {
      content::MediaStreamDevices video_devices =
          media_stream_infobar->GetVideoDevices();
      content::MediaStreamDevices audio_devices =
          media_stream_infobar->GetAudioDevices();
      if (video_devices.empty() || audio_devices.empty()) {
        reply.SendError("No available audio/video devices to autoselect.");
        return;
      }

      media_stream_infobar->Accept(audio_devices[0].device_id,
                                   video_devices[0].device_id);
      infobar_helper->RemoveInfoBar(infobar);
    } else if ("deny" == action) {
      media_stream_infobar->Deny();
      infobar_helper->RemoveInfoBar(infobar);
    }
    reply.SendSuccess(NULL);
    return;
  }
  reply.SendError("Invalid action");
}

namespace {

// Gets info about BrowserChildProcessHost. Must run on IO thread to
// honor the semantics of BrowserChildProcessHostIterator.
// Used by AutomationProvider::GetBrowserInfo().
void GetChildProcessHostInfo(ListValue* child_processes) {
  for (BrowserChildProcessHostIterator iter; !iter.Done(); ++iter) {
    // Only add processes which are already started, since we need their handle.
    if (iter.GetData().handle == base::kNullProcessHandle)
      continue;
    DictionaryValue* item = new DictionaryValue;
    item->SetString("name", iter.GetData().name);
    item->SetString("type",
                    content::GetProcessTypeNameInEnglish(iter.GetData().type));
    item->SetInteger("pid", base::GetProcId(iter.GetData().handle));
    child_processes->Append(item);
  }
}

}  // namespace

// Sample json input: { "command": "GetBrowserInfo" }
// Refer to GetBrowserInfo() in chrome/test/pyautolib/pyauto.py for
// sample json output.
void TestingAutomationProvider::GetBrowserInfo(
    DictionaryValue* args,
    IPC::Message* reply_message) {
  base::ThreadRestrictions::ScopedAllowIO allow_io;  // needed for PathService
  DictionaryValue* properties = new DictionaryValue;
  properties->SetString("ChromeVersion", chrome::kChromeVersion);
  properties->SetString("BrowserProcessExecutableName",
                        chrome::kBrowserProcessExecutableName);
  properties->SetString("HelperProcessExecutableName",
                        chrome::kHelperProcessExecutableName);
  properties->SetString("BrowserProcessExecutablePath",
                        chrome::kBrowserProcessExecutablePath);
  properties->SetString("HelperProcessExecutablePath",
                        chrome::kHelperProcessExecutablePath);
  properties->SetString("command_line_string",
      CommandLine::ForCurrentProcess()->GetCommandLineString());
  FilePath dumps_path;
  PathService::Get(chrome::DIR_CRASH_DUMPS, &dumps_path);
  properties->SetString("DIR_CRASH_DUMPS", dumps_path.value());
#if defined(USE_AURA)
  properties->SetBoolean("aura", true);
#else
  properties->SetBoolean("aura", false);
#endif

  std::string branding;
#if defined(GOOGLE_CHROME_BUILD)
  branding = "Google Chrome";
#elif defined(CHROMIUM_BUILD)
  branding = "Chromium";
#else
  branding = "Unknown Branding";
#endif
  properties->SetString("branding", branding);

  bool is_official_build = false;
#if defined(OFFICIAL_BUILD)
  is_official_build = true;
#endif
  properties->SetBoolean("is_official", is_official_build);

  scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
  return_value->Set("properties", properties);

  return_value->SetInteger("browser_pid", base::GetCurrentProcId());
  // Add info about all windows in a list of dictionaries, one dictionary
  // item per window.
  ListValue* windows = new ListValue;
  int windex = 0;
  for (BrowserList::const_iterator it = BrowserList::begin();
       it != BrowserList::end();
       ++it, ++windex) {
    DictionaryValue* browser_item = new DictionaryValue;
    Browser* browser = *it;
    browser_item->SetInteger("index", windex);
    // Window properties
    gfx::Rect rect = browser->window()->GetRestoredBounds();
    browser_item->SetInteger("x", rect.x());
    browser_item->SetInteger("y", rect.y());
    browser_item->SetInteger("width", rect.width());
    browser_item->SetInteger("height", rect.height());
    browser_item->SetBoolean("fullscreen",
                             browser->window()->IsFullscreen());
    ListValue* visible_page_actions = new ListValue;
    // Add info about all visible page actions. Skipped on panels, which do not
    // have a location bar.
    LocationBar* loc_bar = browser->window()->GetLocationBar();
    if (loc_bar) {
      LocationBarTesting* loc_bar_test =
          loc_bar->GetLocationBarForTesting();
      size_t page_action_visible_count =
          static_cast<size_t>(loc_bar_test->PageActionVisibleCount());
      for (size_t i = 0; i < page_action_visible_count; ++i) {
        StringValue* extension_id = new StringValue(
            loc_bar_test->GetVisiblePageAction(i)->extension_id());
        visible_page_actions->Append(extension_id);
      }
    }
    browser_item->Set("visible_page_actions", visible_page_actions);
    browser_item->SetInteger("selected_tab", browser->active_index());
    browser_item->SetBoolean("incognito",
                             browser->profile()->IsOffTheRecord());
    browser_item->SetString("profile_path",
        browser->profile()->GetPath().BaseName().MaybeAsASCII());
    std::string type;
    switch (browser->type()) {
      case Browser::TYPE_TABBED:
        type = "tabbed";
        break;
      case Browser::TYPE_POPUP:
        type = "popup";
        break;
      case Browser::TYPE_PANEL:
        type = "panel";
        break;
      default:
        type = "unknown";
        break;
    }
    browser_item->SetString("type", type);
    // For each window, add info about all tabs in a list of dictionaries,
    // one dictionary item per tab.
    ListValue* tabs = new ListValue;
    for (int i = 0; i < browser->tab_count(); ++i) {
      WebContents* wc = browser->GetWebContentsAt(i);
      DictionaryValue* tab = new DictionaryValue;
      tab->SetInteger("index", i);
      tab->SetString("url", wc->GetURL().spec());
      tab->SetInteger("renderer_pid",
                      base::GetProcId(wc->GetRenderProcessHost()->GetHandle()));
      tab->Set("infobars", GetInfobarsInfo(wc));
      tab->SetBoolean("pinned", browser->IsTabPinned(i));
      tabs->Append(tab);
    }
    browser_item->Set("tabs", tabs);

    windows->Append(browser_item);
  }
  return_value->Set("windows", windows);

#if defined(OS_LINUX)
  int flags = ChildProcessHost::CHILD_ALLOW_SELF;
#else
  int flags = ChildProcessHost::CHILD_NORMAL;
#endif

  // Add all extension processes in a list of dictionaries, one dictionary
  // item per extension process.
  ListValue* extension_views = new ListValue;
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  std::vector<Profile*> profiles(profile_manager->GetLoadedProfiles());
  for (size_t i = 0; i < profiles.size(); ++i) {
    ExtensionProcessManager* process_manager =
        profiles[i]->GetExtensionProcessManager();
    if (!process_manager)
      continue;
    const ExtensionProcessManager::ViewSet view_set =
        process_manager->GetAllViews();
    for (ExtensionProcessManager::ViewSet::const_iterator jt =
             view_set.begin();
         jt != view_set.end(); ++jt) {
      content::RenderViewHost* render_view_host = *jt;
      // Don't add dead extension processes.
      if (!render_view_host->IsRenderViewLive())
        continue;
      // Don't add views for which we can't obtain an extension.
      // TODO(benwells): work out why this happens. It only happens for one
      // test, and only on the bots.
      const Extension* extension =
          process_manager->GetExtensionForRenderViewHost(render_view_host);
      if (!extension)
        continue;
      DictionaryValue* item = new DictionaryValue;
      item->SetString("name", extension->name());
      item->SetString("extension_id", extension->id());
      item->SetInteger(
          "pid",
          base::GetProcId(render_view_host->GetProcess()->GetHandle()));
      DictionaryValue* view = new DictionaryValue;
      view->SetInteger(
          "render_process_id",
          render_view_host->GetProcess()->GetID());
      view->SetInteger(
          "render_view_id",
          render_view_host->GetRoutingID());
      item->Set("view", view);
      std::string type;
      switch (render_view_host->GetDelegate()->GetRenderViewType()) {
        case chrome::VIEW_TYPE_EXTENSION_BACKGROUND_PAGE:
          type = "EXTENSION_BACKGROUND_PAGE";
          break;
        case chrome::VIEW_TYPE_EXTENSION_POPUP:
          type = "EXTENSION_POPUP";
          break;
        case chrome::VIEW_TYPE_EXTENSION_INFOBAR:
          type = "EXTENSION_INFOBAR";
          break;
        case chrome::VIEW_TYPE_EXTENSION_DIALOG:
          type = "EXTENSION_DIALOG";
          break;
        case chrome::VIEW_TYPE_APP_SHELL:
          type = "APP_SHELL";
          break;
        case chrome::VIEW_TYPE_PANEL:
          type = "PANEL";
          break;
        default:
          type = "unknown";
          break;
      }
      item->SetString("view_type", type);
      item->SetString("url", render_view_host->GetDelegate()->GetURL().spec());
      item->SetBoolean("loaded", !render_view_host->IsLoading());
      extension_views->Append(item);
    }
  }
  return_value->Set("extension_views", extension_views);

  return_value->SetString("child_process_path",
                          ChildProcessHost::GetChildPath(flags).value());
  // Child processes are the processes for plugins and other workers.
  // Add all child processes in a list of dictionaries, one dictionary item
  // per child process.
  ListValue* child_processes = new ListValue;
  return_value->Set("child_processes", child_processes);
  BrowserThread::PostTaskAndReply(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&GetChildProcessHostInfo, child_processes),
      base::Bind(&AutomationJSONReply::SendSuccess,
                 base::Owned(new AutomationJSONReply(this, reply_message)),
                 base::Owned(return_value.release())));
}

// Sample json input: { "command": "GetProcessInfo" }
// Refer to GetProcessInfo() in chrome/test/pyautolib/pyauto.py for
// sample json output.
void TestingAutomationProvider::GetProcessInfo(
    DictionaryValue* args,
    IPC::Message* reply_message) {
  scoped_refptr<ProcessInfoObserver>
      proc_observer(new ProcessInfoObserver(this, reply_message));
  // TODO(jamescook): Maybe this shouldn't update UMA stats?
  proc_observer->StartFetch(MemoryDetails::UPDATE_USER_METRICS);
}

// Sample json input: { "command": "GetNavigationInfo" }
// Refer to GetNavigationInfo() in chrome/test/pyautolib/pyauto.py for
// sample json output.
void TestingAutomationProvider::GetNavigationInfo(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);
  int tab_index;
  WebContents* web_contents = NULL;
  if (!args->GetInteger("tab_index", &tab_index) ||
      !(web_contents = browser->GetWebContentsAt(tab_index))) {
    reply.SendError("tab_index missing or invalid.");
    return;
  }
  scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
  const NavigationController& controller = web_contents->GetController();
  NavigationEntry* nav_entry = controller.GetActiveEntry();
  DCHECK(nav_entry);

  // Security info.
  DictionaryValue* ssl = new DictionaryValue;
  std::map<content::SecurityStyle, std::string> style_to_string;
  style_to_string[content::SECURITY_STYLE_UNKNOWN] = "SECURITY_STYLE_UNKNOWN";
  style_to_string[content::SECURITY_STYLE_UNAUTHENTICATED] =
      "SECURITY_STYLE_UNAUTHENTICATED";
  style_to_string[content::SECURITY_STYLE_AUTHENTICATION_BROKEN] =
      "SECURITY_STYLE_AUTHENTICATION_BROKEN";
  style_to_string[content::SECURITY_STYLE_AUTHENTICATED] =
      "SECURITY_STYLE_AUTHENTICATED";

  SSLStatus ssl_status = nav_entry->GetSSL();
  ssl->SetString("security_style",
                 style_to_string[ssl_status.security_style]);
  ssl->SetBoolean("ran_insecure_content",
      !!(ssl_status.content_status & SSLStatus::RAN_INSECURE_CONTENT));
  ssl->SetBoolean("displayed_insecure_content",
      !!(ssl_status.content_status & SSLStatus::DISPLAYED_INSECURE_CONTENT));
  return_value->Set("ssl", ssl);

  // Page type.
  std::map<content::PageType, std::string> pagetype_to_string;
  pagetype_to_string[content::PAGE_TYPE_NORMAL] = "NORMAL_PAGE";
  pagetype_to_string[content::PAGE_TYPE_ERROR] = "ERROR_PAGE";
  pagetype_to_string[content::PAGE_TYPE_INTERSTITIAL] =
      "INTERSTITIAL_PAGE";
  return_value->SetString("page_type",
                          pagetype_to_string[nav_entry->GetPageType()]);

  return_value->SetString("favicon_url", nav_entry->GetFavicon().url.spec());
  reply.SendSuccess(return_value.get());
}

// Sample json input: { "command": "GetHistoryInfo",
//                      "search_text": "some text" }
// Refer chrome/test/pyautolib/history_info.py for sample json output.
void TestingAutomationProvider::GetHistoryInfo(Browser* browser,
                                               DictionaryValue* args,
                                               IPC::Message* reply_message) {
  consumer_.CancelAllRequests();

  string16 search_text;
  args->GetString("search_text", &search_text);

  // Fetch history.
  HistoryService* hs = browser->profile()->GetHistoryService(
      Profile::EXPLICIT_ACCESS);
  history::QueryOptions options;
  // The observer owns itself.  It deletes itself after it fetches history.
  AutomationProviderHistoryObserver* history_observer =
      new AutomationProviderHistoryObserver(this, reply_message);
  hs->QueryHistory(
      search_text,
      options,
      &consumer_,
      base::Bind(&AutomationProviderHistoryObserver::HistoryQueryComplete,
                 base::Unretained(history_observer)));
}

// Sample json input: { "command": "AddHistoryItem",
//                      "item": { "URL": "http://www.google.com",
//                                "title": "Google",   # optional
//                                "time": 12345        # optional (time_t)
//                               } }
// Refer chrome/test/pyautolib/pyauto.py for details on input.
void TestingAutomationProvider::AddHistoryItem(Browser* browser,
                                               DictionaryValue* args,
                                               IPC::Message* reply_message) {
  DictionaryValue* item = NULL;
  args->GetDictionary("item", &item);
  string16 url_text;
  string16 title;
  base::Time time = base::Time::Now();
  AutomationJSONReply reply(this, reply_message);

  if (!item->GetString("url", &url_text)) {
    reply.SendError("bad args (no URL in dict?)");
    return;
  }
  GURL gurl(url_text);
  item->GetString("title", &title);  // Don't care if it fails.
  int it;
  double dt;
  if (item->GetInteger("time", &it))
    time = base::Time::FromTimeT(it);
  else if (item->GetDouble("time", &dt))
    time = base::Time::FromDoubleT(dt);

  // Ideas for "dummy" values (e.g. id_scope) came from
  // chrome/browser/autocomplete/history_contents_provider_unittest.cc
  HistoryService* hs = browser->profile()->GetHistoryService(
      Profile::EXPLICIT_ACCESS);
  const void* id_scope = reinterpret_cast<void*>(1);
  hs->AddPage(gurl, time,
              id_scope,
              0,
              GURL(),
              content::PAGE_TRANSITION_LINK,
              history::RedirectList(),
              history::SOURCE_BROWSED,
              false);
  if (title.length())
    hs->SetPageTitle(gurl, title);
  reply.SendSuccess(NULL);
}

// Sample json input: { "command": "GetDownloadsInfo" }
// Refer chrome/test/pyautolib/download_info.py for sample json output.
void TestingAutomationProvider::GetDownloadsInfo(Browser* browser,
                                                 DictionaryValue* args,
                                                 IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);
  scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
  ListValue* list_of_downloads = new ListValue;

  DownloadService* download_service(
      DownloadServiceFactory::GetForProfile(browser->profile()));

  if (download_service->HasCreatedDownloadManager()) {
    std::vector<DownloadItem*> downloads;
    download_service->GetDownloadManager()->
        GetAllDownloads(FilePath(), &downloads);

    for (std::vector<DownloadItem*>::iterator it = downloads.begin();
         it != downloads.end();
         it++) {  // Fill info about each download item.
      list_of_downloads->Append(GetDictionaryFromDownloadItem(*it));
    }
  }
  return_value->Set("downloads", list_of_downloads);
  reply.SendSuccess(return_value.get());
}

void TestingAutomationProvider::WaitForAllDownloadsToComplete(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  ListValue* pre_download_ids = NULL;

  if (!args->GetList("pre_download_ids", &pre_download_ids)) {
    AutomationJSONReply(this, reply_message)
        .SendError(StringPrintf("List of IDs of previous downloads required."));
    return;
  }

  DownloadService* download_service =
      DownloadServiceFactory::GetForProfile(browser->profile());
  if (!download_service->HasCreatedDownloadManager()) {
    // No download manager, so no downloads to wait for.
    AutomationJSONReply(this, reply_message).SendSuccess(NULL);
    return;
  }

  // This observer will delete itself.
  new AllDownloadsCompleteObserver(
      this, reply_message, download_service->GetDownloadManager(),
      pre_download_ids);
}

namespace {

DownloadItem* GetDownloadItemFromId(int id, DownloadManager* download_manager) {
  std::vector<DownloadItem*> downloads;
  download_manager->GetAllDownloads(FilePath(), &downloads);
  DownloadItem* selected_item = NULL;

  for (std::vector<DownloadItem*>::iterator it = downloads.begin();
       it != downloads.end();
       it++) {
    DownloadItem* curr_item = *it;
    if (curr_item->GetId() == id) {
      selected_item = curr_item;
      break;
    }
  }
  return selected_item;
}

}  // namespace

// See PerformActionOnDownload() in chrome/test/pyautolib/pyauto.py for sample
// json input and output.
void TestingAutomationProvider::PerformActionOnDownload(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  int id;
  std::string action;

  DownloadService* download_service =
      DownloadServiceFactory::GetForProfile(browser->profile());
  if (!download_service->HasCreatedDownloadManager()) {
    AutomationJSONReply(this, reply_message).SendError("No download manager.");
    return;
  }
  if (!args->GetInteger("id", &id) || !args->GetString("action", &action)) {
    AutomationJSONReply(this, reply_message)
        .SendError("Must include int id and string action.");
    return;
  }

  DownloadManager* download_manager = download_service->GetDownloadManager();
  DownloadItem* selected_item = GetDownloadItemFromId(id, download_manager);
  if (!selected_item) {
    AutomationJSONReply(this, reply_message)
        .SendError(StringPrintf("No download with an id of %d\n", id));
    return;
  }

  // We need to be IN_PROGRESS for these actions.
  if ((action == "toggle_pause" || action == "cancel") &&
      !selected_item->IsInProgress()) {
    AutomationJSONReply(this, reply_message)
        .SendError("Selected DownloadItem is not in progress.");
  }

  if (action == "open") {
    selected_item->AddObserver(
        new AutomationProviderDownloadUpdatedObserver(
            this, reply_message, true));
    selected_item->OpenDownload();
  } else if (action == "toggle_open_files_like_this") {
    DownloadPrefs* prefs =
        DownloadPrefs::FromBrowserContext(selected_item->GetBrowserContext());
    FilePath path = selected_item->GetUserVerifiedFilePath();
    if (!selected_item->ShouldOpenFileBasedOnExtension())
      prefs->EnableAutoOpenBasedOnExtension(path);
    else
      prefs->DisableAutoOpenBasedOnExtension(path);
    AutomationJSONReply(this, reply_message).SendSuccess(NULL);
  } else if (action == "remove") {
    download_manager->AddObserver(
        new AutomationProviderDownloadModelChangedObserver(
            this, reply_message, download_manager));
    selected_item->Remove();
  } else if (action == "decline_dangerous_download") {
    download_manager->AddObserver(
        new AutomationProviderDownloadModelChangedObserver(
            this, reply_message, download_manager));
    selected_item->Delete(DownloadItem::DELETE_DUE_TO_USER_DISCARD);
  } else if (action == "save_dangerous_download") {
    selected_item->AddObserver(new AutomationProviderDownloadUpdatedObserver(
        this, reply_message, false));
    selected_item->DangerousDownloadValidated();
  } else if (action == "toggle_pause") {
    selected_item->AddObserver(new AutomationProviderDownloadUpdatedObserver(
        this, reply_message, false));
    // This will still return if download has already completed.
    selected_item->TogglePause();
  } else if (action == "cancel") {
    selected_item->AddObserver(new AutomationProviderDownloadUpdatedObserver(
        this, reply_message, false));
    selected_item->Cancel(true);
  } else {
    AutomationJSONReply(this, reply_message)
        .SendError(StringPrintf("Invalid action '%s' given.", action.c_str()));
  }
}

// Sample JSON input { "command": "LoadSearchEngineInfo" }
void TestingAutomationProvider::LoadSearchEngineInfo(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  TemplateURLService* url_model =
      TemplateURLServiceFactory::GetForProfile(browser->profile());
  if (url_model->loaded()) {
    AutomationJSONReply(this, reply_message).SendSuccess(NULL);
    return;
  }
  url_model->AddObserver(new AutomationProviderSearchEngineObserver(
      this, browser->profile(), reply_message));
  url_model->Load();
}

// Sample JSON input { "command": "GetSearchEngineInfo" }
// Refer to pyauto.py for sample output.
void TestingAutomationProvider::GetSearchEngineInfo(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  TemplateURLService* url_model =
      TemplateURLServiceFactory::GetForProfile(browser->profile());
  scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
  ListValue* search_engines = new ListValue;
  TemplateURLService::TemplateURLVector template_urls =
      url_model->GetTemplateURLs();
  for (TemplateURLService::TemplateURLVector::const_iterator it =
       template_urls.begin(); it != template_urls.end(); ++it) {
    DictionaryValue* search_engine = new DictionaryValue;
    search_engine->SetString("short_name", UTF16ToUTF8((*it)->short_name()));
    search_engine->SetString("keyword", UTF16ToUTF8((*it)->keyword()));
    search_engine->SetBoolean("in_default_list", (*it)->ShowInDefaultList());
    search_engine->SetBoolean("is_default",
        (*it) == url_model->GetDefaultSearchProvider());
    search_engine->SetBoolean("is_valid", (*it)->url_ref().IsValid());
    search_engine->SetBoolean("supports_replacement",
                              (*it)->url_ref().SupportsReplacement());
    search_engine->SetString("url", (*it)->url());
    search_engine->SetString("host", (*it)->url_ref().GetHost());
    search_engine->SetString("path", (*it)->url_ref().GetPath());
    search_engine->SetString("display_url",
                             UTF16ToUTF8((*it)->url_ref().DisplayURL()));
    search_engines->Append(search_engine);
  }
  return_value->Set("search_engines", search_engines);
  AutomationJSONReply(this, reply_message).SendSuccess(return_value.get());
}

// Refer to pyauto.py for sample JSON input.
void TestingAutomationProvider::AddOrEditSearchEngine(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  TemplateURLService* url_model =
      TemplateURLServiceFactory::GetForProfile(browser->profile());
  string16 new_title;
  string16 new_keyword;
  std::string new_url;
  std::string keyword;
  if (!args->GetString("new_title", &new_title) ||
      !args->GetString("new_keyword", &new_keyword) ||
      !args->GetString("new_url", &new_url)) {
    AutomationJSONReply(this, reply_message).SendError(
        "One or more inputs invalid");
    return;
  }
  std::string new_ref_url = TemplateURLRef::DisplayURLToURLRef(
      UTF8ToUTF16(new_url));
  scoped_ptr<KeywordEditorController> controller(
      new KeywordEditorController(browser->profile()));
  if (args->GetString("keyword", &keyword)) {
    TemplateURL* template_url =
        url_model->GetTemplateURLForKeyword(UTF8ToUTF16(keyword));
    if (template_url == NULL) {
      AutomationJSONReply(this, reply_message).SendError(
          "No match for keyword: " + keyword);
      return;
    }
    url_model->AddObserver(new AutomationProviderSearchEngineObserver(
        this, browser->profile(), reply_message));
    controller->ModifyTemplateURL(template_url, new_title, new_keyword,
                                  new_ref_url);
  } else {
    url_model->AddObserver(new AutomationProviderSearchEngineObserver(
        this, browser->profile(), reply_message));
    controller->AddTemplateURL(new_title, new_keyword, new_ref_url);
  }
}

// Sample json input: { "command": "PerformActionOnSearchEngine",
//                      "keyword": keyword, "action": action }
void TestingAutomationProvider::PerformActionOnSearchEngine(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  TemplateURLService* url_model =
      TemplateURLServiceFactory::GetForProfile(browser->profile());
  std::string keyword;
  std::string action;
  if (!args->GetString("keyword", &keyword) ||
      !args->GetString("action", &action)) {
    AutomationJSONReply(this, reply_message).SendError(
        "One or more inputs invalid");
    return;
  }
  TemplateURL* template_url =
      url_model->GetTemplateURLForKeyword(UTF8ToUTF16(keyword));
  if (template_url == NULL) {
    AutomationJSONReply(this, reply_message).SendError(
        "No match for keyword: " + keyword);
    return;
  }
  if (action == "delete") {
    url_model->AddObserver(new AutomationProviderSearchEngineObserver(
      this, browser->profile(), reply_message));
    url_model->Remove(template_url);
  } else if (action == "default") {
    url_model->AddObserver(new AutomationProviderSearchEngineObserver(
      this, browser->profile(), reply_message));
    url_model->SetDefaultSearchProvider(template_url);
  } else {
    AutomationJSONReply(this, reply_message).SendError(
        "Invalid action: " + action);
  }
}

#if defined(ENABLE_PROTECTOR_SERVICE)
// Sample json output: { "enabled": true,
//                       "showing_change": false }
void TestingAutomationProvider::GetProtectorState(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  protector::ProtectorService* protector_service =
      protector::ProtectorServiceFactory::GetForProfile(browser->profile());
  scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
  return_value->SetBoolean("enabled", protector::IsEnabled());
  return_value->SetBoolean("showing_change",
                           protector_service->IsShowingChange());
  AutomationJSONReply(this, reply_message).SendSuccess(return_value.get());
}

// Sample json inputs:
//    { "command": "PerformProtectorAction", "action": "apply_change" }
//    { "command": "PerformProtectorAction", "action": "discard_change" }
void TestingAutomationProvider::PerformProtectorAction(
    Browser* browser,
    base::DictionaryValue* args,
    IPC::Message* reply_message) {
  protector::ProtectorService* protector_service =
      protector::ProtectorServiceFactory::GetForProfile(browser->profile());
  AutomationJSONReply reply(this, reply_message);
  if (!protector::IsEnabled()) {
    reply.SendError("Protector is disabled");
    return;
  }
  std::string action;
  if (!args->GetString("action", &action)) {
    reply.SendError("Missing 'action' value");
    return;
  }
  protector::BaseSettingChange* change =
      protector_service->GetLastChange();
  if (action == "apply_change")
    protector_service->ApplyChange(change, browser);
  else if (action == "discard_change")
    protector_service->DiscardChange(change, browser);
  else
    return reply.SendError("Invalid 'action' value");
  reply.SendSuccess(NULL);
}
#endif  // defined(ENABLE_PROTECTOR_SERVICE)

// Sample json input: { "command": "GetLocalStatePrefsInfo" }
// Refer chrome/test/pyautolib/prefs_info.py for sample json output.
void TestingAutomationProvider::GetLocalStatePrefsInfo(
    DictionaryValue* args,
    IPC::Message* reply_message) {
  DictionaryValue* items = g_browser_process->local_state()->
      GetPreferenceValues();
  scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
  return_value->Set("prefs", items);  // return_value owns items.
  AutomationJSONReply(this, reply_message).SendSuccess(return_value.get());
}

// Sample json input: { "command": "SetLocalStatePrefs", "path": path,
//                      "value": value }
void TestingAutomationProvider::SetLocalStatePrefs(
    DictionaryValue* args,
    IPC::Message* reply_message) {
  std::string path;
  Value* val;
  AutomationJSONReply reply(this, reply_message);
  if (args->GetString("path", &path) && args->Get("value", &val)) {
    PrefService* pref_service = g_browser_process->local_state();
    const PrefService::Preference* pref =
        pref_service->FindPreference(path.c_str());
    if (!pref) {  // Not a registered pref.
      reply.SendError("pref not registered.");
      return;
    } else if (pref->IsManaged()) {  // Do not attempt to change a managed pref.
      reply.SendError("pref is managed. cannot be changed.");
      return;
    } else {  // Set the pref.
      pref_service->Set(path.c_str(), *val);
    }
  } else {
    reply.SendError("no pref path or value given.");
    return;
  }

  reply.SendSuccess(NULL);
}

// Sample json input: { "command": "GetPrefsInfo", "windex": 0 }
// Refer chrome/test/pyautolib/prefs_info.py for sample json output.
void TestingAutomationProvider::GetPrefsInfo(DictionaryValue* args,
                                             IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);
  Browser* browser;
  std::string error_msg;
  if (!GetBrowserFromJSONArgs(args, &browser, &error_msg)) {
    reply.SendError(error_msg);
    return;
  }
  DictionaryValue* items = browser->profile()->GetPrefs()->
      GetPreferenceValues();

  scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
  return_value->Set("prefs", items);  // return_value owns items.
  reply.SendSuccess(return_value.get());
}

// Sample json input:
// { "command": "SetPrefs",
//   "windex": 0,
//   "path": path,
//   "value": value }
void TestingAutomationProvider::SetPrefs(DictionaryValue* args,
                                         IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);
  Browser* browser;
  std::string error_msg;
  if (!GetBrowserFromJSONArgs(args, &browser, &error_msg)) {
    reply.SendError(error_msg);
    return;
  }
  std::string path;
  Value* val;
  if (args->GetString("path", &path) && args->Get("value", &val)) {
    PrefService* pref_service = browser->profile()->GetPrefs();
    const PrefService::Preference* pref =
        pref_service->FindPreference(path.c_str());
    if (!pref) {  // Not a registered pref.
      reply.SendError("pref not registered.");
      return;
    } else if (pref->IsManaged()) {  // Do not attempt to change a managed pref.
      reply.SendError("pref is managed. cannot be changed.");
      return;
    } else {  // Set the pref.
      pref_service->Set(path.c_str(), *val);
    }
  } else {
    reply.SendError("no pref path or value given.");
    return;
  }

  reply.SendSuccess(NULL);
}

// Sample json input: { "command": "GetOmniboxInfo" }
// Refer chrome/test/pyautolib/omnibox_info.py for sample json output.
void TestingAutomationProvider::GetOmniboxInfo(Browser* browser,
                                               DictionaryValue* args,
                                               IPC::Message* reply_message) {
  scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
  AutomationJSONReply reply(this, reply_message);

  LocationBar* loc_bar = browser->window()->GetLocationBar();
  if (!loc_bar) {
    reply.SendError("The specified browser does not have a location bar.");
    return;
  }
  const OmniboxView* omnibox_view = loc_bar->GetLocationEntry();
  const AutocompleteEditModel* model = omnibox_view->model();

  // Fill up matches.
  ListValue* matches = new ListValue;
  const AutocompleteResult& result = model->result();
  for (AutocompleteResult::const_iterator i = result.begin();
       i != result.end(); ++i) {
    const AutocompleteMatch& match = *i;
    DictionaryValue* item = new DictionaryValue;  // owned by return_value
    item->SetString("type", AutocompleteMatch::TypeToString(match.type));
    item->SetBoolean("starred", match.starred);
    item->SetString("destination_url", match.destination_url.spec());
    item->SetString("contents", match.contents);
    item->SetString("description", match.description);
    matches->Append(item);
  }
  return_value->Set("matches", matches);

  // Fill up other properties.
  DictionaryValue* properties = new DictionaryValue;  // owned by return_value
  properties->SetBoolean("has_focus", model->has_focus());
  properties->SetBoolean("query_in_progress",
                         !model->autocomplete_controller()->done());
  properties->SetString("keyword", model->keyword());
  properties->SetString("text", omnibox_view->GetText());
  return_value->Set("properties", properties);

  reply.SendSuccess(return_value.get());
}

// Sample json input: { "command": "SetOmniboxText",
//                      "text": "goog" }
void TestingAutomationProvider::SetOmniboxText(Browser* browser,
                                               DictionaryValue* args,
                                               IPC::Message* reply_message) {
  string16 text;
  AutomationJSONReply reply(this, reply_message);
  if (!args->GetString("text", &text)) {
    reply.SendError("text missing");
    return;
  }
  browser->FocusLocationBar();
  LocationBar* loc_bar = browser->window()->GetLocationBar();
  if (!loc_bar) {
    reply.SendError("The specified browser does not have a location bar.");
    return;
  }
  OmniboxView* omnibox_view = loc_bar->GetLocationEntry();
  omnibox_view->model()->OnSetFocus(false);
  omnibox_view->SetUserText(text);
  reply.SendSuccess(NULL);
}

// Sample json input: { "command": "OmniboxMovePopupSelection",
//                      "count": 1 }
// Negative count implies up, positive implies down. Count values will be
// capped by the size of the popup list.
void TestingAutomationProvider::OmniboxMovePopupSelection(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  int count;
  AutomationJSONReply reply(this, reply_message);
  if (!args->GetInteger("count", &count)) {
    reply.SendError("count missing");
    return;
  }
  LocationBar* loc_bar = browser->window()->GetLocationBar();
  if (!loc_bar) {
    reply.SendError("The specified browser does not have a location bar.");
    return;
  }
  AutocompleteEditModel* model = loc_bar->GetLocationEntry()->model();
  model->OnUpOrDownKeyPressed(count);
  reply.SendSuccess(NULL);
}

// Sample json input: { "command": "OmniboxAcceptInput" }
void TestingAutomationProvider::OmniboxAcceptInput(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  NavigationController& controller =
      browser->GetSelectedWebContents()->GetController();
  LocationBar* loc_bar = browser->window()->GetLocationBar();
  if (!loc_bar) {
    AutomationJSONReply(this, reply_message).SendError(
        "The specified browser does not have a location bar.");
    return;
  }
  new OmniboxAcceptNotificationObserver(&controller, this, reply_message);
  loc_bar->AcceptInput();
}

// Sample json input: { "command": "GetInstantInfo" }
void TestingAutomationProvider::GetInstantInfo(Browser* browser,
                                               DictionaryValue* args,
                                               IPC::Message* reply_message) {
  DictionaryValue* info = new DictionaryValue;
  if (browser->instant()) {
    InstantController* instant = browser->instant();
    info->SetBoolean("enabled", true);
    info->SetBoolean("active", (instant->GetPreviewContents() != NULL));
    info->SetBoolean("current", instant->IsCurrent());
    if (instant->GetPreviewContents() &&
        instant->GetPreviewContents()->web_contents()) {
      WebContents* contents = instant->GetPreviewContents()->web_contents();
      info->SetBoolean("loading", contents->IsLoading());
      info->SetString("location", contents->GetURL().spec());
      info->SetString("title", contents->GetTitle());
    }
  } else {
    info->SetBoolean("enabled", false);
  }
  scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
  return_value->Set("instant", info);
  AutomationJSONReply(this, reply_message).SendSuccess(return_value.get());
}

// Sample json input: { "command": "GetInitialLoadTimes" }
// Refer to InitialLoadObserver::GetTimingInformation() for sample output.
void TestingAutomationProvider::GetInitialLoadTimes(
    Browser*,
    DictionaryValue*,
    IPC::Message* reply_message) {
  scoped_ptr<DictionaryValue> return_value(
      initial_load_observer_->GetTimingInformation());

  std::string json_return;
  base::JSONWriter::Write(return_value.get(), &json_return);
  AutomationMsg_SendJSONRequest::WriteReplyParams(
      reply_message, json_return, true);
  Send(reply_message);
}

// Sample json input: { "command": "GetPluginsInfo" }
// Refer chrome/test/pyautolib/plugins_info.py for sample json output.
void TestingAutomationProvider::GetPluginsInfo(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  PluginService::GetInstance()->GetPlugins(
      base::Bind(&TestingAutomationProvider::GetPluginsInfoCallback,
          this, browser, args, reply_message));
}

void TestingAutomationProvider::GetPluginsInfoCallback(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message,
    const std::vector<webkit::WebPluginInfo>& plugins) {
  PluginPrefs* plugin_prefs = PluginPrefs::GetForProfile(browser->profile());
  ListValue* items = new ListValue;
  for (std::vector<webkit::WebPluginInfo>::const_iterator it =
           plugins.begin();
       it != plugins.end();
       ++it) {
    DictionaryValue* item = new DictionaryValue;
    item->SetString("name", it->name);
    item->SetString("path", it->path.value());
    item->SetString("version", it->version);
    item->SetString("desc", it->desc);
    item->SetBoolean("enabled", plugin_prefs->IsPluginEnabled(*it));
    // Add info about mime types.
    ListValue* mime_types = new ListValue();
    for (std::vector<webkit::WebPluginMimeType>::const_iterator type_it =
             it->mime_types.begin();
         type_it != it->mime_types.end();
         ++type_it) {
      DictionaryValue* mime_type = new DictionaryValue();
      mime_type->SetString("mimeType", type_it->mime_type);
      mime_type->SetString("description", type_it->description);

      ListValue* file_extensions = new ListValue();
      for (std::vector<std::string>::const_iterator ext_it =
               type_it->file_extensions.begin();
           ext_it != type_it->file_extensions.end();
           ++ext_it) {
        file_extensions->Append(new StringValue(*ext_it));
      }
      mime_type->Set("fileExtensions", file_extensions);

      mime_types->Append(mime_type);
    }
    item->Set("mimeTypes", mime_types);
    items->Append(item);
  }
  scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
  return_value->Set("plugins", items);  // return_value owns items.

  AutomationJSONReply(this, reply_message).SendSuccess(return_value.get());
}

// Sample json input:
//    { "command": "EnablePlugin",
//      "path": "/Library/Internet Plug-Ins/Flash Player.plugin" }
void TestingAutomationProvider::EnablePlugin(Browser* browser,
                                             DictionaryValue* args,
                                             IPC::Message* reply_message) {
  FilePath::StringType path;
  if (!args->GetString("path", &path)) {
    AutomationJSONReply(this, reply_message).SendError("path not specified.");
    return;
  }
  PluginPrefs* plugin_prefs = PluginPrefs::GetForProfile(browser->profile());
  if (!plugin_prefs->CanEnablePlugin(true, FilePath(path))) {
    AutomationJSONReply(this, reply_message).SendError(
        StringPrintf("Could not enable plugin for path %s.", path.c_str()));
    return;
  }
  plugin_prefs->EnablePlugin(
      true, FilePath(path),
      base::Bind(SendSuccessReply, AsWeakPtr(), reply_message));
}

// Sample json input:
//    { "command": "DisablePlugin",
//      "path": "/Library/Internet Plug-Ins/Flash Player.plugin" }
void TestingAutomationProvider::DisablePlugin(Browser* browser,
                                              DictionaryValue* args,
                                              IPC::Message* reply_message) {
  FilePath::StringType path;
  if (!args->GetString("path", &path)) {
    AutomationJSONReply(this, reply_message).SendError("path not specified.");
    return;
  }
  PluginPrefs* plugin_prefs = PluginPrefs::GetForProfile(browser->profile());
  if (!plugin_prefs->CanEnablePlugin(false, FilePath(path))) {
    AutomationJSONReply(this, reply_message).SendError(
        StringPrintf("Could not disable plugin for path %s.", path.c_str()));
    return;
  }
  plugin_prefs->EnablePlugin(
      false, FilePath(path),
      base::Bind(SendSuccessReply, AsWeakPtr(), reply_message));
}

// Sample json input:
//    { "command": "SaveTabContents",
//      "tab_index": 0,
//      "filename": <a full pathname> }
// Sample json output:
//    {}
void TestingAutomationProvider::SaveTabContents(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  int tab_index = 0;
  FilePath::StringType filename;
  FilePath::StringType parent_directory;
  WebContents* web_contents = NULL;

  if (!args->GetInteger("tab_index", &tab_index) ||
      !args->GetString("filename", &filename)) {
    AutomationJSONReply(this, reply_message)
        .SendError("tab_index or filename param missing");
    return;
  } else {
    web_contents = browser->GetWebContentsAt(tab_index);
    if (!web_contents) {
      AutomationJSONReply(this, reply_message).SendError("no tab at tab_index");
      return;
    }
  }
  // We're doing a SAVE_AS_ONLY_HTML so the the directory path isn't
  // used.  Nevertheless, SavePackage requires it be valid.  Sigh.
  parent_directory = FilePath(filename).DirName().value();
  if (!web_contents->SavePage(
          FilePath(filename),
          FilePath(parent_directory),
          content::SAVE_PAGE_TYPE_AS_ONLY_HTML)) {
    AutomationJSONReply(this, reply_message).SendError(
        "Could not initiate SavePage");
    return;
  }
  // The observer will delete itself when done.
  new SavePackageNotificationObserver(
      DownloadServiceFactory::GetForProfile(
          browser->profile())->GetDownloadManager(),
      this, reply_message);
}

// Refer to ImportSettings() in chrome/test/pyautolib/pyauto.py for sample
// json input.
// Sample json output: "{}"
void TestingAutomationProvider::ImportSettings(Browser* browser,
                                               DictionaryValue* args,
                                               IPC::Message* reply_message) {
  // Map from the json string passed over to the import item masks.
  std::map<std::string, importer::ImportItem> string_to_import_item;
  string_to_import_item["HISTORY"] = importer::HISTORY;
  string_to_import_item["FAVORITES"] = importer::FAVORITES;
  string_to_import_item["COOKIES"] = importer::COOKIES;
  string_to_import_item["PASSWORDS"] = importer::PASSWORDS;
  string_to_import_item["SEARCH_ENGINES"] = importer::SEARCH_ENGINES;
  string_to_import_item["HOME_PAGE"] = importer::HOME_PAGE;
  string_to_import_item["ALL"] = importer::ALL;

  ListValue* import_items_list = NULL;
  if (!args->GetString("import_from", &import_settings_data_.browser_name) ||
      !args->GetBoolean("first_run", &import_settings_data_.first_run) ||
      !args->GetList("import_items", &import_items_list)) {
    AutomationJSONReply(this, reply_message)
        .SendError("Incorrect type for one or more of the arguments.");
    return;
  }

  import_settings_data_.import_items = 0;
  int num_items = import_items_list->GetSize();
  for (int i = 0; i < num_items; i++) {
    std::string item;
    // If the provided string is not part of the map, error out.
    if (!import_items_list->GetString(i, &item) ||
        !ContainsKey(string_to_import_item, item)) {
      AutomationJSONReply(this, reply_message)
          .SendError("Invalid item string found in import_items.");
      return;
    }
    import_settings_data_.import_items |= string_to_import_item[item];
  }

  import_settings_data_.browser = browser;
  import_settings_data_.reply_message = reply_message;

  importer_list_ = new ImporterList(NULL);
  importer_list_->DetectSourceProfiles(this);
}

namespace {

// Translates a dictionary password to a PasswordForm struct.
webkit::forms::PasswordForm GetPasswordFormFromDict(
    const DictionaryValue& password_dict) {

  // If the time is specified, change time to the specified time.
  base::Time time = base::Time::Now();
  int it;
  double dt;
  if (password_dict.GetInteger("time", &it))
    time = base::Time::FromTimeT(it);
  else if (password_dict.GetDouble("time", &dt))
    time = base::Time::FromDoubleT(dt);

  std::string signon_realm;
  string16 username_value;
  string16 password_value;
  string16 origin_url_text;
  string16 username_element;
  string16 password_element;
  string16 submit_element;
  string16 action_target_text;
  bool blacklist;
  string16 old_password_element;
  string16 old_password_value;

  // We don't care if any of these fail - they are either optional or checked
  // before this function is called.
  password_dict.GetString("signon_realm", &signon_realm);
  password_dict.GetString("username_value", &username_value);
  password_dict.GetString("password_value", &password_value);
  password_dict.GetString("origin_url", &origin_url_text);
  password_dict.GetString("username_element", &username_element);
  password_dict.GetString("password_element", &password_element);
  password_dict.GetString("submit_element", &submit_element);
  password_dict.GetString("action_target", &action_target_text);
  if (!password_dict.GetBoolean("blacklist", &blacklist))
    blacklist = false;

  GURL origin_gurl(origin_url_text);
  GURL action_target(action_target_text);

  webkit::forms::PasswordForm password_form;
  password_form.signon_realm = signon_realm;
  password_form.username_value = username_value;
  password_form.password_value = password_value;
  password_form.origin = origin_gurl;
  password_form.username_element = username_element;
  password_form.password_element = password_element;
  password_form.submit_element = submit_element;
  password_form.action = action_target;
  password_form.blacklisted_by_user = blacklist;
  password_form.date_created = time;

  return password_form;
}

}  // namespace

// See AddSavedPassword() in chrome/test/functional/pyauto.py for sample json
// input.
// Sample json output: { "password_added": true }
void TestingAutomationProvider::AddSavedPassword(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  DictionaryValue* password_dict = NULL;
  if (!args->GetDictionary("password", &password_dict)) {
    AutomationJSONReply(this, reply_message).SendError(
        "Must specify a password dictionary.");
    return;
  }

  // The "signon realm" is effectively the primary key and must be included.
  // Check here before calling GetPasswordFormFromDict.
  if (!password_dict->HasKey("signon_realm")) {
    AutomationJSONReply(this, reply_message).SendError(
        "Password must include a value for 'signon_realm.'");
    return;
  }

  webkit::forms::PasswordForm new_password =
      GetPasswordFormFromDict(*password_dict);

  // Use IMPLICIT_ACCESS since new passwords aren't added in incognito mode.
  PasswordStore* password_store = PasswordStoreFactory::GetForProfile(
      browser->profile(), Profile::IMPLICIT_ACCESS);

  // The password store does not exist for an incognito window.
  if (password_store == NULL) {
    scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
    return_value->SetBoolean("password_added", false);
    AutomationJSONReply(this, reply_message).SendSuccess(return_value.get());
    return;
  }

  // This observer will delete itself.
  PasswordStoreLoginsChangedObserver* observer =
      new PasswordStoreLoginsChangedObserver(this, reply_message,
                                             PasswordStoreChange::ADD,
                                             "password_added");
  observer->Init();
  password_store->AddLogin(new_password);
}

// See RemoveSavedPassword() in chrome/test/functional/pyauto.py for sample
// json input.
// Sample json output: {}
void TestingAutomationProvider::RemoveSavedPassword(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  DictionaryValue* password_dict = NULL;

  if (!args->GetDictionary("password", &password_dict)) {
    AutomationJSONReply(this, reply_message).SendError(
        "Must specify a password dictionary.");
    return;
  }

  // The "signon realm" is effectively the primary key and must be included.
  // Check here before calling GetPasswordFormFromDict.
  if (!password_dict->HasKey("signon_realm")) {
    AutomationJSONReply(this, reply_message).SendError(
        "Password must include a value for 'signon_realm.'");
    return;
  }
  webkit::forms::PasswordForm to_remove =
      GetPasswordFormFromDict(*password_dict);

  // Use EXPLICIT_ACCESS since passwords can be removed in incognito mode.
  PasswordStore* password_store = PasswordStoreFactory::GetForProfile(
      browser->profile(), Profile::EXPLICIT_ACCESS);
  if (password_store == NULL) {
    AutomationJSONReply(this, reply_message).SendError(
        "Unable to get password store.");
    return;
  }

  // This observer will delete itself.
  PasswordStoreLoginsChangedObserver* observer =
      new PasswordStoreLoginsChangedObserver(
          this, reply_message, PasswordStoreChange::REMOVE, "");
  observer->Init();

  password_store->RemoveLogin(to_remove);
}

// Sample json input: { "command": "GetSavedPasswords" }
// Refer to GetSavedPasswords() in chrome/test/pyautolib/pyauto.py for sample
// json output.
void TestingAutomationProvider::GetSavedPasswords(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  // Use EXPLICIT_ACCESS since saved passwords can be retrieved in
  // incognito mode.
  PasswordStore* password_store = PasswordStoreFactory::GetForProfile(
      browser->profile(), Profile::EXPLICIT_ACCESS);

  if (password_store == NULL) {
    AutomationJSONReply reply(this, reply_message);
    reply.SendError("Unable to get password store.");
    return;
  }
  password_store->GetAutofillableLogins(
      new AutomationProviderGetPasswordsObserver(this, reply_message));
  // Observer deletes itself after sending the result.
}

// Refer to ClearBrowsingData() in chrome/test/pyautolib/pyauto.py for sample
// json input.
// Sample json output: {}
void TestingAutomationProvider::ClearBrowsingData(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  std::map<std::string, BrowsingDataRemover::TimePeriod> string_to_time_period;
  string_to_time_period["LAST_HOUR"] = BrowsingDataRemover::LAST_HOUR;
  string_to_time_period["LAST_DAY"] = BrowsingDataRemover::LAST_DAY;
  string_to_time_period["LAST_WEEK"] = BrowsingDataRemover::LAST_WEEK;
  string_to_time_period["FOUR_WEEKS"] = BrowsingDataRemover::FOUR_WEEKS;
  string_to_time_period["EVERYTHING"] = BrowsingDataRemover::EVERYTHING;

  std::map<std::string, int> string_to_mask_value;
  string_to_mask_value["HISTORY"] = BrowsingDataRemover::REMOVE_HISTORY;
  string_to_mask_value["DOWNLOADS"] = BrowsingDataRemover::REMOVE_DOWNLOADS;
  string_to_mask_value["COOKIES"] = BrowsingDataRemover::REMOVE_SITE_DATA;
  string_to_mask_value["PASSWORDS"] = BrowsingDataRemover::REMOVE_PASSWORDS;
  string_to_mask_value["FORM_DATA"] = BrowsingDataRemover::REMOVE_FORM_DATA;
  string_to_mask_value["CACHE"] = BrowsingDataRemover::REMOVE_CACHE;

  std::string time_period;
  ListValue* to_remove;
  if (!args->GetString("time_period", &time_period) ||
      !args->GetList("to_remove", &to_remove)) {
    AutomationJSONReply(this, reply_message)
        .SendError("time_period must be a string and to_remove a list.");
    return;
  }

  int remove_mask = 0;
  int num_removals = to_remove->GetSize();
  for (int i = 0; i < num_removals; i++) {
    std::string removal;
    // If the provided string is not part of the map, then error out.
    if (!to_remove->GetString(i, &removal) ||
        !ContainsKey(string_to_mask_value, removal)) {
      AutomationJSONReply(this, reply_message)
          .SendError("Invalid browsing data string found in to_remove.");
      return;
    }
    remove_mask |= string_to_mask_value[removal];
  }

  if (!ContainsKey(string_to_time_period, time_period)) {
    AutomationJSONReply(this, reply_message)
        .SendError("Invalid string for time_period.");
    return;
  }

  BrowsingDataRemover* remover = new BrowsingDataRemover(
      profile(), string_to_time_period[time_period], base::Time());

  remover->AddObserver(
      new AutomationProviderBrowsingDataObserver(this, reply_message));
  remover->Remove(remove_mask);
  // BrowsingDataRemover deletes itself using DeleteTask.
  // The observer also deletes itself after sending the reply.
}

namespace {

// Get the TabContentsWrapper from a dictionary of arguments.
TabContentsWrapper* GetTabContentsWrapperFromDict(const Browser* browser,
                                                  const DictionaryValue* args,
                                                  std::string* error_message) {
  int tab_index;
  if (!args->GetInteger("tab_index", &tab_index)) {
    *error_message = "Must include tab_index.";
    return NULL;
  }

  TabContentsWrapper* tab_contents =
      browser->GetTabContentsWrapperAt(tab_index);
  if (!tab_contents) {
    *error_message = StringPrintf("No tab at index %d.", tab_index);
    return NULL;
  }
  return tab_contents;
}

// Get the TranslateInfoBarDelegate from WebContents.
TranslateInfoBarDelegate* GetTranslateInfoBarDelegate(
    WebContents* web_contents) {
  InfoBarTabHelper* infobar_helper =
      TabContentsWrapper::GetCurrentWrapperForContents(web_contents)->
          infobar_tab_helper();
  for (size_t i = 0; i < infobar_helper->infobar_count(); i++) {
    InfoBarDelegate* infobar = infobar_helper->GetInfoBarDelegateAt(i);
    if (infobar->AsTranslateInfoBarDelegate())
      return infobar->AsTranslateInfoBarDelegate();
  }
  // No translate infobar.
  return NULL;
}

}  // namespace

void TestingAutomationProvider::FindInPage(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  std::string error_message;
  TabContentsWrapper* tab_contents =
      GetTabContentsWrapperFromDict(browser, args, &error_message);
  if (!tab_contents) {
    AutomationJSONReply(this, reply_message).SendError(error_message);
    return;
  }
  string16 search_string;
  bool forward;
  bool match_case;
  bool find_next;
  if (!args->GetString("search_string", &search_string)) {
    AutomationJSONReply(this, reply_message).
        SendError("Must include search_string string.");
    return;
  }
  if (!args->GetBoolean("forward", &forward)) {
    AutomationJSONReply(this, reply_message).
        SendError("Must include forward boolean.");
    return;
  }
  if (!args->GetBoolean("match_case", &match_case)) {
    AutomationJSONReply(this, reply_message).
        SendError("Must include match_case boolean.");
    return;
  }
  if (!args->GetBoolean("find_next", &find_next)) {
    AutomationJSONReply(this, reply_message).
        SendError("Must include find_next boolean.");
    return;
  }
  SendFindRequest(tab_contents->web_contents(),
                  true,
                  search_string,
                  forward,
                  match_case,
                  find_next,
                  reply_message);
}

// See GetTranslateInfo() in chrome/test/pyautolib/pyauto.py for sample json
// input and output.
void TestingAutomationProvider::GetTranslateInfo(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  std::string error_message;
  TabContentsWrapper* tab_contents_wrapper =
      GetTabContentsWrapperFromDict(browser, args, &error_message);
  if (!tab_contents_wrapper) {
    AutomationJSONReply(this, reply_message).SendError(error_message);
    return;
  }

  WebContents* web_contents = tab_contents_wrapper->web_contents();
  // Get the translate bar if there is one and pass it to the observer.
  // The observer will check for null and populate the information accordingly.
  TranslateInfoBarDelegate* translate_bar =
      GetTranslateInfoBarDelegate(web_contents);

  TabLanguageDeterminedObserver* observer = new TabLanguageDeterminedObserver(
      this, reply_message, web_contents, translate_bar);
  // If the language for the page hasn't been loaded yet, then just make
  // the observer, otherwise call observe directly.
  TranslateTabHelper* helper = TabContentsWrapper::GetCurrentWrapperForContents(
      web_contents)->translate_tab_helper();
  std::string language = helper->language_state().original_language();
  if (!language.empty()) {
    observer->Observe(chrome::NOTIFICATION_TAB_LANGUAGE_DETERMINED,
                      content::Source<WebContents>(web_contents),
                      content::Details<std::string>(&language));
  }
}

// See SelectTranslateOption() in chrome/test/pyautolib/pyauto.py for sample
// json input.
// Sample json output: {}
void TestingAutomationProvider::SelectTranslateOption(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  std::string option;
  std::string error_message;
  TabContentsWrapper* tab_contents_wrapper =
      GetTabContentsWrapperFromDict(browser, args, &error_message);
  if (!tab_contents_wrapper) {
    AutomationJSONReply(this, reply_message).SendError(error_message);
    return;
  }

  WebContents* web_contents = tab_contents_wrapper->web_contents();
  TranslateInfoBarDelegate* translate_bar =
      GetTranslateInfoBarDelegate(web_contents);
  if (!translate_bar) {
    AutomationJSONReply(this, reply_message)
        .SendError("There is no translate bar open.");
    return;
  }

  if (!args->GetString("option", &option)) {
    AutomationJSONReply(this, reply_message).SendError("Must include option");
    return;
  }

  if (option == "translate_page") {
    // Make a new notification observer which will send the reply.
    new PageTranslatedObserver(this, reply_message, web_contents);
    translate_bar->Translate();
    return;
  } else if (option == "set_target_language") {
    string16 target_language;
    if (!args->GetString("target_language", &target_language)) {
       AutomationJSONReply(this, reply_message)
           .SendError("Must include target_language string.");
      return;
    }
    // Get the target language index based off of the language name.
    size_t target_language_index = TranslateInfoBarDelegate::kNoIndex;
    for (size_t i = 0; i < translate_bar->GetLanguageCount(); i++) {
      if (translate_bar->GetLanguageDisplayableNameAt(i) == target_language) {
        target_language_index = i;
        break;
      }
    }
    if (target_language_index == TranslateInfoBarDelegate::kNoIndex) {
       AutomationJSONReply(this, reply_message)
           .SendError("Invalid target language string.");
       return;
    }
    // If the page has already been translated it will be translated again to
    // the new language. The observer will wait until the page has been
    // translated to reply.
    if (translate_bar->type() == TranslateInfoBarDelegate::AFTER_TRANSLATE) {
      new PageTranslatedObserver(this, reply_message, web_contents);
      translate_bar->SetTargetLanguage(target_language_index);
      return;
    }
    // Otherwise just send the reply back immediately.
    translate_bar->SetTargetLanguage(target_language_index);
    scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
    return_value->SetBoolean("translation_success", true);
    AutomationJSONReply(this, reply_message).SendSuccess(return_value.get());
    return;
  } else if (option == "click_always_translate_lang_button") {
    if (!translate_bar->ShouldShowAlwaysTranslateButton()) {
      AutomationJSONReply(this, reply_message)
          .SendError("Always translate button not showing.");
      return;
    }
    // Clicking 'Always Translate' triggers a translation. The observer will
    // wait until the translation is complete before sending the reply.
    new PageTranslatedObserver(this, reply_message, web_contents);
    translate_bar->AlwaysTranslatePageLanguage();
    return;
  }

  AutomationJSONReply reply(this, reply_message);
  if (option == "never_translate_language") {
    if (translate_bar->IsLanguageBlacklisted()) {
      reply.SendError("The language was already blacklisted.");
      return;
    }
    translate_bar->ToggleLanguageBlacklist();
    reply.SendSuccess(NULL);
  } else if (option == "never_translate_site") {
    if (translate_bar->IsSiteBlacklisted()) {
      reply.SendError("The site was already blacklisted.");
      return;
    }
    translate_bar->ToggleSiteBlacklist();
    reply.SendSuccess(NULL);
  } else if (option == "toggle_always_translate") {
    translate_bar->ToggleAlwaysTranslate();
    reply.SendSuccess(NULL);
  } else if (option == "revert_translation") {
    translate_bar->RevertTranslation();
    reply.SendSuccess(NULL);
  } else if (option == "click_never_translate_lang_button") {
    if (!translate_bar->ShouldShowNeverTranslateButton()) {
      reply.SendError("Always translate button not showing.");
      return;
    }
    translate_bar->NeverTranslatePageLanguage();
    reply.SendSuccess(NULL);
  } else if (option == "decline_translation") {
    // This is the function called when an infobar is dismissed or when the
    // user clicks the 'Nope' translate button.
    translate_bar->TranslationDeclined();
    tab_contents_wrapper->infobar_tab_helper()->RemoveInfoBar(translate_bar);
    reply.SendSuccess(NULL);
  } else {
    reply.SendError("Invalid string found for option.");
  }
}

// Sample json input: { "command": "GetBlockedPopupsInfo",
//                      "tab_index": 1 }
// Refer GetBlockedPopupsInfo() in pyauto.py for sample output.
void TestingAutomationProvider::GetBlockedPopupsInfo(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);
  std::string error_message;
  TabContentsWrapper* tab_contents = GetTabContentsWrapperFromDict(
      browser, args, &error_message);
  if (!tab_contents) {
    reply.SendError(error_message);
    return;
  }
  scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
  BlockedContentTabHelper* blocked_content =
      tab_contents->blocked_content_tab_helper();
  ListValue* blocked_popups_list = new ListValue;
  std::vector<TabContentsWrapper*> blocked_contents;
  blocked_content->GetBlockedContents(&blocked_contents);
  for (std::vector<TabContentsWrapper*>::const_iterator it =
           blocked_contents.begin(); it != blocked_contents.end(); ++it) {
    DictionaryValue* item = new DictionaryValue;
    item->SetString("url", (*it)->web_contents()->GetURL().spec());
    item->SetString("title", (*it)->web_contents()->GetTitle());
    blocked_popups_list->Append(item);
  }
  return_value->Set("blocked_popups", blocked_popups_list);
  reply.SendSuccess(return_value.get());
}

// Refer UnblockAndLaunchBlockedPopup() in pyauto.py for sample input.
void TestingAutomationProvider::UnblockAndLaunchBlockedPopup(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);
  std::string error_message;
  TabContentsWrapper* tab_contents = GetTabContentsWrapperFromDict(
      browser, args, &error_message);
  if (!tab_contents) {
    reply.SendError(error_message);
    return;
  }
  int popup_index;
  if (!args->GetInteger("popup_index", &popup_index)) {
    reply.SendError("Need popup_index arg");
    return;
  }
  scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
  BlockedContentTabHelper* blocked_content =
      tab_contents->blocked_content_tab_helper();
  if (popup_index >= (int)blocked_content->GetBlockedContentsCount()) {
    reply.SendError(StringPrintf("No popup at index %d", popup_index));
    return;
  }
  std::vector<TabContentsWrapper*> blocked_contents;
  blocked_content->GetBlockedContents(&blocked_contents);
  blocked_content->LaunchForContents(blocked_contents[popup_index]);
  reply.SendSuccess(NULL);
}

// Sample json input: { "command": "GetThemeInfo" }
// Refer GetThemeInfo() in chrome/test/pyautolib/pyauto.py for sample output.
void TestingAutomationProvider::GetThemeInfo(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
  const Extension* theme = ThemeServiceFactory::GetThemeForProfile(profile());
  if (theme) {
    return_value->SetString("name", theme->name());
    return_value->Set("images", theme->GetThemeImages()->DeepCopy());
    return_value->Set("colors", theme->GetThemeColors()->DeepCopy());
    return_value->Set("tints", theme->GetThemeTints()->DeepCopy());
  }
  AutomationJSONReply(this, reply_message).SendSuccess(return_value.get());
}

void TestingAutomationProvider::InstallExtension(
    DictionaryValue* args, IPC::Message* reply_message) {
  FilePath::StringType path_string;
  bool with_ui;
  Browser* browser;
  std::string error_msg;
  if (!GetBrowserFromJSONArgs(args, &browser, &error_msg)) {
    AutomationJSONReply(this, reply_message).SendError(error_msg);
    return;
  }
  if (!args->GetString("path", &path_string)) {
    AutomationJSONReply(this, reply_message).SendError(
        "Missing or invalid 'path'");
    return;
  }
  if (!args->GetBoolean("with_ui", &with_ui)) {
    AutomationJSONReply(this, reply_message).SendError(
        "Missing or invalid 'with_ui'");
    return;
  }
  ExtensionService* service = browser->profile()->GetExtensionService();
  ExtensionProcessManager* manager =
      browser->profile()->GetExtensionProcessManager();
  if (service && manager) {
    // The observer will delete itself when done.
    new ExtensionReadyNotificationObserver(
        manager,
        service,
        this,
        reply_message);

    FilePath extension_path(path_string);
    // If the given path has a 'crx' extension, assume it is a packed extension
    // and install it. Otherwise load it as an unpacked extension.
    if (extension_path.MatchesExtension(FILE_PATH_LITERAL(".crx"))) {
      ExtensionInstallUI* client =
          (with_ui ? new ExtensionInstallUI(browser->profile()) : NULL);
      scoped_refptr<CrxInstaller> installer(
          CrxInstaller::Create(service, client));
      if (!with_ui)
        installer->set_allow_silent_install(true);
      installer->set_install_cause(extension_misc::INSTALL_CAUSE_AUTOMATION);
      installer->InstallCrx(extension_path);
    } else {
      scoped_refptr<extensions::UnpackedInstaller> installer(
          extensions::UnpackedInstaller::Create(service));
      installer->set_prompt_for_plugins(with_ui);
      installer->Load(extension_path);
    }
  } else {
    AutomationJSONReply(this, reply_message).SendError(
        "Extensions service/process manager is not available");
  }
}

namespace {

ListValue* GetHostPermissions(const Extension* ext, bool effective_perm) {
  URLPatternSet pattern_set;
  if (effective_perm)
    pattern_set = ext->GetEffectiveHostPermissions();
  else
    pattern_set = ext->GetActivePermissions()->explicit_hosts();

  ListValue* permissions = new ListValue;
  for (URLPatternSet::const_iterator perm = pattern_set.begin();
       perm != pattern_set.end(); ++perm) {
    permissions->Append(new StringValue(perm->GetAsString()));
  }

  return permissions;
}

ListValue* GetAPIPermissions(const Extension* ext) {
  ListValue* permissions = new ListValue;
  std::set<std::string> perm_list =
      ext->GetActivePermissions()->GetAPIsAsStrings();
  for (std::set<std::string>::const_iterator perm = perm_list.begin();
       perm != perm_list.end(); ++perm) {
    permissions->Append(new StringValue(perm->c_str()));
  }
  return permissions;
}

}  // namespace

// Sample json input: { "command": "GetExtensionsInfo" }
// See GetExtensionsInfo() in chrome/test/pyautolib/pyauto.py for sample json
// output.
void TestingAutomationProvider::GetExtensionsInfo(
    DictionaryValue* args,
    IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);
  Browser* browser;
  std::string error_msg;
  if (!GetBrowserFromJSONArgs(args, &browser, &error_msg)) {
    reply.SendError(error_msg);
    return;
  }
  ExtensionService* service = browser->profile()->GetExtensionService();
  if (!service) {
    reply.SendError("No extensions service.");
    return;
  }
  scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
  ListValue* extensions_values = new ListValue;
  const ExtensionSet* extensions = service->extensions();
  const ExtensionSet* disabled_extensions = service->disabled_extensions();
  ExtensionList all;
  all.insert(all.end(),
             extensions->begin(),
             extensions->end());
  all.insert(all.end(),
             disabled_extensions->begin(),
             disabled_extensions->end());
  for (ExtensionList::const_iterator it = all.begin();
       it != all.end(); ++it) {
    const Extension* extension = *it;
    std::string id = extension->id();
    DictionaryValue* extension_value = new DictionaryValue;
    extension_value->SetString("id", id);
    extension_value->SetString("version", extension->VersionString());
    extension_value->SetString("name", extension->name());
    extension_value->SetString("public_key", extension->public_key());
    extension_value->SetString("description", extension->description());
    extension_value->SetString("background_url",
                               extension->GetBackgroundURL().spec());
    extension_value->SetString("options_url",
                               extension->options_url().spec());
    extension_value->Set("host_permissions",
                         GetHostPermissions(extension, false));
    extension_value->Set("effective_host_permissions",
                         GetHostPermissions(extension, true));
    extension_value->Set("api_permissions", GetAPIPermissions(extension));
    Extension::Location location = extension->location();
    extension_value->SetBoolean("is_component",
                                location == Extension::COMPONENT);
    extension_value->SetBoolean("is_internal", location == Extension::INTERNAL);
    extension_value->SetBoolean("is_user_installed",
        location == Extension::INTERNAL || location == Extension::LOAD);
    extension_value->SetBoolean("is_enabled", service->IsExtensionEnabled(id));
    extension_value->SetBoolean("allowed_in_incognito",
                                service->IsIncognitoEnabled(id));
    extension_value->SetBoolean("has_page_action",
                                extension->page_action() != NULL);
    extensions_values->Append(extension_value);
  }
  return_value->Set("extensions", extensions_values);
  reply.SendSuccess(return_value.get());
}

// See UninstallExtensionById() in chrome/test/pyautolib/pyauto.py for sample
// json input.
// Sample json output: {}
void TestingAutomationProvider::UninstallExtensionById(
    DictionaryValue* args,
    IPC::Message* reply_message) {
  const Extension* extension;
  std::string error;
  Browser* browser;
  if (!GetBrowserFromJSONArgs(args, &browser, &error)) {
    AutomationJSONReply(this, reply_message).SendError(error);
    return;
  }
  if (!GetExtensionFromJSONArgs(
          args, "id", browser->profile(), &extension, &error)) {
    AutomationJSONReply(this, reply_message).SendError(error);
    return;
  }
  ExtensionService* service = browser->profile()->GetExtensionService();
  if (!service) {
    AutomationJSONReply(this, reply_message).SendError(
        "No extensions service.");
    return;
  }

  // Wait for a notification indicating that the extension with the given ID
  // has been uninstalled.  This observer will delete itself.
  new ExtensionUninstallObserver(this, reply_message, extension->id());
  service->UninstallExtension(extension->id(), false, NULL);
}

// See SetExtensionStateById() in chrome/test/pyautolib/pyauto.py
// for sample json input.
void TestingAutomationProvider::SetExtensionStateById(
    DictionaryValue* args,
    IPC::Message* reply_message) {
  const Extension* extension;
  std::string error;
  Browser* browser;
  if (!GetBrowserFromJSONArgs(args, &browser, &error)) {
    AutomationJSONReply(this, reply_message).SendError(error);
    return;
  }
  if (!GetExtensionFromJSONArgs(
          args, "id", browser->profile(), &extension, &error)) {
    AutomationJSONReply(this, reply_message).SendError(error);
    return;
  }

  bool enable;
  if (!args->GetBoolean("enable", &enable)) {
    AutomationJSONReply(this, reply_message)
        .SendError("Missing or invalid key: enable");
    return;
  }

  bool allow_in_incognito;
  if (!args->GetBoolean("allow_in_incognito", &allow_in_incognito)) {
    AutomationJSONReply(this, reply_message)
        .SendError("Missing or invalid key: allow_in_incognito");
    return;
  }

  if (allow_in_incognito && !enable) {
    AutomationJSONReply(this, reply_message)
        .SendError("Invalid state: Disabled extension "
                    "cannot be allowed in incognito mode.");
    return;
  }

  ExtensionService* service = browser->profile()->GetExtensionService();
  ExtensionProcessManager* manager =
      browser->profile()->GetExtensionProcessManager();
  if (!service) {
    AutomationJSONReply(this, reply_message)
        .SendError("No extensions service or process manager.");
    return;
  }

  if (enable) {
    if (!service->IsExtensionEnabled(extension->id())) {
      new ExtensionReadyNotificationObserver(
          manager,
          service,
          this,
          reply_message);
      service->EnableExtension(extension->id());
    } else {
      AutomationJSONReply(this, reply_message).SendSuccess(NULL);
    }
  } else {
    service->DisableExtension(extension->id(), Extension::DISABLE_USER_ACTION);
    AutomationJSONReply(this, reply_message).SendSuccess(NULL);
  }

  service->SetIsIncognitoEnabled(extension->id(), allow_in_incognito);
}

namespace {

// Selects the given |browser| and |tab| if not selected already.
void EnsureTabSelected(Browser* browser, WebContents* tab) {
  if (browser->GetSelectedWebContents() != tab ||
      browser != BrowserList::GetLastActive()) {
    browser->ActivateTabAt(browser->GetIndexOfController(&tab->GetController()),
                           true /* user_gesture */);
  }
}

}  // namespace

// See TriggerPageActionById() in chrome/test/pyautolib/pyauto.py
// for sample json input.
void TestingAutomationProvider::TriggerPageActionById(
    DictionaryValue* args,
    IPC::Message* reply_message) {
  std::string error;
  Browser* browser;
  WebContents* tab;
  if (!GetBrowserAndTabFromJSONArgs(args, &browser, &tab, &error)) {
    AutomationJSONReply(this, reply_message).SendError(error);
    return;
  }
  const Extension* extension;
  if (!GetEnabledExtensionFromJSONArgs(
          args, "id", browser->profile(), &extension, &error)) {
    AutomationJSONReply(this, reply_message).SendError(error);
    return;
  }
  ExtensionAction* page_action = extension->page_action();
  if (!page_action) {
    AutomationJSONReply(this, reply_message).SendError(
        "Extension doesn't have any page action.");
    return;
  }
  EnsureTabSelected(browser, tab);

  bool pressed = false;
  LocationBarTesting* loc_bar =
      browser->window()->GetLocationBar()->GetLocationBarForTesting();
  size_t page_action_visible_count =
      static_cast<size_t>(loc_bar->PageActionVisibleCount());
  for (size_t i = 0; i < page_action_visible_count; ++i) {
    if (loc_bar->GetVisiblePageAction(i) == page_action) {
      loc_bar->TestPageActionPressed(i);
      pressed = true;
      break;
    }
  }
  if (!pressed) {
    AutomationJSONReply(this, reply_message).SendError(
        "Extension's page action is not visible.");
    return;
  }

  if (page_action->HasPopup(ExtensionTabUtil::GetTabId(tab))) {
    // This observer will delete itself.
    new ExtensionPopupObserver(
        this, reply_message, extension->id());
  } else {
    AutomationJSONReply(this, reply_message).SendSuccess(NULL);
  }
}

// See TriggerBrowserActionById() in chrome/test/pyautolib/pyauto.py
// for sample json input.
void TestingAutomationProvider::TriggerBrowserActionById(
    DictionaryValue* args,
    IPC::Message* reply_message) {
  std::string error;
  Browser* browser;
  WebContents* tab;
  if (!GetBrowserAndTabFromJSONArgs(args, &browser, &tab, &error)) {
    AutomationJSONReply(this, reply_message).SendError(error);
    return;
  }
  const Extension* extension;
  if (!GetEnabledExtensionFromJSONArgs(
          args, "id", browser->profile(), &extension, &error)) {
    AutomationJSONReply(this, reply_message).SendError(error);
    return;
  }
  ExtensionAction* action = extension->browser_action();
  if (!action) {
    AutomationJSONReply(this, reply_message).SendError(
        "Extension doesn't have any browser action.");
    return;
  }
  EnsureTabSelected(browser, tab);

  BrowserActionTestUtil browser_actions(browser);
  int num_browser_actions = browser_actions.NumberOfBrowserActions();
  int action_index = -1;
#if defined(TOOLKIT_VIEWS)
  for (int i = 0; i < num_browser_actions; ++i) {
    if (extension->id() == browser_actions.GetExtensionId(i)) {
      action_index = i;
      break;
    }
  }
#else
  // TODO(kkania): Implement the platform-specific GetExtensionId() in
  // BrowserActionTestUtil.
  if (num_browser_actions != 1) {
    AutomationJSONReply(this, reply_message).SendError(StringPrintf(
        "Found %d browser actions. Only one browser action must be active.",
        num_browser_actions));
    return;
  }
  // This extension has a browser action, and there's only one action, so this
  // must be the first one.
  action_index = 0;
#endif
  if (action_index == -1) {
    AutomationJSONReply(this, reply_message).SendError(
        "Extension's browser action is not visible.");
    return;
  }
  browser_actions.Press(action_index);

  if (action->HasPopup(ExtensionTabUtil::GetTabId(tab))) {
    // This observer will delete itself.
    new ExtensionPopupObserver(
        this, reply_message, extension->id());
  } else {
    AutomationJSONReply(this, reply_message).SendSuccess(NULL);
  }
}

// Sample json input: { "command": "UpdateExtensionsNow" }
// Sample json output: {}
void TestingAutomationProvider::UpdateExtensionsNow(
    DictionaryValue* args,
    IPC::Message* reply_message) {
  std::string error;
  Browser* browser;
  if (!GetBrowserFromJSONArgs(args, &browser, &error)) {
    AutomationJSONReply(this, reply_message).SendError(error);
    return;
  }
  ExtensionService* service = browser->profile()->GetExtensionService();
  if (!service) {
    AutomationJSONReply(this, reply_message).SendError(
        "No extensions service.");
    return;
  }

  extensions::ExtensionUpdater* updater = service->updater();
  if (!updater) {
    AutomationJSONReply(this, reply_message).SendError(
        "No updater for extensions service.");
    return;
  }

  ExtensionProcessManager* manager =
      browser->profile()->GetExtensionProcessManager();
  if (!manager) {
    AutomationJSONReply(this, reply_message).SendError(
        "No extension process manager.");
    return;
  }

  // Create a new observer that waits until the extensions have been fully
  // updated (we should not send the reply until after all extensions have
  // been updated).  This observer will delete itself.
  new ExtensionsUpdatedObserver(manager, this, reply_message);
  updater->CheckNow();
}

#if !defined(NO_TCMALLOC) && (defined(OS_LINUX) || defined(OS_CHROMEOS))
// Sample json input: { "command": "HeapProfilerDump",
//                      "process_type": "renderer",
//                      "reason": "Perf bot",
//                      "tab_index": 0,
//                      "windex": 0 }
// "auto_id" is acceptable instead of "tab_index" and "windex".
void TestingAutomationProvider::HeapProfilerDump(
    DictionaryValue* args,
    IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);

  std::string process_type_string;
  if (!args->GetString("process_type", &process_type_string)) {
    reply.SendError("No process type is specified");
    return;
  }

  std::string reason_string;
  if (args->GetString("reason", &reason_string))
    reason_string += " (via PyAuto testing)";
  else
    reason_string = "By PyAuto testing";

  if (process_type_string == "browser") {
    if (!::IsHeapProfilerRunning()) {
      reply.SendError("The heap profiler is not running");
      return;
    }
    ::HeapProfilerDump(reason_string.c_str());
    reply.SendSuccess(NULL);
    return;
  } else if (process_type_string == "renderer") {
    WebContents* web_contents;
    std::string error;

    if (!GetTabFromJSONArgs(args, &web_contents, &error)) {
      reply.SendError(error);
      return;
    }

    RenderViewHost* render_view = web_contents->GetRenderViewHost();
    if (!render_view) {
      reply.SendError("Tab has no associated RenderViewHost");
      return;
    }

    TabContentsWrapper* tab_contents =
        TabContentsWrapper::GetCurrentWrapperForContents(web_contents);
    tab_contents->automation_tab_helper()->HeapProfilerDump(reason_string);
    reply.SendSuccess(NULL);
    return;
  }

  reply.SendError("Process type is not supported");
}
#endif  // !defined(NO_TCMALLOC) && (defined(OS_LINUX) || defined(OS_CHROMEOS))

namespace {

void SendSuccessIfAlive(
    base::WeakPtr<AutomationProvider> provider,
    IPC::Message* reply_message) {
  if (provider)
    AutomationJSONReply(provider.get(), reply_message).SendSuccess(NULL);
}

}  // namespace

void TestingAutomationProvider::OverrideGeoposition(
    base::DictionaryValue* args,
    IPC::Message* reply_message) {
  double latitude, longitude, altitude;
  if (!args->GetDouble("latitude", &latitude) ||
      !args->GetDouble("longitude", &longitude) ||
      !args->GetDouble("altitude", &altitude)) {
    AutomationJSONReply(this, reply_message).SendError(
        "Missing or invalid geolocation parameters");
    return;
  }
  content::Geoposition position;
  position.latitude = latitude;
  position.longitude = longitude;
  position.altitude = altitude;
  position.accuracy = 0.;
  position.timestamp = base::Time::Now();
  content::OverrideLocationForTesting(
      position,
      base::Bind(&SendSuccessIfAlive, AsWeakPtr(), reply_message));
}

// Sample json input:
// { "command": "AppendSwitchASCIIToCommandLine",
//   "switch": "instant-field-trial",
//   "value": "disabled" }
void TestingAutomationProvider::AppendSwitchASCIIToCommandLine(
         DictionaryValue* args, IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);
  std::string switch_name, switch_value;
  if (!args->GetString("switch", &switch_name) ||
      !args->GetString("value", &switch_value)) {
    reply.SendError("Missing or invalid command line switch");
    return;
  }
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(switch_name,
                                                      switch_value);
  reply.SendSuccess(NULL);
}

// Sample json input:
//    { "command": "GetAutofillProfile" }
// Refer to GetAutofillProfile() in chrome/test/pyautolib/pyauto.py for sample
// json output.
void TestingAutomationProvider::GetAutofillProfile(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);
  // Get the AutofillProfiles currently in the database.
  int tab_index = 0;
  if (!args->GetInteger("tab_index", &tab_index)) {
    reply.SendError("Invalid or missing tab_index integer value.");
    return;
  }

  TabContentsWrapper* tab_contents =
      browser->GetTabContentsWrapperAt(tab_index);
  if (tab_contents) {
    PersonalDataManager* pdm = PersonalDataManagerFactory::GetForProfile(
        tab_contents->profile()->GetOriginalProfile());
    if (pdm) {
      std::vector<AutofillProfile*> autofill_profiles = pdm->profiles();
      std::vector<CreditCard*> credit_cards = pdm->credit_cards();

      ListValue* profiles = GetListFromAutofillProfiles(autofill_profiles);
      ListValue* cards = GetListFromCreditCards(credit_cards);

      scoped_ptr<DictionaryValue> return_value(new DictionaryValue);

      return_value->Set("profiles", profiles);
      return_value->Set("credit_cards", cards);
      reply.SendSuccess(return_value.get());
    } else {
      reply.SendError("No PersonalDataManager.");
      return;
    }
  } else {
    reply.SendError("No tab at that index.");
    return;
  }
}

// Refer to FillAutofillProfile() in chrome/test/pyautolib/pyauto.py for sample
// json input.
// Sample json output: {}
void TestingAutomationProvider::FillAutofillProfile(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  ListValue* profiles = NULL;
  ListValue* cards = NULL;

  // It's ok for profiles/credit_cards elements to be missing.
  args->GetList("profiles", &profiles);
  args->GetList("credit_cards", &cards);

  std::string error_mesg;

  std::vector<AutofillProfile> autofill_profiles;
  std::vector<CreditCard> credit_cards;
  // Create an AutofillProfile for each of the dictionary profiles.
  if (profiles) {
    autofill_profiles = GetAutofillProfilesFromList(*profiles, &error_mesg);
  }
  // Create a CreditCard for each of the dictionary values.
  if (cards) {
    credit_cards = GetCreditCardsFromList(*cards, &error_mesg);
  }
  if (!error_mesg.empty()) {
    AutomationJSONReply(this, reply_message).SendError(error_mesg);
    return;
  }

  // Save the AutofillProfiles.
  int tab_index = 0;
  if (!args->GetInteger("tab_index", &tab_index)) {
    AutomationJSONReply(this, reply_message).SendError(
        "Invalid or missing tab_index integer");
    return;
  }

  TabContentsWrapper* tab_contents =
      browser->GetTabContentsWrapperAt(tab_index);

  if (tab_contents) {
    PersonalDataManager* pdm =
        PersonalDataManagerFactory::GetForProfile(tab_contents->profile());
    if (pdm) {
      if (profiles || cards) {
        // This observer will delete itself.
        AutofillChangedObserver* observer = new AutofillChangedObserver(
            this, reply_message, autofill_profiles.size(), credit_cards.size());
        observer->Init();

        if (profiles)
          pdm->SetProfiles(&autofill_profiles);
        if (cards)
          pdm->SetCreditCards(&credit_cards);

        return;
      }
    } else {
      AutomationJSONReply(this, reply_message).SendError(
          "No PersonalDataManager.");
      return;
    }
  } else {
    AutomationJSONReply(this, reply_message).SendError("No tab at that index.");
    return;
  }
  AutomationJSONReply(this, reply_message).SendSuccess(NULL);
}

void TestingAutomationProvider::SubmitAutofillForm(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  if (SendErrorIfModalDialogActive(this, reply_message))
    return;

  int tab_index;
  if (!args->GetInteger("tab_index", &tab_index)) {
    AutomationJSONReply(this, reply_message)
        .SendError("'tab_index' missing or invalid.");
    return;
  }
  TabContentsWrapper* tab_contents =
      browser->GetTabContentsWrapperAt(tab_index);
  if (!tab_contents) {
    AutomationJSONReply(this, reply_message).SendError(
        StringPrintf("No such tab at index %d", tab_index));
    return;
  }

  string16 frame_xpath, javascript;
  if (!args->GetString("frame_xpath", &frame_xpath)) {
    AutomationJSONReply(this, reply_message)
        .SendError("'frame_xpath' missing or invalid.");
    return;
  }
  if (!args->GetString("javascript", &javascript)) {
    AutomationJSONReply(this, reply_message)
        .SendError("'javascript' missing or invalid.");
    return;
  }

  PersonalDataManager* pdm = PersonalDataManagerFactory::GetForProfile(
      tab_contents->profile()->GetOriginalProfile());
  if (!pdm) {
    AutomationJSONReply(this, reply_message)
        .SendError("No PersonalDataManager.");
    return;
  }

  // This observer will delete itself.
  new AutofillFormSubmittedObserver(this, reply_message, pdm);

  // Set the routing id of this message with the controller.
  // This routing id needs to be remembered for the reverse
  // communication while sending back the response of
  // this javascript execution.
  std::string set_automation_id;
  base::SStringPrintf(&set_automation_id,
                      "window.domAutomationController.setAutomationId(%d);",
                      reply_message->routing_id());
  tab_contents->web_contents()->GetRenderViewHost()->
      ExecuteJavascriptInWebFrame(frame_xpath, UTF8ToUTF16(set_automation_id));
  tab_contents->web_contents()->GetRenderViewHost()->
      ExecuteJavascriptInWebFrame(frame_xpath, javascript);
}

void TestingAutomationProvider::AutofillTriggerSuggestions(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  int tab_index;
  if (!args->GetInteger("tab_index", &tab_index)) {
    AutomationJSONReply(this, reply_message).SendError(
        "Invalid or missing args");
    return;
  }

  WebContents* web_contents = browser->GetWebContentsAt(tab_index);
  if (!web_contents) {
    AutomationJSONReply(this, reply_message).SendError(
        StringPrintf("No such tab at index %d", tab_index));
    return;
  }

  new AutofillDisplayedObserver(
      chrome::NOTIFICATION_AUTOFILL_DID_SHOW_SUGGESTIONS,
      web_contents->GetRenderViewHost(), this, reply_message);
  SendWebKeyPressEventAsync(ui::VKEY_DOWN, web_contents);
}

void TestingAutomationProvider::AutofillHighlightSuggestion(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  int tab_index;
  if (!args->GetInteger("tab_index", &tab_index)) {
    AutomationJSONReply(this, reply_message).SendError(
        "Invalid or missing args");
    return;
  }

  WebContents* web_contents = browser->GetWebContentsAt(tab_index);
  if (!web_contents) {
    AutomationJSONReply(this, reply_message).SendError(
        StringPrintf("No such tab at index %d", tab_index));
    return;
  }

  std::string direction;
  if (!args->GetString("direction", &direction) || (direction != "up" &&
                                                    direction != "down")) {
    AutomationJSONReply(this, reply_message).SendError(
        "Must specify a direction of either 'up' or 'down'.");
    return;
  }
  int key_code = (direction == "up") ? ui::VKEY_UP : ui::VKEY_DOWN;

  new AutofillDisplayedObserver(
      chrome::NOTIFICATION_AUTOFILL_DID_FILL_FORM_DATA,
      web_contents->GetRenderViewHost(), this, reply_message);
  SendWebKeyPressEventAsync(key_code, web_contents);
}

void TestingAutomationProvider::AutofillAcceptSelection(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  int tab_index;
  if (!args->GetInteger("tab_index", &tab_index)) {
    AutomationJSONReply(this, reply_message).SendError(
        "Invalid or missing args");
    return;
  }

  WebContents* web_contents = browser->GetWebContentsAt(tab_index);
  if (!web_contents) {
    AutomationJSONReply(this, reply_message).SendError(
        StringPrintf("No such tab at index %d", tab_index));
    return;
  }

  new AutofillDisplayedObserver(
      chrome::NOTIFICATION_AUTOFILL_DID_FILL_FORM_DATA,
      web_contents->GetRenderViewHost(), this, reply_message);
  SendWebKeyPressEventAsync(ui::VKEY_RETURN, web_contents);
}

// Sample json output: { "success": true }
void TestingAutomationProvider::SignInToSync(Browser* browser,
                                             DictionaryValue* args,
                                             IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);
  std::string username;
  std::string password;
  if (!args->GetString("username", &username) ||
      !args->GetString("password", &password)) {
      reply.SendError("Invalid or missing args");
      return;
  }
  if (sync_waiter_.get() == NULL) {
    sync_waiter_.reset(new ProfileSyncServiceHarness(
        browser->profile(), username, password));
  } else {
    sync_waiter_->SetCredentials(username, password);
  }
  if (sync_waiter_->SetupSync()) {
    scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
    return_value->SetBoolean("success", true);
    reply.SendSuccess(return_value.get());
  } else {
    reply.SendError("Signing in to sync was unsuccessful");
  }
}

// Sample json output:
// {u'summary': u'SYNC DISABLED'}
//
// { u'last synced': u'Just now',
//   u'sync url': u'clients4.google.com',
//   u'updates received': 42,
//   u'synced datatypes': [ u'Bookmarks',
//                          u'Preferences',
//                          u'Passwords',
//                          u'Autofill',
//                          u'Themes',
//                          u'Extensions',
//                          u'Apps']}
void TestingAutomationProvider::GetSyncInfo(Browser* browser,
                                            DictionaryValue* args,
                                            IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);
  DictionaryValue* sync_info = new DictionaryValue;
  scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
  if (sync_waiter_.get() == NULL) {
    sync_waiter_.reset(
        ProfileSyncServiceHarness::CreateAndAttach(browser->profile()));
  }
  if (!sync_waiter_->IsSyncAlreadySetup()) {
    sync_info->SetString("summary", "SYNC DISABLED");
  } else {
    ProfileSyncService* service = sync_waiter_->service();
    ProfileSyncService::Status status = sync_waiter_->GetStatus();
    sync_info->SetString("sync url", service->sync_service_url().host());
    sync_info->SetString("last synced", service->GetLastSyncedTimeString());
    sync_info->SetInteger("updates received", status.updates_received);
    ListValue* synced_datatype_list = new ListValue;
    const syncable::ModelTypeSet synced_datatypes =
        service->GetPreferredDataTypes();
    for (syncable::ModelTypeSet::Iterator it = synced_datatypes.First();
         it.Good(); it.Inc()) {
      synced_datatype_list->Append(
          new StringValue(syncable::ModelTypeToString(it.Get())));
    }
    sync_info->Set("synced datatypes", synced_datatype_list);
  }
  return_value->Set("sync_info", sync_info);
  reply.SendSuccess(return_value.get());
}

// Sample json output: { "success": true }
void TestingAutomationProvider::AwaitFullSyncCompletion(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);
  if (sync_waiter_.get() == NULL) {
    sync_waiter_.reset(
        ProfileSyncServiceHarness::CreateAndAttach(browser->profile()));
  }
  if (!sync_waiter_->IsSyncAlreadySetup()) {
    reply.SendError("Not signed in to sync");
    return;
  }
  // Ensure that the profile sync service and sync backend host are initialized
  // before waiting for sync cycle completion. In cases where the browser is
  // restarted with sync enabled, these operations may still be in flight.
  if (ProfileSyncServiceFactory::GetInstance()->GetForProfile(
          browser->profile()) == NULL) {
    reply.SendError("ProfileSyncService initialization failed.");
    return;
  }
  if (!sync_waiter_->service()->sync_initialized() &&
      !sync_waiter_->AwaitBackendInitialized()) {
    reply.SendError("Sync backend host initialization failed.");
    return;
  }
  if (!sync_waiter_->AwaitFullSyncCompletion("Waiting for sync cycle")) {
    reply.SendError("Sync cycle did not complete.");
    return;
  }
  scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
  return_value->SetBoolean("success", true);
  reply.SendSuccess(return_value.get());
}

// Sample json output: { "success": true }
void TestingAutomationProvider::AwaitSyncRestart(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);
  if (sync_waiter_.get() == NULL) {
    sync_waiter_.reset(
        ProfileSyncServiceHarness::CreateAndAttach(browser->profile()));
  }
  if (!sync_waiter_->IsSyncAlreadySetup()) {
    reply.SendError("Not signed in to sync");
    return;
  }
  if (ProfileSyncServiceFactory::GetInstance()->GetForProfile(
          browser->profile()) == NULL) {
    reply.SendError("ProfileSyncService initialization failed.");
    return;
  }
  if (!sync_waiter_->AwaitSyncRestart()) {
    reply.SendError("Sync did not successfully restart.");
    return;
  }
  scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
  return_value->SetBoolean("success", true);
  reply.SendSuccess(return_value.get());
}

// Refer to EnableSyncForDatatypes() in chrome/test/pyautolib/pyauto.py for
// sample json input. Sample json output: { "success": true }
void TestingAutomationProvider::EnableSyncForDatatypes(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);
  if (sync_waiter_.get() == NULL) {
    sync_waiter_.reset(
        ProfileSyncServiceHarness::CreateAndAttach(browser->profile()));
  }
  if (!sync_waiter_->IsSyncAlreadySetup()) {
    reply.SendError("Not signed in to sync");
    return;
  }
  ListValue* datatypes = NULL;
  if (!args->GetList("datatypes", &datatypes)) {
    reply.SendError("Invalid or missing args");
    return;
  }
  std::string first_datatype;
  datatypes->GetString(0, &first_datatype);
  if (first_datatype == "All") {
    sync_waiter_->EnableSyncForAllDatatypes();
  } else {
    int num_datatypes = datatypes->GetSize();
    for (int i = 0; i < num_datatypes; ++i) {
      std::string datatype_string;
      datatypes->GetString(i, &datatype_string);
      syncable::ModelType datatype =
          syncable::ModelTypeFromString(datatype_string);
      if (datatype == syncable::UNSPECIFIED) {
        AutomationJSONReply(this, reply_message).SendError(StringPrintf(
            "Invalid datatype string: %s.", datatype_string.c_str()));
        return;
      }
      sync_waiter_->EnableSyncForDatatype(datatype);
      sync_waiter_->AwaitFullSyncCompletion(StringPrintf(
          "Enabling datatype: %s", datatype_string.c_str()));
    }
  }
  scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
  return_value->SetBoolean("success", true);
  reply.SendSuccess(return_value.get());
}

// Refer to DisableSyncForDatatypes() in chrome/test/pyautolib/pyauto.py for
// sample json input. Sample json output: { "success": true }
void TestingAutomationProvider::DisableSyncForDatatypes(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);
  if (sync_waiter_.get() == NULL) {
    sync_waiter_.reset(
        ProfileSyncServiceHarness::CreateAndAttach(browser->profile()));
  }
  if (!sync_waiter_->IsSyncAlreadySetup()) {
    reply.SendError("Not signed in to sync");
    return;
  }
  ListValue* datatypes = NULL;
  if (!args->GetList("datatypes", &datatypes)) {
    reply.SendError("Invalid or missing args");
    return;
  }
  std::string first_datatype;
  if (!datatypes->GetString(0, &first_datatype)) {
    reply.SendError("Invalid or missing string");
    return;
  }
  if (first_datatype == "All") {
    sync_waiter_->DisableSyncForAllDatatypes();
  } else {
    int num_datatypes = datatypes->GetSize();
    for (int i = 0; i < num_datatypes; i++) {
      std::string datatype_string;
      datatypes->GetString(i, &datatype_string);
      syncable::ModelType datatype =
          syncable::ModelTypeFromString(datatype_string);
      if (datatype == syncable::UNSPECIFIED) {
        AutomationJSONReply(this, reply_message).SendError(StringPrintf(
            "Invalid datatype string: %s.", datatype_string.c_str()));
        return;
      }
      sync_waiter_->DisableSyncForDatatype(datatype);
      sync_waiter_->AwaitFullSyncCompletion(StringPrintf(
          "Disabling datatype: %s", datatype_string.c_str()));
    }
    scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
    return_value->SetBoolean("success", true);
    reply.SendSuccess(return_value.get());
  }
}

/* static */
ListValue* TestingAutomationProvider::GetListFromAutofillProfiles(
    const std::vector<AutofillProfile*>& autofill_profiles) {
  ListValue* profiles = new ListValue;

  std::map<AutofillFieldType, std::string> autofill_type_to_string
      = GetAutofillFieldToStringMap();

  // For each AutofillProfile, transform it to a dictionary object to return.
  for (std::vector<AutofillProfile*>::const_iterator it =
           autofill_profiles.begin();
       it != autofill_profiles.end(); ++it) {
    AutofillProfile* profile = *it;
    DictionaryValue* profile_info = new DictionaryValue;
    // For each of the types, if it has one or more values, add those values
    // to the dictionary.
    for (std::map<AutofillFieldType, std::string>::iterator
         type_it = autofill_type_to_string.begin();
         type_it != autofill_type_to_string.end(); ++type_it) {
      std::vector<string16> value_list;
      profile->GetMultiInfo(type_it->first, &value_list);
      ListValue* values_to_return = new ListValue;
      for (std::vector<string16>::iterator value_it = value_list.begin();
           value_it != value_list.end(); ++value_it) {
        string16 value = *value_it;
        if (value.length())  // If there was something stored for that value.
          values_to_return->Append(new StringValue(value));
      }
      if (!values_to_return->empty())
        profile_info->Set(type_it->second, values_to_return);
    }
    profiles->Append(profile_info);
  }
  return profiles;
}

/* static */
ListValue* TestingAutomationProvider::GetListFromCreditCards(
    const std::vector<CreditCard*>& credit_cards) {
  ListValue* cards = new ListValue;

  std::map<AutofillFieldType, std::string> credit_card_type_to_string =
      GetCreditCardFieldToStringMap();

  // For each AutofillProfile, transform it to a dictionary object to return.
  for (std::vector<CreditCard*>::const_iterator it =
           credit_cards.begin();
       it != credit_cards.end(); ++it) {
    CreditCard* card = *it;
    DictionaryValue* card_info = new DictionaryValue;
    // For each of the types, if it has a value, add it to the dictionary.
    for (std::map<AutofillFieldType, std::string>::iterator type_it =
        credit_card_type_to_string.begin();
        type_it != credit_card_type_to_string.end(); ++type_it) {
      string16 value = card->GetInfo(type_it->first);
      // If there was something stored for that value.
      if (value.length()) {
        card_info->SetString(type_it->second, value);
      }
    }
    cards->Append(card_info);
  }
  return cards;
}

/* static */
std::vector<AutofillProfile>
TestingAutomationProvider::GetAutofillProfilesFromList(
    const ListValue& profiles, std::string* error_message) {
  std::vector<AutofillProfile> autofill_profiles;
  DictionaryValue* profile_info = NULL;
  ListValue* current_value = NULL;

  std::map<AutofillFieldType, std::string> autofill_type_to_string =
      GetAutofillFieldToStringMap();

  int num_profiles = profiles.GetSize();
  for (int i = 0; i < num_profiles; i++) {
    profiles.GetDictionary(i, &profile_info);
    AutofillProfile profile;
    // Loop through the possible profile types and add those provided.
    for (std::map<AutofillFieldType, std::string>::iterator type_it =
         autofill_type_to_string.begin();
         type_it != autofill_type_to_string.end(); ++type_it) {
      if (profile_info->HasKey(type_it->second)) {
        if (profile_info->GetList(type_it->second, &current_value)) {
          std::vector<string16> value_list;
          for (ListValue::iterator list_it = current_value->begin();
               list_it != current_value->end(); ++list_it) {
            string16 value;
            if ((*list_it)->GetAsString(&value)) {
              value_list.insert(value_list.end(), value);
            } else {
              *error_message = "All list values must be strings";
              return autofill_profiles;
            }
          }
          profile.SetMultiInfo(type_it->first, value_list);
        } else {
          *error_message= "All values must be lists";
          return autofill_profiles;
        }
      }
    }
    autofill_profiles.push_back(profile);
  }
  return autofill_profiles;
}

/* static */
std::vector<CreditCard> TestingAutomationProvider::GetCreditCardsFromList(
    const ListValue& cards, std::string* error_message) {
  std::vector<CreditCard> credit_cards;
  DictionaryValue* card_info = NULL;
  string16 current_value;

  std::map<AutofillFieldType, std::string> credit_card_type_to_string =
      GetCreditCardFieldToStringMap();

  int num_credit_cards = cards.GetSize();
  for (int i = 0; i < num_credit_cards; i++) {
    if (!cards.GetDictionary(i, &card_info))
      continue;
    CreditCard card;
    // Loop through the possible credit card fields and add those provided.
    for (std::map<AutofillFieldType, std::string>::iterator type_it =
        credit_card_type_to_string.begin();
        type_it != credit_card_type_to_string.end(); ++type_it) {
      if (card_info->HasKey(type_it->second)) {
        if (card_info->GetString(type_it->second, &current_value)) {
          card.SetInfo(type_it->first, current_value);
        } else {
          *error_message= "All values must be strings";
          break;
        }
      }
    }
    credit_cards.push_back(card);
  }
  return credit_cards;
}

/* static */
std::map<AutofillFieldType, std::string>
    TestingAutomationProvider::GetAutofillFieldToStringMap() {
  std::map<AutofillFieldType, std::string> autofill_type_to_string;
  // Strings ordered by order of fields when adding a profile in Autofill prefs.
  autofill_type_to_string[NAME_FIRST] = "NAME_FIRST";
  autofill_type_to_string[NAME_MIDDLE] = "NAME_MIDDLE";
  autofill_type_to_string[NAME_LAST] = "NAME_LAST";
  autofill_type_to_string[COMPANY_NAME] = "COMPANY_NAME";
  autofill_type_to_string[EMAIL_ADDRESS] = "EMAIL_ADDRESS";
  autofill_type_to_string[ADDRESS_HOME_LINE1] = "ADDRESS_HOME_LINE1";
  autofill_type_to_string[ADDRESS_HOME_LINE2] = "ADDRESS_HOME_LINE2";
  autofill_type_to_string[ADDRESS_HOME_CITY] = "ADDRESS_HOME_CITY";
  autofill_type_to_string[ADDRESS_HOME_STATE] = "ADDRESS_HOME_STATE";
  autofill_type_to_string[ADDRESS_HOME_ZIP] = "ADDRESS_HOME_ZIP";
  autofill_type_to_string[ADDRESS_HOME_COUNTRY] = "ADDRESS_HOME_COUNTRY";
  autofill_type_to_string[PHONE_HOME_COUNTRY_CODE] =
      "PHONE_HOME_COUNTRY_CODE";
  autofill_type_to_string[PHONE_HOME_CITY_CODE] = "PHONE_HOME_CITY_CODE";
  autofill_type_to_string[PHONE_HOME_WHOLE_NUMBER] =
      "PHONE_HOME_WHOLE_NUMBER";
  return autofill_type_to_string;
}

/* static */
std::map<AutofillFieldType, std::string>
    TestingAutomationProvider::GetCreditCardFieldToStringMap() {
  std::map<AutofillFieldType, std::string> credit_card_type_to_string;
  credit_card_type_to_string[CREDIT_CARD_NAME] = "CREDIT_CARD_NAME";
  credit_card_type_to_string[CREDIT_CARD_NUMBER] = "CREDIT_CARD_NUMBER";
  credit_card_type_to_string[CREDIT_CARD_EXP_MONTH] = "CREDIT_CARD_EXP_MONTH";
  credit_card_type_to_string[CREDIT_CARD_EXP_4_DIGIT_YEAR] =
      "CREDIT_CARD_EXP_4_DIGIT_YEAR";
  return credit_card_type_to_string;
}

// Refer to GetAllNotifications() in chrome/test/pyautolib/pyauto.py for
// sample json input/output.
void TestingAutomationProvider::GetAllNotifications(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  new GetAllNotificationsObserver(this, reply_message);
}

// Refer to CloseNotification() in chrome/test/pyautolib/pyauto.py for
// sample json input.
// Returns empty json message.
void TestingAutomationProvider::CloseNotification(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  int index;
  if (!args->GetInteger("index", &index)) {
    AutomationJSONReply(this, reply_message)
        .SendError("'index' missing or invalid.");
    return;
  }
  NotificationUIManager* manager = g_browser_process->notification_ui_manager();
  BalloonCollection* collection = manager->balloon_collection();
  const BalloonCollection::Balloons& balloons = collection->GetActiveBalloons();
  int balloon_count = static_cast<int>(balloons.size());
  if (index < 0 || index >= balloon_count) {
    AutomationJSONReply(this, reply_message)
        .SendError(StringPrintf("No notification at index %d", index));
    return;
  }
  std::vector<const Notification*> queued_notes;
  manager->GetQueuedNotificationsForTesting(&queued_notes);
  if (queued_notes.empty()) {
    new OnNotificationBalloonCountObserver(
        this, reply_message, balloon_count - 1);
  } else {
    new NewNotificationBalloonObserver(this, reply_message);
  }
  manager->CancelById(balloons[index]->notification().notification_id());
}

// Refer to WaitForNotificationCount() in chrome/test/pyautolib/pyauto.py for
// sample json input.
// Returns empty json message.
void TestingAutomationProvider::WaitForNotificationCount(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  int count;
  if (!args->GetInteger("count", &count)) {
    AutomationJSONReply(this, reply_message)
        .SendError("'count' missing or invalid.");
    return;
  }
  // This will delete itself when finished.
  new OnNotificationBalloonCountObserver(this, reply_message, count);
}

// Sample JSON input: { "command": "GetNTPInfo" }
// For output, refer to chrome/test/pyautolib/ntp_model.py.
void TestingAutomationProvider::GetNTPInfo(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  // This observer will delete itself.
  new NTPInfoObserver(this, reply_message, &consumer_);
}

void TestingAutomationProvider::RemoveNTPMostVisitedThumbnail(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);
  std::string url;
  if (!args->GetString("url", &url)) {
    reply.SendError("Missing or invalid 'url' key.");
    return;
  }
  history::TopSites* top_sites = browser->profile()->GetTopSites();
  if (!top_sites) {
    reply.SendError("TopSites service is not initialized.");
    return;
  }
  top_sites->AddBlacklistedURL(GURL(url));
  reply.SendSuccess(NULL);
}

void TestingAutomationProvider::RestoreAllNTPMostVisitedThumbnails(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);
  history::TopSites* top_sites = browser->profile()->GetTopSites();
  if (!top_sites) {
    reply.SendError("TopSites service is not initialized.");
    return;
  }
  top_sites->ClearBlacklistedURLs();
  reply.SendSuccess(NULL);
}

void TestingAutomationProvider::KillRendererProcess(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  int pid;
  if (!args->GetInteger("pid", &pid)) {
    AutomationJSONReply(this, reply_message)
        .SendError("'pid' key missing or invalid.");
    return;
  }
  base::ProcessHandle process;
  if (!base::OpenProcessHandle(static_cast<base::ProcessId>(pid), &process)) {
    AutomationJSONReply(this, reply_message).SendError(base::StringPrintf(
        "Failed to open process handle for pid %d", pid));
    return;
  }
  new RendererProcessClosedObserver(this, reply_message);
  base::KillProcess(process, 0, false);
  base::CloseProcessHandle(process);
}

bool TestingAutomationProvider::BuildWebKeyEventFromArgs(
    DictionaryValue* args,
    std::string* error,
    NativeWebKeyboardEvent* event) {
  int type, modifiers;
  bool is_system_key;
  string16 unmodified_text, text;
  std::string key_identifier;
  if (!args->GetInteger("type", &type)) {
    *error = "'type' missing or invalid.";
    return false;
  }
  if (!args->GetBoolean("isSystemKey", &is_system_key)) {
    *error = "'isSystemKey' missing or invalid.";
    return false;
  }
  if (!args->GetString("unmodifiedText", &unmodified_text)) {
    *error = "'unmodifiedText' missing or invalid.";
    return false;
  }
  if (!args->GetString("text", &text)) {
    *error = "'text' missing or invalid.";
    return false;
  }
  if (!args->GetInteger("nativeKeyCode", &event->nativeKeyCode)) {
    *error = "'nativeKeyCode' missing or invalid.";
    return false;
  }
  if (!args->GetInteger("windowsKeyCode", &event->windowsKeyCode)) {
    *error = "'windowsKeyCode' missing or invalid.";
    return false;
  }
  if (!args->GetInteger("modifiers", &modifiers)) {
    *error = "'modifiers' missing or invalid.";
    return false;
  }
  if (args->GetString("keyIdentifier", &key_identifier)) {
    base::strlcpy(event->keyIdentifier,
                  key_identifier.c_str(),
                  WebKit::WebKeyboardEvent::keyIdentifierLengthCap);
  } else {
    event->setKeyIdentifierFromWindowsKeyCode();
  }

  if (type == automation::kRawKeyDownType) {
    event->type = WebKit::WebInputEvent::RawKeyDown;
  } else if (type == automation::kKeyDownType) {
    event->type = WebKit::WebInputEvent::KeyDown;
  } else if (type == automation::kKeyUpType) {
    event->type = WebKit::WebInputEvent::KeyUp;
  } else if (type == automation::kCharType) {
    event->type = WebKit::WebInputEvent::Char;
  } else {
    *error = "'type' refers to an unrecognized keyboard event type";
    return false;
  }

  string16 unmodified_text_truncated = unmodified_text.substr(
      0, WebKit::WebKeyboardEvent::textLengthCap - 1);
  memcpy(event->unmodifiedText,
         unmodified_text_truncated.c_str(),
         unmodified_text_truncated.length() + 1);
  string16 text_truncated = text.substr(
      0, WebKit::WebKeyboardEvent::textLengthCap - 1);
  memcpy(event->text, text_truncated.c_str(), text_truncated.length() + 1);

  event->modifiers = 0;
  if (modifiers & automation::kShiftKeyMask)
    event->modifiers |= WebKit::WebInputEvent::ShiftKey;
  if (modifiers & automation::kControlKeyMask)
    event->modifiers |= WebKit::WebInputEvent::ControlKey;
  if (modifiers & automation::kAltKeyMask)
    event->modifiers |= WebKit::WebInputEvent::AltKey;
  if (modifiers & automation::kMetaKeyMask)
    event->modifiers |= WebKit::WebInputEvent::MetaKey;

  event->isSystemKey = is_system_key;
  event->timeStampSeconds = base::Time::Now().ToDoubleT();
  event->skip_in_browser = true;
  return true;
}

void TestingAutomationProvider::BuildSimpleWebKeyEvent(
    WebKit::WebInputEvent::Type type,
    int windows_key_code,
    NativeWebKeyboardEvent* event) {
  event->nativeKeyCode = 0;
  event->windowsKeyCode = windows_key_code;
  event->setKeyIdentifierFromWindowsKeyCode();
  event->type = type;
  event->modifiers = 0;
  event->isSystemKey = false;
  event->timeStampSeconds = base::Time::Now().ToDoubleT();
  event->skip_in_browser = true;
}

void TestingAutomationProvider::SendWebKeyPressEventAsync(
    int key_code,
    WebContents* web_contents) {
  // Create and send a "key down" event for the specified key code.
  NativeWebKeyboardEvent event_down;
  BuildSimpleWebKeyEvent(WebKit::WebInputEvent::RawKeyDown, key_code,
                         &event_down);
  web_contents->GetRenderViewHost()->ForwardKeyboardEvent(event_down);

  // Create and send a corresponding "key up" event.
  NativeWebKeyboardEvent event_up;
  BuildSimpleWebKeyEvent(WebKit::WebInputEvent::KeyUp, key_code, &event_up);
  web_contents->GetRenderViewHost()->ForwardKeyboardEvent(event_up);
}

void TestingAutomationProvider::SendWebkitKeyEvent(
    DictionaryValue* args,
    IPC::Message* reply_message) {
  if (SendErrorIfModalDialogActive(this, reply_message))
    return;

  NativeWebKeyboardEvent event;
  // In the event of an error, BuildWebKeyEventFromArgs handles telling what
  // went wrong and sending the reply message; if it fails, we just have to
  // stop here.
  std::string error;
  if (!BuildWebKeyEventFromArgs(args, &error, &event)) {
    AutomationJSONReply(this, reply_message).SendError(error);
    return;
  }

  RenderViewHost* view;
  if (!GetRenderViewFromJSONArgs(args, profile(), &view, &error)) {
    AutomationJSONReply(this, reply_message).SendError(error);
    return;
  }
  new InputEventAckNotificationObserver(this, reply_message, event.type, 1);
  view->ForwardKeyboardEvent(event);
}

void TestingAutomationProvider::SendOSLevelKeyEventToTab(
    DictionaryValue* args,
    IPC::Message* reply_message) {
  if (SendErrorIfModalDialogActive(this, reply_message))
    return;

  int modifiers, keycode;
  if (!args->GetInteger("keyCode", &keycode)) {
    AutomationJSONReply(this, reply_message)
        .SendError("'keyCode' missing or invalid.");
    return;
  }
  if (!args->GetInteger("modifiers", &modifiers)) {
    AutomationJSONReply(this, reply_message)
        .SendError("'modifiers' missing or invalid.");
    return;
  }

  std::string error;
  Browser* browser;
  WebContents* web_contents;
  if (!GetBrowserAndTabFromJSONArgs(args, &browser, &web_contents, &error)) {
    AutomationJSONReply(this, reply_message).SendError(error);
    return;
  }
  // The key events will be sent to the browser window, we need the current tab
  // containing the element we send the text in to be shown.
  browser->ActivateTabAt(
      browser->GetIndexOfController(&web_contents->GetController()), true);

  BrowserWindow* browser_window = browser->window();
  if (!browser_window) {
    AutomationJSONReply(this, reply_message)
        .SendError("Could not get the browser window");
    return;
  }
  gfx::NativeWindow window = browser_window->GetNativeHandle();
  if (!window) {
    AutomationJSONReply(this, reply_message)
        .SendError("Could not get the browser window handle");
    return;
  }

  bool control = !!(modifiers & automation::kControlKeyMask);
  bool shift = !!(modifiers & automation::kShiftKeyMask);
  bool alt = !!(modifiers & automation::kAltKeyMask);
  bool meta = !!(modifiers & automation::kMetaKeyMask);
  if (!ui_controls::SendKeyPressNotifyWhenDone(
          window, static_cast<ui::KeyboardCode>(keycode),
          control, shift, alt, meta,
          base::Bind(SendSuccessReply, AsWeakPtr(), reply_message))) {
    AutomationJSONReply(this, reply_message)
        .SendError("Could not send the native key event");
  }
}

void TestingAutomationProvider::ProcessWebMouseEvent(
    DictionaryValue* args,
    IPC::Message* reply_message) {
  if (SendErrorIfModalDialogActive(this, reply_message))
    return;

  RenderViewHost* view;
  std::string error;
  if (!GetRenderViewFromJSONArgs(args, profile(), &view, &error)) {
    AutomationJSONReply(this, reply_message).SendError(error);
    return;
  }

  int type;
  int button;
  int modifiers;
  WebKit::WebMouseEvent event;
  if (!args->GetInteger("type", &type) ||
      !args->GetInteger("button", &button) ||
      !args->GetInteger("x", &event.x) ||
      !args->GetInteger("y", &event.y) ||
      !args->GetInteger("click_count", &event.clickCount) ||
      !args->GetInteger("modifiers", &modifiers)) {
    AutomationJSONReply(this, reply_message)
        .SendError("WebMouseEvent has missing or invalid parameters");
    return;
  }
  if (type == automation::kMouseDown) {
    event.type = WebKit::WebInputEvent::MouseDown;
  } else if (type == automation::kMouseUp) {
    event.type = WebKit::WebInputEvent::MouseUp;
  } else if (type == automation::kMouseMove) {
    event.type = WebKit::WebInputEvent::MouseMove;
  } else if (type == automation::kMouseEnter) {
    event.type = WebKit::WebInputEvent::MouseEnter;
  } else if (type == automation::kMouseLeave) {
    event.type = WebKit::WebInputEvent::MouseLeave;
  } else if (type == automation::kContextMenu) {
    event.type = WebKit::WebInputEvent::ContextMenu;
  } else {
    AutomationJSONReply(this, reply_message)
        .SendError("'type' refers to an unrecognized mouse event type");
    return;
  }
  if (button == automation::kLeftButton) {
    event.button = WebKit::WebMouseEvent::ButtonLeft;
  } else if (button == automation::kMiddleButton) {
    event.button = WebKit::WebMouseEvent::ButtonMiddle;
  } else if (button == automation::kRightButton) {
    event.button = WebKit::WebMouseEvent::ButtonRight;
  } else if (button == automation::kNoButton) {
    event.button = WebKit::WebMouseEvent::ButtonNone;
  } else {
    AutomationJSONReply(this, reply_message)
        .SendError("'button' refers to an unrecognized button");
    return;
  }
  event.modifiers = 0;
  if (modifiers & automation::kShiftKeyMask)
    event.modifiers |= WebKit::WebInputEvent::ShiftKey;
  if (modifiers & automation::kControlKeyMask)
    event.modifiers |= WebKit::WebInputEvent::ControlKey;
  if (modifiers & automation::kAltKeyMask)
    event.modifiers |= WebKit::WebInputEvent::AltKey;
  if (modifiers & automation::kMetaKeyMask)
    event.modifiers |= WebKit::WebInputEvent::MetaKey;

  view->ForwardMouseEvent(event);
  new InputEventAckNotificationObserver(this, reply_message, event.type,
                                        1);
}

namespace {

// Gets the active JavaScript modal dialog, or NULL if none.
JavaScriptAppModalDialog* GetActiveJavaScriptModalDialog(
    ErrorCode* error_code) {
  AppModalDialogQueue* dialog_queue = AppModalDialogQueue::GetInstance();
  if (!dialog_queue->HasActiveDialog() ||
      !dialog_queue->active_dialog()->IsJavaScriptModalDialog()) {
    *error_code = automation::kNoJavaScriptModalDialogOpen;
    return NULL;
  }
  return static_cast<JavaScriptAppModalDialog*>(dialog_queue->active_dialog());
}

}  // namespace

void TestingAutomationProvider::GetAppModalDialogMessage(
    DictionaryValue* args, IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);
  ErrorCode code;
  JavaScriptAppModalDialog* dialog = GetActiveJavaScriptModalDialog(&code);
  if (!dialog) {
    reply.SendErrorCode(code);
    return;
  }
  DictionaryValue result_dict;
  result_dict.SetString("message", UTF16ToUTF8(dialog->message_text()));
  reply.SendSuccess(&result_dict);
}

void TestingAutomationProvider::AcceptOrDismissAppModalDialog(
    DictionaryValue* args, IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);
  bool accept;
  if (!args->GetBoolean("accept", &accept)) {
    reply.SendError("Missing or invalid 'accept'");
    return;
  }

  ErrorCode code;
  JavaScriptAppModalDialog* dialog = GetActiveJavaScriptModalDialog(&code);
  if (!dialog) {
    reply.SendErrorCode(code);
    return;
  }
  if (accept) {
    std::string prompt_text;
    if (args->GetString("prompt_text", &prompt_text))
      dialog->SetOverridePromptText(UTF8ToUTF16(prompt_text));
    dialog->native_dialog()->AcceptAppModalDialog();
  } else {
    dialog->native_dialog()->CancelAppModalDialog();
  }
  reply.SendSuccess(NULL);
}

// Sample JSON input: { "command": "LaunchApp",
//                      "id": "ahfgeienlihckogmohjhadlkjgocpleb" }
// Sample JSON output: {}
void TestingAutomationProvider::LaunchApp(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  std::string id;
  if (!args->GetString("id", &id)) {
    AutomationJSONReply(this, reply_message).SendError(
        "Must include string id.");
    return;
  }

  ExtensionService* service = browser->profile()->GetExtensionService();
  if (!service) {
    AutomationJSONReply(this, reply_message).SendError(
        "No extensions service.");
    return;
  }

  const Extension* extension = service->GetExtensionById(
      id, false  /* do not include disabled extensions */);
  if (!extension) {
    AutomationJSONReply(this, reply_message).SendError(
        StringPrintf("Extension with ID '%s' doesn't exist or is disabled.",
                     id.c_str()));
    return;
  }

  // Look at preferences to find the right launch container.  If no preference
  // is set, launch as a regular tab.
  extension_misc::LaunchContainer launch_container =
      service->extension_prefs()->GetLaunchContainer(
          extension, ExtensionPrefs::LAUNCH_REGULAR);

  WebContents* old_contents = browser->GetSelectedWebContents();
  if (!old_contents) {
    AutomationJSONReply(this, reply_message).SendError(
        "Cannot identify selected tab contents.");
    return;
  }

  // This observer will delete itself.
  new AppLaunchObserver(&old_contents->GetController(), this, reply_message,
                        launch_container);
  Browser::OpenApplication(profile(), extension, launch_container, GURL(),
                           CURRENT_TAB);
}

// Sample JSON input: { "command": "SetAppLaunchType",
//                      "id": "ahfgeienlihckogmohjhadlkjgocpleb",
//                      "launch_type": "pinned" }
// Sample JSON output: {}
void TestingAutomationProvider::SetAppLaunchType(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);

  std::string id;
  if (!args->GetString("id", &id)) {
    reply.SendError("Must include string id.");
    return;
  }

  std::string launch_type_str;
  if (!args->GetString("launch_type", &launch_type_str)) {
    reply.SendError("Must specify app launch type.");
    return;
  }

  ExtensionService* service = browser->profile()->GetExtensionService();
  if (!service) {
    reply.SendError("No extensions service.");
    return;
  }

  const Extension* extension = service->GetExtensionById(
      id, true  /* include disabled extensions */);
  if (!extension) {
    reply.SendError(
        StringPrintf("Extension with ID '%s' doesn't exist.", id.c_str()));
    return;
  }

  ExtensionPrefs::LaunchType launch_type;
  if (launch_type_str == "pinned") {
    launch_type = ExtensionPrefs::LAUNCH_PINNED;
  } else if (launch_type_str == "regular") {
    launch_type = ExtensionPrefs::LAUNCH_REGULAR;
  } else if (launch_type_str == "fullscreen") {
    launch_type = ExtensionPrefs::LAUNCH_FULLSCREEN;
  } else if (launch_type_str == "window") {
    launch_type = ExtensionPrefs::LAUNCH_WINDOW;
  } else {
    reply.SendError(
        StringPrintf("Unexpected launch type '%s'.", launch_type_str.c_str()));
    return;
  }

  service->extension_prefs()->SetLaunchType(extension->id(), launch_type);
  reply.SendSuccess(NULL);
}

// Sample json input: { "command": "GetV8HeapStats",
//                      "tab_index": 0 }
// Refer to GetV8HeapStats() in chrome/test/pyautolib/pyauto.py for
// sample json output.
void TestingAutomationProvider::GetV8HeapStats(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  WebContents* web_contents;
  int tab_index;
  std::string error;

  if (!args->GetInteger("tab_index", &tab_index)) {
    AutomationJSONReply(this, reply_message).SendError(
        "Missing 'tab_index' argument.");
    return;
  }

  web_contents = browser->GetWebContentsAt(tab_index);
  if (!web_contents) {
    AutomationJSONReply(this, reply_message).SendError(
        StringPrintf("Could not get WebContents at tab index %d", tab_index));
    return;
  }

  RenderViewHost* render_view = web_contents->GetRenderViewHost();

  // This observer will delete itself.
  new V8HeapStatsObserver(
      this, reply_message,
      base::GetProcId(render_view->GetProcess()->GetHandle()));
  render_view->Send(new ChromeViewMsg_GetV8HeapStats);
}

// Sample json input: { "command": "GetFPS",
//                      "tab_index": 0 }
// Refer to GetFPS() in chrome/test/pyautolib/pyauto.py for
// sample json output.
void TestingAutomationProvider::GetFPS(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  WebContents* web_contents;
  int tab_index;
  std::string error;

  if (!args->GetInteger("tab_index", &tab_index)) {
    AutomationJSONReply(this, reply_message).SendError(
        "Missing 'tab_index' argument.");
    return;
  }

  web_contents = browser->GetWebContentsAt(tab_index);
  if (!web_contents) {
    AutomationJSONReply(this, reply_message).SendError(
        StringPrintf("Could not get WebContents at tab index %d", tab_index));
    return;
  }

  RenderViewHost* render_view = web_contents->GetRenderViewHost();
  int routing_id = render_view->GetRoutingID();

  // This observer will delete itself.
  new FPSObserver(
      this, reply_message,
      base::GetProcId(render_view->GetProcess()->GetHandle()),
      routing_id);
  render_view->Send(new ChromeViewMsg_GetFPS(routing_id));
}

void TestingAutomationProvider::IsFullscreenForBrowser(Browser* browser,
    base::DictionaryValue* args,
    IPC::Message* reply_message) {
  DictionaryValue dict;
  dict.SetBoolean("result",
                  browser->fullscreen_controller_->IsFullscreenForBrowser());
  AutomationJSONReply(this, reply_message).SendSuccess(&dict);
}

void TestingAutomationProvider::IsFullscreenForTab(Browser* browser,
    base::DictionaryValue* args,
    IPC::Message* reply_message) {
  DictionaryValue dict;
  dict.SetBoolean("result", browser->IsFullscreenForTabOrPending());
  AutomationJSONReply(this, reply_message).SendSuccess(&dict);
}

void TestingAutomationProvider::IsMouseLocked(Browser* browser,
    base::DictionaryValue* args,
    IPC::Message* reply_message) {
  DictionaryValue dict;
  dict.SetBoolean("result", browser->GetSelectedWebContents()->
      GetRenderViewHost()->GetView()->IsMouseLocked());
  AutomationJSONReply(this, reply_message).SendSuccess(&dict);
}

void TestingAutomationProvider::IsMouseLockPermissionRequested(
    Browser* browser,
    base::DictionaryValue* args,
    IPC::Message* reply_message) {
  FullscreenExitBubbleType type =
      browser->fullscreen_controller_->GetFullscreenExitBubbleType();
  bool mouse_lock = false;
  fullscreen_bubble::PermissionRequestedByType(type, NULL, &mouse_lock);
  DictionaryValue dict;
  dict.SetBoolean("result", mouse_lock);
  AutomationJSONReply(this, reply_message).SendSuccess(&dict);
}

void TestingAutomationProvider::IsFullscreenPermissionRequested(
    Browser* browser,
    base::DictionaryValue* args,
    IPC::Message* reply_message) {
  FullscreenExitBubbleType type =
      browser->fullscreen_controller_->GetFullscreenExitBubbleType();
  bool fullscreen = false;
  fullscreen_bubble::PermissionRequestedByType(type, &fullscreen, NULL);
  DictionaryValue dict;
  dict.SetBoolean("result", fullscreen);
  AutomationJSONReply(this, reply_message).SendSuccess(&dict);
}

void TestingAutomationProvider::IsFullscreenBubbleDisplayed(Browser* browser,
    base::DictionaryValue* args,
    IPC::Message* reply_message) {
  FullscreenExitBubbleType type =
      browser->fullscreen_controller_->GetFullscreenExitBubbleType();
  DictionaryValue dict;
  dict.SetBoolean("result",
                  type != FEB_TYPE_BROWSER_FULLSCREEN_EXIT_INSTRUCTION);
  AutomationJSONReply(this, reply_message).SendSuccess(&dict);
}

void TestingAutomationProvider::IsFullscreenBubbleDisplayingButtons(
    Browser* browser,
    base::DictionaryValue* args,
    IPC::Message* reply_message) {
  FullscreenExitBubbleType type =
      browser->fullscreen_controller_->GetFullscreenExitBubbleType();
  DictionaryValue dict;
  dict.SetBoolean("result", fullscreen_bubble::ShowButtonsForType(type));
  AutomationJSONReply(this, reply_message).SendSuccess(&dict);
}

void TestingAutomationProvider::AcceptCurrentFullscreenOrMouseLockRequest(
    Browser* browser,
    base::DictionaryValue* args,
    IPC::Message* reply_message) {
  WebContents* fullscreen_tab = browser->GetSelectedWebContents();
  FullscreenExitBubbleType type =
      browser->fullscreen_controller_->GetFullscreenExitBubbleType();
  browser->OnAcceptFullscreenPermission(fullscreen_tab->GetURL(), type);
  AutomationJSONReply(this, reply_message).SendSuccess(NULL);
}

void TestingAutomationProvider::DenyCurrentFullscreenOrMouseLockRequest(
    Browser* browser,
    base::DictionaryValue* args,
    IPC::Message* reply_message) {
  FullscreenExitBubbleType type =
      browser->fullscreen_controller_->GetFullscreenExitBubbleType();
  browser->OnDenyFullscreenPermission(type);
  AutomationJSONReply(this, reply_message).SendSuccess(NULL);
}

void TestingAutomationProvider::WaitForAllViewsToStopLoading(
    DictionaryValue* args,
    IPC::Message* reply_message) {
  if (AppModalDialogQueue::GetInstance()->HasActiveDialog()) {
    AutomationJSONReply(this, reply_message).SendSuccess(NULL);
    return;
  }

  // This class will send the message immediately if no tab is loading.
  new AllViewsStoppedLoadingObserver(
      this, reply_message, profile()->GetExtensionProcessManager());
}

void TestingAutomationProvider::GetPolicyDefinitionList(
    DictionaryValue* args,
    IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);

#if !defined(ENABLE_CONFIGURATION_POLICY)
  reply.SendError("Configuration Policy disabled");
#else
  DictionaryValue response;

  const policy::PolicyDefinitionList* list =
      policy::GetChromePolicyDefinitionList();
  // Value::Type to python type.
  std::map<Value::Type, std::string> types;
  types[Value::TYPE_BOOLEAN] = "bool";
  types[Value::TYPE_DICTIONARY] = "dict";
  types[Value::TYPE_INTEGER] = "int";
  types[Value::TYPE_LIST] = "list";
  types[Value::TYPE_STRING] = "str";

  const policy::PolicyDefinitionList::Entry* entry;
  for (entry = list->begin; entry != list->end; ++entry) {
    if (types.find(entry->value_type) == types.end()) {
      std::string error("Unrecognized policy type for policy ");
      reply.SendError(error + entry->name);
      return;
    }
    Value* type = Value::CreateStringValue(types[entry->value_type]);
    Value* device_policy = Value::CreateBooleanValue(entry->device_policy);
    ListValue* definition = new ListValue;
    definition->Append(type);
    definition->Append(device_policy);
    response.Set(entry->name, definition);
  }

  reply.SendSuccess(&response);
#endif
}

void TestingAutomationProvider::RefreshPolicies(
    base::DictionaryValue* args,
    IPC::Message* reply_message) {
#if !defined(ENABLE_CONFIGURATION_POLICY)
  AutomationJSONReply(this, reply_message).SendError(
      "Configuration Policy disabled");
#else
  // Some policies (e.g. URLBlacklist) post tasks to other message loops
  // before they start enforcing updated policy values; make sure those tasks
  // have finished after a policy update.
  // Updates of the URLBlacklist are done on IO, after building the blacklist
  // on FILE, which is initiated from IO.
  base::Closure reply =
      base::Bind(SendSuccessReply, AsWeakPtr(), reply_message);
  g_browser_process->policy_service()->RefreshPolicies(
      base::Bind(PostTask, BrowserThread::IO,
          base::Bind(PostTask, BrowserThread::FILE,
              base::Bind(PostTask, BrowserThread::IO,
                  base::Bind(PostTask, BrowserThread::UI, reply)))));
#endif
}

static int AccessArray(const int arr[], int index) {
  return arr[index];
}

void TestingAutomationProvider::SimulateAsanMemoryBug(
    base::DictionaryValue* args, IPC::Message* reply_message) {
  int testarray[41];
  AccessArray(testarray, 42);
  AutomationJSONReply(this, reply_message).SendSuccess(NULL);
}

void TestingAutomationProvider::GetIndicesFromTab(
    DictionaryValue* args,
    IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);
  int id_or_handle = 0;
  bool has_id = args->HasKey("tab_id");
  bool has_handle = args->HasKey("tab_handle");
  if (has_id && has_handle) {
    reply.SendError(
        "Both 'tab_id' and 'tab_handle' were specified. Only one is allowed");
    return;
  } else if (!has_id && !has_handle) {
    reply.SendError("Either 'tab_id' or 'tab_handle' must be specified");
    return;
  }
  if (has_id && !args->GetInteger("tab_id", &id_or_handle)) {
    reply.SendError("'tab_id' is invalid");
    return;
  }
  if (has_handle && (!args->GetInteger("tab_handle", &id_or_handle) ||
                     !tab_tracker_->ContainsHandle(id_or_handle))) {
    reply.SendError("'tab_handle' is invalid");
    return;
  }
  int id = id_or_handle;
  if (has_handle) {
    TabContentsWrapper* tab = TabContentsWrapper::GetCurrentWrapperForContents(
        tab_tracker_->GetResource(id_or_handle)->GetWebContents());
    id = tab->restore_tab_helper()->session_id().id();
  }
  BrowserList::const_iterator iter = BrowserList::begin();
  int browser_index = 0;
  for (; iter != BrowserList::end(); ++iter, ++browser_index) {
    Browser* browser = *iter;
    for (int tab_index = 0; tab_index < browser->tab_count(); ++tab_index) {
      TabContentsWrapper* tab = browser->GetTabContentsWrapperAt(tab_index);
      if (tab->restore_tab_helper()->session_id().id() == id) {
        DictionaryValue dict;
        dict.SetInteger("windex", browser_index);
        dict.SetInteger("tab_index", tab_index);
        reply.SendSuccess(&dict);
        return;
      }
    }
  }
  reply.SendError("Could not find tab among current browser windows");
}

void TestingAutomationProvider::NavigateToURL(
    DictionaryValue* args,
    IPC::Message* reply_message) {
  if (SendErrorIfModalDialogActive(this, reply_message))
    return;

  int navigation_count;
  std::string url, error;
  Browser* browser;
  WebContents* web_contents;
  if (!GetBrowserAndTabFromJSONArgs(args, &browser, &web_contents, &error)) {
    AutomationJSONReply(this, reply_message).SendError(error);
    return;
  }
  if (!args->GetString("url", &url)) {
    AutomationJSONReply(this, reply_message)
        .SendError("'url' missing or invalid");
    return;
  }
  if (!args->GetInteger("navigation_count", &navigation_count)) {
    AutomationJSONReply(this, reply_message)
        .SendError("'navigation_count' missing or invalid");
    return;
  }
  new NavigationNotificationObserver(
      &web_contents->GetController(), this, reply_message,
      navigation_count, false, true);
  browser->OpenURLFromTab(web_contents, OpenURLParams(
      GURL(url), content::Referrer(), CURRENT_TAB,
      content::PAGE_TRANSITION_TYPED, false));
}

void TestingAutomationProvider::WaitUntilNavigationCompletes(
    DictionaryValue* args,
    IPC::Message* reply_message) {
  if (SendErrorIfModalDialogActive(this, reply_message))
    return;

  std::string error;
  Browser* browser;
  WebContents* web_contents;
  if (!GetBrowserAndTabFromJSONArgs(args, &browser, &web_contents, &error)) {
    AutomationJSONReply(this, reply_message).SendError(error);
    return;
  }
  NavigationNotificationObserver* observer =
      new NavigationNotificationObserver(&web_contents->GetController(), this,
                                         reply_message, 1, true, true);
  if (!web_contents->IsLoading()) {
    AutomationJSONReply(this, reply_message).SendSuccess(NULL);
    delete observer;
    return;
  }
}

void TestingAutomationProvider::ExecuteJavascriptJSON(
    DictionaryValue* args,
    IPC::Message* reply_message) {
  if (SendErrorIfModalDialogActive(this, reply_message))
    return;

  string16 frame_xpath, javascript;
  std::string error;
  RenderViewHost* render_view;
  if (!GetRenderViewFromJSONArgs(args, profile(), &render_view, &error)) {
    AutomationJSONReply(this, reply_message).SendError(
        Error(automation::kInvalidId, error));
    return;
  }
  if (!args->GetString("frame_xpath", &frame_xpath)) {
    AutomationJSONReply(this, reply_message)
        .SendError("'frame_xpath' missing or invalid");
    return;
  }
  if (!args->GetString("javascript", &javascript)) {
    AutomationJSONReply(this, reply_message)
        .SendError("'javascript' missing or invalid");
    return;
  }

  new DomOperationMessageSender(this, reply_message, true);
  ExecuteJavascriptInRenderViewFrame(frame_xpath, javascript, reply_message,
                                     render_view);
}

void TestingAutomationProvider::ExecuteJavascriptInRenderView(
    DictionaryValue* args,
    IPC::Message* reply_message) {
  string16 frame_xpath, javascript, extension_id, url_text;
  std::string error;
  int render_process_id, render_view_id;
  if (!args->GetString("frame_xpath", &frame_xpath)) {
    AutomationJSONReply(this, reply_message)
        .SendError("'frame_xpath' missing or invalid");
    return;
  }
  if (!args->GetString("javascript", &javascript)) {
    AutomationJSONReply(this, reply_message)
        .SendError("'javascript' missing or invalid");
    return;
  }
  if (!args->GetInteger("view.render_process_id", &render_process_id)) {
    AutomationJSONReply(this, reply_message)
        .SendError("'view.render_process_id' missing or invalid");
    return;
  }
  if (!args->GetInteger("view.render_view_id", &render_view_id)) {
    AutomationJSONReply(this, reply_message)
        .SendError("'view.render_view_id' missing or invalid");
    return;
  }

  RenderViewHost* rvh = RenderViewHost::FromID(render_process_id,
                                               render_view_id);
  if (!rvh) {
    AutomationJSONReply(this, reply_message).SendError(
            "A RenderViewHost object was not found with the given view ID.");
    return;
  }

  new DomOperationMessageSender(this, reply_message, true);
  ExecuteJavascriptInRenderViewFrame(frame_xpath, javascript, reply_message,
                                     rvh);
}

void TestingAutomationProvider::AddDomEventObserver(
    DictionaryValue* args,
    IPC::Message* reply_message) {
  if (SendErrorIfModalDialogActive(this, reply_message))
    return;

  AutomationJSONReply reply(this, reply_message);
  std::string event_name;
  int automation_id;
  bool recurring;
  if (!args->GetString("event_name", &event_name)) {
    reply.SendError("'event_name' missing or invalid");
    return;
  }
  if (!args->GetInteger("automation_id", &automation_id)) {
    reply.SendError("'automation_id' missing or invalid");
    return;
  }
  if (!args->GetBoolean("recurring", &recurring)) {
    reply.SendError("'recurring' missing or invalid");
    return;
  }

  if (!automation_event_queue_.get())
    automation_event_queue_.reset(new AutomationEventQueue);

  int observer_id = automation_event_queue_->AddObserver(
      new DomEventObserver(automation_event_queue_.get(), event_name,
                           automation_id, recurring));
  scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
  return_value->SetInteger("observer_id", observer_id);
  reply.SendSuccess(return_value.get());
}

void TestingAutomationProvider::RemoveEventObserver(
    DictionaryValue* args,
    IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);
  int observer_id;
  if (!args->GetInteger("observer_id", &observer_id) ||
      !automation_event_queue_.get()) {
    reply.SendError("'observer_id' missing or invalid");
    return;
  }
  if (automation_event_queue_->RemoveObserver(observer_id)) {
    reply.SendSuccess(NULL);
    return;
  }
  reply.SendError("Invalid observer id.");
}

void TestingAutomationProvider::ClearEventQueue(
    DictionaryValue* args,
    IPC::Message* reply_message) {
  automation_event_queue_.reset();
  AutomationJSONReply(this, reply_message).SendSuccess(NULL);
}

void TestingAutomationProvider::GetNextEvent(
    DictionaryValue* args,
    IPC::Message* reply_message) {
  scoped_ptr<AutomationJSONReply> reply(
      new AutomationJSONReply(this, reply_message));
  int observer_id;
  bool blocking;
  if (!args->GetInteger("observer_id", &observer_id)) {
    reply->SendError("'observer_id' missing or invalid");
    return;
  }
  if (!args->GetBoolean("blocking", &blocking)) {
    reply->SendError("'blocking' missing or invalid");
    return;
  }
  if (!automation_event_queue_.get()) {
    reply->SendError(
        "No observers are attached to the queue. Did you create any?");
    return;
  }

  // The reply will be freed once a matching event is added to the queue.
  automation_event_queue_->GetNextEvent(reply.release(), observer_id, blocking);
}

void TestingAutomationProvider::GoForward(
    DictionaryValue* args,
    IPC::Message* reply_message) {
  if (SendErrorIfModalDialogActive(this, reply_message))
    return;

  WebContents* web_contents;
  std::string error;
  if (!GetTabFromJSONArgs(args, &web_contents, &error)) {
    AutomationJSONReply(this, reply_message).SendError(error);
    return;
  }
  NavigationController& controller = web_contents->GetController();
  if (!controller.CanGoForward()) {
    DictionaryValue dict;
    dict.SetBoolean("did_go_forward", false);
    AutomationJSONReply(this, reply_message).SendSuccess(&dict);
    return;
  }
  new NavigationNotificationObserver(&controller, this, reply_message,
                                     1, false, true);
  controller.GoForward();
}

void TestingAutomationProvider::GoBack(
    DictionaryValue* args,
    IPC::Message* reply_message) {
  if (SendErrorIfModalDialogActive(this, reply_message))
    return;

  WebContents* web_contents;
  std::string error;
  if (!GetTabFromJSONArgs(args, &web_contents, &error)) {
    AutomationJSONReply(this, reply_message).SendError(error);
    return;
  }
  NavigationController& controller = web_contents->GetController();
  if (!controller.CanGoBack()) {
    DictionaryValue dict;
    dict.SetBoolean("did_go_back", false);
    AutomationJSONReply(this, reply_message).SendSuccess(&dict);
    return;
  }
  new NavigationNotificationObserver(&controller, this, reply_message,
                                     1, false, true);
  controller.GoBack();
}

void TestingAutomationProvider::ReloadJSON(
    DictionaryValue* args,
    IPC::Message* reply_message) {
  if (SendErrorIfModalDialogActive(this, reply_message))
    return;

  WebContents* web_contents;
  std::string error;
  if (!GetTabFromJSONArgs(args, &web_contents, &error)) {
    AutomationJSONReply(this, reply_message).SendError(error);
    return;
  }
  NavigationController& controller = web_contents->GetController();
  new NavigationNotificationObserver(&controller, this, reply_message,
                                     1, false, true);
  controller.Reload(false);
}

void TestingAutomationProvider::CaptureEntirePageJSON(
    DictionaryValue* args,
    IPC::Message* reply_message) {
  if (SendErrorIfModalDialogActive(this, reply_message))
    return;

  WebContents* web_contents;
  std::string error;

  if (!GetTabFromJSONArgs(args, &web_contents, &error)) {
    AutomationJSONReply(this, reply_message).SendError(error);
    return;
  }

  FilePath::StringType path_str;
  if (!args->GetString("path", &path_str)) {
    AutomationJSONReply(this, reply_message)
        .SendError("'path' missing or invalid");
    return;
  }

  RenderViewHost* render_view = web_contents->GetRenderViewHost();
  if (render_view) {
    FilePath path(path_str);
    // This will delete itself when finished.
    PageSnapshotTaker* snapshot_taker = new PageSnapshotTaker(
        this, reply_message,
        TabContentsWrapper::GetCurrentWrapperForContents(web_contents), path);
    snapshot_taker->Start();
  } else {
    AutomationJSONReply(this, reply_message)
        .SendError("Tab has no associated RenderViewHost");
  }
}

void TestingAutomationProvider::GetCookiesJSON(
    DictionaryValue* args, IPC::Message* reply_message) {
  automation_util::GetCookiesJSON(this, args, reply_message);
}

void TestingAutomationProvider::DeleteCookieJSON(
    DictionaryValue* args, IPC::Message* reply_message) {
  automation_util::DeleteCookieJSON(this, args, reply_message);
}

void TestingAutomationProvider::SetCookieJSON(
    DictionaryValue* args, IPC::Message* reply_message) {
  automation_util::SetCookieJSON(this, args, reply_message);
}

void TestingAutomationProvider::GetTabIds(
    DictionaryValue* args, IPC::Message* reply_message) {
  ListValue* id_list = new ListValue();
  BrowserList::const_iterator iter = BrowserList::begin();
  for (; iter != BrowserList::end(); ++iter) {
    Browser* browser = *iter;
    for (int i = 0; i < browser->tab_count(); ++i) {
      int id = browser->GetTabContentsWrapperAt(i)->restore_tab_helper()->
          session_id().id();
      id_list->Append(Value::CreateIntegerValue(id));
    }
  }
  DictionaryValue dict;
  dict.Set("ids", id_list);
  AutomationJSONReply(this, reply_message).SendSuccess(&dict);
}

void TestingAutomationProvider::GetViews(
    DictionaryValue* args, IPC::Message* reply_message) {
  ListValue* view_list = new ListValue();
  printing::PrintPreviewTabController* preview_controller =
      printing::PrintPreviewTabController::GetInstance();
  BrowserList::const_iterator browser_iter = BrowserList::begin();
  for (; browser_iter != BrowserList::end(); ++browser_iter) {
    Browser* browser = *browser_iter;
    for (int i = 0; i < browser->tab_count(); ++i) {
      TabContentsWrapper* tab = browser->GetTabContentsWrapperAt(i);
      DictionaryValue* dict = new DictionaryValue();
      AutomationId id = automation_util::GetIdForTab(tab);
      dict->Set("auto_id", id.ToValue());
      view_list->Append(dict);
      if (preview_controller) {
        TabContentsWrapper* preview_tab =
            preview_controller->GetPrintPreviewForTab(tab);
        if (preview_tab) {
          DictionaryValue* dict = new DictionaryValue();
          AutomationId id = automation_util::GetIdForTab(preview_tab);
          dict->Set("auto_id", id.ToValue());
          view_list->Append(dict);
        }
      }
    }
  }

  ExtensionProcessManager* extension_mgr =
      profile()->GetExtensionProcessManager();
  const ExtensionProcessManager::ViewSet all_views =
      extension_mgr->GetAllViews();
  ExtensionProcessManager::ViewSet::const_iterator iter;
  for (iter = all_views.begin(); iter != all_views.end(); ++iter) {
    content::RenderViewHost* host = (*iter);
    AutomationId id = automation_util::GetIdForExtensionView(host);
    if (!id.is_valid())
      continue;
    const Extension* extension =
        extension_mgr->GetExtensionForRenderViewHost(host);
    DictionaryValue* dict = new DictionaryValue();
    dict->Set("auto_id", id.ToValue());
    dict->SetString("extension_id", extension->id());
    view_list->Append(dict);
  }
  DictionaryValue dict;
  dict.Set("views", view_list);
  AutomationJSONReply(this, reply_message).SendSuccess(&dict);
}

void TestingAutomationProvider::IsTabIdValid(
    DictionaryValue* args, IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);
  int id;
  if (!args->GetInteger("id", &id)) {
    reply.SendError("'id' missing or invalid");
    return;
  }
  bool is_valid = false;
  BrowserList::const_iterator iter = BrowserList::begin();
  for (; iter != BrowserList::end(); ++iter) {
    Browser* browser = *iter;
    for (int i = 0; i < browser->tab_count(); ++i) {
      TabContentsWrapper* tab = browser->GetTabContentsWrapperAt(i);
      if (tab->restore_tab_helper()->session_id().id() == id) {
        is_valid = true;
        break;
      }
    }
  }
  DictionaryValue dict;
  dict.SetBoolean("is_valid", is_valid);
  reply.SendSuccess(&dict);
}

void TestingAutomationProvider::DoesAutomationObjectExist(
    DictionaryValue* args, IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);
  AutomationId id;
  std::string error_msg;
  if (!GetAutomationIdFromJSONArgs(args, "auto_id", &id, &error_msg)) {
    reply.SendError(error_msg);
    return;
  }
  DictionaryValue dict;
  dict.SetBoolean(
      "does_exist",
      automation_util::DoesObjectWithIdExist(id, profile()));
  reply.SendSuccess(&dict);
}

void TestingAutomationProvider::CloseTabJSON(
    DictionaryValue* args, IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);
  Browser* browser;
  WebContents* tab;
  std::string error;
  // Close tabs synchronously.
  if (GetBrowserAndTabFromJSONArgs(args, &browser, &tab, &error)) {
    browser->CloseTabContents(tab);
    reply.SendSuccess(NULL);
    return;
  }

  // Close other types of views asynchronously.
  RenderViewHost* view;
  if (!GetRenderViewFromJSONArgs(args, profile(), &view, &error)) {
    reply.SendError(error);
    return;
  }
  view->ClosePage();
  reply.SendSuccess(NULL);
}

void TestingAutomationProvider::SetViewBounds(
    base::DictionaryValue* args,
    IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);
  int x, y, width, height;
  if (!args->GetInteger("bounds.x", &x) ||
      !args->GetInteger("bounds.y", &y) ||
      !args->GetInteger("bounds.width", &width) ||
      !args->GetInteger("bounds.height", &height)) {
    reply.SendError("Missing or invalid 'bounds'");
    return;
  }
  Browser* browser;
  std::string error;
  if (!GetBrowserFromJSONArgs(args, &browser, &error)) {
    reply.SendError(Error(automation::kInvalidId, error));
    return;
  }
  browser->window()->SetBounds(gfx::Rect(x, y, width, height));
  reply.SendSuccess(NULL);
}

void TestingAutomationProvider::ActivateTabJSON(
    DictionaryValue* args,
    IPC::Message* reply_message) {
  if (SendErrorIfModalDialogActive(this, reply_message))
    return;

  AutomationJSONReply reply(this, reply_message);
  Browser* browser;
  WebContents* web_contents;
  std::string error;
  if (!GetBrowserAndTabFromJSONArgs(args, &browser, &web_contents, &error)) {
    reply.SendError(error);
    return;
  }
  browser->ActivateTabAt(
      browser->GetIndexOfController(&web_contents->GetController()), true);
  reply.SendSuccess(NULL);
}

void TestingAutomationProvider::IsPageActionVisible(
    base::DictionaryValue* args,
    IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);

  WebContents* tab;
  std::string error;
  if (!GetTabFromJSONArgs(args, &tab, &error)) {
    reply.SendError(error);
    return;
  }
  Browser* browser = automation_util::GetBrowserForTab(tab);
  const Extension* extension;
  if (!GetEnabledExtensionFromJSONArgs(
          args, "extension_id", browser->profile(), &extension, &error)) {
    reply.SendError(error);
    return;
  }
  ExtensionAction* page_action = extension->page_action();
  if (!page_action) {
    reply.SendError("Extension doesn't have any page action");
    return;
  }
  if (!browser) {
    reply.SendError("Tab does not belong to an open browser");
    return;
  }
  EnsureTabSelected(browser, tab);

  bool is_visible = false;
  LocationBarTesting* loc_bar =
      browser->window()->GetLocationBar()->GetLocationBarForTesting();
  size_t page_action_visible_count =
      static_cast<size_t>(loc_bar->PageActionVisibleCount());
  for (size_t i = 0; i < page_action_visible_count; ++i) {
    if (loc_bar->GetVisiblePageAction(i) == page_action) {
      is_visible = true;
      break;
    }
  }
  DictionaryValue dict;
  dict.SetBoolean("is_visible", is_visible);
  reply.SendSuccess(&dict);
}

void TestingAutomationProvider::GetChromeDriverAutomationVersion(
    DictionaryValue* args,
    IPC::Message* reply_message) {
  DictionaryValue reply_dict;
  reply_dict.SetInteger("version", automation::kChromeDriverAutomationVersion);
  AutomationJSONReply(this, reply_message).SendSuccess(&reply_dict);
}

void TestingAutomationProvider::CreateNewAutomationProvider(
    DictionaryValue* args,
    IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);
  std::string channel_id;
  if (!args->GetString("channel_id", &channel_id)) {
    reply.SendError("'channel_id' missing or invalid");
    return;
  }

  AutomationProvider* provider = new TestingAutomationProvider(profile_);
  provider->DisableInitialLoadObservers();
  // TODO(kkania): Remove this when crbug.com/91311 is fixed.
  // Named server channels should ideally be created and closed on the file
  // thread, within the IPC channel code.
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  if (!provider->InitializeChannel(
          automation::kNamedInterfacePrefix + channel_id)) {
    reply.SendError("Failed to initialize channel: " + channel_id);
    return;
  }
  DCHECK(g_browser_process);
  g_browser_process->GetAutomationProviderList()->AddProvider(provider);
  reply.SendSuccess(NULL);
}

void TestingAutomationProvider::WaitForTabCountToBecome(
    int browser_handle,
    int target_tab_count,
    IPC::Message* reply_message) {
  if (!browser_tracker_->ContainsHandle(browser_handle)) {
    AutomationMsg_WaitForTabCountToBecome::WriteReplyParams(reply_message,
                                                            false);
    Send(reply_message);
    return;
  }

  Browser* browser = browser_tracker_->GetResource(browser_handle);

  // The observer will delete itself.
  new TabCountChangeObserver(this, browser, reply_message, target_tab_count);
}

void TestingAutomationProvider::WaitForInfoBarCount(
    int tab_handle,
    size_t target_count,
    IPC::Message* reply_message) {
  if (!tab_tracker_->ContainsHandle(tab_handle)) {
    AutomationMsg_WaitForInfoBarCount::WriteReplyParams(reply_message_, false);
    Send(reply_message_);
    return;
  }

  NavigationController* controller = tab_tracker_->GetResource(tab_handle);
  if (!controller) {
    AutomationMsg_WaitForInfoBarCount::WriteReplyParams(reply_message_, false);
    Send(reply_message_);
    return;
  }

  // The delegate will delete itself.
  new InfoBarCountObserver(this, reply_message,
      TabContentsWrapper::GetCurrentWrapperForContents(
          controller->GetWebContents()), target_count);
}

void TestingAutomationProvider::ResetToDefaultTheme() {
  ThemeServiceFactory::GetForProfile(profile_)->UseDefaultTheme();
}

void TestingAutomationProvider::WaitForProcessLauncherThreadToGoIdle(
    IPC::Message* reply_message) {
  new WaitForProcessLauncherThreadToGoIdleObserver(this, reply_message);
}

void TestingAutomationProvider::OnRemoveProvider() {
  if (g_browser_process)
    g_browser_process->GetAutomationProviderList()->RemoveProvider(this);
}
