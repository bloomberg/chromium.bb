// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROME_NOTIFICATION_TYPES_H_
#define CHROME_BROWSER_CHROME_NOTIFICATION_TYPES_H_

#include "build/build_config.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/notification_types.h"
#else
#include "content/public/browser/notification_types.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#define PREVIOUS_END extensions::NOTIFICATION_EXTENSIONS_END
#else
#define PREVIOUS_END content::NOTIFICATION_CONTENT_END
#endif

// **
// ** NOTICE
// **
// ** The notification system is deprecated, obsolete, and is slowly being
// ** removed. See https://crbug.com/268984.
// **
// ** Please don't add any new notification types, and please help migrate
// ** existing uses of the notification types below to use the Observer and
// ** Callback patterns.
// **

namespace chrome {

enum NotificationType {
  NOTIFICATION_CHROME_START = PREVIOUS_END,

  // Browser-window ----------------------------------------------------------

  // This message is sent after a window has been opened.  The source is a
  // Source<Browser> containing the affected Browser.  No details are
  // expected.
  // DEPRECATED: Use BrowserListObserver::OnBrowserAdded()
  NOTIFICATION_BROWSER_OPENED = NOTIFICATION_CHROME_START,

  // This message is sent when closing a browser has been cancelled, either by
  // the user cancelling a beforeunload dialog, or IsClosingPermitted()
  // disallowing closing. This notification implies that no BROWSER_CLOSING or
  // BROWSER_CLOSED notification will be sent.
  // The source is a Source<Browser> containing the affected browser. No details
  // are expected.
  NOTIFICATION_BROWSER_CLOSE_CANCELLED,

  // Sent when the language (English, French...) for a page has been detected.
  // The details Details<std::string> contain the ISO 639-1 language code and
  // the source is Source<WebContents>.
  NOTIFICATION_TAB_LANGUAGE_DETERMINED,

  // Sent when a page has been translated. The source is the tab for that page
  // (Source<WebContents>) and the details are the language the page was
  // originally in and the language it was translated to
  // (std::pair<std::string, std::string>).
  NOTIFICATION_PAGE_TRANSLATED,

  // The user has changed the browser theme. The source is a
  // Source<ThemeService>. There are no details.
  NOTIFICATION_BROWSER_THEME_CHANGED,

  // Application-wide ----------------------------------------------------------

  // This message is sent when the application is terminating (the last
  // browser window has shutdown as part of an explicit user-initiated exit,
  // or the user closed the last browser window on Windows/Linux and there are
  // no BackgroundContents keeping the browser running). No source or details
  // are passed.
  NOTIFICATION_APP_TERMINATING,

#if defined(OS_MACOSX)
  // This notification is sent when the app has no key window, such as when
  // all windows are closed but the app is still active. No source or details
  // are provided.
  NOTIFICATION_NO_KEY_WINDOW,
#endif

  // This is sent when the user has chosen to exit the app, but before any
  // browsers have closed. This is sent if the user chooses to exit (via exit
  // menu item or keyboard shortcut) or to restart the process (such as in flags
  // page), not if Chrome exits by some other means (such as the user closing
  // the last window). No source or details are passed.
  //
  // Note that receiving this notification does not necessarily mean the process
  // will exit because the shutdown process can be cancelled by an unload
  // handler.  Use APP_TERMINATING for such needs.
  NOTIFICATION_CLOSE_ALL_BROWSERS_REQUEST,

  // Tabs --------------------------------------------------------------------

  // Sent when a tab is added to a WebContentsDelegate. The source is the
  // WebContentsDelegate and the details is the added WebContents.
  NOTIFICATION_TAB_ADDED,

  // This notification is sent after a tab has been appended to the tab_strip.
  // The source is a Source<WebContents> of the tab being added. There
  // are no details.
  NOTIFICATION_TAB_PARENTED,

