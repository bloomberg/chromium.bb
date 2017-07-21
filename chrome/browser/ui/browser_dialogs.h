// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_DIALOGS_H_
#define CHROME_BROWSER_UI_BROWSER_DIALOGS_H_

#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "chrome/browser/ui/bookmarks/bookmark_editor.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/native_widget_types.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/arc/intent_helper/arc_navigation_throttle.h"
#endif  // OS_CHROMEOS

class Browser;
class LoginHandler;
class Profile;
class WebShareTarget;
struct WebApplicationInfo;

namespace content {
class BrowserContext;
class ColorChooser;
class WebContents;
}

namespace extensions {
class Extension;
}

namespace net {
class AuthChallengeInfo;
class URLRequest;
}

namespace payments {
class PaymentRequest;
class PaymentRequestDialog;
}

namespace safe_browsing {
class ChromeCleanerController;
class ChromeCleanerDialogController;
}

namespace task_manager {
class TaskManagerTableModel;
}

namespace ui {
class WebDialogDelegate;
}

namespace chrome {

// Shows or hides the Task Manager. |browser| can be NULL when called from Ash.
// Returns a pointer to the underlying TableModel, which can be ignored, or used
// for testing.
task_manager::TaskManagerTableModel* ShowTaskManager(Browser* browser);
void HideTaskManager();

#if !defined(OS_MACOSX)
// Creates and shows an HTML dialog with the given delegate and context.
// The window is automatically destroyed when it is closed.
// Returns the created window.
//
// Make sure to use the returned window only when you know it is safe
// to do so, i.e. before OnDialogClosed() is called on the delegate.
gfx::NativeWindow ShowWebDialog(gfx::NativeView parent,
                                content::BrowserContext* context,
                                ui::WebDialogDelegate* delegate);
#endif  // !defined(OS_MACOSX)

#if defined(USE_ASH)
// Creates and shows an HTML dialog with the given delegate and browser context.
// The dialog is placed in the ash window hierarchy in the given container. The
// window is automatically destroyed when it is closed.
// Returns the created window.
// See ash/public/cpp/shell_window_ids.h for |container_id| values. The window
// is destroyed when it is closed. See also chrome::ShowWebDialog().
gfx::NativeWindow ShowWebDialogInContainer(int container_id,
                                           content::BrowserContext* context,
                                           ui::WebDialogDelegate* delegate);
#endif  // defined(USE_ASH)

// Shows the create chrome app shortcut dialog box.
// |close_callback| may be null.
void ShowCreateChromeAppShortcutsDialog(
    gfx::NativeWindow parent_window,
    Profile* profile,
    const extensions::Extension* app,
    const base::Callback<void(bool /* created */)>& close_callback);

// Callback type used with the ShowBookmarkAppDialog() method. The boolean
// parameter is true when the user accepts the dialog. The WebApplicationInfo
// parameter contains the WebApplicationInfo as edited by the user.
using ShowBookmarkAppDialogCallback =
    base::OnceCallback<void(bool, const WebApplicationInfo&)>;

// Shows the Bookmark App bubble.
// See Extension::InitFromValueFlags::FROM_BOOKMARK for a description of
// bookmark apps.
//
// |web_app_info| is the WebApplicationInfo being converted into an app.
void ShowBookmarkAppDialog(gfx::NativeWindow parent_window,
                           const WebApplicationInfo& web_app_info,
                           ShowBookmarkAppDialogCallback callback);

// Shows a color chooser that reports to the given WebContents.
content::ColorChooser* ShowColorChooser(content::WebContents* web_contents,
                                        SkColor initial_color);

#if defined(OS_MACOSX)

// Bridging methods that show/hide the toolkit-views based Task Manager on Mac.
task_manager::TaskManagerTableModel* ShowTaskManagerViews(Browser* browser);
void HideTaskManagerViews();

// Show the Views "Chrome Update" dialog.
void ShowUpdateChromeDialogViews(gfx::NativeWindow parent);

#endif  // OS_MACOSX

#if defined(TOOLKIT_VIEWS)

// Creates a toolkit-views based LoginHandler (e.g. HTTP-Auth dialog).
LoginHandler* CreateLoginHandlerViews(net::AuthChallengeInfo* auth_info,
                                      net::URLRequest* request);

// Shows the toolkit-views based BookmarkEditor.
void ShowBookmarkEditorViews(gfx::NativeWindow parent_window,
                             Profile* profile,
                             const BookmarkEditor::EditDetails& details,
                             BookmarkEditor::Configuration configuration);

payments::PaymentRequestDialog* CreatePaymentRequestDialog(
    payments::PaymentRequest* request);

// Used to return the target the user picked or nullptr if the user cancelled
// the share.
using WebShareTargetPickerCallback =
    base::OnceCallback<void(const WebShareTarget*)>;

// Shows the dialog to choose a share target app. |targets| is a list of app
// title and manifest URL pairs that will be shown in a list. If the user picks
// a target, this calls |callback| with the manifest URL of the chosen target,
// or supplies null if the user cancelled the share.
void ShowWebShareTargetPickerDialog(gfx::NativeWindow parent_window,
                                    std::vector<WebShareTarget> targets,
                                    WebShareTargetPickerCallback callback);

#endif  // TOOLKIT_VIEWS

// Values used in the Dialog.Creation UMA metric. Each value represents a
// different type of dialog box.
// These values are written to logs. New enum values can be added, but existing
// enums must never be renumbered or deleted and reused.
enum class DialogIdentifier {
  UNKNOWN = 0,
  TRANSLATE = 1,
  BOOKMARK = 2,
  BOOKMARK_EDITOR = 3,
  DESKTOP_MEDIA_PICKER = 4,
  OUTDATED_UPGRADE = 5,
  ONE_CLICK_SIGNIN = 6,
  PROFILE_SIGNIN_CONFIRMATION = 7,
  HUNG_RENDERER = 8,
  SESSION_CRASHED = 9,
  CONFIRM_BUBBLE = 10,
  UPDATE_RECOMMENDED = 11,
  CRYPTO_PASSWORD = 12,
  SAFE_BROWSING_DOWNLOAD_FEEDBACK = 13,
  FIRST_RUN = 14,
  NETWORK_SHARE_PROFILE_WARNING = 15,
  CONFLICTING_MODULE = 16,
  CRITICAL_NOTIFICATION = 17,
  IME_WARNING = 18,
  TOOLBAR_ACTIONS_BAR = 19,
  GLOBAL_ERROR = 20,
  EXTENSION_INSTALL = 21,
  EXTENSION_UNINSTALL = 22,
  EXTENSION_INSTALLED = 23,
  PAYMENT_REQUEST = 24,
  SAVE_CARD = 25,
  CARD_UNMASK = 26,
  SIGN_IN = 27,
  SIGN_IN_SYNC_CONFIRMATION = 28,
  SIGN_IN_ERROR = 29,
  SIGN_IN_EMAIL_CONFIRMATION = 30,
  PROFILE_CHOOSER = 31,
  ACCOUNT_CHOOSER = 32,
  ARC_APP = 33,
  AUTO_SIGNIN_FIRST_RUN = 34,
  BOOKMARK_APP_CONFIRMATION = 35,
  CHOOSER_UI = 36,
  CHOOSER = 37,
  COLLECTED_COOKIES = 38,
  CONSTRAINED_WEB = 39,
  CONTENT_SETTING_CONTENTS = 40,
  CREATE_CHROME_APPLICATION_SHORTCUT = 41,
  DOWNLOAD_DANGER_PROMPT = 42,
  DOWNLOAD_IN_PROGRESS = 43,
  ECHO = 44,
  ENROLLMENT = 45,
  EXTENSION = 46,
  EXTENSION_POPUP_AURA = 47,
  EXTERNAL_PROTOCOL = 48,
  EXTERNAL_PROTOCOL_CHROMEOS = 49,
  FIRST_RUN_DIALOG = 50,
  HOME_PAGE_UNDO = 51,
  IDLE_ACTION_WARNING = 52,
  IMPORT_LOCK = 53,
  INTENT_PICKER = 54,
  INVERT = 55,
  JAVA_SCRIPT = 56,
  JAVA_SCRIPT_APP_MODAL_X11 = 57,
  LOGIN_HANDLER = 58,
  MANAGE_PASSWORDS = 59,
  MEDIA_GALLERIES = 60,
  MULTIPROFILES_INTRO = 61,
  MULTIPROFILES_SESSION_ABORTED = 62,
  NATIVE_CONTAINER = 63,
  NETWORK_CONFIG = 64,
  PERMISSIONS = 65,
  PLATFORM_KEYS_CERTIFICATE_SELECTOR = 66,
  PLATFORM_VERIFICATION = 67,
  PROXIMITY_AUTH_ERROR = 68,
  REQUEST_PIN = 69,
  SSL_CLIENT_CERTIFICATE_SELECTOR = 70,
  SIMPLE_MESSAGE_BOX = 71,
  TAB_MODAL_CONFIRM = 72,
  TASK_MANAGER = 73,
  TELEPORT_WARNING = 74,
  USER_MANAGER = 75,
  USER_MANAGER_PROFILE = 76,
  VALIDATION_MESSAGE = 77,
  WEB_SHARE_TARGET_PICKER = 78,
  ZOOM = 79,
  MAX_VALUE
};

// Record an UMA metric counting the creation of a dialog box of this type.
void RecordDialogCreation(DialogIdentifier identifier);

#if defined(OS_WIN)

// Shows the Chrome Cleanup dialog asking the user if they want to clean their
// system from unwanted software. This is called when unwanted software has been
// detected on the system.
void ShowChromeCleanerPrompt(
    Browser* browser,
    safe_browsing::ChromeCleanerDialogController* dialog_controller,
    safe_browsing::ChromeCleanerController* cleaner_controller);

#endif  // OS_WIN

}  // namespace chrome

#if defined(OS_CHROMEOS)

// This callback informs the package name of the app selected by the user, along
// with the reason why the Bubble was closed. The string param must have a valid
// package name, except when the CloseReason is ERROR or DIALOG_DEACTIVATED, for
// these cases we return a dummy value which won't be used at all and has no
// significance.
using IntentPickerResponse =
    base::Callback<void(const std::string&,
                        arc::ArcNavigationThrottle::CloseReason)>;

// Return a pointer to the IntentPickerBubbleView::ShowBubble method.
using BubbleShowPtr =
    void (*)(content::WebContents*,
             const std::vector<arc::ArcNavigationThrottle::AppInfo>&,
             const IntentPickerResponse&);

BubbleShowPtr ShowIntentPickerBubble();

#endif  // OS_CHROMEOS

#endif  // CHROME_BROWSER_UI_BROWSER_DIALOGS_H_
