// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/location_bar/content_setting_decoration.h"

#include <algorithm>

#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_content_setting_bubble_model_delegate.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_list.h"
#import "chrome/browser/ui/cocoa/content_settings/content_setting_bubble_cocoa.h"
#include "chrome/browser/ui/cocoa/last_active_browser_cocoa.h"
#import "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"
#import "chrome/browser/ui/cocoa/themed_window.h"
#include "chrome/browser/ui/content_settings/content_setting_bubble_model.h"
#include "chrome/browser/ui/content_settings/content_setting_image_model.h"
#include "chrome/grit/theme_resources.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "skia/ext/skia_utils_mac.h"
#include "ui/base/cocoa/appkit_utils.h"
#include "ui/base/cocoa/cocoa_base_utils.h"
#import "ui/base/cocoa/nsview_additions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/mac/coordinate_conversion.h"
#include "ui/gfx/scoped_ns_graphics_context_save_gstate_mac.h"

using content::WebContents;

namespace {

// The bubble point should look like it points to the bottom of the respective
// icon. The offset should be 2px.
const CGFloat kPageBubblePointYOffset = 2.0;

// Duration of animation, 3 seconds. The ContentSettingAnimationState breaks
// this up into different states of varying lengths.
const NSTimeInterval kAnimationDuration = 3.0;

// Interval of the animation timer, 60Hz.
const NSTimeInterval kAnimationInterval = 1.0 / 60.0;

// The % of time it takes to open or close the animating text, ie at 0.2, the
// opening takes 20% of the whole animation and the closing takes 20%. The
// remainder of the animation is with the text at full width.
const double kInMotionInterval = 0.2;

// Used to create a % complete of the "in motion" part of the animation, eg
// it should be 1.0 (100%) when the progress is 0.2.
const double kInMotionMultiplier = 1.0 / kInMotionInterval;

// Padding for the animated text with respect to the image.
const CGFloat kTextMarginPadding = 4;
const CGFloat kIconMarginPadding = 4;
const CGFloat kBorderPadding = 3;

// Padding between the divider and the decoration on the right.
const CGFloat kDividerPadding = 1;

// Padding between the divider and the text.
const CGFloat kTextDividerPadding = 2;

// Color of the vector graphic icons. Used when the location is not dark.
// SkColorSetARGB(0xCC, 0xFF, 0xFF 0xFF);
const SkColor kVectorIconColor = 0xCCFFFFFF;

// Different states in which the animation can be. In |kOpening|, the text
// is getting larger. In |kOpen|, the text should be displayed at full size.
// In |kClosing|, the text is again getting smaller. The durations in which
// the animation remains in each state are internal to
// |ContentSettingAnimationState|.
enum AnimationState {
  kNoAnimation,
  kOpening,
  kOpen,
  kClosing
};

}  // namespace


// An ObjC class that handles the multiple states of the text animation and
// bridges NSTimer calls back to the ContentSettingDecoration that owns it.
// Should be lazily instantiated to only exist when the decoration requires
// animation.
// NOTE: One could make this class more generic, but this class only exists
// because CoreAnimation cannot be used (there are no views to work with).
@interface ContentSettingAnimationState : NSObject {
 @private
  ContentSettingDecoration* owner_;  // Weak, owns this.
  double progress_;  // Counter, [0..1], with aninmation progress.
  NSTimer* timer_;  // Animation timer. Owns this, owned by the run loop.
}

// [0..1], the current progress of the animation. -animationState will return
// |kNoAnimation| when progress is <= 0 or >= 1. Useful when state is
// |kOpening| or |kClosing| as a multiplier for displaying width. Don't use
// to track state transitions, use -animationState instead.
@property (readonly, nonatomic) double progress;

// Designated initializer. |owner| must not be nil. Animation timer will start
// as soon as the object is created.
- (id)initWithOwner:(ContentSettingDecoration*)owner;

// Returns the current animation state based on how much time has elapsed.
- (AnimationState)animationState;

// Call when |owner| is going away or the animation needs to be stopped.
// Ensures that any dangling references are cleared. Can be called multiple
// times.
- (void)stopAnimation;

@end

@implementation ContentSettingAnimationState

@synthesize progress = progress_;

- (id)initWithOwner:(ContentSettingDecoration*)owner {
  self = [super init];
  if (self) {
    owner_ = owner;
    timer_ = [NSTimer scheduledTimerWithTimeInterval:kAnimationInterval
                                              target:self
                                            selector:@selector(timerFired:)
                                            userInfo:nil
                                             repeats:YES];
  }
  return self;
}

- (void)dealloc {
  DCHECK(!timer_);
  [super dealloc];
}

// Clear weak references and stop the timer.
- (void)stopAnimation {
  owner_ = nil;
  [timer_ invalidate];
  timer_ = nil;
}