  // This message is sent before a tab has been closed.  The source is a
  // Source<NavigationController> with a pointer to the controller for the
  // closed tab.  No details are expected.
  //
  // See also content::NOTIFICATION_WEB_CONTENTS_DESTROYED, which is sent when
  // the WebContents containing the NavigationController is destroyed.
  NOTIFICATION_TAB_CLOSING,

  // Authentication ----------------------------------------------------------

  // This is sent when a login prompt is shown.  The source is the
  // Source<NavigationController> for the tab in which the prompt is shown.
  // Details are a LoginNotificationDetails which provide the LoginHandler
  // that should be given authentication.
  NOTIFICATION_AUTH_NEEDED,

  // This is sent when authentication credentials have been supplied (either
  // by the user or by an automation service), but before we've actually
  // received another response from the server.  The source is the
  // Source<NavigationController> for the tab in which the prompt was shown.
  // Details are an AuthSuppliedLoginNotificationDetails which provide the
  // LoginHandler that should be given authentication as well as the supplied
  // username and password.
  NOTIFICATION_AUTH_SUPPLIED,

  // This is sent when an authentication request has been dismissed without
  // supplying credentials (either by the user or by an automation service).
  // The source is the Source<NavigationController> for the tab in which the
  // prompt was shown. Details are a LoginNotificationDetails which provide
  // the LoginHandler that should be cancelled.
  NOTIFICATION_AUTH_CANCELLED,

  // Profiles -----------------------------------------------------------------

  // Sent after a Profile has been created. This notification is sent both for
  // normal and OTR profiles.
  // The details are none and the source is the new profile.
  NOTIFICATION_PROFILE_CREATED,

  // Sent after a Profile has been added to ProfileManager.
  // The details are none and the source is the new profile.
  NOTIFICATION_PROFILE_ADDED,

  // Use KeyedServiceShutdownNotifier instead this notification type (you did
  // read the comment at the top of the file, didn't you?).
  // Sent early in the process of destroying a Profile, at the time a user
  // initiates the deletion of a profile versus the much later time when the
  // profile object is actually destroyed (use NOTIFICATION_PROFILE_DESTROYED).
  // The details are none and the source is a Profile*.
  NOTIFICATION_PROFILE_DESTRUCTION_STARTED,

  // Use KeyedServiceShutdownNotifier instead this notification type (you did
  // read the comment at the top of the file, didn't you?).
  // Sent before a Profile is destroyed. This notification is sent both for
  // normal and OTR profiles.
  // The details are none and the source is a Profile*.
  NOTIFICATION_PROFILE_DESTROYED,

  // Sent after the URLRequestContextGetter for a Profile has been initialized.
  // The details are none and the source is a Profile*.
  NOTIFICATION_PROFILE_URL_REQUEST_CONTEXT_GETTER_INITIALIZED,

  // Printing ----------------------------------------------------------------

  // Notification from PrintJob that an event occurred. It can be that a page
  // finished printing or that the print job failed. Details is
  // PrintJob::EventDetails. Source is a PrintJob.
  NOTIFICATION_PRINT_JOB_EVENT,

  // Sent when a PrintJob has been released.
  // Source is the WebContents that holds the print job.
  NOTIFICATION_PRINT_JOB_RELEASED,

  // Misc --------------------------------------------------------------------

#if defined(OS_CHROMEOS)
  // Sent when a chromium os user logs in.
  // The details are a chromeos::User object.
  NOTIFICATION_LOGIN_USER_CHANGED,

  // Sent immediately after the logged-in user's profile is ready.
  // The details are a Profile object.
  NOTIFICATION_LOGIN_USER_PROFILE_PREPARED,

  // Sent when the chromium session of a particular user is started.
  // If this is a new user on the machine this will not be sent until a profile
  // picture has been selected, unlike NOTIFICATION_LOGIN_USER_CHANGED which is
  // sent immediately after the user has logged in. This will be sent again if
  // the browser crashes and restarts.
  // The details are a chromeos::User object.
  NOTIFICATION_SESSION_STARTED,

