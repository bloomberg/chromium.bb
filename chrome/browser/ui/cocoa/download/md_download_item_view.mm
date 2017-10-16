// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/download/md_download_item_view.h"

#include "base/strings/sys_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/download/download_item_model.h"
#import "chrome/browser/themes/theme_properties.h"
#import "chrome/browser/ui/cocoa/download/download_item_controller.h"
#import "chrome/browser/ui/cocoa/download/download_shelf_context_menu_controller.h"
#import "chrome/browser/ui/cocoa/download/md_download_item_progress_indicator.h"
#import "chrome/browser/ui/cocoa/harmony_button.h"
#import "chrome/browser/ui/cocoa/md_hover_button.h"
#import "chrome/browser/ui/cocoa/md_util.h"
#import "chrome/browser/ui/cocoa/nsview_additions.h"
#import "chrome/browser/ui/cocoa/themed_window.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "third_party/google_toolbox_for_mac/src/AppKit/GTMUILocalizerAndLayoutTweaker.h"
#import "ui/base/cocoa/a11y_util.h"
#import "ui/base/cocoa/controls/textfield_utils.h"
#import "ui/base/cocoa/nsview_additions.h"
#import "ui/base/cocoa/quartzcore_additions.h"
#include "ui/base/l10n/l10n_util_mac.h"
#import "ui/base/theme_provider.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/font_list.h"
#import "ui/gfx/image/image_skia_util_mac.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_elider.h"

namespace {

// Size of a download item in a non-dangerous state.
constexpr CGSize kNormalSize = {239, 44};

constexpr CGFloat kDangerousDownloadIconX = 16;
constexpr CGFloat kDangerousDownloadIconSize = 16;
constexpr CGFloat kDangerousDownloadLabelX =
    kDangerousDownloadIconX * 2 + kDangerousDownloadIconSize;
constexpr CGFloat kDangerousDownloadLabelButtonSpacing = 6;
constexpr CGFloat kDangerousDownloadLabelMinWidth = 140;
constexpr CGFloat kDangerousDownloadLabelYInset = 8;

constexpr CGRect kProgressIndicatorFrame{{12, 10}, {24, 24}};
constexpr CGFloat kImageXInset = 16;
constexpr CGSize kImageSize{16, 16};
constexpr CGFloat kDividerWidth = 1;
constexpr CGFloat kDividerYInset = 8;
constexpr CGFloat kButtonLeadingInset = 8;
constexpr CGFloat kButtonTrailingInset = 5;
constexpr CGFloat kButtonYInset = 6;

constexpr CGFloat kTextX = 46;
constexpr CGFloat kFilenameY = 15;
constexpr CGFloat kFilenameWithStatusY = 22;
constexpr CGFloat kStatusTextY = 8;
constexpr CGFloat kMenuButtonSpacing = 8;

constexpr CGFloat kMenuButtonTrailingMargin = 9;
constexpr CGFloat kMenuButtonSize = 24;
constexpr const gfx::VectorIcon* kMenuButtonIcon = &kHorizontalMenuIcon;

NSTextField* MakeLabel(
    NSFont* font,
    NSLineBreakMode lineBreakMode = NSLineBreakByTruncatingTail) {
  NSTextField* label = [TextFieldUtils labelWithString:@""];
  label.font = font;
  label.selectable = NO;
  if (@available(macOS 10.10, *)) {
    label.lineBreakMode = lineBreakMode;
  } else {
    static_cast<NSTextFieldCell*>(label.cell).lineBreakMode = lineBreakMode;
  }
  [label sizeToFit];
  return label;
}
}  // namespace

@interface MDDownloadItemMenuButton : MDHoverButton
@property(nonatomic, assign) MDHoverButton* suppressButton;
@end

@implementation MDDownloadItemMenuButton {
  NSPopUpButtonCell* popUpCell_;
}
@synthesize suppressButton = suppressButton_;

- (instancetype)initWithFrame:(NSRect)frameRect {
  if ((self = [super initWithFrame:frameRect])) {
    self.icon = kMenuButtonIcon;
    self.imagePosition = NSImageOnly;
  }
  return self;
}

- (void)setHoverState:(HoverState)hoverState {
  suppressButton_.hoverSuppressed = hoverState != kHoverStateNone;
  [super setHoverState:hoverState];
}

