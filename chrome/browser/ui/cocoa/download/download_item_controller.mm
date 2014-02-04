// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/download/download_item_controller.h"

#include "base/mac/bundle_locations.h"
#include "base/mac/mac_util.h"
#include "base/metrics/histogram.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_shelf_context_menu.h"
#import "chrome/browser/themes/theme_properties.h"
#import "chrome/browser/themes/theme_service.h"
#import "chrome/browser/ui/cocoa/download/download_item_button.h"
#import "chrome/browser/ui/cocoa/download/download_item_cell.h"
#include "chrome/browser/ui/cocoa/download/download_item_mac.h"
#import "chrome/browser/ui/cocoa/download/download_shelf_context_menu_controller.h"
#import "chrome/browser/ui/cocoa/download/download_shelf_controller.h"
#import "chrome/browser/ui/cocoa/themed_window.h"
#import "chrome/browser/ui/cocoa/ui_localizer.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/page_navigator.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "third_party/google_toolbox_for_mac/src/AppKit/GTMUILocalizerAndLayoutTweaker.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/text_elider.h"
#include "ui/gfx/font.h"
#include "ui/gfx/image/image.h"

using content::DownloadItem;

namespace {

// NOTE: Mac currently doesn't use this like Windows does.  Mac uses this to
// control the min size on the dangerous download text.  TVL sent a query off to
// UX to fully spec all the the behaviors of download items and truncations
// rules so all platforms can get inline in the future.
const int kTextWidth = 140;            // Pixels

// The maximum width in pixels for the file name tooltip.
const int kToolTipMaxWidth = 900;


// Helper to widen a view.
void WidenView(NSView* view, CGFloat widthChange) {
  // If it is an NSBox, the autoresize of the contentView is the issue.
  NSView* contentView = view;
  if ([view isKindOfClass:[NSBox class]]) {
    contentView = [(NSBox*)view contentView];
  }
  BOOL autoresizesSubviews = [contentView autoresizesSubviews];
  if (autoresizesSubviews) {
    [contentView setAutoresizesSubviews:NO];
  }

  NSRect frame = [view frame];
  frame.size.width += widthChange;
  [view setFrame:frame];

  if (autoresizesSubviews) {
    [contentView setAutoresizesSubviews:YES];
  }
}

}  // namespace

class DownloadShelfContextMenuMac : public DownloadShelfContextMenu {
 public:
  DownloadShelfContextMenuMac(DownloadItem* downloadItem,
                              content::PageNavigator* navigator)
      : DownloadShelfContextMenu(downloadItem, navigator) { }

  // DownloadShelfContextMenu::GetMenuModel is protected.
  using DownloadShelfContextMenu::GetMenuModel;
};

@interface DownloadItemController (Private)
- (void)themeDidChangeNotification:(NSNotification*)aNotification;
- (void)updateTheme:(ui::ThemeProvider*)themeProvider;
- (void)setState:(DownoadItemState)state;
@end

// Implementation of DownloadItemController

@implementation DownloadItemController

- (id)initWithDownload:(DownloadItem*)downloadItem
                 shelf:(DownloadShelfController*)shelf
             navigator:(content::PageNavigator*)navigator {
  if ((self = [super initWithNibName:@"DownloadItem"
                              bundle:base::mac::FrameworkBundle()])) {
    // Must be called before [self view], so that bridge_ is set in awakeFromNib
    bridge_.reset(new DownloadItemMac(downloadItem, self));
    menuBridge_.reset(new DownloadShelfContextMenuMac(downloadItem, navigator));

    NSNotificationCenter* defaultCenter = [NSNotificationCenter defaultCenter];
    [defaultCenter addObserver:self
                      selector:@selector(themeDidChangeNotification:)
                          name:kBrowserThemeDidChangeNotification
                        object:nil];

    shelf_ = shelf;
    state_ = kNormal;
    creationTime_ = base::Time::Now();
    font_list_.reset(new gfx::FontList(
        ui::ResourceBundle::GetSharedInstance().GetFontList(
            ui::ResourceBundle::BaseFont)));
  }
  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [progressView_ setController:nil];
  [[self view] removeFromSuperview];
  [super dealloc];
}