  // Sent when a network error message is displayed on the WebUI login screen.
  // First paint event of this fires NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE.
  NOTIFICATION_LOGIN_NETWORK_ERROR_SHOWN,

  // Sent when the specific part of login/lock WebUI is considered to be
  // visible. That moment is tracked as the first paint event after one of the:
  // NOTIFICATION_LOGIN_NETWORK_ERROR_SHOWN
  //
  // Possible series of notifications:
  // 1. Boot into fresh OOBE
  //    NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE
  // 2. Boot into user pods list (normal boot). Same for lock screen.
  //    NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE
  // 3. Boot into GAIA sign in UI (user pods display disabled or no users):
  //    if no network is connected or flaky network
  //    (NOTIFICATION_LOGIN_NETWORK_ERROR_SHOWN +
  //     NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE)
  //    NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE
  // 4. Boot into retail mode
  //    NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE
  NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,

  // Send when kiosk auto-launch warning screen is visible.
  NOTIFICATION_KIOSK_AUTOLAUNCH_WARNING_VISIBLE,

  // Send when kiosk auto-launch warning screen had completed.
  NOTIFICATION_KIOSK_AUTOLAUNCH_WARNING_COMPLETED,

  // Send when enable consumer kiosk warning screen is visible.
  NOTIFICATION_KIOSK_ENABLE_WARNING_VISIBLE,

  // Send when consumer kiosk has been enabled.
  NOTIFICATION_KIOSK_ENABLED,

  // Send when enable consumer kiosk warning screen had completed.
  NOTIFICATION_KIOSK_ENABLE_WARNING_COMPLETED,

  // Sent when kiosk app list is loaded in UI.
  NOTIFICATION_KIOSK_APPS_LOADED,

  // Sent when the user list has changed.
  NOTIFICATION_USER_LIST_CHANGED,

  // Sent when the screen lock state has changed. The source is
  // ScreenLocker and the details is a bool specifing that the
  // screen is locked. When details is a false, the source object
  // is being deleted, so the receiver shouldn't use the screen locker
  // object.
  NOTIFICATION_SCREEN_LOCK_STATE_CHANGED,
#endif

#if defined(TOOLKIT_VIEWS)
  // Sent when a bookmark's context menu is shown. Used to notify
  // tests that the context menu has been created and shown.
  NOTIFICATION_BOOKMARK_CONTEXT_MENU_SHOWN,

  // Notification that the nested loop using during tab dragging has returned.
  // Used for testing.
  NOTIFICATION_TAB_DRAG_LOOP_DONE,
#endif

  // Sent when the CaptivePortalService checks if we're behind a captive portal.
  // The Source is the Profile the CaptivePortalService belongs to, and the
  // Details are a Details<CaptivePortalService::CheckResults>.
  NOTIFICATION_CAPTIVE_PORTAL_CHECK_RESULT,

  // Sent when the applications in the NTP app launcher have been reordered.
  // The details, if not NoDetails, is the std::string ID of the extension that
  // was moved.
  NOTIFICATION_APP_LAUNCHER_REORDERED,

  // Sent when an app is installed and an NTP has been shown. Source is the
  // WebContents that was shown, and Details is the string ID of the extension
  // which was installed.
  NOTIFICATION_APP_INSTALLED_TO_NTP,

  // Note:-
  // Currently only Content and Chrome define and use notifications.
  // Custom notifications not belonging to Content and Chrome should start
  // from here.
  NOTIFICATION_CHROME_END,
};

}  // namespace chrome

// **
// ** NOTICE
// **
// ** The notification system is deprecated, obsolete, and is slowly being
// ** removed. See https://crbug.com/268984.
// **
// ** Please don't add any new notification types, and please help migrate
// ** existing uses of the notification types below to use the Observer and
// ** Callback patterns.
// **

#endif  // CHROME_BROWSER_CHROME_NOTIFICATION_TYPES_H_
