// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/i18n/rtl.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/bundle_installer.h"
#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "chrome/browser/tab_contents/confirm_infobar_delegate.h"
#include "chrome/browser/tab_contents/simple_alert_infobar_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/extensions/extension_installed_bubble_controller.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_action.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"

using extensions::BundleInstaller;

// When an extension is installed on Mac with neither browser action nor
// page action icons, show an infobar instead of a popup bubble.
static void ShowGenericExtensionInstalledInfoBar(
    const Extension* new_extension,
    const SkBitmap& icon,
    Profile* profile) {
  Browser* browser = browser::FindLastActiveWithProfile(profile);
  if (!browser)
    return;

  TabContentsWrapper* wrapper = browser->GetSelectedTabContentsWrapper();
  if (!wrapper)
    return;

  string16 extension_name = UTF8ToUTF16(new_extension->name());
  base::i18n::AdjustStringForLocaleDirection(&extension_name);
  string16 msg = l10n_util::GetStringFUTF16(IDS_EXTENSION_INSTALLED_HEADING,
                                            extension_name)
                 + UTF8ToUTF16(" ")
                 + l10n_util::GetStringUTF16(
                       IDS_EXTENSION_INSTALLED_MANAGE_INFO_MAC);
  InfoBarTabHelper* infobar_helper = wrapper->infobar_tab_helper();
  InfoBarDelegate* delegate = new SimpleAlertInfoBarDelegate(
      infobar_helper, new gfx::Image(icon), msg, true);
  infobar_helper->AddInfoBar(delegate);
}

namespace browser {

void ShowExtensionInstalledBubble(
    const Extension* extension,
    Browser* browser,
    const SkBitmap& icon,
    Profile* profile) {
  if ((extension->browser_action()) || !extension->omnibox_keyword().empty() ||
      (extension->page_action() &&
      !extension->page_action()->default_icon_path().empty())) {
    // The controller is deallocated when the window is closed, so no need to
    // worry about it here.
    [[ExtensionInstalledBubbleController alloc]
        initWithParentWindow:browser->window()->GetNativeHandle()
                   extension:extension
                      bundle:NULL
                     browser:browser
                        icon:icon];
  } else {
    // If the extension is of type GENERIC, meaning it doesn't have a UI
    // surface to display for this window, launch infobar instead of popup
    // bubble, because we have no guaranteed wrench menu button to point to.
    ShowGenericExtensionInstalledInfoBar(extension, icon, profile);
  }
}

} // namespace browser

void extensions::BundleInstaller::ShowInstalledBubble(
    const BundleInstaller* bundle, Browser* browser) {
  // The controller is deallocated when the window is closed, so no need to
  // worry about it here.
  [[ExtensionInstalledBubbleController alloc]
        initWithParentWindow:browser->window()->GetNativeHandle()
                   extension:NULL
                      bundle:bundle
                     browser:browser
                        icon:SkBitmap()];
}