- (void)awakeFromNib {
  [progressView_ setController:self];

  GTMUILocalizerAndLayoutTweaker* localizerAndLayoutTweaker =
      [[[GTMUILocalizerAndLayoutTweaker alloc] init] autorelease];
  [localizerAndLayoutTweaker applyLocalizer:localizer_ tweakingUI:[self view]];

  [self setStateFromDownload:bridge_->download_model()];

  bridge_->LoadIcon();
  [self updateToolTip];
}

- (void)showDangerousWarning:(DownloadItemModel*)downloadModel {
  // The transition from safe -> dangerous should only happen once. The code
  // assumes that the danger type of the download doesn't change once it's set.
  if ([self isDangerousMode])
    return;

  [self setState:kDangerous];

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  NSImage* alertIcon;

  NSString* dangerousWarning = base::SysUTF16ToNSString(
      downloadModel->GetWarningText(*font_list_, kTextWidth));
  DCHECK(dangerousWarning);
  [dangerousDownloadLabel_ setStringValue:dangerousWarning];
  CGFloat labelWidthChange =
      [GTMUILocalizerAndLayoutTweaker
        sizeToFitFixedHeightTextField:dangerousDownloadLabel_
                             minWidth:kTextWidth];
  CGFloat buttonWidthChange = 0.0;

  if (downloadModel->MightBeMalicious()) {
    alertIcon = rb.GetNativeImageNamed(IDR_SAFEBROWSING_WARNING).ToNSImage();
    buttonWidthChange = [maliciousButtonTweaker_ changedWidth];

    // Move the buttons to account for the change in label size.
    NSPoint frameOrigin = [maliciousButtonTweaker_ frame].origin;
    frameOrigin.x += labelWidthChange;
    [maliciousButtonTweaker_ setFrameOrigin:frameOrigin];

    [dangerousButtonTweaker_ setHidden:YES];
    [maliciousButtonTweaker_ setHidden:NO];
  } else {
    alertIcon = rb.GetNativeImageNamed(IDR_WARNING).ToNSImage();
    buttonWidthChange = [dangerousButtonTweaker_ changedWidth];

    // The text on the confirm button can change depending on the type of the
    // download.
    NSString* confirmButtonTitle =
        base::SysUTF16ToNSString(downloadModel->GetWarningConfirmButtonText());
    DCHECK(confirmButtonTitle);
    [dangerousDownloadConfirmButton_ setTitle:confirmButtonTitle];

    // Since the text of the confirm button changed, dangerousButtonTweaker
    // should be resized.
    NSSize sizeChange =
        [GTMUILocalizerAndLayoutTweaker sizeToFitView:dangerousButtonTweaker_];
    buttonWidthChange += sizeChange.width;

    // Move the button to account for the change in label size.
    NSPoint frameOrigin = [dangerousButtonTweaker_ frame].origin;
    frameOrigin.x += labelWidthChange;
    [dangerousButtonTweaker_ setFrameOrigin:frameOrigin];

    [dangerousButtonTweaker_ setHidden:NO];
    [maliciousButtonTweaker_ setHidden:YES];
  }
  DCHECK(alertIcon);
  [image_ setImage:alertIcon];

  // Grow the parent views
  WidenView([self view], labelWidthChange + buttonWidthChange);
  WidenView(dangerousDownloadView_, labelWidthChange + buttonWidthChange);
}

- (void)setStateFromDownload:(DownloadItemModel*)downloadModel {
  DCHECK_EQ([self download], downloadModel->download());

  // Handle dangerous downloads.
  if (downloadModel->IsDangerous()) {
    [self showDangerousWarning:downloadModel];
    return;
  }

  // Set path to draggable download on completion.
  if (downloadModel->download()->GetState() == DownloadItem::COMPLETE)
    [progressView_ setDownload:downloadModel->download()->GetTargetFilePath()];

  [cell_ setStateFromDownload:downloadModel];
}

- (void)setIcon:(NSImage*)icon {
  [cell_ setImage:icon];
}

- (void)remove {
  // We are deleted after this!
  [shelf_ remove:self];
}