- (void)showMenuWithEvent:(NSEvent*)event {
  // NSPopupButtonCell knows how to position a menu relative to a button.
  base::scoped_nsobject<NSPopUpButtonCell> popUpCell(
      [[NSPopUpButtonCell alloc] initTextCell:@"" pullsDown:YES]);
  popUpCell_ = popUpCell;  // Cleared below, within this function.
  popUpCell_.menu = [self.superview menu];
  self.hoverState = kHoverStateMouseDown;
  // The inset positions the menu flush with the edges of the buttion.
  [popUpCell_ trackMouse:event
                  inRect:NSInsetRect(self.bounds, -4, 0)
                  ofView:self
            untilMouseUp:YES];
  popUpCell_ = nil;
  [self checkImageState];
}

- (void)mouseDown:(NSEvent*)event {
  [self showMenuWithEvent:event];
}

- (void)performClick:(id)sender {
  [self showMenuWithEvent:[NSApp currentEvent]];
}

- (BOOL)accessibilityPerformPress {
  [self performClick:nil];
  return YES;
}

- (void)viewWillMoveToWindow:(NSWindow*)newWindow {
  // If the menu is still visible, dismiss it.
  if (!newWindow) {
    [popUpCell_.menu cancelTrackingWithoutAnimation];
  }
}

@end

@interface MDDownloadItemDangerView : NSView
@property(nonatomic, assign) NSButton* button;
@end

@implementation MDDownloadItemDangerView {
  NSImageView* iconView_;
  NSTextField* label_;
}
@synthesize button = button_;

- (instancetype)initWithFrame:(NSRect)frameRect {
  if ((self = [super initWithFrame:frameRect])) {
    NSRect iconRect =
        NSMakeRect(kDangerousDownloadIconX,
                   NSMidY(self.bounds) - kDangerousDownloadIconSize / 2,
                   kDangerousDownloadIconSize, kDangerousDownloadIconSize);
    base::scoped_nsobject<NSImageView> iconView(
        [[NSImageView alloc] initWithFrame:[self cr_localizedRect:iconRect]]);
    iconView_ = iconView;
    iconView_.autoresizingMask =
        [NSView cr_localizedAutoresizingMask:NSViewMaxXMargin];
    iconView_.image = NSImageFromImageSkia(
        gfx::CreateVectorIcon(vector_icons::kWarningIcon,
                              kDangerousDownloadIconSize, gfx::kGoogleRed700));
    ui::a11y_util::HideImageFromAccessibilityOrder(iconView_);
    [self addSubview:iconView_];

    label_ = MakeLabel([NSFont systemFontOfSize:10], NSLineBreakByWordWrapping);
    label_.frame = NSInsetRect(self.bounds, 0, kDangerousDownloadLabelYInset);
    label_.autoresizingMask =
        [NSView cr_localizedAutoresizingMask:NSViewMaxXMargin];
    [self addSubview:label_];

    base::scoped_nsobject<HarmonyButton> button(
        [[HarmonyButton alloc] initWithFrame:NSZeroRect]);
    button_ = button;
    button_.title = l10n_util::GetNSString(IDS_DISCARD_DOWNLOAD);
    [button_ sizeToFit];
    NSRect buttonRect = button_.frame;
    buttonRect.origin.x = NSWidth(self.bounds) - NSWidth(buttonRect);
    buttonRect.origin.y = NSMidY(self.bounds) - NSMidY(button_.bounds);
    button_.frame = [self cr_localizedRect:buttonRect];
    button_.autoresizingMask =
        [NSView cr_localizedAutoresizingMask:NSViewMinXMargin];
    [self addSubview:button_];
  }
  return self;
}

- (CGFloat)preferredWidth {
  CGFloat preferredWidth = kDangerousDownloadLabelX + NSWidth(label_.frame) +
                           kDangerousDownloadLabelButtonSpacing +
                           NSWidth(button_.frame);
  return NSWidth([self backingAlignedRect:NSMakeRect(0, 0, preferredWidth, 0)
                                  options:NSAlignAllEdgesOutward]);
}

- (void)setStateFromDownload:(DownloadItemModel*)downloadModel {
  label_.stringValue = base::SysUTF16ToNSString(downloadModel->GetWarningText(
      gfx::FontList(gfx::Font(label_.font)), 140));
  [GTMUILocalizerAndLayoutTweaker
      sizeToFitFixedHeightTextField:label_
                           minWidth:kDangerousDownloadLabelMinWidth];
  NSRect labelRect = label_.frame;
  labelRect.origin.x = kDangerousDownloadLabelX;
  labelRect.origin.y = NSMidY(self.bounds) - NSMidY(label_.bounds);
  label_.frame = [self cr_localizedRect:labelRect];
}

// NSView overrides

