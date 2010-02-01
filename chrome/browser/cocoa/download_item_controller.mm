// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/download_item_controller.h"

#include "app/gfx/text_elider.h"
#include "app/l10n_util_mac.h"
#include "app/resource_bundle.h"
#include "base/mac_util.h"
#include "base/sys_string_conversions.h"
#import "chrome/browser/cocoa/download_item_button.h"
#import "chrome/browser/cocoa/download_item_cell.h"
#include "chrome/browser/cocoa/download_item_mac.h"
#import "chrome/browser/cocoa/download_shelf_controller.h"
#import "chrome/browser/cocoa/ui_localizer.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_shelf.h"
#include "chrome/browser/download/download_util.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#import "third_party/GTM/AppKit/GTMTheme.h"
#include "third_party/GTM/AppKit/GTMUILocalizerAndLayoutTweaker.h"

static const int kTextWidth = 140;            // Pixels

namespace {

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

// A class for the chromium-side part of the download shelf context menu.

class DownloadShelfContextMenuMac : public DownloadShelfContextMenu {
 public:
  DownloadShelfContextMenuMac(BaseDownloadItemModel* model)
      : DownloadShelfContextMenu(model) { }

  using DownloadShelfContextMenu::ExecuteItemCommand;
  using DownloadShelfContextMenu::ItemIsChecked;
  using DownloadShelfContextMenu::IsItemCommandEnabled;

