// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_DIALOGS_H_
#define CHROME_BROWSER_UI_BROWSER_DIALOGS_H_

#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "build/build_config.h"
#include "chrome/browser/ui/bookmarks/bookmark_editor.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/native_widget_types.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/arc/intent_helper/arc_navigation_throttle.h"
#endif  // OS_CHROMEOS

class Browser;
class ContentSettingBubbleModel;
class GURL;
class LoginHandler;
class Profile;

namespace base {
struct Feature;
}

namespace bookmarks {
class BookmarkBubbleObserver;
}

namespace content {
class BrowserContext;
class ColorChooser;
class WebContents;
}

namespace extensions {
class Extension;
}

namespace gfx {
class Point;
}

namespace net {
class AuthChallengeInfo;
class URLRequest;
}

namespace security_state {
struct SecurityInfo;
}  // namespace security_state

namespace task_manager {
class TaskManagerTableModel;
}

namespace ui {
class WebDialogDelegate;
}

namespace chrome {

#if defined(OS_MACOSX)
// Makes ToolkitViewsDialogsEnabled() available to chrome://flags.
extern const base::Feature kMacViewsNativeDialogs;

// Makes ToolkitViewsWebUIDialogsEnabled() available to chrome://flags.
extern const base::Feature kMacViewsWebUIDialogs;
#endif  // OS_MACOSX

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
// The dialog is placed in the ash window hierarchy in the given container.
// See ash/public/cpp/shell_window_ids.h for |container_id| values. The window
// is destroyed when it is closed. See also chrome::ShowWebDialog().
void ShowWebDialogInContainer(int container_id,
                              content::BrowserContext* context,
                              ui::WebDialogDelegate* delegate);
#endif  // defined(USE_ASH)

#if !defined(OS_MACOSX)
// Shows the create web app shortcut dialog box.
void ShowCreateWebAppShortcutsDialog(gfx::NativeWindow parent_window,
                                     content::WebContents* web_contents);
#endif  // !defined(OS_MACOSX)

// Shows the create chrome app shortcut dialog box.
// |close_callback| may be null.
void ShowCreateChromeAppShortcutsDialog(
    gfx::NativeWindow parent_window,
    Profile* profile,
    const extensions::Extension* app,
    const base::Callback<void(bool /* created */)>& close_callback);

// Shows a color chooser that reports to the given WebContents.
content::ColorChooser* ShowColorChooser(content::WebContents* web_contents,
                                        SkColor initial_color);

#if defined(OS_MACOSX)

// For Mac, returns true if Chrome should show an equivalent toolkit-views based
// dialog instead of a native-looking Cocoa dialog.
bool ToolkitViewsDialogsEnabled();

// For Mac, returns true if Chrome should show an equivalent toolkit-views based
// dialog instead of a WebUI-styled Cocoa dialog.
bool ToolkitViewsWebUIDialogsEnabled();

// Shows a Views website settings bubble at the given anchor point.
void ShowWebsiteSettingsBubbleViewsAtPoint(
    const gfx::Point& anchor_point,
    Profile* profile,
    content::WebContents* web_contents,
    const GURL& virtual_url,
    const security_state::SecurityInfo& security_info);

// Show a Views bookmark bubble at the given point. This occurs when the
// bookmark star is clicked or "Bookmark This Page..." is selected from a menu
// or via a key equivalent.
void ShowBookmarkBubbleViewsAtPoint(const gfx::Point& anchor_point,
                                    gfx::NativeView parent,
                                    bookmarks::BookmarkBubbleObserver* observer,
                                    Browser* browser,
                                    const GURL& url,
                                    bool newly_bookmarked);

// Bridging methods that show/hide the toolkit-views based Task Manager on Mac.
task_manager::TaskManagerTableModel* ShowTaskManagerViews(Browser* browser);
void HideTaskManagerViews();

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

#if defined(OS_MACOSX)

// This is a class so that it can be friended from ContentSettingBubbleContents,
// which allows it to call SetAnchorRect().
class ContentSettingBubbleViewsBridge {
 public:
  static void Show(gfx::NativeView parent_view,
                   ContentSettingBubbleModel* model,
                   content::WebContents* web_contents,
                   const gfx::Point& anchor);
 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ContentSettingBubbleViewsBridge);
};

#endif  // OS_MACOSX

#endif  // TOOLKIT_VIEWS

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