- (void)layout {
  if (const ui::ThemeProvider* themeProvider = [self.window themeProvider]) {
    label_.textColor =
        themeProvider->GetNSColor(ThemeProperties::COLOR_TAB_TEXT);
  }
  [super layout];
}

@end

@interface MDDownloadItemView ()<NSAccessibilityGroup,
                                 HoverButtonDragDelegate,
                                 NSDraggingSource>
@end

@implementation MDDownloadItemView {
  base::FilePath downloadPath_;
  MDDownloadItemMenuButton* menuButton_;
  NSView* dividerView_;

  // Normal state
  MDHoverButton* button_;
  NSImageView* imageView_;
  MDDownloadItemProgressIndicator* progressIndicator_;
  NSTextField* filenameView_;
  NSTextField* statusTextView_;

  // Danger state
  MDDownloadItemDangerView* dangerView_;
}
@synthesize controller = controller_;

- (instancetype)init {
  if ((self = [super initWithFrame:NSMakeRect(0, 0, kNormalSize.width,
                                              kNormalSize.height)])) {
    const NSRect bounds = self.bounds;
    const NSRect buttonRect =
        NSMakeRect(kButtonLeadingInset, kButtonYInset,
                   NSWidth(bounds) - kButtonLeadingInset - kButtonTrailingInset,
                   NSHeight(bounds) - kButtonYInset * 2);
    base::scoped_nsobject<MDHoverButton> button(
        [[MDHoverButton alloc] initWithFrame:buttonRect]);
    button_ = button;
    button_.imagePosition = NSImageOnly;
    [self addSubview:button_];

    base::scoped_nsobject<MDDownloadItemProgressIndicator> progressIndicator(
        [[MDDownloadItemProgressIndicator alloc]
            initWithFrame:[self cr_localizedRect:kProgressIndicatorFrame]]);
    progressIndicator_ = progressIndicator;
    progressIndicator_.autoresizingMask =
        [NSView cr_localizedAutoresizingMask:NSViewMaxXMargin];
    [self addSubview:progressIndicator_];

    base::scoped_nsobject<NSImageView> imageView([[NSImageView alloc]
        initWithFrame:[self
                          cr_localizedRect:NSMakeRect(kImageXInset,
                                                      NSMidY(bounds) -
                                                          kImageSize.height / 2,
                                                      kImageSize.width,
                                                      kImageSize.height)]]);
    imageView_ = imageView;
    imageView_.autoresizingMask =
        [NSView cr_localizedAutoresizingMask:NSViewMaxXMargin];
    ui::a11y_util::HideImageFromAccessibilityOrder(imageView_);
    [self addSubview:imageView_];

    NSRect menuButtonRect = NSMakeRect(
        NSMaxX(bounds) - kMenuButtonSize - kMenuButtonTrailingMargin,
        NSMidY(bounds) - kMenuButtonSize / 2, kMenuButtonSize, kMenuButtonSize);
    base::scoped_nsobject<MDDownloadItemMenuButton> menuButton(
        [[MDDownloadItemMenuButton alloc]
            initWithFrame:[button_ cr_localizedRect:menuButtonRect]]);
    menuButton_ = menuButton;
    menuButton_.autoresizingMask = [NSView
        cr_localizedAutoresizingMask:NSViewMinXMargin | NSViewMinYMargin |
                                     NSViewMaxYMargin];
    menuButton_.suppressButton = button_;
    [menuButton_
        cr_setAccessibilityLabel:l10n_util::GetNSStringWithFixup(IDS_OPTIONS)];
    [self addSubview:menuButton_];

    NSRect dividerRect =
        NSInsetRect(NSMakeRect(NSWidth(self.bounds) - kDividerWidth, 0,
                               kDividerWidth, NSHeight(self.bounds)),
                    0, kDividerYInset);
    base::scoped_nsobject<NSView> dividerView(
        [[NSView alloc] initWithFrame:[self cr_localizedRect:dividerRect]]);
    dividerView_ = dividerView;
    dividerView_.autoresizingMask = [NSView
        cr_localizedAutoresizingMask:NSViewMinXMargin | NSViewHeightSizable];
    dividerView_.wantsLayer = YES;
    [self addSubview:dividerView_];

    filenameView_ = MakeLabel([NSFont systemFontOfSize:12]);
    NSRect filenameRect =
        NSMakeRect(kTextX, kFilenameY,
                   (NSMinX(menuButtonRect) - kMenuButtonSpacing) - kTextX,
                   NSHeight(filenameView_.bounds));
    filenameView_.frame = [self cr_localizedRect:filenameRect];
    filenameView_.autoresizingMask =
        [NSView cr_localizedAutoresizingMask:NSViewMaxXMargin];
    [self addSubview:filenameView_];

    statusTextView_ = MakeLabel([NSFont systemFontOfSize:12]);
    NSRect statusTextRect =
        NSMakeRect(kTextX, kStatusTextY, NSWidth(filenameRect),
                   NSHeight(statusTextView_.bounds));
    statusTextView_.frame = [self cr_localizedRect:statusTextRect];
    statusTextView_.autoresizingMask =
        [NSView cr_localizedAutoresizingMask:NSViewMaxXMargin];
    statusTextView_.hidden = YES;
    [self addSubview:statusTextView_];
  }
  return self;
}

