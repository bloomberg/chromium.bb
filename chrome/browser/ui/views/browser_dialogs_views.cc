// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_dialogs.h"

#include <memory>

#include "build/build_config.h"
#include "chrome/browser/chooser_controller/chooser_controller.h"
#include "chrome/browser/extensions/api/chrome_device_permissions_prompt.h"
#include "chrome/browser/extensions/chrome_extension_chooser_dialog.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/ui/login/login_handler.h"
#include "chrome/browser/ui/views/task_manager_view.h"
#include "chrome/browser/ui/views_mode_controller.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/ui/views/intent_picker_bubble_view.h"
#endif  // OS_CHROMEOS

// This file provides definitions of desktop browser dialog-creation methods for
// all toolkit-views platforms other than Mac. It also provides the definitions
// on Mac when mac_views_browser=1 is specified in GYP_DEFINES. The file is
// excluded in a Mac Cocoa build: definitions under chrome/browser/ui/cocoa may
// select at runtime whether to show a Cocoa dialog, or the toolkit-views dialog
// provided by browser_dialogs.h.
// static
scoped_refptr<LoginHandler> LoginHandler::Create(
    net::AuthChallengeInfo* auth_info,
    content::ResourceRequestInfo::WebContentsGetter web_contents_getter,
    const base::Callback<void(const base::Optional<net::AuthCredentials>&)>&
        auth_required_callback) {
  return chrome::CreateLoginHandlerViews(auth_info, web_contents_getter,
                                         auth_required_callback);
}

// static
void BookmarkEditor::Show(gfx::NativeWindow parent_window,
                          Profile* profile,
                          const EditDetails& details,
                          Configuration configuration) {
  chrome::ShowBookmarkEditorViews(parent_window, profile, details,
                                  configuration);
}

// static
ExtensionInstallPrompt::ShowDialogCallback
ExtensionInstallPrompt::GetDefaultShowDialogCallback() {
#if defined(OS_MACOSX)
  if (views_mode_controller::IsViewsBrowserCocoa())
    return GetDefaultShowDialogCallbackCocoa();
#endif
  return ExtensionInstallPrompt::GetViewsShowDialogCallback();
}

void ChromeDevicePermissionsPrompt::ShowDialog() {
  ShowDialogViews();
}

void ChromeExtensionChooserDialog::ShowDialog(
    std::unique_ptr<ChooserController> chooser_controller) const {
  ShowDialogImpl(std::move(chooser_controller));
}

namespace chrome {

#if !defined(OS_MACOSX)
task_manager::TaskManagerTableModel* ShowTaskManager(Browser* browser) {
  return task_manager::TaskManagerView::Show(browser);
}

void HideTaskManager() {
  task_manager::TaskManagerView::Hide();
}
#endif

}  // namespace chrome

#if defined(OS_CHROMEOS)

BubbleShowPtr ShowIntentPickerBubble() {
  return IntentPickerBubbleView::ShowBubble;
}

#endif  // OS_CHROMEOS
