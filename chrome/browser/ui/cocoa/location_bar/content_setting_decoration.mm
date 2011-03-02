// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/location_bar/content_setting_decoration.h"

#include <algorithm>

#include "base/command_line.h"
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/content_setting_image_model.h"
#include "chrome/browser/content_setting_bubble_model.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#import "chrome/browser/ui/cocoa/content_settings/content_setting_bubble_cocoa.h"
#import "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "net/base/net_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image.h"

namespace {

// How far to offset up from the bottom of the view to get the top
// border of the popup 2px below the bottom of the Omnibox.
const CGFloat kPopupPointYOffset = 2.0;

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
const CGFloat kIconMarginPadding = 2;
const CGFloat kBorderPadding = 3;

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
  [self stopAnimation];
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
    ContentSettingsType settings_type,
    LocationBarViewMac* owner,
    Profile* profile)
    : content_setting_image_model_(
          ContentSettingImageModel::CreateContentSettingImageModel(
              settings_type)),
      owner_(owner),
      profile_(profile),
      text_width_(0.0) {
}

ContentSettingDecoration::~ContentSettingDecoration() {
  // Just in case the timer is still holding onto the animation object, force
  // cleanup so it can't get back to |this|.
  [animation_ stopAnimation];
}

bool ContentSettingDecoration::UpdateFromTabContents(
    TabContents* tab_contents) {
  bool was_visible = IsVisible();
  int old_icon = content_setting_image_model_->get_icon();
  content_setting_image_model_->UpdateFromTabContents(tab_contents);
  SetVisible(content_setting_image_model_->is_visible());
  bool decoration_changed = was_visible != IsVisible() ||
      old_icon != content_setting_image_model_->get_icon();
  if (IsVisible()) {
    // TODO(thakis): We should use pdfs for these icons on OSX.
    // http://crbug.com/35847
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    SetImage(rb.GetNativeImageNamed(content_setting_image_model_->get_icon()));
    SetToolTip(base::SysUTF8ToNSString(
        content_setting_image_model_->get_tooltip()));
    // Check if there is an animation and start it if it hasn't yet started.
    bool has_animated_text =
        content_setting_image_model_->explanatory_string_id();
    // Check if the animation is enabled.
    bool animation_enabled = !CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kDisableBlockContentAnimation);
    if (has_animated_text && animation_enabled && !animation_) {
      // Start animation, its timer will drive reflow. Note the text is
      // cached so it is not allowed to change during the animation.
      animation_.reset(
          [[ContentSettingAnimationState alloc] initWithOwner:this]);
      animated_text_.reset(CreateAnimatedText());
      text_width_ = MeasureTextWidth();
    } else if (!has_animated_text) {
      // Decoration no longer has animation, stop it (ok to always do this).
      [animation_ stopAnimation];
      animation_.reset(nil);
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

// Returns an attributed string with the animated text. Caller is responsible
// for releasing.
NSAttributedString* ContentSettingDecoration::CreateAnimatedText() {
  NSString* text =
      l10n_util::GetNSString(
          content_setting_image_model_->explanatory_string_id());
  NSDictionary* attributes =
      [NSDictionary dictionaryWithObject:[NSFont labelFontOfSize:14]
                                  forKey:NSFontAttributeName];
  NSAttributedString* attr_string =
      [[NSAttributedString alloc] initWithString:text attributes:attributes];
  return attr_string;
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
                     NSMaxY(draw_frame) - kPopupPointYOffset);
}

bool ContentSettingDecoration::AcceptsMousePress() {
  return true;
}

bool ContentSettingDecoration::OnMousePressed(NSRect frame) {
  // Get host. This should be shared on linux/win/osx medium-term.
  TabContents* tabContents =
      BrowserList::GetLastActive()->GetSelectedTabContents();
  if (!tabContents)
    return true;

  // Prerender icon does not include a bubble.
  ContentSettingsType content_settings_type =
      content_setting_image_model_->get_content_settings_type();
  if (content_settings_type == CONTENT_SETTINGS_TYPE_PRERENDER)
    return true;

  GURL url = tabContents->GetURL();
  std::wstring displayHost;
  net::AppendFormattedHost(
      url,
      UTF8ToWide(profile_->GetPrefs()->GetString(prefs::kAcceptLanguages)),
      &displayHost, NULL, NULL);

  // Find point for bubble's arrow in screen coordinates.
  // TODO(shess): |owner_| is only being used to fetch |field|.
  // Consider passing in |control_view|.  Or refactoring to be
  // consistent with other decorations (which don't currently bring up
  // their bubble directly).
  AutocompleteTextField* field = owner_->GetAutocompleteTextField();
  NSPoint anchor = GetBubblePointInFrame(frame);
  anchor = [field convertPoint:anchor toView:nil];
  anchor = [[field window] convertBaseToScreen:anchor];

  // Open bubble.
  ContentSettingBubbleModel* model =
      ContentSettingBubbleModel::CreateContentSettingBubbleModel(
          tabContents, profile_, content_settings_type);
  [ContentSettingBubbleController showForModel:model
                                   parentWindow:[field window]
                                     anchoredAt:anchor];
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
      switch (state) {
        case kOpening:
          preferred_width += text_width_ * kInMotionMultiplier * progress;
          break;
        case kOpen:
          preferred_width += text_width_;
          break;
        case kClosing:
          preferred_width += text_width_ * kInMotionMultiplier * (1 - progress);
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
    // Draw the background. Cache the gradient.
    if (!gradient_) {
      // Colors chosen to match Windows code.
      NSColor* start_color =
          [NSColor colorWithCalibratedRed:1.0 green:0.97 blue:0.83 alpha:1.0];
      NSColor* end_color =
          [NSColor colorWithCalibratedRed:1.0 green:0.90 blue:0.68 alpha:1.0];
      NSArray* color_array =
          [NSArray arrayWithObjects:start_color, end_color, nil];
      gradient_.reset([[NSGradient alloc] initWithColors:color_array]);
    }

    NSGraphicsContext* context = [NSGraphicsContext currentContext];
    [context saveGraphicsState];

    NSRectClip(frame);

    frame = NSInsetRect(frame, 0.0, kBorderPadding);
    [gradient_ drawInRect:frame angle:90.0];
    NSColor* border_color =
        [NSColor colorWithCalibratedRed:0.91 green:0.73 blue:0.4 alpha:1.0];
    [border_color set];
    NSFrameRect(frame);

    // Draw the icon.
    NSImage* icon = GetImage();
    NSRect icon_rect = frame;
    if (icon) {
      icon_rect.origin.x += kIconMarginPadding;
      icon_rect.size.width = [icon size].width;
      ImageDecoration::DrawInFrame(icon_rect, control_view);
    }

    // Draw the text, clipped to fit on the right. While handling clipping,
    // NSAttributedString's drawInRect: won't draw a word if it doesn't fit
    // in the bounding box so instead use drawAtPoint: with a manual clip
    // rect.
    NSRect remainder = frame;
    remainder.origin.x = NSMaxX(icon_rect);
    NSInsetRect(remainder, kTextMarginPadding, kTextMarginPadding);
    // .get() needed to fix compiler warning (confusion with NSImageRep).
    [animated_text_.get() drawAtPoint:remainder.origin];

    [context restoreGraphicsState];
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
  // hides, indicating something has changed with the TabContents (probably
  // navigation).
}
