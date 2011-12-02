// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
#include "chrome/browser/automation/ui_controls.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_storage.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/debugger/devtools_window.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/download/download_service.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/download/save_package_file_picker.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/browser_action_test_util.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/extension_updater.h"
#include "chrome/browser/extensions/unpacked_installer.h"
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
#include "chrome/browser/platform_util.h"
#include "chrome/browser/plugin_prefs.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/printing/print_view_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/sessions/restore_tab_helper.h"
#include "chrome/browser/sessions/session_service_factory.h"
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
#include "chrome/browser/ui/browser_init.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/constrained_window_tab_helper.h"
#include "chrome/browser/ui/find_bar/find_bar.h"
#include "chrome/browser/ui/login/login_prompt.h"
#include "chrome/browser/ui/omnibox/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/search_engines/keyword_editor_controller.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
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
#include "content/browser/plugin_service.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/interstitial_page.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/common_param_traits.h"
#include "net/base/cookie_store.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
#include "ui/base/events.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/base/ui_base_types.h"
#include "webkit/glue/webdropdata.h"
#include "webkit/plugins/webplugininfo.h"

#if defined(ENABLE_CONFIGURATION_POLICY)
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/policy/configuration_policy_provider.h"
#include "chrome/browser/policy/policy_map.h"
#include "policy/policy_constants.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/dbus/dbus_thread_manager.h"
#include "chrome/browser/ui/webui/active_downloads_ui.h"
#else
#include "chrome/browser/download/download_shelf.h"
#endif

using automation_util::SendErrorIfModalDialogActive;
using content::BrowserThread;

namespace {

void SendMouseClick(int flags) {
  ui_controls::MouseButton button = ui_controls::LEFT;
  if ((flags & ui::EF_LEFT_BUTTON_DOWN) ==
      ui::EF_LEFT_BUTTON_DOWN) {
    button = ui_controls::LEFT;
  } else if ((flags & ui::EF_RIGHT_BUTTON_DOWN) ==
             ui::EF_RIGHT_BUTTON_DOWN) {
    button = ui_controls::RIGHT;
  } else if ((flags & ui::EF_MIDDLE_BUTTON_DOWN) ==
             ui::EF_MIDDLE_BUTTON_DOWN) {
    button = ui_controls::MIDDLE;
  } else {
    NOTREACHED();
  }

  ui_controls::SendMouseClick(button);
}

class AutomationInterstitialPage : public InterstitialPage {
 public:
  AutomationInterstitialPage(TabContents* tab,
                             const GURL& url,
                             const std::string& contents)
      : InterstitialPage(tab, true, url),
        contents_(contents) {
  }

  virtual std::string GetHTMLContents() { return contents_; }

 private:
  const std::string contents_;

  DISALLOW_COPY_AND_ASSIGN(AutomationInterstitialPage);
};

}  // namespace

TestingAutomationProvider::TestingAutomationProvider(Profile* profile)
    : AutomationProvider(profile),
#if defined(TOOLKIT_VIEWS)
      popup_menu_waiter_(NULL),
#endif
      redirect_query_(0) {
  BrowserList::AddObserver(this);
  registrar_.Add(this, chrome::NOTIFICATION_SESSION_END,
                 content::NotificationService::AllSources());
#if defined(OS_CHROMEOS)
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->
      AddObserver(this);
#endif
}

TestingAutomationProvider::~TestingAutomationProvider() {
#if defined(OS_CHROMEOS)
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->
      RemoveObserver(this);
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
  bool handled = true;
  bool deserialize_success = true;
  IPC_BEGIN_MESSAGE_MAP_EX(TestingAutomationProvider,
                           message,
                           deserialize_success)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_CloseBrowser, CloseBrowser)
    IPC_MESSAGE_HANDLER(AutomationMsg_CloseBrowserRequestAsync,
                        CloseBrowserAsync)
    IPC_MESSAGE_HANDLER(AutomationMsg_ActivateTab, ActivateTab)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_AppendTab, AppendTab)
    IPC_MESSAGE_HANDLER(AutomationMsg_ActiveTabIndex, GetActiveTabIndex)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_CloseTab, CloseTab)
    IPC_MESSAGE_HANDLER(AutomationMsg_GetCookies, GetCookies)
    IPC_MESSAGE_HANDLER(AutomationMsg_SetCookie, SetCookie)
    IPC_MESSAGE_HANDLER(AutomationMsg_DeleteCookie, DeleteCookie)
    IPC_MESSAGE_HANDLER(AutomationMsg_ShowCollectedCookiesDialog,
                        ShowCollectedCookiesDialog)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(
        AutomationMsg_NavigateToURLBlockUntilNavigationsComplete,
        NavigateToURLBlockUntilNavigationsComplete)
    IPC_MESSAGE_HANDLER(AutomationMsg_NavigationAsync, NavigationAsync)
    IPC_MESSAGE_HANDLER(AutomationMsg_NavigationAsyncWithDisposition,
                        NavigationAsyncWithDisposition)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_Reload, Reload)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_SetAuth, SetAuth)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_CancelAuth, CancelAuth)
    IPC_MESSAGE_HANDLER(AutomationMsg_NeedsAuth, NeedsAuth)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_RedirectsFrom,
                                    GetRedirectsFrom)
    IPC_MESSAGE_HANDLER(AutomationMsg_BrowserWindowCount, GetBrowserWindowCount)
    IPC_MESSAGE_HANDLER(AutomationMsg_NormalBrowserWindowCount,
                        GetNormalBrowserWindowCount)
    IPC_MESSAGE_HANDLER(AutomationMsg_BrowserWindow, GetBrowserWindow)
    IPC_MESSAGE_HANDLER(AutomationMsg_GetBrowserLocale, GetBrowserLocale)
    IPC_MESSAGE_HANDLER(AutomationMsg_LastActiveBrowserWindow,
                        GetLastActiveBrowserWindow)
    IPC_MESSAGE_HANDLER(AutomationMsg_ActiveWindow, GetActiveWindow)
    IPC_MESSAGE_HANDLER(AutomationMsg_FindTabbedBrowserWindow,
                        FindTabbedBrowserWindow)
    IPC_MESSAGE_HANDLER(AutomationMsg_IsWindowActive, IsWindowActive)
    IPC_MESSAGE_HANDLER(AutomationMsg_ActivateWindow, ActivateWindow)
    IPC_MESSAGE_HANDLER(AutomationMsg_IsWindowMaximized, IsWindowMaximized)
    IPC_MESSAGE_HANDLER(AutomationMsg_WindowExecuteCommandAsync,
                        ExecuteBrowserCommandAsync)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_WindowExecuteCommand,
                        ExecuteBrowserCommand)
    IPC_MESSAGE_HANDLER(AutomationMsg_TerminateSession, TerminateSession)
    IPC_MESSAGE_HANDLER(AutomationMsg_WindowViewBounds, WindowGetViewBounds)
    IPC_MESSAGE_HANDLER(AutomationMsg_GetWindowBounds, GetWindowBounds)
    IPC_MESSAGE_HANDLER(AutomationMsg_SetWindowBounds, SetWindowBounds)
    IPC_MESSAGE_HANDLER(AutomationMsg_SetWindowVisible, SetWindowVisible)
    IPC_MESSAGE_HANDLER(AutomationMsg_WindowClick, WindowSimulateClick)
    IPC_MESSAGE_HANDLER(AutomationMsg_WindowMouseMove, WindowSimulateMouseMove)
    IPC_MESSAGE_HANDLER(AutomationMsg_WindowKeyPress, WindowSimulateKeyPress)
    IPC_MESSAGE_HANDLER(AutomationMsg_TabCount, GetTabCount)
    IPC_MESSAGE_HANDLER(AutomationMsg_Type, GetType)
    IPC_MESSAGE_HANDLER(AutomationMsg_IsBrowserInApplicationMode,
                        IsBrowserInApplicationMode)
    IPC_MESSAGE_HANDLER(AutomationMsg_Tab, GetTab)
    IPC_MESSAGE_HANDLER(AutomationMsg_TabProcessID, GetTabProcessID)
    IPC_MESSAGE_HANDLER(AutomationMsg_TabTitle, GetTabTitle)
    IPC_MESSAGE_HANDLER(AutomationMsg_TabIndex, GetTabIndex)
    IPC_MESSAGE_HANDLER(AutomationMsg_TabURL, GetTabURL)
    IPC_MESSAGE_HANDLER(AutomationMsg_ShelfVisibility, GetShelfVisibility)
    IPC_MESSAGE_HANDLER(AutomationMsg_IsFullscreen, IsFullscreen)
    IPC_MESSAGE_HANDLER(AutomationMsg_IsFullscreenBubbleVisible,
                        GetFullscreenBubbleVisibility)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_DomOperation,
                                    ExecuteJavascript)
    IPC_MESSAGE_HANDLER(AutomationMsg_ConstrainedWindowCount,
                        GetConstrainedWindowCount)
#if defined(TOOLKIT_VIEWS)
    IPC_MESSAGE_HANDLER(AutomationMsg_GetFocusedViewID, GetFocusedViewID)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_WaitForFocusedViewIDToChange,
                                    WaitForFocusedViewIDToChange)
    IPC_MESSAGE_HANDLER(AutomationMsg_StartTrackingPopupMenus,
                        StartTrackingPopupMenus)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_WaitForPopupMenuToOpen,
                                    WaitForPopupMenuToOpen)
#endif  // defined(TOOLKIT_VIEWS)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_InspectElement,
                                    HandleInspectElementRequest)
    IPC_MESSAGE_HANDLER(AutomationMsg_DownloadDirectory, GetDownloadDirectory)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_OpenNewBrowserWindowOfType,
                                    OpenNewBrowserWindowOfType)
    IPC_MESSAGE_HANDLER(AutomationMsg_WindowForBrowser, GetWindowForBrowser)
    IPC_MESSAGE_HANDLER(AutomationMsg_BrowserForWindow, GetBrowserForWindow)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_ShowInterstitialPage,
                                    ShowInterstitialPage)
    IPC_MESSAGE_HANDLER(AutomationMsg_HideInterstitialPage,
                        HideInterstitialPage)
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
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_PrintNow, PrintNow)
    IPC_MESSAGE_HANDLER(AutomationMsg_SavePage, SavePage)
    IPC_MESSAGE_HANDLER(AutomationMsg_OpenFindInPage,
                        HandleOpenFindInPageRequest)
    IPC_MESSAGE_HANDLER(AutomationMsg_FindWindowVisibility,
                        GetFindWindowVisibility)
    IPC_MESSAGE_HANDLER(AutomationMsg_FindWindowLocation,
                        HandleFindWindowLocationRequest)
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
    IPC_MESSAGE_HANDLER(AutomationMsg_GetInfoBarCount, GetInfoBarCount)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_ClickInfoBarAccept,
                                    ClickInfoBarAccept)
    IPC_MESSAGE_HANDLER(AutomationMsg_GetLastNavigationTime,
                        GetLastNavigationTime)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_WaitForNavigation,
                                    WaitForNavigation)
    IPC_MESSAGE_HANDLER(AutomationMsg_SetIntPreference, SetIntPreference)
    IPC_MESSAGE_HANDLER(AutomationMsg_ShowingAppModalDialog,
                        GetShowingAppModalDialog)
    IPC_MESSAGE_HANDLER(AutomationMsg_ClickAppModalDialogButton,
                        ClickAppModalDialogButton)
    IPC_MESSAGE_HANDLER(AutomationMsg_SetStringPreference, SetStringPreference)
    IPC_MESSAGE_HANDLER(AutomationMsg_GetBooleanPreference,
                        GetBooleanPreference)
    IPC_MESSAGE_HANDLER(AutomationMsg_SetBooleanPreference,
                        SetBooleanPreference)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(
        AutomationMsg_WaitForBrowserWindowCountToBecome,
        WaitForBrowserWindowCountToBecome)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(
        AutomationMsg_WaitForAppModalDialogToBeShown,
        WaitForAppModalDialogToBeShown)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(
        AutomationMsg_GoBackBlockUntilNavigationsComplete,
        GoBackBlockUntilNavigationsComplete)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(
        AutomationMsg_GoForwardBlockUntilNavigationsComplete,
        GoForwardBlockUntilNavigationsComplete)
    IPC_MESSAGE_HANDLER(AutomationMsg_SavePackageShouldPromptUser,
                        SavePackageShouldPromptUser)
    IPC_MESSAGE_HANDLER(AutomationMsg_WindowTitle, GetWindowTitle)
    IPC_MESSAGE_HANDLER(AutomationMsg_SetShelfVisibility, SetShelfVisibility)
    IPC_MESSAGE_HANDLER(AutomationMsg_BlockedPopupCount, GetBlockedPopupCount)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_SendJSONRequest,
                                    SendJSONRequest)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_WaitForTabCountToBecome,
                                    WaitForTabCountToBecome)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_WaitForInfoBarCount,
                                    WaitForInfoBarCount)
    IPC_MESSAGE_HANDLER(AutomationMsg_GetPageCurrentEncoding,
                        GetPageCurrentEncoding)
    IPC_MESSAGE_HANDLER(AutomationMsg_ShutdownSessionService,
                        ShutdownSessionService)
    IPC_MESSAGE_HANDLER(AutomationMsg_SetContentSetting, SetContentSetting)
    IPC_MESSAGE_HANDLER(AutomationMsg_LoadBlockedPlugins, LoadBlockedPlugins)
    IPC_MESSAGE_HANDLER(AutomationMsg_ResetToDefaultTheme, ResetToDefaultTheme)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(
        AutomationMsg_WaitForProcessLauncherThreadToGoIdle,
        WaitForProcessLauncherThreadToGoIdle)
    IPC_MESSAGE_HANDLER(AutomationMsg_GetParentBrowserOfTab,
                        GetParentBrowserOfTab)

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