// Returns the current state based on how much time has elapsed.
- (AnimationState)animationState {
  if (progress_ <= 0.0 || progress_ >= 1.0)
    return kNoAnimation;
  if (progress_ <= kInMotionInterval)
    return kOpening;
  if (progress_ >= 1.0 - kInMotionInterval)
    return kClosing;
  return kOpen;
}

- (void)timerFired:(NSTimer*)timer {
  // Increment animation progress, normalized to [0..1].
  progress_ += kAnimationInterval / kAnimationDuration;
  progress_ = std::min(progress_, 1.0);
  owner_->AnimationTimerFired();
  // Stop timer if it has reached the end of its life.
  if (progress_ >= 1.0)
    [self stopAnimation];
}

@end


ContentSettingDecoration::ContentSettingDecoration(
    ContentSettingImageModel* model,
    LocationBarViewMac* owner,
    Profile* profile)
    : content_setting_image_model_(model),
      owner_(owner),
      profile_(profile),
      text_width_(0.0) {
}

ContentSettingDecoration::~ContentSettingDecoration() {
  // Just in case the timer is still holding onto the animation object, force
  // cleanup so it can't get back to |this|.
  [animation_ stopAnimation];
}

bool ContentSettingDecoration::UpdateFromWebContents(
    WebContents* web_contents) {
  bool was_visible = IsVisible();
  bool did_icon_change =
      content_setting_image_model_->UpdateFromWebContentsAndCheckIfIconChanged(
          web_contents);
  SetVisible(content_setting_image_model_->is_visible());
  bool decoration_changed = was_visible != IsVisible() || did_icon_change;
  if (IsVisible()) {
    SkColor icon_color =
        owner_->IsLocationBarDark() ? kVectorIconColor : gfx::kChromeIconGrey;
    SetImage(content_setting_image_model_->GetIcon(icon_color).ToNSImage());

    SetToolTip(
        base::SysUTF16ToNSString(content_setting_image_model_->get_tooltip()));

    // Check if there is an animation and start it if it hasn't yet started.
    bool has_animated_text =
        content_setting_image_model_->explanatory_string_id();

    // Check if the animation has already run.
    if (has_animated_text &&
        content_setting_image_model_->ShouldRunAnimation(web_contents) &&
        !animation_) {
      // Mark the animation as having been run.
      content_setting_image_model_->SetAnimationHasRun(web_contents);
      // Start animation, its timer will drive reflow. Note the text is
      // cached so it is not allowed to change during the animation.
      animation_.reset(
          [[ContentSettingAnimationState alloc] initWithOwner:this]);
      animated_text_ = CreateAnimatedText();
      text_width_ = MeasureTextWidth();
    } else if (!has_animated_text) {
      // Decoration no longer has animation, stop it (ok to always do this).
      [animation_ stopAnimation];
      animation_.reset();
    }
  } else {
    // Decoration no longer visible, stop/clear animation.
    [animation_ stopAnimation];
    animation_.reset(nil);
  }
  return decoration_changed;
}

CGFloat ContentSettingDecoration::MeasureTextWidth() {
  return [animated_text_ size].width;
}

base::scoped_nsobject<NSAttributedString>
ContentSettingDecoration::CreateAnimatedText() {
  NSString* text =
      l10n_util::GetNSString(
          content_setting_image_model_->explanatory_string_id());
  base::scoped_nsobject<NSMutableParagraphStyle> style(
      [[NSMutableParagraphStyle alloc] init]);
  // Set line break mode to clip the text, otherwise drawInRect: won't draw a
  // word if it doesn't fit in the bounding box.
  [style setLineBreakMode:NSLineBreakByClipping];

  SkColor text_color = owner_->IsLocationBarDark() ? kMaterialDarkModeTextColor
                                                   : gfx::kChromeIconGrey;
  NSDictionary* attributes = @{
    NSFontAttributeName : GetFont(),
    NSParagraphStyleAttributeName : style,
    NSForegroundColorAttributeName :
        skia::SkColorToCalibratedNSColor(text_color)
  };

  return base::scoped_nsobject<NSAttributedString>(
      [[NSAttributedString alloc] initWithString:text attributes:attributes]);
}

NSPoint ContentSettingDecoration::GetBubblePointInFrame(NSRect frame) {
  // Compute the frame as if there is no animation pill in the Omnibox. Place
  // the bubble where the icon would be without animation, so when the animation
  // ends, the bubble is pointing in the right place.
  NSSize image_size = [GetImage() size];
  frame.origin.x += frame.size.width - image_size.width;
  frame.size = image_size;

  const NSRect draw_frame = GetDrawRectInFrame(frame);
  return NSMakePoint(NSMidX(draw_frame),
                     NSMaxY(draw_frame) + kPageBubblePointYOffset);
}

bool ContentSettingDecoration::AcceptsMousePress() {
  return true;
}