- (CGFloat)preferredWidth {
  if (dangerView_) {
    CGFloat delta = [dangerView_ preferredWidth] - NSWidth(dangerView_.frame);
    return NSWidth(self.frame) + delta;
  } else {
    return kNormalSize.width;
  }
}

- (void)layout {
  const ui::ThemeProvider* themeProvider = [self.window themeProvider];
  if (!themeProvider)
    return;
  const BOOL darkTheme = [self.window hasDarkTheme];
  NSColor* textColor =
      themeProvider->GetNSColor(ThemeProperties::COLOR_TAB_TEXT);
  filenameView_.textColor = textColor;
  statusTextView_.textColor = themeProvider->ShouldIncreaseContrast()
                                  ? textColor
                                  : [textColor colorWithAlphaComponent:0.6];
  dividerView_.layer.backgroundColor =
      themeProvider->ShouldIncreaseContrast()
          ? CGColorGetConstantColor(darkTheme ? kCGColorWhite : kCGColorBlack)
          : themeProvider
                ->GetNSColor(ThemeProperties::COLOR_TOOLBAR_VERTICAL_SEPARATOR)
                .CGColor;
  [super layout];
}

- (id)target {
  return button_.target;
}

- (void)setTarget:(id)target {
  button_.target = target;
}

- (NSMenu*)menu {
  return [[[[DownloadShelfContextMenuController alloc]
      initWithItemController:controller_
                withDelegate:nil] autorelease] menu];
}

- (SEL)action {
  return button_.action;
}

- (void)setAction:(SEL)action {
  button_.action = action;
}

- (NSArray<NSView*>*)normalViews {
  return @[
    button_, imageView_, progressIndicator_, filenameView_, statusTextView_
  ];
}

- (void)setStateFromDownload:(DownloadItemModel*)downloadModel {
  const content::DownloadItem& download = *downloadModel->download();
  if (download.IsDangerous()) {
    if (!dangerView_) {
      for (NSView* view in [self normalViews]) {
        view.hidden = YES;
      }
      NSRect dangerViewRect =
          NSMakeRect(0, 0,
                     NSWidth(self.bounds) - NSWidth(menuButton_.frame) -
                         kMenuButtonSpacing - kMenuButtonTrailingMargin,
                     NSHeight(self.bounds));
      base::scoped_nsobject<MDDownloadItemDangerView> dangerView(
          [[MDDownloadItemDangerView alloc]
              initWithFrame:[self cr_localizedRect:dangerViewRect]]);
      dangerView_ = dangerView;
      dangerView_.autoresizingMask =
          [NSView cr_localizedAutoresizingMask:NSViewWidthSizable];
      dangerView_.button.target = controller_;
      dangerView_.button.action = @selector(discardDownload:);
      [self addSubview:dangerView_];
    }
    [dangerView_ setStateFromDownload:downloadModel];
    return;
  } else if (dangerView_) {
    for (NSView* view in [self normalViews]) {
      view.hidden = NO;
    }
    [dangerView_ removeFromSuperview];
    dangerView_ = nil;
  }
  downloadPath_ = download.GetFullPath();
  button_.dragDelegate =
      downloadModel->download()->GetState() == content::DownloadItem::COMPLETE
          ? self
          : nil;

  [button_
      cr_setAccessibilityLabel:l10n_util::GetNSStringWithFixup(
                                   download.IsDone()
                                       ? IDS_DOWNLOAD_MENU_OPEN
                                       : IDS_DOWNLOAD_MENU_OPEN_WHEN_COMPLETE)];

  [self
      cr_setAccessibilityLabel:base::SysUTF8ToNSString(
                                   download.GetFileNameToReportUser().value())];

  progressIndicator_.progress = downloadModel->PercentComplete() / 100.0;
  progressIndicator_.paused = download.IsPaused();

  button_.enabled = ^{
    switch (download.GetState()) {
      case content::DownloadItem::IN_PROGRESS:
      case content::DownloadItem::COMPLETE:
        return YES;
      default:
        return NO;
    }
  }();

  switch (download.GetState()) {
    case content::DownloadItem::COMPLETE:
      [progressIndicator_ complete];
      break;
    case content::DownloadItem::IN_PROGRESS:
      break;
    case content::DownloadItem::CANCELLED:
    case content::DownloadItem::INTERRUPTED:
      [progressIndicator_ cancel];
      break;
    case content::DownloadItem::MAX_DOWNLOAD_STATE:
      NOTREACHED();
      break;
  }

  filenameView_.stringValue = base::SysUTF16ToNSString(
      gfx::ElideFilename(downloadModel->download()->GetFileNameToReportUser(),
                         gfx::FontList(gfx::Font(filenameView_.font)),
                         NSWidth(filenameView_.bounds)));

  NSString* statusString =
      base::SysUTF16ToNSString(downloadModel->GetStatusText());
  const BOOL hadStatusBefore = statusTextView_.stringValue.length > 0;
  const BOOL hasStatus = statusString.length > 0;
  statusTextView_.stringValue = statusString;
  if (hasStatus != hadStatusBefore) {
    [NSAnimationContext runAnimationGroup:^(NSAnimationContext* context) {
      context.duration = 0.25;
      context.timingFunction =
          CAMediaTimingFunction.cr_materialEaseInOutTimingFunction;
      // Explicitly animate position.y so that x position isn't animated for
      // a new download (which would happen with view.animator).
      [filenameView_.layer
          addAnimation:[CABasicAnimation animationWithKeyPath:@"position.y"]
                forKey:nil];
      [filenameView_ setFrameOrigin:NSMakePoint(NSMinX(filenameView_.frame),
                                                hasStatus ? kFilenameWithStatusY
                                                          : kFilenameY)];
      statusTextView_.animator.hidden = !hasStatus;
    }
                        completionHandler:nil];
  }
}