void TestingAutomationProvider::CloseBrowserAsync(int browser_handle) {
  if (!browser_tracker_->ContainsHandle(browser_handle))
    return;

  Browser* browser = browser_tracker_->GetResource(browser_handle);
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
      append_tab_response =
          GetIndexForNavigationController(&contents->controller(), browser);
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
    Browser* browser = Browser::GetBrowserForController(controller, &index);
    DCHECK(browser);
    new TabClosedNotificationObserver(this, wait_until_closed, reply_message);
    browser->CloseTabContents(controller->tab_contents());
    return;
  }

  AutomationMsg_CloseTab::WriteReplyParams(reply_message, false);
  Send(reply_message);
}

void TestingAutomationProvider::GetCookies(const GURL& url, int handle,
                                           int* value_size,
                                           std::string* value) {
  TabContents *contents = tab_tracker_->ContainsHandle(handle) ?
      tab_tracker_->GetResource(handle)->tab_contents() : NULL;
  automation_util::GetCookies(url, contents, value_size, value);
}

void TestingAutomationProvider::SetCookie(const GURL& url,
                                          const std::string& value,
                                          int handle,
                                          int* response_value) {
  TabContents *contents = tab_tracker_->ContainsHandle(handle) ?
      tab_tracker_->GetResource(handle)->tab_contents() : NULL;
  automation_util::SetCookie(url, value, contents, response_value);
}

void TestingAutomationProvider::DeleteCookie(const GURL& url,
                                             const std::string& cookie_name,
                                             int handle, bool* success) {
  TabContents *contents = tab_tracker_->ContainsHandle(handle) ?
      tab_tracker_->GetResource(handle)->tab_contents() : NULL;
  automation_util::DeleteCookie(url, cookie_name, contents, success);
}

void TestingAutomationProvider::ShowCollectedCookiesDialog(
    int handle, bool* success) {
  *success = false;
  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* controller = tab_tracker_->GetResource(handle);
    TabContents* tab_contents = controller->tab_contents();
    Browser* browser = Browser::GetBrowserForController(controller, NULL);
    browser->ShowCollectedCookiesDialog(
        TabContentsWrapper::GetCurrentWrapperForContents(tab_contents));
    *success = true;
  }
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
      browser->OpenURL(
          url, GURL(), CURRENT_TAB, content::PAGE_TRANSITION_TYPED);
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
  NavigationAsyncWithDisposition(handle, url, CURRENT_TAB, status);
}