- (void)updateVisibility:(id)sender {
  if ([[self view] window])
    [self updateTheme:[[[self view] window] themeProvider]];

  NSView* view = [self view];
  NSRect containerFrame = [[view superview] frame];
  [view setHidden:(NSMaxX([view frame]) > NSWidth(containerFrame))];
}

- (void)downloadWasOpened {
  [shelf_ downloadWasOpened:self];
}

- (IBAction)handleButtonClick:(id)sender {
  NSEvent* event = [NSApp currentEvent];
  DownloadItem* download = [self download];
  if ([event modifierFlags] & NSCommandKeyMask) {
    // Let cmd-click show the file in Finder, like e.g. in Safari and Spotlight.
    download->ShowDownloadInShell();
  } else {
    download->OpenDownload();
  }
}

- (NSSize)preferredSize {
  if (state_ == kNormal)
    return [progressView_ frame].size;
  DCHECK_EQ(kDangerous, state_);
  return [dangerousDownloadView_ frame].size;
}

- (DownloadItem*)download {
  return bridge_->download_model()->download();
}

- (ui::MenuModel*)contextMenuModel {
  return menuBridge_->GetMenuModel();
}

- (void)updateToolTip {
  base::string16 tooltip_text =
      bridge_->download_model()->GetTooltipText(*font_list_, kToolTipMaxWidth);
  [progressView_ setToolTip:base::SysUTF16ToNSString(tooltip_text)];
}

- (void)clearDangerousMode {
  [self setState:kNormal];
  // The state change hide the dangerouse download view and is now showing the
  // download progress view.  This means the view is likely to be a different
  // size, so trigger a shelf layout to fix up spacing.
  [shelf_ layoutItems];
}

- (BOOL)isDangerousMode {
  return state_ == kDangerous;
}

- (void)setState:(DownoadItemState)state {
  if (state_ == state)
    return;
  state_ = state;
  if (state_ == kNormal) {
    [progressView_ setHidden:NO];
    [dangerousDownloadView_ setHidden:YES];
  } else {
    DCHECK_EQ(kDangerous, state_);
    [progressView_ setHidden:YES];
    [dangerousDownloadView_ setHidden:NO];
  }
  // NOTE: Do not relayout the shelf, as this could get called during initial
  // setup of the the item, so the localized text and sizing might not have
  // happened yet.
}

// Called after a theme change took place, possibly for a different profile.
- (void)themeDidChangeNotification:(NSNotification*)notification {
  [self updateTheme:[[[self view] window] themeProvider]];
}

// Adapt appearance to the current theme. Called after theme changes and before
// this is shown for the first time.
- (void)updateTheme:(ui::ThemeProvider*)themeProvider {
  if (!themeProvider)
    return;

  NSColor* color = themeProvider->GetNSColor(ThemeProperties::COLOR_TAB_TEXT);
  [dangerousDownloadLabel_ setTextColor:color];
}

- (IBAction)saveDownload:(id)sender {
  // The user has confirmed a dangerous download.  We record how quickly the
  // user did this to detect whether we're being clickjacked.
  UMA_HISTOGRAM_LONG_TIMES("clickjacking.save_download",
                           base::Time::Now() - creationTime_);
  // This will change the state and notify us.
  bridge_->download_model()->download()->ValidateDangerousDownload();
}

- (IBAction)discardDownload:(id)sender {
  UMA_HISTOGRAM_LONG_TIMES("clickjacking.discard_download",
                           base::Time::Now() - creationTime_);
  DownloadItem* download = bridge_->download_model()->download();
  download->Remove();
  // WARNING: we are deleted at this point.  Don't access 'this'.
}

- (IBAction)dismissMaliciousDownload:(id)sender {
  [self remove];
  // WARNING: we are deleted at this point.
}

- (IBAction)showContextMenu:(id)sender {
  base::scoped_nsobject<DownloadShelfContextMenuController> menuController(
      [[DownloadShelfContextMenuController alloc]
        initWithItemController:self
                  withDelegate:nil]);
  [NSMenu popUpContextMenu:[menuController menu]
                 withEvent:[NSApp currentEvent]
                   forView:[self view]];
}

@end