bool ContentSettingDecoration::OnMousePressed(NSRect frame, NSPoint location) {
  // Get host. This should be shared on linux/win/osx medium-term.
  Browser* browser = owner_->browser();
  WebContents* web_contents = owner_->GetWebContents();
  if (!web_contents)
    return true;

  // Find point for bubble's arrow in screen coordinates.
  // TODO(shess): |owner_| is only being used to fetch |field|.
  // Consider passing in |control_view|.  Or refactoring to be
  // consistent with other decorations (which don't currently bring up
  // their bubble directly).
  AutocompleteTextField* field = owner_->GetAutocompleteTextField();
  NSPoint anchor = GetBubblePointInFrame(frame);
  anchor = [field convertPoint:anchor toView:nil];
  anchor = ui::ConvertPointFromWindowToScreen([field window], anchor);

  // Open bubble.
  ContentSettingBubbleModel* model =
      content_setting_image_model_->CreateBubbleModel(
          browser->content_setting_bubble_model_delegate(),
          web_contents,
          profile_);

  if (chrome::ToolkitViewsDialogsEnabled()) {
    gfx::Point origin = gfx::ScreenPointFromNSPoint(anchor);
    chrome::ContentSettingBubbleViewsBridge::Show(
        [web_contents->GetTopLevelNativeWindow() contentView],
        model, web_contents, origin);
  } else {
    [ContentSettingBubbleController showForModel:model
                                     webContents:web_contents
                                    parentWindow:[field window]
                                      decoration:this
                                      anchoredAt:anchor];
  }

  return true;
}

NSString* ContentSettingDecoration::GetToolTip() {
  return tooltip_.get();
}

void ContentSettingDecoration::SetToolTip(NSString* tooltip) {
  tooltip_.reset([tooltip retain]);
}

// Override to handle the case where there is text to display during the
// animation. The width is based on the animator's progress.
CGFloat ContentSettingDecoration::GetWidthForSpace(CGFloat width) {
  CGFloat preferred_width = ImageDecoration::GetWidthForSpace(width);
  if (animation_.get()) {
    AnimationState state = [animation_ animationState];
    if (state != kNoAnimation) {
      CGFloat progress = [animation_ progress];
      // Add the margins, fixed for all animation states.
      preferred_width += kIconMarginPadding + kTextMarginPadding;

      // Add the width of the text based on the state of the animation.
      CGFloat text_width = text_width_ + kDividerPadding;
      switch (state) {
        case kOpening:
          preferred_width += text_width * kInMotionMultiplier * progress;
          break;
        case kOpen:
          preferred_width += text_width;
          break;
        case kClosing:
          preferred_width += text_width * kInMotionMultiplier * (1 - progress);
          break;
        default:
          // Do nothing.
          break;
      }
    }
  }
  return preferred_width;
}

void ContentSettingDecoration::DrawInFrame(NSRect frame, NSView* control_view) {
  if ([animation_ animationState] != kNoAnimation) {
    NSRect background_rect = NSInsetRect(frame, 0.0, kBorderPadding);
    // This code is almost identical to code that appears in BubbleDecoration.
    // Unfortunately ContentSettingDecoration does not descend from
    // BubbleDecoration. Even if we move the code to LocationBarDecoration (the
    // common ancestor) the semantics here are slightly different: there's a
    // general DrawBackgroundInFrame() method for LocationBarDecorations that
    // draws the background and then calls DrawInFrame(), but for some reason
    // this ContentSettingDecoration's DrawInFrame() also draws the background.
    // In short, moving this code upstream to a common parent requires a non-
    // trivial bit of refactoring.
    // Draw the icon.
    NSImage* icon = GetImage();
    NSRect icon_rect = background_rect;
    if (icon) {
      icon_rect.origin.x += kIconMarginPadding;
      icon_rect.size.width = [icon size].width;
      ImageDecoration::DrawInFrame(icon_rect, control_view);
    }

    NSRect remainder = frame;
    remainder.origin.x = NSMaxX(icon_rect) + kTextMarginPadding;
    remainder.size.width =
        NSMaxX(background_rect) - NSMinX(remainder) - kTextDividerPadding;
    DrawAttributedString(animated_text_, remainder);

    // Draw the divider if available.
    if (state() == DecorationMouseState::NONE && !active()) {
      NSBezierPath* line = [NSBezierPath bezierPath];
      [line setLineWidth:1];
      [line moveToPoint:NSMakePoint(NSMaxX(background_rect) - kDividerPadding,
                                    NSMinY(background_rect))];
      [line lineToPoint:NSMakePoint(NSMaxX(background_rect) - kDividerPadding,
                                    NSMaxY(background_rect))];
      [GetDividerColor(owner_->IsLocationBarDark()) set];
      [line stroke];
    }
  } else {
    // No animation, draw the image as normal.
    ImageDecoration::DrawInFrame(frame, control_view);
  }
}

void ContentSettingDecoration::AnimationTimerFired() {
  owner_->Layout();
  // Even after the animation completes, the |animator_| object should be kept
  // alive to prevent the animation from re-appearing if the page opens
  // additional popups later. The animator will be cleared when the decoration
  // hides, indicating something has changed with the WebContents (probably
  // navigation).
}