  using DownloadShelfContextMenu::SHOW_IN_FOLDER;
  using DownloadShelfContextMenu::OPEN_WHEN_COMPLETE;
  using DownloadShelfContextMenu::ALWAYS_OPEN_TYPE;
  using DownloadShelfContextMenu::CANCEL;
  using DownloadShelfContextMenu::TOGGLE_PAUSE;
};

@interface DownloadItemController (Private)
- (void)themeDidChangeNotification:(NSNotification*)aNotification;
- (void)updateTheme:(GTMTheme*)theme;
- (void)setState:(DownoadItemState)state;
@end

// Implementation of DownloadItemController

@implementation DownloadItemController

- (id)initWithModel:(BaseDownloadItemModel*)downloadModel
              shelf:(DownloadShelfController*)shelf {
  if ((self = [super initWithNibName:@"DownloadItem"
                              bundle:mac_util::MainAppBundle()])) {
    // Must be called before [self view], so that bridge_ is set in awakeFromNib
    bridge_.reset(new DownloadItemMac(downloadModel, self));
    menuBridge_.reset(new DownloadShelfContextMenuMac(downloadModel));

    NSNotificationCenter* defaultCenter = [NSNotificationCenter defaultCenter];
    [defaultCenter addObserver:self
                      selector:@selector(themeDidChangeNotification:)
                          name:kGTMThemeDidChangeNotification
                        object:nil];

    shelf_ = shelf;
    state_ = kNormal;
    creationTime_ = base::Time::Now();
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

  [self setStateFromDownload:bridge_->download_model()];

  GTMUILocalizerAndLayoutTweaker* localizerAndLayoutTweaker =
      [[[GTMUILocalizerAndLayoutTweaker alloc] init] autorelease];
  [localizerAndLayoutTweaker applyLocalizer:localizer_ tweakingUI:[self view]];

  // Since the shelf keeps laying out views as more items are added, relying on
  // the WidthBaseTweaker to resize the dangerous download part does not work.
  DCHECK(buttonTweaker_ != nil);
  CGFloat widthChange = [buttonTweaker_ changedWidth];
  // Grow the parent views
  WidenView([self view], widthChange);
  WidenView(dangerousDownloadView_, widthChange);
  // Slide the two buttons over.
  NSPoint frameOrigin = [buttonTweaker_ frame].origin;
  frameOrigin.x += widthChange;
  [buttonTweaker_ setFrameOrigin:frameOrigin];

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  NSImage* alertIcon = rb.GetNSImageNamed(IDR_WARNING);
  DCHECK(alertIcon);
  [image_ setImage:alertIcon];

  bridge_->LoadIcon();
}

- (void)setStateFromDownload:(BaseDownloadItemModel*)downloadModel {
  DCHECK_EQ(bridge_->download_model(), downloadModel);

  // Handle dangerous downloads.
  if (downloadModel->download()->safety_state() == DownloadItem::DANGEROUS) {
    [self setState:kDangerous];

    NSString* dangerousWarning;
    NSString* confirmButtonTitle;
    // The dangerous download label and button text are different for an
    // extension file.
    if (downloadModel->download()->is_extension_install()) {
      dangerousWarning = l10n_util::GetNSStringWithFixup(
          IDS_PROMPT_DANGEROUS_DOWNLOAD_EXTENSION);
      confirmButtonTitle = l10n_util::GetNSStringWithFixup(
          IDS_CONTINUE_EXTENSION_DOWNLOAD);
    } else {
      NSFont* font = [dangerousDownloadLabel_ font];
      gfx::Font chromeFont = gfx::Font::CreateFont(
          base::SysNSStringToWide([font fontName]), [font pointSize]);
      string16 elidedFilename = WideToUTF16(ElideFilename(
          downloadModel->download()->original_name(), chromeFont, kTextWidth));
      dangerousWarning = l10n_util::GetNSStringFWithFixup(
          IDS_PROMPT_DANGEROUS_DOWNLOAD, elidedFilename);
      confirmButtonTitle = l10n_util::GetNSStringWithFixup(IDS_SAVE_DOWNLOAD);
    }
    [dangerousDownloadLabel_ setStringValue:dangerousWarning];
    [dangerousDownloadConfirmButton_ setTitle:confirmButtonTitle];
    return;
  }

  // Set correct popup menu. Also, set draggable download on completion.
  if (downloadModel->download()->state() == DownloadItem::COMPLETE) {
    [progressView_ setMenu:completeDownloadMenu_];
    [progressView_ setDownload:downloadModel->download()->full_path()];
  } else {
    [progressView_ setMenu:activeDownloadMenu_];
  }

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
    [self updateTheme:[[self view] gtm_theme]];

  // TODO(thakis): Make this prettier, by fading the items out or overlaying
  // the partial visible one with a horizontal alpha gradient -- crbug.com/17830
  NSView* view = [self view];
  NSRect containerFrame = [[view superview] frame];
  [view setHidden:(NSMaxX([view frame]) > NSWidth(containerFrame))];
}

- (IBAction)handleButtonClick:(id)sender {
  DownloadItem* download = bridge_->download_model()->download();
  if (download->state() == DownloadItem::IN_PROGRESS)
    download->set_open_when_complete(!download->open_when_complete());
  else if (download->state() == DownloadItem::COMPLETE)
    download_util::OpenDownload(download);
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

- (void)clearDangerousMode {
  [self setState:kNormal];
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
  [shelf_ layoutItems];
}

// Called after the current theme has changed.
- (void)themeDidChangeNotification:(NSNotification*)aNotification {
  GTMTheme* theme = [aNotification object];
  [self updateTheme:theme];
}

// Adapt appearance to the current theme. Called after theme changes and before
// this is shown for the first time.
- (void)updateTheme:(GTMTheme*)theme {
  NSColor* color = [theme textColorForStyle:GTMThemeStyleTabBarSelected
                                      state:GTMThemeStateActiveWindow];
  [dangerousDownloadLabel_ setTextColor:color];
}

- (IBAction)saveDownload:(id)sender {
  // The user has confirmed a dangerous download.  We record how quickly the
  // user did this to detect whether we're being clickjacked.
  UMA_HISTOGRAM_LONG_TIMES("clickjacking.save_download",
                           base::Time::Now() - creationTime_);
  // This will change the state and notify us.
  bridge_->download_model()->download()->manager()->DangerousDownloadValidated(
      bridge_->download_model()->download());
}

- (IBAction)discardDownload:(id)sender {
  UMA_HISTOGRAM_LONG_TIMES("clickjacking.discard_download",
                           base::Time::Now() - creationTime_);
  if (bridge_->download_model()->download()->state() ==
      DownloadItem::IN_PROGRESS)
    bridge_->download_model()->download()->Cancel(true);
  bridge_->download_model()->download()->Remove(true);
  // WARNING: we are deleted at this point.  Don't access 'this'.
}


// Sets the enabled and checked state of a particular menu item for this
// download. We translate the NSMenuItem selection to menu selections understood
// by the non platform specific download context menu.
- (BOOL)validateMenuItem:(NSMenuItem *)item {
  SEL action = [item action];

  int actionId = 0;
  if (action == @selector(handleOpen:)) {
    actionId = DownloadShelfContextMenuMac::OPEN_WHEN_COMPLETE;
  } else if (action == @selector(handleAlwaysOpen:)) {
    actionId = DownloadShelfContextMenuMac::ALWAYS_OPEN_TYPE;
  } else if (action == @selector(handleReveal:)) {
    actionId = DownloadShelfContextMenuMac::SHOW_IN_FOLDER;
  } else if (action == @selector(handleCancel:)) {
    actionId = DownloadShelfContextMenuMac::CANCEL;
  } else if (action == @selector(handleTogglePause:)) {
    actionId = DownloadShelfContextMenuMac::TOGGLE_PAUSE;
  } else {
    NOTREACHED();
    return YES;
  }

  if (menuBridge_->ItemIsChecked(actionId))
    [item setState:NSOnState];
  else
    [item setState:NSOffState];

  return menuBridge_->IsItemCommandEnabled(actionId) ? YES : NO;
}

- (IBAction)handleOpen:(id)sender {
  menuBridge_->ExecuteItemCommand(
      DownloadShelfContextMenuMac::OPEN_WHEN_COMPLETE);
}

- (IBAction)handleAlwaysOpen:(id)sender {
  menuBridge_->ExecuteItemCommand(
      DownloadShelfContextMenuMac::ALWAYS_OPEN_TYPE);
}

- (IBAction)handleReveal:(id)sender {
  menuBridge_->ExecuteItemCommand(DownloadShelfContextMenuMac::SHOW_IN_FOLDER);
}

- (IBAction)handleCancel:(id)sender {
  menuBridge_->ExecuteItemCommand(DownloadShelfContextMenuMac::CANCEL);
}

- (IBAction)handleTogglePause:(id)sender {
  menuBridge_->ExecuteItemCommand(DownloadShelfContextMenuMac::TOGGLE_PAUSE);
}

@end