void TestingAutomationProvider::NavigationAsyncWithDisposition(
    int handle,
    const GURL& url,
    WindowOpenDisposition disposition,
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
      browser->OpenURL(
          url, GURL(), disposition, content::PAGE_TRANSITION_TYPED);
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

void TestingAutomationProvider::SetAuth(int tab_handle,
                                        const std::wstring& username,
                                        const std::wstring& password,
                                        IPC::Message* reply_message) {
  if (tab_tracker_->ContainsHandle(tab_handle)) {
    NavigationController* tab = tab_tracker_->GetResource(tab_handle);
    LoginHandlerMap::iterator iter = login_handler_map_.find(tab);

    if (iter != login_handler_map_.end()) {
      // If auth is needed again after this, assume login has failed.  This is
      // not strictly correct, because a navigation can require both proxy and
      // server auth, but it should be OK for now.
      LoginHandler* handler = iter->second;
      new NavigationNotificationObserver(
          tab, this, reply_message, 1, false, false);
      handler->SetAuth(WideToUTF16Hack(username), WideToUTF16Hack(password));
      return;
    }
  }

  AutomationMsg_SetAuth::WriteReplyParams(
      reply_message, AUTOMATION_MSG_NAVIGATION_AUTH_NEEDED);
  Send(reply_message);
}

void TestingAutomationProvider::CancelAuth(int tab_handle,
                                           IPC::Message* reply_message) {
  if (tab_tracker_->ContainsHandle(tab_handle)) {
    NavigationController* tab = tab_tracker_->GetResource(tab_handle);
    LoginHandlerMap::iterator iter = login_handler_map_.find(tab);

    if (iter != login_handler_map_.end()) {
      // If auth is needed again after this, something is screwy.
      LoginHandler* handler = iter->second;
      new NavigationNotificationObserver(
          tab, this, reply_message, 1, false, false);
      handler->CancelAuth();
      return;
    }
  }

  AutomationMsg_CancelAuth::WriteReplyParams(
      reply_message, AUTOMATION_MSG_NAVIGATION_AUTH_NEEDED);
  Send(reply_message);
}

void TestingAutomationProvider::NeedsAuth(int tab_handle, bool* needs_auth) {
  *needs_auth = false;

  if (tab_tracker_->ContainsHandle(tab_handle)) {
    NavigationController* tab = tab_tracker_->GetResource(tab_handle);
    LoginHandlerMap::iterator iter = login_handler_map_.find(tab);

    if (iter != login_handler_map_.end()) {
      // The LoginHandler will be in our map IFF the tab needs auth.
      *needs_auth = true;
    }
  }
}

void TestingAutomationProvider::GetRedirectsFrom(int tab_handle,
                                                 const GURL& source_url,
                                                 IPC::Message* reply_message) {
  if (redirect_query_) {
    LOG(ERROR) << "Can only handle one redirect query at once.";
  } else if (tab_tracker_->ContainsHandle(tab_handle)) {
    NavigationController* tab = tab_tracker_->GetResource(tab_handle);
    Profile* profile = Profile::FromBrowserContext(tab->browser_context());
    HistoryService* history_service =
        profile->GetHistoryService(Profile::EXPLICIT_ACCESS);

    DCHECK(history_service) << "Tab " << tab_handle << "'s profile " <<
                               "has no history service";
    if (history_service) {
      DCHECK(!reply_message_);
      reply_message_ = reply_message;
      // Schedule a history query for redirects. The response will be sent
      // asynchronously from the callback the history system uses to notify us
      // that it's done: OnRedirectQueryComplete.
      redirect_query_ = history_service->QueryRedirectsFrom(
          source_url, &consumer_,
          base::Bind(&TestingAutomationProvider::OnRedirectQueryComplete,
                     base::Unretained(this)));
      return;  // Response will be sent when query completes.
    }
  }

  // Send failure response.
  std::vector<GURL> empty;
  AutomationMsg_RedirectsFrom::WriteReplyParams(reply_message, false, empty);
  Send(reply_message);
}

void TestingAutomationProvider::GetBrowserWindowCount(int* window_count) {
  *window_count = static_cast<int>(BrowserList::size());
}

void TestingAutomationProvider::GetNormalBrowserWindowCount(int* window_count) {
  *window_count = static_cast<int>(
      BrowserList::GetBrowserCountForType(profile_, true));
}

void TestingAutomationProvider::GetBrowserWindow(int index, int* handle) {
  *handle = 0;
  Browser* browser = automation_util::GetBrowserAt(index);
  if (browser)
    *handle = browser_tracker_->Add(browser);
}

void TestingAutomationProvider::FindTabbedBrowserWindow(int* handle) {
  *handle = 0;
  Browser* browser = BrowserList::FindTabbedBrowser(profile_, false);
  if (browser)
    *handle = browser_tracker_->Add(browser);
}

void TestingAutomationProvider::GetLastActiveBrowserWindow(int* handle) {
  *handle = 0;
  Browser* browser = BrowserList::GetLastActive();
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

void TestingAutomationProvider::GetBrowserLocale(string16* locale) {
  *locale = ASCIIToUTF16(g_browser_process->GetApplicationLocale());
}

void TestingAutomationProvider::IsWindowActive(int handle,
                                               bool* success,
                                               bool* is_active) {
  if (window_tracker_->ContainsHandle(handle)) {
    *is_active =
        platform_util::IsWindowActive(window_tracker_->GetResource(handle));
    *success = true;
  } else {
    *success = false;
    *is_active = false;
  }
}

void TestingAutomationProvider::WindowSimulateClick(const IPC::Message& message,
                                                    int handle,
                                                    const gfx::Point& click,
                                                    int flags) {
  if (window_tracker_->ContainsHandle(handle)) {
    // TODO(phajdan.jr): This is flaky. We should wait for the final click.
    ui_controls::SendMouseMoveNotifyWhenDone(
        click.x(), click.y(), base::Bind(&SendMouseClick, flags));
  }
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

  TabContents* tab_contents;
  std::string error;
  if (!GetTabFromJSONArgs(args, &tab_contents, &error)) {
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

  tab_contents->render_view_host()->ForwardMouseEvent(mouse_event);

  mouse_event.type = WebKit::WebInputEvent::MouseUp;
  new InputEventAckNotificationObserver(this, reply_message, mouse_event.type,
                                        1);
  tab_contents->render_view_host()->ForwardMouseEvent(mouse_event);
}

void TestingAutomationProvider::WebkitMouseMove(
    DictionaryValue* args, IPC::Message* reply_message) {
  if (SendErrorIfModalDialogActive(this, reply_message))
    return;

  TabContents* tab_contents;
  std::string error;
  if (!GetTabFromJSONArgs(args, &tab_contents, &error)) {
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
  tab_contents->render_view_host()->ForwardMouseEvent(mouse_event);
}

void TestingAutomationProvider::WebkitMouseDrag(DictionaryValue* args,
                                                IPC::Message* reply_message) {
  if (SendErrorIfModalDialogActive(this, reply_message))
    return;

  TabContents* tab_contents;
  std::string error;
  if (!GetTabFromJSONArgs(args, &tab_contents, &error)) {
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
  tab_contents->render_view_host()->ForwardMouseEvent(mouse_event);

  // Step 2- Left click mouse down, the mouse button is fixed.
  mouse_event.type = WebKit::WebInputEvent::MouseDown;
  mouse_event.button = WebKit::WebMouseEvent::ButtonLeft;
  mouse_event.clickCount = 1;
  tab_contents->render_view_host()->ForwardMouseEvent(mouse_event);

  // Step 3 - Move the mouse to the end position.
  // TODO(JMikhail): See if we should simulate the by not making such
  // a drastic jump by placing incrmental stops along the way.
  mouse_event.type = WebKit::WebInputEvent::MouseMove;
  mouse_event.x = end_x;
  mouse_event.y = end_y;
  mouse_event.clickCount = 0;
  tab_contents->render_view_host()->ForwardMouseEvent(mouse_event);

  // Step 4 - Release the left mouse button.
  mouse_event.type = WebKit::WebInputEvent::MouseUp;
  mouse_event.clickCount = 1;
  new InputEventAckNotificationObserver(this, reply_message, mouse_event.type,
                                        1);
  tab_contents->render_view_host()->ForwardMouseEvent(mouse_event);
}

void TestingAutomationProvider::WebkitMouseButtonDown(
    DictionaryValue* args, IPC::Message* reply_message) {
  if (SendErrorIfModalDialogActive(this, reply_message))
    return;

  TabContents* tab_contents;
  std::string error;
  if (!GetTabFromJSONArgs(args, &tab_contents, &error)) {
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
  tab_contents->render_view_host()->ForwardMouseEvent(mouse_event);
}

void TestingAutomationProvider::WebkitMouseButtonUp(
    DictionaryValue* args, IPC::Message* reply_message) {
  if (SendErrorIfModalDialogActive(this, reply_message))
    return;

  TabContents* tab_contents;
  std::string error;
  if (!GetTabFromJSONArgs(args, &tab_contents, &error)) {
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
  tab_contents->render_view_host()->ForwardMouseEvent(mouse_event);
}

void TestingAutomationProvider::WebkitMouseDoubleClick(
    DictionaryValue* args, IPC::Message* reply_message) {
  if (SendErrorIfModalDialogActive(this, reply_message))
    return;

  TabContents* tab_contents;
  std::string error;
  if (!GetTabFromJSONArgs(args, &tab_contents, &error)) {
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
  tab_contents->render_view_host()->ForwardMouseEvent(mouse_event);

  mouse_event.type = WebKit::WebInputEvent::MouseUp;
  new InputEventAckNotificationObserver(this, reply_message, mouse_event.type,
                                        2);
  tab_contents->render_view_host()->ForwardMouseEvent(mouse_event);

  mouse_event.type = WebKit::WebInputEvent::MouseDown;
  mouse_event.clickCount = 2;
  tab_contents->render_view_host()->ForwardMouseEvent(mouse_event);

  mouse_event.type = WebKit::WebInputEvent::MouseUp;
  tab_contents->render_view_host()->ForwardMouseEvent(mouse_event);
}

void TestingAutomationProvider::DragAndDropFilePaths(
    DictionaryValue* args, IPC::Message* reply_message) {
  if (SendErrorIfModalDialogActive(this, reply_message))
    return;

  TabContents* tab_contents;
  std::string error;
  if (!GetTabFromJSONArgs(args, &tab_contents, &error)) {
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

    drop_data.filenames.push_back(path);
  }

  const gfx::Point client(x, y);
  // We don't set any values in screen variable because DragTarget*** ignore the
  // screen argument.
  const gfx::Point screen;

  int operations = 0;
  operations |= WebKit::WebDragOperationCopy;
  operations |= WebKit::WebDragOperationLink;
  operations |= WebKit::WebDragOperationMove;

  RenderViewHost* host = tab_contents->render_view_host();
  host->DragTargetDragEnter(
      drop_data, client, screen,
      static_cast<WebKit::WebDragOperationsMask>(operations));
  new DragTargetDropAckNotificationObserver(this, reply_message);
  host->DragTargetDrop(client, screen);
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

void TestingAutomationProvider::IsBrowserInApplicationMode(int handle,
                                                           bool* is_application,
                                                           bool* success) {
  *is_application = false;
  *success = false;

  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    if (browser) {
      *success = true;
      *is_application = browser->is_app();
    }
  }
}

void TestingAutomationProvider::GetTab(int win_handle,
                                       int tab_index,
                                       int* tab_handle) {
  *tab_handle = 0;
  if (browser_tracker_->ContainsHandle(win_handle) && (tab_index >= 0)) {
    Browser* browser = browser_tracker_->GetResource(win_handle);
    if (tab_index < browser->tab_count()) {
      TabContents* tab_contents = browser->GetTabContentsAt(tab_index);
      *tab_handle = tab_tracker_->Add(&tab_contents->controller());
    }
  }
}

void TestingAutomationProvider::GetTabProcessID(int handle, int* process_id) {
  *process_id = -1;

  if (tab_tracker_->ContainsHandle(handle)) {
    *process_id = 0;
    TabContents* tab_contents =
        tab_tracker_->GetResource(handle)->tab_contents();
    content::RenderProcessHost* rph = tab_contents->GetRenderProcessHost();
    if (rph)
      *process_id = base::GetProcId(rph->GetHandle());
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
    Browser* browser = Browser::GetBrowserForController(tab, NULL);
    *tabstrip_index = browser->tabstrip_model()->GetIndexOfController(tab);
  }
}

void TestingAutomationProvider::GetTabURL(int handle,
                                          bool* success,
                                          GURL* url) {
  *success = false;
  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* tab = tab_tracker_->GetResource(handle);
    // Return what the user would see in the location bar.
    *url = tab->GetActiveEntry()->virtual_url();
    *success = true;
  }
}

void TestingAutomationProvider::GetShelfVisibility(int handle, bool* visible) {
  *visible = false;

  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    if (browser) {
#if defined(OS_CHROMEOS)
      *visible = ActiveDownloadsUI::GetPopup();
#else
      *visible = browser->window()->IsDownloadShelfVisible();
#endif
    }
  }
}

void TestingAutomationProvider::IsFullscreen(int handle, bool* visible) {
  *visible = false;

  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    if (browser)
      *visible = browser->window()->IsFullscreen();
  }
}

void TestingAutomationProvider::GetFullscreenBubbleVisibility(int handle,
                                                              bool* visible) {
  *visible = false;

  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    if (browser)
      *visible = browser->window()->IsFullscreenBubbleVisible();
  }
}

namespace {

void ExecuteJavascriptInRenderViewFrame(
    const string16& frame_xpath,
    const string16& script,
    IPC::Message* reply_message,
    RenderViewHost* render_view_host) {
  // Set the routing id of this message with the controller.
  // This routing id needs to be remembered for the reverse
  // communication while sending back the response of
  // this javascript execution.
  std::string set_automation_id;
  base::SStringPrintf(&set_automation_id,
                      "window.domAutomationController.setAutomationId(%d);",
                      reply_message->routing_id());

  render_view_host->ExecuteJavascriptInWebFrame(
      frame_xpath, UTF8ToUTF16(set_automation_id));
  render_view_host->ExecuteJavascriptInWebFrame(
      frame_xpath, script);
}

}  // namespace

void TestingAutomationProvider::ExecuteJavascript(
    int handle,
    const std::wstring& frame_xpath,
    const std::wstring& script,
    IPC::Message* reply_message) {
  TabContents* tab_contents = GetTabContentsForHandle(handle, NULL);
  if (!tab_contents) {
    AutomationMsg_DomOperation::WriteReplyParams(reply_message, std::string());
    Send(reply_message);
    return;
  }

  new DomOperationMessageSender(this, reply_message, false);
  ExecuteJavascriptInRenderViewFrame(WideToUTF16Hack(frame_xpath),
                                     WideToUTF16Hack(script), reply_message,
                                     tab_contents->render_view_host());
}

void TestingAutomationProvider::GetConstrainedWindowCount(int handle,
                                                          int* count) {
  *count = -1;  // -1 is the error code
  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* nav_controller = tab_tracker_->GetResource(handle);
    TabContents* tab_contents = nav_controller->tab_contents();
    if (tab_contents) {
      TabContentsWrapper* wrapper =
          TabContentsWrapper::GetCurrentWrapperForContents(tab_contents);
      if (wrapper) {
        *count = static_cast<int>(wrapper->constrained_window_tab_helper()->
                                  constrained_window_count());
      }
    }
  }
}

void TestingAutomationProvider::HandleInspectElementRequest(
    int handle, int x, int y, IPC::Message* reply_message) {
  TabContents* tab_contents = GetTabContentsForHandle(handle, NULL);
  if (tab_contents) {
    DCHECK(!reply_message_);
    reply_message_ = reply_message;

    DevToolsWindow::InspectElement(tab_contents->render_view_host(), x, y);
  } else {
    AutomationMsg_InspectElement::WriteReplyParams(reply_message, -1);
    Send(reply_message);
  }
}

void TestingAutomationProvider::GetDownloadDirectory(
    int handle, FilePath* download_directory) {
  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* tab = tab_tracker_->GetResource(handle);
    DownloadManager* dlm = tab->browser_context()->GetDownloadManager();
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

void TestingAutomationProvider::GetBrowserForWindow(int window_handle,
                                                    bool* success,
                                                    int* browser_handle) {
  *success = false;
  *browser_handle = 0;

  gfx::NativeWindow window = window_tracker_->GetResource(window_handle);
  if (!window)
    return;

  BrowserList::const_iterator iter = BrowserList::begin();
  for (;iter != BrowserList::end(); ++iter) {
    gfx::NativeWindow this_window = (*iter)->window()->GetNativeHandle();
    if (window == this_window) {
      // Add() returns the existing handle for the resource if any.
      *browser_handle = browser_tracker_->Add(*iter);
      *success = true;
      return;
    }
  }
}

void TestingAutomationProvider::ShowInterstitialPage(
    int tab_handle,
    const std::string& html_text,
    IPC::Message* reply_message) {
  if (tab_tracker_->ContainsHandle(tab_handle)) {
    NavigationController* controller = tab_tracker_->GetResource(tab_handle);
    TabContents* tab_contents = controller->tab_contents();

    new NavigationNotificationObserver(controller, this, reply_message, 1,
                                       false, false);

    AutomationInterstitialPage* interstitial =
        new AutomationInterstitialPage(tab_contents,
                                       GURL("about:interstitial"),
                                       html_text);
    interstitial->Show();
    return;
  }

  AutomationMsg_ShowInterstitialPage::WriteReplyParams(
      reply_message, AUTOMATION_MSG_NAVIGATION_ERROR);
  Send(reply_message);
}

void TestingAutomationProvider::HideInterstitialPage(int tab_handle,
                                                     bool* success) {
  *success = false;
  TabContents* tab_contents = GetTabContentsForHandle(tab_handle, NULL);
  if (tab_contents && tab_contents->interstitial_page()) {
    tab_contents->interstitial_page()->DontProceed();
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
    *security_style = entry->ssl().security_style();
    *ssl_cert_status = entry->ssl().cert_status();
    *insecure_content_status = entry->ssl().content_status();
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
    *page_type = entry->page_type();
    *success = true;
    // In order to return the proper result when an interstitial is shown and
    // no navigation entry were created for it we need to ask the TabContents.
    if (*page_type == content::PAGE_TYPE_NORMAL &&
        tab->tab_contents()->showing_interstitial_page())
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
    if (entry->page_type() == content::PAGE_TYPE_INTERSTITIAL) {
      TabContents* tab_contents = tab->tab_contents();
      InterstitialPage* ssl_blocking_page =
          InterstitialPage::GetInterstitialPage(tab_contents);
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

void TestingAutomationProvider::PrintNow(int tab_handle,
                                         IPC::Message* reply_message) {
  NavigationController* tab = NULL;
  TabContents* tab_contents = GetTabContentsForHandle(tab_handle, &tab);
  if (tab_contents) {
    FindAndActivateTab(tab);

    content::NotificationObserver* observer =
        new DocumentPrintedNotificationObserver(this, reply_message);

    TabContentsWrapper* wrapper =
        TabContentsWrapper::GetCurrentWrapperForContents(tab_contents);
    if (!wrapper->print_view_manager()->PrintNow()) {
      // Clean up the observer. It will send the reply message.
      delete observer;
    }

    // Return now to avoid sending reply message twice.
    return;
  }

  AutomationMsg_PrintNow::WriteReplyParams(reply_message, false);
  Send(reply_message);
}

void TestingAutomationProvider::SavePage(int tab_handle,
                                         const FilePath& file_name,
                                         const FilePath& dir_path,
                                         int type,
                                         bool* success) {
  SavePackage::SavePackageType save_type =
      static_cast<SavePackage::SavePackageType>(type);
  if (save_type < SavePackage::SAVE_AS_ONLY_HTML ||
      save_type > SavePackage::SAVE_AS_COMPLETE_HTML) {
    *success = false;
    return;
  }

  if (!tab_tracker_->ContainsHandle(tab_handle)) {
    *success = false;
    return;
  }

  NavigationController* nav = tab_tracker_->GetResource(tab_handle);
  Browser* browser = FindAndActivateTab(nav);
  if (!browser->command_updater()->IsCommandEnabled(IDC_SAVE_PAGE)) {
    *success = false;
    return;
  }

  nav->tab_contents()->SavePage(file_name, dir_path, save_type);
  *success = true;
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

void TestingAutomationProvider::HandleFindWindowLocationRequest(int handle,
                                                                int* x,
                                                                int* y) {
  gfx::Point position(0, 0);
  bool visible = false;
  if (browser_tracker_->ContainsHandle(handle)) {
     Browser* browser = browser_tracker_->GetResource(handle);
     FindBarTesting* find_bar =
       browser->GetFindBarController()->find_bar()->GetFindBarTesting();
     find_bar->GetFindBarWindowInfo(&position, &visible);
  }

  *x = position.x();
  *y = position.y();
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
    bool *success) {
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

void TestingAutomationProvider::GetInfoBarCount(int handle, size_t* count) {
  *count = static_cast<size_t>(-1);  // -1 means error.
  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* nav_controller = tab_tracker_->GetResource(handle);
    if (nav_controller) {
      TabContentsWrapper* wrapper =
          TabContentsWrapper::GetCurrentWrapperForContents(
              nav_controller->tab_contents());
      *count = wrapper->infobar_tab_helper()->infobar_count();
    }
  }
}

void TestingAutomationProvider::ClickInfoBarAccept(
    int handle,
    size_t info_bar_index,
    bool wait_for_navigation,
    IPC::Message* reply_message) {
  bool success = false;
  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* nav_controller = tab_tracker_->GetResource(handle);
    if (nav_controller) {
      InfoBarTabHelper* infobar_helper =
          TabContentsWrapper::GetCurrentWrapperForContents(
              nav_controller->tab_contents())->infobar_tab_helper();
      if (info_bar_index < infobar_helper->infobar_count()) {
        if (wait_for_navigation) {
          new NavigationNotificationObserver(nav_controller, this,
                                             reply_message, 1, false, false);
        }
        InfoBarDelegate* delegate =
            infobar_helper->GetInfoBarDelegateAt(info_bar_index);
        if (delegate->AsConfirmInfoBarDelegate())
          delegate->AsConfirmInfoBarDelegate()->Accept();
        success = true;
      }
    }
  }

  // This "!wait_for_navigation || !success condition" logic looks suspicious.
  // It will send a failure message when success is true but
  // |wait_for_navigation| is false.
  // TODO(phajdan.jr): investgate whether the reply param (currently
  // AUTOMATION_MSG_NAVIGATION_ERROR) should depend on success.
  if (!wait_for_navigation || !success)
    AutomationMsg_ClickInfoBarAccept::WriteReplyParams(
        reply_message, AUTOMATION_MSG_NAVIGATION_ERROR);
}

void TestingAutomationProvider::GetLastNavigationTime(
    int handle,
    int64* last_navigation_time) {
  base::Time time(tab_tracker_->GetLastNavigationTime(handle));
  *last_navigation_time = time.ToInternalValue();
}

void TestingAutomationProvider::WaitForNavigation(int handle,
                                                  int64 last_navigation_time,
                                                  IPC::Message* reply_message) {
  NavigationController* controller = tab_tracker_->GetResource(handle);
  base::Time time(tab_tracker_->GetLastNavigationTime(handle));

  if (time.ToInternalValue() > last_navigation_time || !controller) {
    AutomationMsg_WaitForNavigation::WriteReplyParams(reply_message,
        controller == NULL ? AUTOMATION_MSG_NAVIGATION_ERROR :
                             AUTOMATION_MSG_NAVIGATION_SUCCESS);
    Send(reply_message);
    return;
  }

  new NavigationNotificationObserver(
      controller, this, reply_message, 1, true, false);
}

void TestingAutomationProvider::SetIntPreference(int handle,
                                                 const std::string& name,
                                                 int value,
                                                 bool* success) {
  *success = false;
  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    browser->profile()->GetPrefs()->SetInteger(name.c_str(), value);
    *success = true;
  }
}

void TestingAutomationProvider::SetStringPreference(int handle,
                                                    const std::string& name,
                                                    const std::string& value,
                                                    bool* success) {
  *success = false;
  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    browser->profile()->GetPrefs()->SetString(name.c_str(), value);
    *success = true;
  }
}

void TestingAutomationProvider::GetBooleanPreference(int handle,
                                                     const std::string& name,
                                                     bool* success,
                                                     bool* value) {
  *success = false;
  *value = false;
  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    *value = browser->profile()->GetPrefs()->GetBoolean(name.c_str());
    *success = true;
  }
}

void TestingAutomationProvider::SetBooleanPreference(int handle,
                                                     const std::string& name,
                                                     bool value,
                                                     bool* success) {
  *success = false;
  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    browser->profile()->GetPrefs()->SetBoolean(name.c_str(), value);
    *success = true;
  }
}

void TestingAutomationProvider::GetShowingAppModalDialog(bool* showing_dialog,
                                                         int* dialog_button) {
  AppModalDialog* active_dialog =
      AppModalDialogQueue::GetInstance()->active_dialog();
  if (!active_dialog) {
    *showing_dialog = false;
    *dialog_button = ui::DIALOG_BUTTON_NONE;
    return;
  }
  NativeAppModalDialog* native_dialog = active_dialog->native_dialog();
  *showing_dialog = (native_dialog != NULL);
  if (*showing_dialog)
    *dialog_button = native_dialog->GetAppModalDialogButtons();
  else
    *dialog_button = ui::DIALOG_BUTTON_NONE;
}

void TestingAutomationProvider::ClickAppModalDialogButton(int button,
                                                          bool* success) {
  *success = false;

  NativeAppModalDialog* native_dialog =
      AppModalDialogQueue::GetInstance()->active_dialog()->native_dialog();
  if (native_dialog &&
      (native_dialog->GetAppModalDialogButtons() & button) == button) {
    if ((button & ui::DIALOG_BUTTON_OK) == ui::DIALOG_BUTTON_OK) {
      native_dialog->AcceptAppModalDialog();
      *success =  true;
    }
    if ((button & ui::DIALOG_BUTTON_CANCEL) == ui::DIALOG_BUTTON_CANCEL) {
      DCHECK(!*success) << "invalid param, OK and CANCEL specified";
      native_dialog->CancelAppModalDialog();
      *success =  true;
    }
  }
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

void TestingAutomationProvider::WaitForAppModalDialogToBeShown(
    IPC::Message* reply_message) {
  if (AppModalDialogQueue::GetInstance()->HasActiveDialog()) {
    AutomationMsg_WaitForAppModalDialogToBeShown::WriteReplyParams(
        reply_message, true);
    Send(reply_message);
    return;
  }

  // Set up an observer (it will delete itself).
  new AppModalDialogShownObserver(this, reply_message);
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

void TestingAutomationProvider::SavePackageShouldPromptUser(
    bool should_prompt) {
  SavePackageFilePicker::SetShouldPromptUser(should_prompt);
}

void TestingAutomationProvider::SetShelfVisibility(int handle, bool visible) {
  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    if (browser) {
#if defined(OS_CHROMEOS)
      Browser* popup_browser = ActiveDownloadsUI::GetPopup();
      if (!popup_browser && visible)
        ActiveDownloadsUI::OpenPopup(browser->profile());
      if (popup_browser && !visible)
        popup_browser->CloseWindow();
#else
      if (visible)
        browser->window()->GetDownloadShelf()->Show();
      else
        browser->window()->GetDownloadShelf()->Close();
#endif
    }
  }
}

void TestingAutomationProvider::GetBlockedPopupCount(int handle, int* count) {
  *count = -1;  // -1 is the error code
  if (tab_tracker_->ContainsHandle(handle)) {
      NavigationController* nav_controller = tab_tracker_->GetResource(handle);
      TabContentsWrapper* tab_contents =
          TabContentsWrapper::GetCurrentWrapperForContents(
              nav_controller->tab_contents());
      if (tab_contents) {
        BlockedContentTabHelper* blocked_content =
            tab_contents->blocked_content_tab_helper();
        *count = static_cast<int>(blocked_content->GetBlockedContentsCount());
      }
  }
}

void TestingAutomationProvider::SendJSONRequest(int handle,
                                                const std::string& json_request,
                                                IPC::Message* reply_message) {
  scoped_ptr<Value> values;
  base::JSONReader reader;
  std::string error;
  values.reset(reader.ReadAndReturnError(json_request, true, NULL, &error));
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
      &TestingAutomationProvider::WaitForAllTabsToStopLoading;
  handler_map["GetIndicesFromTab"] =
      &TestingAutomationProvider::GetIndicesFromTab;
  handler_map["NavigateToURL"] =
      &TestingAutomationProvider::NavigateToURL;
  handler_map["GetLocalStatePrefsInfo"] =
      &TestingAutomationProvider::GetLocalStatePrefsInfo;
  handler_map["SetLocalStatePrefs"] =
      &TestingAutomationProvider::SetLocalStatePrefs;
  handler_map["ExecuteJavascript"] =
      &TestingAutomationProvider::ExecuteJavascriptJSON;
  handler_map["ExecuteJavascriptInRenderView"] =
      &TestingAutomationProvider::ExecuteJavascriptInRenderView;
  handler_map["GoForward"] =
      &TestingAutomationProvider::GoForward;
  handler_map["GoBack"] =
      &TestingAutomationProvider::GoBack;
  handler_map["Reload"] =
      &TestingAutomationProvider::ReloadJSON;
  handler_map["GetTabURL"] =
      &TestingAutomationProvider::GetTabURLJSON;
  handler_map["GetTabTitle"] =
      &TestingAutomationProvider::GetTabTitleJSON;
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
  handler_map["IsTabIdValid"] =
      &TestingAutomationProvider::IsTabIdValid;
  handler_map["CloseTab"] =
      &TestingAutomationProvider::CloseTabJSON;
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
  handler_map["ActivateTab"] =
      &TestingAutomationProvider::ActivateTabJSON;
  handler_map["GetAppModalDialogMessage"] =
      &TestingAutomationProvider::GetAppModalDialogMessage;
  handler_map["AcceptOrDismissAppModalDialog"] =
      &TestingAutomationProvider::AcceptOrDismissAppModalDialog;
  handler_map["GetChromeDriverAutomationVersion"] =
      &TestingAutomationProvider::GetChromeDriverAutomationVersion;
  handler_map["UpdateExtensionsNow"] =
      &TestingAutomationProvider::UpdateExtensionsNow;
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
  handler_map["SetPolicies"] =
      &TestingAutomationProvider::SetPolicies;
  handler_map["GetPolicyDefinitionList"] =
      &TestingAutomationProvider::GetPolicyDefinitionList;
  handler_map["InstallExtension"] =
      &TestingAutomationProvider::InstallExtension;
  handler_map["GetExtensionsInfo"] =
      &TestingAutomationProvider::GetExtensionsInfo;
  handler_map["RefreshPolicies"] =
      &TestingAutomationProvider::RefreshPolicies;
  handler_map["TriggerPageActionById"] =
      &TestingAutomationProvider::TriggerPageActionById;
  handler_map["TriggerBrowserActionById"] =
      &TestingAutomationProvider::TriggerBrowserActionById;
#if defined(OS_CHROMEOS)
  handler_map["GetLoginInfo"] = &TestingAutomationProvider::GetLoginInfo;
  handler_map["ShowCreateAccountUI"] =
      &TestingAutomationProvider::ShowCreateAccountUI;
  handler_map["LoginAsGuest"] = &TestingAutomationProvider::LoginAsGuest;
  handler_map["Login"] = &TestingAutomationProvider::Login;

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

  browser_handler_map["GetPrefsInfo"] =
      &TestingAutomationProvider::GetPrefsInfo;
  browser_handler_map["SetPrefs"] = &TestingAutomationProvider::SetPrefs;

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

  browser_handler_map["UninstallExtensionById"] =
      &TestingAutomationProvider::UninstallExtensionById;
  browser_handler_map["SetExtensionStateById"] =
      &TestingAutomationProvider::SetExtensionStateById;

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
  browser_handler_map["MoveNTPMostVisitedThumbnail"] =
      &TestingAutomationProvider::MoveNTPMostVisitedThumbnail;
  browser_handler_map["RemoveNTPMostVisitedThumbnail"] =
      &TestingAutomationProvider::RemoveNTPMostVisitedThumbnail;
  browser_handler_map["UnpinNTPMostVisitedThumbnail"] =
      &TestingAutomationProvider::UnpinNTPMostVisitedThumbnail;
  browser_handler_map["RestoreAllNTPMostVisitedThumbnails"] =
      &TestingAutomationProvider::RestoreAllNTPMostVisitedThumbnails;

  browser_handler_map["KillRendererProcess"] =
      &TestingAutomationProvider::KillRendererProcess;

  browser_handler_map["LaunchApp"] = &TestingAutomationProvider::LaunchApp;
  browser_handler_map["SetAppLaunchType"] =
      &TestingAutomationProvider::SetAppLaunchType;
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

ListValue* TestingAutomationProvider::GetInfobarsInfo(TabContents* tc) {
  // Each infobar may have different properties depending on the type.
  ListValue* infobars = new ListValue;
  InfoBarTabHelper* infobar_helper =
      TabContentsWrapper::GetCurrentWrapperForContents(tc)->
          infobar_tab_helper();
  for (size_t i = 0; i < infobar_helper->infobar_count(); ++i) {
    DictionaryValue* infobar_item = new DictionaryValue;
    InfoBarDelegate* infobar = infobar_helper->GetInfoBarDelegateAt(i);
    if (infobar->AsConfirmInfoBarDelegate()) {
      // Also covers ThemeInstalledInfoBarDelegate.
      infobar_item->SetString("type", "confirm_infobar");
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
  reply.SendError("Invalid action");
}

namespace {

// Gets info about BrowserChildProcessHost. Must run on IO thread to
// honor the semantics of BrowserChildProcessHost.
// Used by AutomationProvider::GetBrowserInfo().
void GetChildProcessHostInfo(ListValue* child_processes) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  for (BrowserChildProcessHost::Iterator iter; !iter.Done(); ++iter) {
    // Only add processes which are already started,
    // since we need their handle.
    if ((*iter)->handle() == base::kNullProcessHandle)
      continue;
    DictionaryValue* item = new DictionaryValue;
    item->SetString("name", iter->name());
    item->SetString("type",
                    content::GetProcessTypeNameInEnglish(iter->type()));
    item->SetInteger("pid", base::GetProcId(iter->handle()));
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
    LocationBarTesting* loc_bar =
        browser->window()->GetLocationBar()->GetLocationBarForTesting();
    size_t page_action_visible_count =
        static_cast<size_t>(loc_bar->PageActionVisibleCount());
    for (size_t i = 0; i < page_action_visible_count; ++i) {
      StringValue* extension_id = new StringValue(
          loc_bar->GetVisiblePageAction(i)->extension_id());
      visible_page_actions->Append(extension_id);
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
      TabContents* tc = browser->GetTabContentsAt(i);
      DictionaryValue* tab = new DictionaryValue;
      tab->SetInteger("index", i);
      tab->SetString("url", tc->GetURL().spec());
      tab->SetInteger("renderer_pid",
                      base::GetProcId(tc->GetRenderProcessHost()->GetHandle()));
      tab->Set("infobars", GetInfobarsInfo(tc));
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
    ExtensionProcessManager::const_iterator jt;
    for (jt = process_manager->begin(); jt != process_manager->end(); ++jt) {
      ExtensionHost* ex_host = *jt;
      // Don't add dead extension processes.
      if (!ex_host->IsRenderViewLive())
        continue;
      DictionaryValue* item = new DictionaryValue;
      item->SetString("name", ex_host->extension()->name());
      item->SetString("extension_id", ex_host->extension()->id());
      item->SetInteger(
          "pid",
          base::GetProcId(ex_host->render_process_host()->GetHandle()));
      DictionaryValue* view = new DictionaryValue;
      view->SetInteger(
          "render_process_id",
          ex_host->render_process_host()->GetID());
      view->SetInteger(
          "render_view_id",
          ex_host->render_view_host()->routing_id());
      item->Set("view", view);
      std::string type;
      switch (ex_host->extension_host_type()) {
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
        default:
          type = "unknown";
          break;
      }
      item->SetString("view_type", type);
      item->SetString("url", ex_host->GetURL().spec());
      item->SetBoolean("loaded", ex_host->did_stop_loading());
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
  proc_observer->StartFetch();
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
  TabContents* tab_contents = NULL;
  if (!args->GetInteger("tab_index", &tab_index) ||
      !(tab_contents = browser->GetTabContentsAt(tab_index))) {
    reply.SendError("tab_index missing or invalid.");
    return;
  }
  scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
  const NavigationController& controller = tab_contents->controller();
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

  NavigationEntry::SSLStatus ssl_status = nav_entry->ssl();
  ssl->SetString("security_style",
                 style_to_string[ssl_status.security_style()]);
  ssl->SetBoolean("ran_insecure_content", ssl_status.ran_insecure_content());
  ssl->SetBoolean("displayed_insecure_content",
                  ssl_status.displayed_insecure_content());
  return_value->Set("ssl", ssl);

  // Page type.
  std::map<content::PageType, std::string> pagetype_to_string;
  pagetype_to_string[content::PAGE_TYPE_NORMAL] = "NORMAL_PAGE";
  pagetype_to_string[content::PAGE_TYPE_ERROR] = "ERROR_PAGE";
  pagetype_to_string[content::PAGE_TYPE_INTERSTITIAL] =
      "INTERSTITIAL_PAGE";
  return_value->SetString("page_type",
                          pagetype_to_string[nav_entry->page_type()]);

  return_value->SetString("favicon_url", nav_entry->favicon().url().spec());
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

  if (action == "open") {
    selected_item->AddObserver(
        new AutomationProviderDownloadUpdatedObserver(
            this, reply_message, true));
    selected_item->OpenDownload();
  } else if (action == "toggle_open_files_like_this") {
    DownloadPrefs* prefs =
        DownloadPrefs::FromDownloadManager(selected_item->GetDownloadManager());
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
      this, reply_message));
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
  std::vector<const TemplateURL*> template_urls = url_model->GetTemplateURLs();
  for (std::vector<const TemplateURL*>::const_iterator it =
       template_urls.begin(); it != template_urls.end(); ++it) {
    DictionaryValue* search_engine = new DictionaryValue;
    search_engine->SetString("short_name", UTF16ToUTF8((*it)->short_name()));
    search_engine->SetString("description", UTF16ToUTF8((*it)->description()));
    search_engine->SetString("keyword", UTF16ToUTF8((*it)->keyword()));
    search_engine->SetBoolean("in_default_list", (*it)->ShowInDefaultList());
    search_engine->SetBoolean("is_default",
        (*it) == url_model->GetDefaultSearchProvider());
    search_engine->SetBoolean("is_valid", (*it)->url()->IsValid());
    search_engine->SetBoolean("supports_replacement",
                              (*it)->url()->SupportsReplacement());
    search_engine->SetString("url", (*it)->url()->url());
    search_engine->SetString("host", (*it)->url()->GetHost());
    search_engine->SetString("path", (*it)->url()->GetPath());
    search_engine->SetString("display_url",
                             UTF16ToUTF8((*it)->url()->DisplayURL()));
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
  const TemplateURL* template_url;
  string16 new_title;
  string16 new_keyword;
  std::string new_url;
  std::string keyword;
  if (!args->GetString("new_title", &new_title) ||
      !args->GetString("new_keyword", &new_keyword) ||
      !args->GetString("new_url", &new_url)) {
    AutomationJSONReply(this, reply_message)
        .SendError("One or more inputs invalid");
    return;
  }
  std::string new_ref_url = TemplateURLRef::DisplayURLToURLRef(
      UTF8ToUTF16(new_url));
  scoped_ptr<KeywordEditorController> controller(
      new KeywordEditorController(browser->profile()));
  if (args->GetString("keyword", &keyword)) {
    template_url = url_model->GetTemplateURLForKeyword(UTF8ToUTF16(keyword));
    if (template_url == NULL) {
      AutomationJSONReply(this, reply_message)
          .SendError(StringPrintf("No match for keyword: %s", keyword.c_str()));
      return;
    }
    url_model->AddObserver(new AutomationProviderSearchEngineObserver(
        this, reply_message));
    controller->ModifyTemplateURL(template_url, new_title, new_keyword,
                                  new_ref_url);
  } else {
    url_model->AddObserver(new AutomationProviderSearchEngineObserver(
        this, reply_message));
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
  const TemplateURL* template_url(
      url_model->GetTemplateURLForKeyword(UTF8ToUTF16(keyword)));
  if (template_url == NULL) {
    AutomationJSONReply(this, reply_message)
        .SendError(StringPrintf("No match for keyword: %s", keyword.c_str()));
    return;
  }
  if (action == "delete") {
    url_model->AddObserver(new AutomationProviderSearchEngineObserver(
      this, reply_message));
    url_model->Remove(template_url);
  } else if (action == "default") {
    url_model->AddObserver(new AutomationProviderSearchEngineObserver(
      this, reply_message));
    url_model->SetDefaultSearchProvider(template_url);
  } else {
    AutomationJSONReply(this, reply_message)
        .SendError(StringPrintf("Invalid action: %s", action.c_str()));
  }
}

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

// Sample json input: { "command": "GetPrefsInfo" }
// Refer chrome/test/pyautolib/prefs_info.py for sample json output.
void TestingAutomationProvider::GetPrefsInfo(Browser* browser,
                                             DictionaryValue* args,
                                             IPC::Message* reply_message) {
  DictionaryValue* items = browser->profile()->GetPrefs()->
      GetPreferenceValues();

  scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
  return_value->Set("prefs", items);  // return_value owns items.
  AutomationJSONReply(this, reply_message).SendSuccess(return_value.get());
}

// Sample json input: { "command": "SetPrefs", "path": path, "value": value }
void TestingAutomationProvider::SetPrefs(Browser* browser,
                                         DictionaryValue* args,
                                         IPC::Message* reply_message) {
  std::string path;
  Value* val;
  AutomationJSONReply reply(this, reply_message);
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

  LocationBar* loc_bar = browser->window()->GetLocationBar();
  OmniboxView* omnibox_view = loc_bar->location_entry();
  AutocompleteEditModel* model = omnibox_view->model();

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

  AutomationJSONReply(this, reply_message).SendSuccess(return_value.get());
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
  OmniboxView* omnibox_view = loc_bar->location_entry();
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
  AutocompleteEditModel* model = loc_bar->location_entry()->model();
  model->OnUpOrDownKeyPressed(count);
  reply.SendSuccess(NULL);
}

// Sample json input: { "command": "OmniboxAcceptInput" }
void TestingAutomationProvider::OmniboxAcceptInput(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  NavigationController& controller =
      browser->GetSelectedTabContents()->controller();
  new OmniboxAcceptNotificationObserver(&controller, this, reply_message);
  browser->window()->GetLocationBar()->AcceptInput();
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
        instant->GetPreviewContents()->tab_contents()) {
      TabContents* contents = instant->GetPreviewContents()->tab_contents();
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
  base::JSONWriter::Write(return_value.get(), false, &json_return);
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
  AutomationJSONReply reply(this, reply_message);
  if (!args->GetString("path", &path)) {
    reply.SendError("path not specified.");
  } else if (!PluginPrefs::GetForProfile(browser->profile())->EnablePlugin(
      true, FilePath(path))) {
    reply.SendError(StringPrintf("Could not enable plugin for path %s.",
                                 path.c_str()));
  } else {
    reply.SendSuccess(NULL);
  }
}

// Sample json input:
//    { "command": "DisablePlugin",
//      "path": "/Library/Internet Plug-Ins/Flash Player.plugin" }
void TestingAutomationProvider::DisablePlugin(Browser* browser,
                                              DictionaryValue* args,
                                              IPC::Message* reply_message) {
  FilePath::StringType path;
  AutomationJSONReply reply(this, reply_message);
  if (!args->GetString("path", &path)) {
    reply.SendError("path not specified.");
  } else if (!PluginPrefs::GetForProfile(browser->profile())->EnablePlugin(
      false, FilePath(path))) {
    reply.SendError(StringPrintf("Could not disable plugin for path %s.",
                                 path.c_str()));
  } else {
    reply.SendSuccess(NULL);
  }
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
  TabContents* tab_contents = NULL;

  if (!args->GetInteger("tab_index", &tab_index) ||
      !args->GetString("filename", &filename)) {
    AutomationJSONReply(this, reply_message)
        .SendError("tab_index or filename param missing");
    return;
  } else {
    tab_contents = browser->GetTabContentsAt(tab_index);
    if (!tab_contents) {
      AutomationJSONReply(this, reply_message).SendError("no tab at tab_index");
      return;
    }
  }
  // We're doing a SAVE_AS_ONLY_HTML so the the directory path isn't
  // used.  Nevertheless, SavePackage requires it be valid.  Sigh.
  parent_directory = FilePath(filename).DirName().value();
  if (!tab_contents->SavePage(
          FilePath(filename),
          FilePath(parent_directory),
          SavePackage::SAVE_AS_ONLY_HTML)) {
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
webkit_glue::PasswordForm GetPasswordFormFromDict(
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

  webkit_glue::PasswordForm password_form;
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

  webkit_glue::PasswordForm new_password =
      GetPasswordFormFromDict(*password_dict);

  // Use IMPLICIT_ACCESS since new passwords aren't added in incognito mode.
  PasswordStore* password_store =
      browser->profile()->GetPasswordStore(Profile::IMPLICIT_ACCESS);

  // The password store does not exist for an incognito window.
  if (password_store == NULL) {
    scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
    return_value->SetBoolean("password_added", false);
    AutomationJSONReply(this, reply_message).SendSuccess(return_value.get());
    return;
  }

  // This observer will delete itself.
  PasswordStoreLoginsChangedObserver *observer =
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
  webkit_glue::PasswordForm to_remove =
      GetPasswordFormFromDict(*password_dict);

  // Use EXPLICIT_ACCESS since passwords can be removed in incognito mode.
  PasswordStore* password_store =
      browser->profile()->GetPasswordStore(Profile::EXPLICIT_ACCESS);
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
  PasswordStore* password_store =
      browser->profile()->GetPasswordStore(Profile::EXPLICIT_ACCESS);

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

// Get the TranslateInfoBarDelegate from TabContents.
TranslateInfoBarDelegate* GetTranslateInfoBarDelegate(
    TabContents* tab_contents) {
  InfoBarTabHelper* infobar_helper =
      TabContentsWrapper::GetCurrentWrapperForContents(tab_contents)->
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
  SendFindRequest(tab_contents->tab_contents(),
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

  TabContents* tab_contents = tab_contents_wrapper->tab_contents();
  // Get the translate bar if there is one and pass it to the observer.
  // The observer will check for null and populate the information accordingly.
  TranslateInfoBarDelegate* translate_bar =
      GetTranslateInfoBarDelegate(tab_contents);

  TabLanguageDeterminedObserver* observer = new TabLanguageDeterminedObserver(
      this, reply_message, tab_contents, translate_bar);
  // If the language for the page hasn't been loaded yet, then just make
  // the observer, otherwise call observe directly.
  TranslateTabHelper* helper = TabContentsWrapper::GetCurrentWrapperForContents(
      tab_contents)->translate_tab_helper();
  std::string language = helper->language_state().original_language();
  if (!language.empty()) {
    observer->Observe(chrome::NOTIFICATION_TAB_LANGUAGE_DETERMINED,
                      content::Source<TabContents>(tab_contents),
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

  TabContents* tab_contents = tab_contents_wrapper->tab_contents();
  TranslateInfoBarDelegate* translate_bar =
      GetTranslateInfoBarDelegate(tab_contents);
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
    new PageTranslatedObserver(this, reply_message, tab_contents);
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
      new PageTranslatedObserver(this, reply_message, tab_contents);
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
    new PageTranslatedObserver(this, reply_message, tab_contents);
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
    item->SetString("url", (*it)->tab_contents()->GetURL().spec());
    item->SetString("title", (*it)->tab_contents()->GetTitle());
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
  ExtensionService* service = profile()->GetExtensionService();
  ExtensionProcessManager* manager = profile()->GetExtensionProcessManager();
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
          (with_ui ? new ExtensionInstallUI(profile()) : NULL);
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
  ExtensionService* service = profile()->GetExtensionService();
  if (!service) {
    reply.SendError("No extensions service.");
    return;
  }
  scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
  ListValue* extensions_values = new ListValue;
  const ExtensionList* extensions = service->extensions();
  const ExtensionList* disabled_extensions = service->disabled_extensions();
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
                               extension->background_url().spec());
    extension_value->SetString("options_url",
                               extension->options_url().spec());
    extension_value->Set("host_permissions",
                         GetHostPermissions(extension, false));
    extension_value->Set("effective_host_permissions",
                         GetHostPermissions(extension, true));
    extension_value->Set("api_permissions", GetAPIPermissions(extension));
    extension_value->SetBoolean("is_component",
                                extension->location() == Extension::COMPONENT);
    extension_value->SetBoolean("is_internal",
                                extension->location() == Extension::INTERNAL);
    extension_value->SetBoolean("is_enabled", service->IsExtensionEnabled(id));
    extension_value->SetBoolean("allowed_in_incognito",
                                service->IsIncognitoEnabled(id));
    extensions_values->Append(extension_value);
  }
  return_value->Set("extensions", extensions_values);
  reply.SendSuccess(return_value.get());
}

// See UninstallExtensionById() in chrome/test/pyautolib/pyauto.py for sample
// json input.
// Sample json output: {}
void TestingAutomationProvider::UninstallExtensionById(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  std::string id;
  if (!args->GetString("id", &id)) {
    AutomationJSONReply(this, reply_message).SendError(
        "Must include string id.");
    return;
  }
  ExtensionService* service = profile()->GetExtensionService();
  if (!service) {
    AutomationJSONReply(this, reply_message).SendError(
        "No extensions service.");
    return;
  }

  if (!service->GetExtensionById(id, true) &&
      !service->GetTerminatedExtension(id)) {
    // The extension ID does not correspond to any extension, whether crashed
    // or not.
    AutomationJSONReply(this, reply_message).SendError(base::StringPrintf(
        "Extension does not exist: %s.", id.c_str()));
    return;
  }

  // Wait for a notification indicating that the extension with the given ID
  // has been uninstalled.  This observer will delete itself.
  new ExtensionUninstallObserver(this, reply_message, id);
  service->UninstallExtension(id, false, NULL);
}

// See SetExtensionStateById() in chrome/test/pyautolib/pyauto.py
// for sample json input.
void TestingAutomationProvider::SetExtensionStateById(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);
  std::string id;
  if (!args->GetString("id", &id)) {
    reply.SendError("Missing or invalid key: id");
    return;
  }

  bool enable;
  if (!args->GetBoolean("enable", &enable)) {
    reply.SendError("Missing or invalid key: enable");
    return;
  }

  bool allow_in_incognito;
  if (!args->GetBoolean("allow_in_incognito", &allow_in_incognito)) {
    reply.SendError("Missing or invalid key: allow_in_incognito");
    return;
  }

  if (allow_in_incognito && !enable) {
    reply.SendError("Invalid state: Disabled extension "
                    "cannot be allowed in incognito mode.");
    return;
  }

  ExtensionService* service = profile()->GetExtensionService();
  if (!service) {
    reply.SendError("No extensions service.");
    return;
  }

  if (!service->GetExtensionById(id, true) &&
      !service->GetTerminatedExtension(id)) {
    // The extension ID does not correspond to any extension, whether crashed
    // or not.
    reply.SendError(base::StringPrintf("Extension does not exist: %s.",
                                       id.c_str()));
    return;
  }

  if (enable)
    service->EnableExtension(id);
  else
    service->DisableExtension(id);

  service->SetIsIncognitoEnabled(id, allow_in_incognito);
  reply.SendSuccess(NULL);
}

// See TriggerPageActionById() in chrome/test/pyautolib/pyauto.py
// for sample json input.
void TestingAutomationProvider::TriggerPageActionById(
    DictionaryValue* args,
    IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);

  std::string error;
  Browser* browser;
  if (!GetBrowserFromJSONArgs(args, &browser, &error)) {
    reply.SendError(error);
    return;
  }
  std::string id;
  if (!args->GetString("id", &id)) {
    reply.SendError("Missing or invalid key: id");
    return;
  }

  ExtensionService* service = browser->profile()->GetExtensionService();
  if (!service) {
    reply.SendError("No extensions service.");
    return;
  }
  if (!service->GetInstalledExtension(id)) {
    // The extension ID does not correspond to any extension, whether crashed
    // or not.
    reply.SendError(base::StringPrintf("Extension %s is not installed.",
                                       id.c_str()));
    return;
  }
  const Extension* extension = service->GetExtensionById(id, false);
  if (!extension) {
    reply.SendError("Extension is disabled or has crashed.");
    return;
  }

  if (ExtensionAction* page_action = extension->page_action()) {
    LocationBarTesting* loc_bar =
        browser->window()->GetLocationBar()->GetLocationBarForTesting();
    size_t page_action_visible_count =
        static_cast<size_t>(loc_bar->PageActionVisibleCount());
    for (size_t i = 0; i < page_action_visible_count; ++i) {
      if (loc_bar->GetVisiblePageAction(i) == page_action) {
        loc_bar->TestPageActionPressed(i);
        reply.SendSuccess(NULL);
        return;
      }
    }
    reply.SendError("Extension doesn't have any visible page action icon.");
  } else {
    reply.SendError("Extension doesn't have any page action.");
  }
}

// See TriggerBrowserActionById() in chrome/test/pyautolib/pyauto.py
// for sample json input.
void TestingAutomationProvider::TriggerBrowserActionById(
    DictionaryValue* args,
    IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);

  std::string error;
  Browser* browser;
  if (!GetBrowserFromJSONArgs(args, &browser, &error)) {
    reply.SendError(error);
    return;
  }
  std::string id;
  if (!args->GetString("id", &id)) {
    reply.SendError("Missing or invalid key: id");
    return;
  }

  ExtensionService* service = browser->profile()->GetExtensionService();
  if (!service) {
    reply.SendError("No extensions service.");
    return;
  }
  if (!service->GetInstalledExtension(id)) {
    // The extension ID does not correspond to any extension, whether crashed
    // or not.
    reply.SendError(base::StringPrintf("Extension %s is not installed.",
                                       id.c_str()));
    return;
  }
  const Extension* extension = service->GetExtensionById(id, false);
  if (!extension) {
    reply.SendError("Extension is disabled or has crashed.");
    return;
  }

  if (extension->browser_action()) {
    BrowserActionTestUtil browser_actions(browser);
    int num_browser_actions = browser_actions.NumberOfBrowserActions();
    // TODO: Implement the platform-specific GetExtensionId() in
    // BrowserActionTestUtil.
    if (num_browser_actions != 1) {
      reply.SendError(StringPrintf(
          "Found %d browser actions. Only one browser action must be active.",
          num_browser_actions));
      return;
    }
    browser_actions.Press(0);
    reply.SendSuccess(NULL);
    return;
  } else {
    reply.SendError("Extension doesn't have any browser action.");
    return;
  }
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
  tab_contents->render_view_host()->ExecuteJavascriptInWebFrame(
      frame_xpath, UTF8ToUTF16(set_automation_id));
  tab_contents->render_view_host()->ExecuteJavascriptInWebFrame(
      frame_xpath, javascript);
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

  TabContents* tab_contents = browser->GetTabContentsAt(tab_index);
  if (!tab_contents) {
    AutomationJSONReply(this, reply_message).SendError(
        StringPrintf("No such tab at index %d", tab_index));
    return;
  }

  new AutofillDisplayedObserver(
      chrome::NOTIFICATION_AUTOFILL_DID_SHOW_SUGGESTIONS,
      tab_contents->render_view_host(), this, reply_message);
  SendWebKeyPressEventAsync(ui::VKEY_DOWN, tab_contents);
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

  TabContents* tab_contents = browser->GetTabContentsAt(tab_index);
  if (!tab_contents) {
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
      tab_contents->render_view_host(), this, reply_message);
  SendWebKeyPressEventAsync(key_code, tab_contents);
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

  TabContents* tab_contents = browser->GetTabContentsAt(tab_index);
  if (!tab_contents) {
    AutomationJSONReply(this, reply_message).SendError(
        StringPrintf("No such tab at index %d", tab_index));
    return;
  }

  new AutofillDisplayedObserver(
      chrome::NOTIFICATION_AUTOFILL_DID_FILL_FORM_DATA,
      tab_contents->render_view_host(), this, reply_message);
  SendWebKeyPressEventAsync(ui::VKEY_RETURN, tab_contents);
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
// { u'authenticated': True,
//   u'last synced': u'Just now',
//   u'summary': u'READY',
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
    sync_info->SetString("summary",
        ProfileSyncService::BuildSyncStatusSummaryText(status.summary));
    sync_info->SetString("sync url", service->sync_service_url().host());
    sync_info->SetBoolean("authenticated", status.authenticated);
    sync_info->SetString("last synced", service->GetLastSyncedTimeString());
    sync_info->SetInteger("updates received", status.updates_received);
    ListValue* synced_datatype_list = new ListValue;
    syncable::ModelTypeSet synced_datatypes;
    service->GetPreferredDataTypes(&synced_datatypes);
    for (syncable::ModelTypeSet::iterator it = synced_datatypes.begin();
         it != synced_datatypes.end(); ++it) {
      synced_datatype_list->Append(
          new StringValue(syncable::ModelTypeToString(*it)));
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
  if (!browser->profile()->GetProfileSyncService()) {
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
  ProfileSyncService::Status status = sync_waiter_->GetStatus();
  if (status.summary == ProfileSyncService::Status::READY) {
    scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
    return_value->SetBoolean("success", true);
    reply.SendSuccess(return_value.get());
  } else {
    std::string error_msg = "Wait for sync cycle was unsuccessful. "
                            "Sync status: ";
    error_msg.append(
        ProfileSyncService::BuildSyncStatusSummaryText(status.summary));
    reply.SendError(error_msg);
  }
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
  if (!browser->profile()->GetProfileSyncService()) {
    reply.SendError("ProfileSyncService initialization failed.");
    return;
  }
  if (!sync_waiter_->AwaitSyncRestart()) {
    reply.SendError("Sync did not successfully restart.");
    return;
  }
  ProfileSyncService::Status status = sync_waiter_->GetStatus();
  if (status.summary == ProfileSyncService::Status::READY) {
    scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
    return_value->SetBoolean("success", true);
    reply.SendSuccess(return_value.get());
  } else {
    std::string error_msg = "Wait for sync restart was unsuccessful. "
                            "Sync status: ";
    error_msg.append(
        ProfileSyncService::BuildSyncStatusSummaryText(status.summary));
    reply.SendError(error_msg);
  }
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
  ProfileSyncService::Status status = sync_waiter_->GetStatus();
  if (status.summary == ProfileSyncService::Status::READY ||
      status.summary == ProfileSyncService::Status::SYNCING) {
    scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
    return_value->SetBoolean("success", true);
    reply.SendSuccess(return_value.get());
  } else {
    reply.SendError("Enabling sync for given datatypes was unsuccessful");
  }
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
    ProfileSyncService::Status status = sync_waiter_->GetStatus();
    if (status.summary != ProfileSyncService::Status::READY &&
        status.summary != ProfileSyncService::Status::SYNCING) {
      scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
      return_value->SetBoolean("success", true);
      reply.SendSuccess(return_value.get());
    } else {
      reply.SendError("Disabling all sync datatypes was unsuccessful");
    }
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
    ProfileSyncService::Status status = sync_waiter_->GetStatus();
    if (status.summary == ProfileSyncService::Status::READY ||
        status.summary == ProfileSyncService::Status::SYNCING) {
      scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
      return_value->SetBoolean("success", true);
      reply.SendSuccess(return_value.get());
    } else {
      reply.SendError("Disabling sync for given datatypes was unsuccessful");
    }
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

void TestingAutomationProvider::MoveNTPMostVisitedThumbnail(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);
  std::string url, error;
  int index, old_index;
  if (!args->GetString("url", &url)) {
    reply.SendError("Missing or invalid 'url' key.");
    return;
  }
  if (!args->GetInteger("index", &index)) {
    reply.SendError("Missing or invalid 'index' key.");
    return;
  }
  if (!args->GetInteger("old_index", &old_index)) {
    reply.SendError("Missing or invalid 'old_index' key");
    return;
  }
  history::TopSites* top_sites = browser->profile()->GetTopSites();
  if (!top_sites) {
    reply.SendError("TopSites service is not initialized.");
    return;
  }
  GURL swapped;
  // If thumbnail A is switching positions with a pinned thumbnail B, B
  // should be pinned in the old index of A.
  if (top_sites->GetPinnedURLAtIndex(index, &swapped)) {
    top_sites->AddPinnedURL(swapped, old_index);
  }
  top_sites->AddPinnedURL(GURL(url), index);
  reply.SendSuccess(NULL);
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

void TestingAutomationProvider::UnpinNTPMostVisitedThumbnail(
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
  top_sites->RemovePinnedURL(GURL(url));
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
    TabContents* tab_contents) {
  // Create and send a "key down" event for the specified key code.
  NativeWebKeyboardEvent event_down;
  BuildSimpleWebKeyEvent(WebKit::WebInputEvent::RawKeyDown, key_code,
                         &event_down);
  tab_contents->render_view_host()->ForwardKeyboardEvent(event_down);

  // Create and send a corresponding "key up" event.
  NativeWebKeyboardEvent event_up;
  BuildSimpleWebKeyEvent(WebKit::WebInputEvent::KeyUp, key_code, &event_up);
  tab_contents->render_view_host()->ForwardKeyboardEvent(event_up);
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

  TabContents* tab_contents;
  if (!GetTabFromJSONArgs(args, &tab_contents, &error)) {
    AutomationJSONReply(this, reply_message).SendError(error);
    return;
  }
  new InputEventAckNotificationObserver(this, reply_message, event.type, 1);
  tab_contents->render_view_host()->ForwardKeyboardEvent(event);
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
  TabContents* tab_contents;
  if (!GetBrowserAndTabFromJSONArgs(args, &browser, &tab_contents, &error)) {
    AutomationJSONReply(this, reply_message).SendError(error);
    return;
  }
  // The key events will be sent to the browser window, we need the current tab
  // containing the element we send the text in to be shown.
  browser->ActivateTabAt(
      browser->GetIndexOfController(&tab_contents->controller()), true);

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
          base::Bind(&TestingAutomationProvider::SendSuccessReply, this,
                     reply_message))) {
    AutomationJSONReply(this, reply_message)
        .SendError("Could not send the native key event");
  }
}

void TestingAutomationProvider::SendSuccessReply(IPC::Message* reply_message) {
  AutomationJSONReply(this, reply_message).SendSuccess(NULL);
}

namespace {

// Gets the active JavaScript modal dialog, or NULL if none.
JavaScriptAppModalDialog* GetActiveJavaScriptModalDialog(
    std::string* error_msg) {
  AppModalDialogQueue* dialog_queue = AppModalDialogQueue::GetInstance();
  if (!dialog_queue->HasActiveDialog()) {
    *error_msg = "No modal dialog is showing";
    return NULL;
  }
  if (!dialog_queue->active_dialog()->IsJavaScriptModalDialog()) {
    *error_msg = "No JavaScript modal dialog is showing";
    return NULL;
  }
  return static_cast<JavaScriptAppModalDialog*>(dialog_queue->active_dialog());
}

}  // namespace

void TestingAutomationProvider::GetAppModalDialogMessage(
    DictionaryValue* args, IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);
  std::string error_msg;
  JavaScriptAppModalDialog* dialog = GetActiveJavaScriptModalDialog(&error_msg);
  if (!dialog) {
    reply.SendError(error_msg);
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

  std::string error_msg;
  JavaScriptAppModalDialog* dialog = GetActiveJavaScriptModalDialog(&error_msg);
  if (!dialog) {
    reply.SendError(error_msg);
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

  TabContents* old_contents = browser->GetSelectedTabContents();
  if (!old_contents) {
    AutomationJSONReply(this, reply_message).SendError(
        "Cannot identify selected tab contents.");
    return;
  }

  // This observer will delete itself.
  new AppLaunchObserver(&old_contents->controller(), this, reply_message,
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

void TestingAutomationProvider::WaitForAllTabsToStopLoading(
    DictionaryValue* args,
    IPC::Message* reply_message) {
  if (AppModalDialogQueue::GetInstance()->HasActiveDialog()) {
    AutomationJSONReply(this, reply_message).SendSuccess(NULL);
    return;
  }

  // This class will send the message immediately if no tab is loading.
  new AllTabsStoppedLoadingObserver(this, reply_message);
}

void TestingAutomationProvider::SetPolicies(
    DictionaryValue* args,
    IPC::Message* reply_message) {
  scoped_ptr<AutomationJSONReply> reply(
      new AutomationJSONReply(this, reply_message));

#if !defined(ENABLE_CONFIGURATION_POLICY) || defined(OFFICIAL_BUILD)
  reply->SendError("Configuration Policy disabled");
#else
  const policy::PolicyDefinitionList* list =
      policy::GetChromePolicyDefinitionList();
  policy::BrowserPolicyConnector* connector =
      g_browser_process->browser_policy_connector();
  struct {
    std::string name;
    policy::ConfigurationPolicyProvider* provider;
  } providers[] = {
    { "managed_cloud",        connector->GetManagedCloudProvider()        },
    { "managed_platform",     connector->GetManagedPlatformProvider()     },
    { "recommended_cloud",    connector->GetRecommendedCloudProvider()    },
    { "recommended_platform", connector->GetRecommendedPlatformProvider() }
  };
  // Verify if all the requested providers exist before changing anything.
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(providers); ++i) {
    DictionaryValue* policies = NULL;
    if (args->GetDictionary(providers[i].name, &policies) &&
        policies &&
        !providers[i].provider) {
      reply->SendError("Provider not available: " + providers[i].name);
      return;
    }
  }
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(providers); ++i) {
    DictionaryValue* policies = NULL;
    if (args->GetDictionary(providers[i].name, &policies) && policies) {
      policy::PolicyMap* map = new policy::PolicyMap;
      map->LoadFrom(policies, list);
      providers[i].provider->OverridePolicies(map);
    }
  }

  // Make sure the policies are in effect before returning. This will go
  // away once all platforms rely on directly installing the policy files and
  // using RefreshPolicies, and SetPolicies is removed.
  PolicyUpdatesObserver::PostCallbackAfterPolicyUpdates(
      base::Bind(&AutomationJSONReply::SendSuccess,
                 base::Owned(reply.release()),
                 static_cast<const Value*>(NULL)));
#endif  // defined(OFFICIAL_BUILD)
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
  types[Value::TYPE_INTEGER] = "int";
  types[Value::TYPE_STRING] = "str";
  types[Value::TYPE_LIST] = "list";

  const policy::PolicyDefinitionList::Entry* entry;
  for (entry = list->begin; entry != list->end; ++entry) {
    if (types.find(entry->value_type) == types.end()) {
      std::string error("Unrecognized policy type for policy ");
      reply.SendError(error + entry->name);
      return;
    }
    response.SetString(entry->name, types[entry->value_type]);
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
  policy::BrowserPolicyConnector* connector =
      g_browser_process->browser_policy_connector();
  new PolicyUpdatesObserver(this, reply_message, connector);
  connector->RefreshPolicies();
#endif
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
        tab_tracker_->GetResource(id_or_handle)->tab_contents());
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
  TabContents* tab_contents;
  if (!GetBrowserAndTabFromJSONArgs(args, &browser, &tab_contents, &error)) {
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
      &tab_contents->controller(), this, reply_message,
      navigation_count, false, true);
  browser->OpenURLFromTab(tab_contents, OpenURLParams(
      GURL(url), GURL(), CURRENT_TAB,
      content::PAGE_TRANSITION_TYPED, false));
}

void TestingAutomationProvider::ExecuteJavascriptJSON(
    DictionaryValue* args,
    IPC::Message* reply_message) {
  if (SendErrorIfModalDialogActive(this, reply_message))
    return;

  string16 frame_xpath, javascript;
  std::string error;
  TabContents* tab_contents;
  if (!GetTabFromJSONArgs(args, &tab_contents, &error)) {
    AutomationJSONReply(this, reply_message).SendError(error);
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
                                     tab_contents->render_view_host());
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

void TestingAutomationProvider::GoForward(
    DictionaryValue* args,
    IPC::Message* reply_message) {
  if (SendErrorIfModalDialogActive(this, reply_message))
    return;

  TabContents* tab_contents;
  std::string error;
  if (!GetTabFromJSONArgs(args, &tab_contents, &error)) {
    AutomationJSONReply(this, reply_message).SendError(error);
    return;
  }
  NavigationController& controller = tab_contents->controller();
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

  TabContents* tab_contents;
  std::string error;
  if (!GetTabFromJSONArgs(args, &tab_contents, &error)) {
    AutomationJSONReply(this, reply_message).SendError(error);
    return;
  }
  NavigationController& controller = tab_contents->controller();
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

  TabContents* tab_contents;
  std::string error;
  if (!GetTabFromJSONArgs(args, &tab_contents, &error)) {
    AutomationJSONReply(this, reply_message).SendError(error);
    return;
  }
  NavigationController& controller = tab_contents->controller();
  new NavigationNotificationObserver(&controller, this, reply_message,
                                     1, false, true);
  controller.Reload(false);
}

void TestingAutomationProvider::GetTabURLJSON(
    DictionaryValue* args,
    IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);
  TabContents* tab_contents;
  std::string error;
  if (!GetTabFromJSONArgs(args, &tab_contents, &error)) {
    reply.SendError(error);
    return;
  }
  DictionaryValue dict;
  dict.SetString("url", tab_contents->GetURL().possibly_invalid_spec());
  reply.SendSuccess(&dict);
}

void TestingAutomationProvider::GetTabTitleJSON(
    DictionaryValue* args,
    IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);
  TabContents* tab_contents;
  std::string error;
  if (!GetTabFromJSONArgs(args, &tab_contents, &error)) {
    reply.SendError(error);
    return;
  }
  DictionaryValue dict;
  dict.SetString("title", tab_contents->GetTitle());
  reply.SendSuccess(&dict);
}

void TestingAutomationProvider::CaptureEntirePageJSON(
    DictionaryValue* args,
    IPC::Message* reply_message) {
  if (SendErrorIfModalDialogActive(this, reply_message))
    return;

  TabContents* tab_contents;
  std::string error;

  if (!GetTabFromJSONArgs(args, &tab_contents, &error)) {
    AutomationJSONReply(this, reply_message).SendError(error);
    return;
  }

  FilePath::StringType path_str;
  if (!args->GetString("path", &path_str)) {
    AutomationJSONReply(this, reply_message)
        .SendError("'path' missing or invalid");
    return;
  }

  RenderViewHost* render_view = tab_contents->render_view_host();
  if (render_view) {
    FilePath path(path_str);
    // This will delete itself when finished.
    PageSnapshotTaker* snapshot_taker = new PageSnapshotTaker(
        this, reply_message,
        TabContentsWrapper::GetCurrentWrapperForContents(tab_contents), path);
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

void TestingAutomationProvider::CloseTabJSON(
    DictionaryValue* args, IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);
  Browser* browser;
  TabContents* tab_contents;
  std::string error;
  if (!GetBrowserAndTabFromJSONArgs(args, &browser, &tab_contents, &error)) {
    reply.SendError(error);
    return;
  }
  browser->CloseTabContents(tab_contents);
  reply.SendSuccess(NULL);
}

void TestingAutomationProvider::ActivateTabJSON(
    DictionaryValue* args,
    IPC::Message* reply_message) {
  if (SendErrorIfModalDialogActive(this, reply_message))
    return;

  AutomationJSONReply reply(this, reply_message);
  Browser* browser;
  TabContents* tab_contents;
  std::string error;
  if (!GetBrowserAndTabFromJSONArgs(args, &browser, &tab_contents, &error)) {
    reply.SendError(error);
    return;
  }
  browser->ActivateTabAt(
      browser->GetIndexOfController(&tab_contents->controller()), true);
  reply.SendSuccess(NULL);
}

// Sample json input: { "command": "UpdateExtensionsNow" }
// Sample json output: {}
void TestingAutomationProvider::UpdateExtensionsNow(
    DictionaryValue* args,
    IPC::Message* reply_message) {
  ExtensionService* service = profile()->GetExtensionService();
  if (!service) {
    AutomationJSONReply(this, reply_message).SendError(
        "No extensions service.");
    return;
  }

  ExtensionUpdater* updater = service->updater();
  if (!updater) {
    AutomationJSONReply(this, reply_message).SendError(
        "No updater for extensions service.");
    return;
  }

  ExtensionProcessManager* manager = profile()->GetExtensionProcessManager();
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
  // TODO(kkania): Remove this when crbug.com/91311 is fixed.
  // Named server channels should ideally be created and closed on the file
  // thread, within the IPC channel code.
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  if (!provider->InitializeChannel(
          automation::kNamedInterfacePrefix + channel_id)) {
    reply.SendError("Failed to initialize channel: " + channel_id);
    return;
  }
  provider->SetExpectedTabCount(0);
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
          controller->tab_contents()), target_count);
}

// Gets the current used encoding name of the page in the specified tab.
void TestingAutomationProvider::GetPageCurrentEncoding(
    int tab_handle, std::string* current_encoding) {
  if (tab_tracker_->ContainsHandle(tab_handle)) {
    NavigationController* nav = tab_tracker_->GetResource(tab_handle);
    Browser* browser = FindAndActivateTab(nav);
    DCHECK(browser);

    if (browser->command_updater()->IsCommandEnabled(IDC_ENCODING_MENU))
      *current_encoding = nav->tab_contents()->encoding();
  }
}

void TestingAutomationProvider::ShutdownSessionService(int handle,
                                                       bool* result) {
  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    SessionServiceFactory::ShutdownForProfile(browser->profile());
    *result = true;
  } else {
    *result = false;
  }
}

void TestingAutomationProvider::SetContentSetting(
    int handle,
    const std::string& host,
    ContentSettingsType content_type,
    ContentSetting setting,
    bool* success) {
  *success = false;
  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    HostContentSettingsMap* map =
        browser->profile()->GetHostContentSettingsMap();
    if (host.empty()) {
      map->SetDefaultContentSetting(content_type, setting);
    } else {
      map->SetContentSetting(ContentSettingsPattern::FromString(host),
                             ContentSettingsPattern::Wildcard(),
                             content_type,
                             std::string(),
                             setting);
    }
    *success = true;
  }
}

void TestingAutomationProvider::LoadBlockedPlugins(int tab_handle,
                                                   bool* success) {
  *success = false;
  if (tab_tracker_->ContainsHandle(tab_handle)) {
    NavigationController* nav = tab_tracker_->GetResource(tab_handle);
    if (!nav)
      return;
    TabContents* contents = nav->tab_contents();
    if (!contents)
      return;
    RenderViewHost* host = contents->render_view_host();
    host->Send(new ChromeViewMsg_LoadBlockedPlugins(host->routing_id()));
    *success = true;
  }
}

void TestingAutomationProvider::ResetToDefaultTheme() {
  ThemeServiceFactory::GetForProfile(profile_)->UseDefaultTheme();
}

void TestingAutomationProvider::WaitForProcessLauncherThreadToGoIdle(
    IPC::Message* reply_message) {
  new WaitForProcessLauncherThreadToGoIdleObserver(this, reply_message);
}

void TestingAutomationProvider::GetParentBrowserOfTab(int tab_handle,
                                                      int* browser_handle,
                                                      bool* success) {
  *success = false;
  if (tab_tracker_->ContainsHandle(tab_handle)) {
    NavigationController* controller = tab_tracker_->GetResource(tab_handle);
    int index;
    Browser* browser = Browser::GetBrowserForController(controller, &index);
    if (browser) {
      *browser_handle = browser_tracker_->Add(browser);
      *success = true;
    }
  }
}

// TODO(brettw) change this to accept GURLs when history supports it
void TestingAutomationProvider::OnRedirectQueryComplete(
    HistoryService::Handle request_handle,
    GURL from_url,
    bool success,
    history::RedirectList* redirects) {
  DCHECK_EQ(redirect_query_, request_handle);
  DCHECK(reply_message_ != NULL);

  std::vector<GURL> redirects_gurl;
  reply_message_->WriteBool(success);
  if (success) {
    for (size_t i = 0; i < redirects->size(); i++)
      redirects_gurl.push_back(redirects->at(i));
  }

  IPC::ParamTraits<std::vector<GURL> >::Write(reply_message_, redirects_gurl);

  Send(reply_message_);
  redirect_query_ = 0;
  reply_message_ = NULL;
}

void TestingAutomationProvider::OnRemoveProvider() {
  if (g_browser_process)
    g_browser_process->GetAutomationProviderList()->RemoveProvider(this);
}