- (NSView*)hitTest:(NSPoint)aPoint {
  NSView* hitView = [super hitTest:aPoint];
  if (hitView && ![hitView isKindOfClass:[NSButton class]]) {
    return button_;
  }
  return hitView;
}

- (void)setImage:(NSImage*)image {
  imageView_.image = image;
}

- (void)beginDragFromHoverButton:(HoverButton*)button event:(NSEvent*)event {
  NSAttributedString* filename = filenameView_.attributedStringValue;
  NSSize filenameSize = filename.size;
  NSRect imageRect = NSMakeRect(0, 0, 32, 32);
  NSRect labelRect = [self
      backingAlignedRect:NSMakeRect(35, 32 / 2 - filenameSize.height / 2,
                                    filenameSize.width, filenameSize.height)
                 options:NSAlignAllEdgesOutward];
  NSDraggingItem* draggingItem = [[[NSDraggingItem alloc]
      initWithPasteboardWriter:[NSURL
                                   fileURLWithPath:base::SysUTF8ToNSString(
                                                       downloadPath_.value())]]
      autorelease];
  draggingItem.imageComponentsProvider = ^{
    NSDraggingImageComponent* imageComponent =
        [[[NSDraggingImageComponent alloc]
            initWithKey:NSDraggingImageComponentIconKey] autorelease];
    NSImage* image = imageView_.image;
    imageComponent.contents = image;
    imageComponent.frame = imageRect;
    NSDraggingImageComponent* labelComponent =
        [[[NSDraggingImageComponent alloc]
            initWithKey:NSDraggingImageComponentLabelKey] autorelease];

    labelComponent.contents = [NSImage imageWithSize:labelRect.size
                                             flipped:NO
                                      drawingHandler:^(NSRect rect) {
                                        [filename drawAtPoint:NSZeroPoint];
                                        return YES;
                                      }];
    labelComponent.frame = labelRect;
    return @[ imageComponent, labelComponent ];
  };
  NSPoint dragOrigin =
      [self convertPoint:[self.window mouseLocationOutsideOfEventStream]
                fromView:nil];
  draggingItem.draggingFrame =
      [self backingAlignedRect:NSOffsetRect(imageRect, dragOrigin.x - 16,
                                            dragOrigin.y - 16)
                       options:NSAlignAllEdgesOutward];
  [self beginDraggingSessionWithItems:@[ draggingItem ]
                                event:event
                               source:self];
}

- (NSDragOperation)draggingSession:(NSDraggingSession*)session
    sourceOperationMaskForDraggingContext:(NSDraggingContext)context {
  return NSDragOperationCopy;
}

@end
