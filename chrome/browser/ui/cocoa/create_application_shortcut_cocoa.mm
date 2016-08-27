// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#import "base/mac/scoped_nsobject.h"
#import "chrome/browser/web_applications/web_app_mac.h"
#import "ui/base/l10n/l10n_util_mac.h"
#include "base/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/cocoa/key_equivalent_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/gfx/image/image.h"

@interface CrCreateAppShortcutCheckboxObserver : NSObject {
 @private
  NSButton* checkbox_;
  NSButton* continueButton_;
}

- (id)initWithCheckbox:(NSButton*)checkbox
        continueButton:(NSButton*)continueButton;
- (void)startObserving;
- (void)stopObserving;
@end

@implementation CrCreateAppShortcutCheckboxObserver

- (id)initWithCheckbox:(NSButton*)checkbox
        continueButton:(NSButton*)continueButton {
  if ((self = [super init])) {
    checkbox_ = checkbox;
    continueButton_ = continueButton;
  }
  return self;
}

- (void)startObserving {
  [checkbox_ addObserver:self forKeyPath:@"cell.state" options:0 context:nil];
}

- (void)stopObserving {
  [checkbox_ removeObserver:self forKeyPath:@"cell.state"];
}

- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary*)change
                       context:(void*)context {
  [continueButton_ setEnabled:([checkbox_ state] == NSOnState)];
}

@end

namespace {

// Called when the app's ShortcutInfo (with icon) is loaded when creating app
// shortcuts.
void CreateAppShortcutInfoLoaded(
    Profile* profile,
    const extensions::Extension* app,
    const base::Callback<void(bool)>& close_callback,
    std::unique_ptr<web_app::ShortcutInfo> shortcut_info) {
  base::scoped_nsobject<NSAlert> alert([[NSAlert alloc] init]);

  NSButton* continue_button = [alert
      addButtonWithTitle:l10n_util::GetNSString(IDS_CREATE_SHORTCUTS_COMMIT)];
  [continue_button setKeyEquivalent:kKeyEquivalentReturn];

  NSButton* cancel_button =
      [alert addButtonWithTitle:l10n_util::GetNSString(IDS_CANCEL)];
  [cancel_button setKeyEquivalent:kKeyEquivalentEscape];

  [alert setMessageText:l10n_util::GetNSString(IDS_CREATE_SHORTCUTS_LABEL)];
  [alert setAlertStyle:NSInformationalAlertStyle];

  base::scoped_nsobject<NSButton> application_folder_checkbox(
      [[NSButton alloc] initWithFrame:NSZeroRect]);
  [application_folder_checkbox setButtonType:NSSwitchButton];
  [application_folder_checkbox
      setTitle:l10n_util::GetNSString(IDS_CREATE_SHORTCUTS_APP_FOLDER_CHKBOX)];
  [application_folder_checkbox setState:NSOnState];
  [application_folder_checkbox sizeToFit];

  base::scoped_nsobject<CrCreateAppShortcutCheckboxObserver> checkbox_observer(
      [[CrCreateAppShortcutCheckboxObserver alloc]
          initWithCheckbox:application_folder_checkbox
            continueButton:continue_button]);
  [checkbox_observer startObserving];

  [alert setAccessoryView:application_folder_checkbox];

  const int kIconPreviewSizePixels = 128;
  const int kIconPreviewTargetSize = 64;
  const gfx::Image* icon = shortcut_info->favicon.GetBest(
      kIconPreviewSizePixels, kIconPreviewSizePixels);

  if (icon && !icon->IsEmpty()) {
    NSImage* icon_image = icon->ToNSImage();
    [icon_image
        setSize:NSMakeSize(kIconPreviewTargetSize, kIconPreviewTargetSize)];
    [alert setIcon:icon_image];
  }

  bool dialog_accepted = false;
  if ([alert runModal] == NSAlertFirstButtonReturn &&
      [application_folder_checkbox state] == NSOnState) {
    dialog_accepted = true;
    CreateShortcuts(web_app::SHORTCUT_CREATION_BY_USER,
                    web_app::ShortcutLocations(), profile, app);
  }

  [checkbox_observer stopObserving];

  if (!close_callback.is_null())
    close_callback.Run(dialog_accepted);
}

}  // namespace

namespace chrome {

void ShowCreateChromeAppShortcutsDialog(
    gfx::NativeWindow /*parent_window*/,
    Profile* profile,
    const extensions::Extension* app,
    const base::Callback<void(bool)>& close_callback) {
  web_app::GetShortcutInfoForApp(
      app, profile,
      base::Bind(&CreateAppShortcutInfoLoaded, profile, app, close_callback));
}

}  // namespace chrome
