// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/profiles/profile_chooser_controller.h"

#import <Carbon/Carbon.h>  // kVK_Return.
#import <Cocoa/Cocoa.h>
#include <stddef.h>

#include "base/mac/bundle_locations.h"
#include "base/macros.h"
#include "base/metrics/user_metrics.h"
#include "base/scoped_observer.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/avatar_menu.h"
#include "chrome/browser/profiles/avatar_menu_observer.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/chrome_signin_helper.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_error_controller_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#import "chrome/browser/ui/cocoa/browser_window_utils.h"
#include "chrome/browser/ui/cocoa/chrome_style.h"
#import "chrome/browser/ui/cocoa/info_bubble_view.h"
#import "chrome/browser/ui/cocoa/info_bubble_window.h"
#include "chrome/browser/ui/cocoa/l10n_util.h"
#include "chrome/browser/ui/cocoa/profiles/signin_view_controller_delegate_mac.h"
#import "chrome/browser/ui/cocoa/profiles/user_manager_mac.h"
#include "chrome/browser/ui/profile_chooser_constants.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/user_manager.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/browser/signin_metrics.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "skia/ext/skia_utils_mac.h"
#import "third_party/google_toolbox_for_mac/src/AppKit/GTMUILocalizerAndLayoutTweaker.h"
#import "ui/base/cocoa/cocoa_base_utils.h"
#import "ui/base/cocoa/controls/blue_label_button.h"
#import "ui/base/cocoa/controls/hyperlink_button_cell.h"
#import "ui/base/cocoa/controls/hyperlink_text_view.h"
#import "ui/base/cocoa/hover_image_button.h"
#include "ui/base/cocoa/window_size_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia_util_mac.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_elider.h"
#include "ui/native_theme/common_theme.h"
#include "ui/native_theme/native_theme.h"

namespace {

// Constants taken from the Windows/Views implementation at:
// chrome/browser/ui/views/profile_chooser_view.cc
const int kMdImageSide = 40;

const CGFloat kFixedMenuWidth = 240.0;
const int kIconImageSide = 18;
const CGFloat kVerticalSpacing = 16.0;
const CGFloat kSmallVerticalSpacing = 10.0;
const CGFloat kRelatedControllVerticalSpacing = 8.0;
const CGFloat kHorizontalSpacing = 16.0;
const CGFloat kTitleFontSize = 15.0;
const CGFloat kTextFontSize = 12.0;
const CGFloat kProfileButtonHeight = 30;
const int kBlueButtonHeight = 30;
const CGFloat kFocusRingLineWidth = 2;

// Fixed size for embedded sign in pages as defined in Gaia.
const CGFloat kFixedGaiaViewWidth = 360;

// Fixed size for the account removal view.
const CGFloat kFixedAccountRemovalViewWidth = 280;

// Fixed size for the switch user view.
const int kFixedSwitchUserViewWidth = 320;

// The tag number for the primary account.
const int kPrimaryProfileTag = -1;

NSImage* CreateProfileImage(const gfx::Image& icon,
                            int imageSize,
                            profiles::AvatarShape shape) {
  return (profiles::GetSizedAvatarIcon(icon, true /* image is a square */,
                                       imageSize, imageSize, shape))
      .ToNSImage();
}

// Updates the window size and position.
void SetWindowSize(NSWindow* window, NSSize size) {
  NSRect frame = [window frame];
  frame.origin.x += frame.size.width - size.width;
  frame.origin.y += frame.size.height - size.height;
  frame.size = size;
  [window setFrame:frame display:YES];
}

NSString* ElideEmail(const std::string& email, CGFloat width) {
  const base::string16 elidedEmail = gfx::ElideText(
      base::UTF8ToUTF16(email), gfx::FontList(), width, gfx::ELIDE_EMAIL);
  return base::SysUTF16ToNSString(elidedEmail);
}

NSString* ElideMessage(const base::string16& message, CGFloat width) {
  return base::SysUTF16ToNSString(
      gfx::ElideText(message, gfx::FontList(), width, gfx::ELIDE_TAIL));
}

// Builds a label with the given |title| anchored at |frame_origin|. Sets the
// text color to |text_color| if not null.
NSTextField* BuildLabel(NSString* title,
                        NSPoint frame_origin,
                        NSColor* text_color) {
  base::scoped_nsobject<NSTextField> label(
      [[NSTextField alloc] initWithFrame:NSZeroRect]);
  [label setStringValue:title];
  [label setEditable:NO];
  [label setAlignment:NSNaturalTextAlignment];
  [label setBezeled:NO];
  [label setFont:[NSFont labelFontOfSize:kTextFontSize]];
  [label setDrawsBackground:NO];
  [label setFrameOrigin:frame_origin];
  [label sizeToFit];

  if (text_color)
    [[label cell] setTextColor:text_color];

  return label.autorelease();
}

// Builds an NSTextView that has the contents set to the specified |message|,
// with a non-underlined |link| inserted at |link_offset|. The view is anchored
// at the specified |frame_origin| and has a fixed |frame_width|.
NSTextView* BuildFixedWidthTextViewWithLink(
    id<NSTextViewDelegate> delegate,
    NSString* message,
    NSString* link,
    int link_offset,
    NSPoint frame_origin,
    CGFloat frame_width) {
  base::scoped_nsobject<HyperlinkTextView> text_view(
      [[HyperlinkTextView alloc] initWithFrame:NSZeroRect]);
  NSColor* link_color = skia::SkColorToCalibratedNSColor(
      chrome_style::GetLinkColor());
  NSMutableString* finalMessage =
      [NSMutableString stringWithFormat:@"%@\n", message];
  [finalMessage insertString:link atIndex:link_offset];
  // Adds a padding row at the bottom, because |boundingRectWithSize| below cuts
  // off the last row sometimes.
  [text_view setMessage:finalMessage
               withFont:[NSFont labelFontOfSize:kTextFontSize]
           messageColor:[NSColor blackColor]];
  [text_view addLinkRange:NSMakeRange(link_offset, [link length])
                  withURL:nil
                linkColor:link_color];

  // Removes the underlining from the link.
  [text_view setLinkTextAttributes:nil];
  NSTextStorage* text_storage = [text_view textStorage];
  NSRange link_range = NSMakeRange(link_offset, [link length]);
  [text_storage addAttribute:NSUnderlineStyleAttributeName
                       value:[NSNumber numberWithInt:NSUnderlineStyleNone]
                       range:link_range];

  NSRect frame = [[text_view attributedString]
      boundingRectWithSize:NSMakeSize(frame_width, 0)
                   options:NSStringDrawingUsesLineFragmentOrigin];
  frame.origin = frame_origin;
  [text_view setFrame:frame];
  [text_view setDelegate:delegate];
  return text_view.autorelease();
}

// Returns the native dialog background color.
NSColor* GetDialogBackgroundColor() {
  return skia::SkColorToCalibratedNSColor(
      ui::NativeTheme::GetInstanceForNativeUi()->GetSystemColor(
          ui::NativeTheme::kColorId_DialogBackground));
}

// Builds a title card with one back button right aligned and one label center
// aligned.
NSView* BuildTitleCard(NSRect frame_rect,
                       const base::string16& message,
                       id back_button_target,
                       SEL back_button_action) {
  base::scoped_nsobject<NSView> container(
      [[NSView alloc] initWithFrame:frame_rect]);

  base::scoped_nsobject<HoverImageButton> button(
      [[HoverImageButton alloc] initWithFrame:frame_rect]);
  [button setBordered:NO];
  ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();
  [button setDefaultImage:rb->GetNativeImageNamed(IDR_BACK).ToNSImage()];
  [button setHoverImage:rb->GetNativeImageNamed(IDR_BACK_H).ToNSImage()];
  [button setPressedImage:rb->GetNativeImageNamed(IDR_BACK_P).ToNSImage()];
  [button setTarget:back_button_target];
  [button setAction:back_button_action];
  [button setFrameSize:NSMakeSize(kProfileButtonHeight, kProfileButtonHeight)];
  [button setFrameOrigin:NSMakePoint(kHorizontalSpacing, 0)];

  CGFloat max_label_width = frame_rect.size.width -
      (kHorizontalSpacing * 2 + kProfileButtonHeight) * 2;
  NSTextField* title_label = BuildLabel(
      ElideMessage(message, max_label_width),
      NSZeroPoint, nil);
  [title_label setAlignment:NSCenterTextAlignment];
  [title_label setFont:[NSFont labelFontOfSize:kTitleFontSize]];
  [title_label sizeToFit];
  CGFloat x_offset = (frame_rect.size.width - NSWidth([title_label frame])) / 2;
  CGFloat y_offset =
      (NSHeight([button frame]) - NSHeight([title_label frame])) / 2;
  [title_label setFrameOrigin:NSMakePoint(x_offset, y_offset)];

  [container addSubview:button];
  [container addSubview:title_label];
  CGFloat height = std::max(NSMaxY([title_label frame]),
                            NSMaxY([button frame])) + kVerticalSpacing;
  [container setFrameSize:NSMakeSize(NSWidth([container frame]), height)];

  return container.autorelease();
}

bool HasAuthError(Profile* profile) {
  const SigninErrorController* error_controller =
      SigninErrorControllerFactory::GetForProfile(profile);
  return error_controller && error_controller->HasError();
}

std::string GetAuthErrorAccountId(Profile* profile) {
  const SigninErrorController* error_controller =
      SigninErrorControllerFactory::GetForProfile(profile);
  if (!error_controller)
    return std::string();

  return error_controller->error_account_id();
}

}  // namespace

// Custom WebContentsDelegate that allows handling of hotkeys and suppresses
// the context menu.
class GaiaWebContentsDelegate : public content::WebContentsDelegate {
 public:
  GaiaWebContentsDelegate() {}
  ~GaiaWebContentsDelegate() override {}

 private:
  // content::WebContentsDelegate:
  bool HandleContextMenu(const content::ContextMenuParams& params) override;
  void HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override;

  DISALLOW_COPY_AND_ASSIGN(GaiaWebContentsDelegate);
};

bool GaiaWebContentsDelegate::HandleContextMenu(
    const content::ContextMenuParams& params) {
  // Suppresses the context menu because some features, such as inspecting
  // elements, are not appropriate in a bubble.
  return true;
}

void GaiaWebContentsDelegate::HandleKeyboardEvent(
    content::WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  if (![BrowserWindowUtils shouldHandleKeyboardEvent:event])
    return;

  int chrome_command_id = [BrowserWindowUtils getCommandId:event];

  bool is_text_editing_command = [BrowserWindowUtils isTextEditingEvent:event];

  // TODO(guohui): maybe should add an accelerator for the back button.
  if (chrome_command_id == IDC_CLOSE_WINDOW || chrome_command_id == IDC_EXIT ||
      is_text_editing_command) {
    [[NSApp mainMenu] performKeyEquivalent:event.os_event];
  }
}

// Class that listens to changes to the OAuth2Tokens for the active profile,
// changes to the avatar menu model or browser close notifications.
class ActiveProfileObserverBridge : public AvatarMenuObserver,
                                    public chrome::BrowserListObserver,
                                    public OAuth2TokenService::Observer {
 public:
  ActiveProfileObserverBridge(ProfileChooserController* controller,
                              Browser* browser)
      : controller_(controller),
        browser_(browser),
        browser_list_observer_(this),
        token_observer_registered_(false) {
    browser_list_observer_.Add(BrowserList::GetInstance());
    if (!browser_->profile()->IsGuestSession())
      AddTokenServiceObserver();
  }

  ~ActiveProfileObserverBridge() override { RemoveTokenServiceObserver(); }

 private:
  void AddTokenServiceObserver() {
    ProfileOAuth2TokenService* oauth2_token_service =
        ProfileOAuth2TokenServiceFactory::GetForProfile(browser_->profile());
    DCHECK(oauth2_token_service);
    oauth2_token_service->AddObserver(this);
    token_observer_registered_ = true;
  }

  void RemoveTokenServiceObserver() {
    if (!token_observer_registered_)
      return;
    DCHECK(browser_->profile());
    ProfileOAuth2TokenService* oauth2_token_service =
        ProfileOAuth2TokenServiceFactory::GetForProfile(browser_->profile());
    DCHECK(oauth2_token_service);
    oauth2_token_service->RemoveObserver(this);
    token_observer_registered_ = false;
  }

  // OAuth2TokenService::Observer:
  void OnRefreshTokenAvailable(const std::string& account_id) override {
    // Tokens can only be added by adding an account through the inline flow,
    // which is started from the account management view. Refresh it to show the
    // update.
    profiles::BubbleViewMode viewMode = [controller_ viewMode];
    if (viewMode == profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT ||
        viewMode == profiles::BUBBLE_VIEW_MODE_GAIA_ADD_ACCOUNT ||
        viewMode == profiles::BUBBLE_VIEW_MODE_GAIA_REAUTH) {
      [controller_ showMenuWithViewMode:
                       signin::IsAccountConsistencyMirrorEnabled()
                           ? profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT
                           : profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER];
    }
  }

  void OnRefreshTokenRevoked(const std::string& account_id) override {
    // Tokens can only be removed from the account management view. Refresh it
    // to show the update.
    if ([controller_ viewMode] == profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT)
      [controller_
          showMenuWithViewMode:profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT];
  }

  // AvatarMenuObserver:
  void OnAvatarMenuChanged(AvatarMenu* avatar_menu) override {
    profiles::BubbleViewMode viewMode = [controller_ viewMode];
    if (viewMode == profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER ||
        viewMode == profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT) {
      [controller_ showMenuWithViewMode:viewMode];
    }
  }

  // chrome::BrowserListObserver:
  void OnBrowserClosing(Browser* browser) override {
    if (browser_ == browser) {
      RemoveTokenServiceObserver();
      // Clean up the bubble's WebContents (used by the Gaia embedded view), to
      // make sure the guest profile doesn't have any dangling host renderers.
      // This can happen if Chrome is quit using Command-Q while the bubble is
      // still open, which won't give the bubble a chance to be closed and
      // clean up the WebContents itself.
      [controller_ cleanUpEmbeddedViewContents];
    }
  }

  ProfileChooserController* controller_;  // Weak; owns this.
  Browser* browser_;  // Weak.
  ScopedObserver<BrowserList, BrowserListObserver> browser_list_observer_;

  // The observer can be removed both when closing the browser, and by just
  // closing the avatar bubble. However, in the case of closing the browser,
  // the avatar bubble will also be closed afterwards, resulting in a second
  // attempt to remove the observer. This ensures the observer is only
  // removed once.
  bool token_observer_registered_;

  DISALLOW_COPY_AND_ASSIGN(ActiveProfileObserverBridge);
};

// Custom button cell that adds a leading padding before the button image, and
// a custom spacing between the button image and title.
@interface CustomPaddingImageButtonCell : NSButtonCell {
 @private
  // Padding added to the leading margin of the button.
  int leadingMarginSpacing_;
  // Spacing between the cell image and title.
  int imageTitleSpacing_;
  // Padding added to the traling margin of the button.
  int trailingMarginSpacing_;
}

- (id)initWithLeadingMarginSpacing:(int)leadingMarginSpacing
                 imageTitleSpacing:(int)imageTitleSpacing;

- (void)setTrailingMarginSpacing:(int)trailingMarginSpacing;
@end

@implementation CustomPaddingImageButtonCell
- (id)initWithLeadingMarginSpacing:(int)leadingMarginSpacing
                 imageTitleSpacing:(int)imageTitleSpacing {
  if ((self = [super init])) {
    leadingMarginSpacing_ = leadingMarginSpacing;
    imageTitleSpacing_ = imageTitleSpacing;
  }
  return self;
}

- (void)setTrailingMarginSpacing:(int)trailingMarginSpacing {
  trailingMarginSpacing_ = trailingMarginSpacing;
}

- (NSRect)drawTitle:(NSAttributedString*)title
          withFrame:(NSRect)frame
             inView:(NSView*)controlView {
  NSRect marginRect;
  NSDivideRect(frame, &marginRect, &frame, leadingMarginSpacing_,
               cocoa_l10n_util::LeadingEdge());
  // The title frame origin isn't aware of the leading margin spacing added
  // in -drawImage, so it must be added when drawing the title as well.
  NSDivideRect(frame, &marginRect, &frame, imageTitleSpacing_,
               cocoa_l10n_util::LeadingEdge());
  NSDivideRect(frame, &marginRect, &frame, trailingMarginSpacing_,
               cocoa_l10n_util::TrailingEdge());

  return [super drawTitle:title withFrame:frame inView:controlView];
}

- (void)drawImage:(NSImage*)image
        withFrame:(NSRect)frame
           inView:(NSView*)controlView {
  if (cocoa_l10n_util::ShouldDoExperimentalRTLLayout())
    frame.origin.x -= leadingMarginSpacing_;
  else
    frame.origin.x = leadingMarginSpacing_;
  [super drawImage:image withFrame:frame inView:controlView];
}

- (NSSize)cellSize {
  NSSize buttonSize = [super cellSize];
  buttonSize.width += leadingMarginSpacing_;
  buttonSize.width += imageTitleSpacing_;
  return buttonSize;
}

- (NSFocusRingType)focusRingType {
  // This is taken care of by the custom drawing code.
  return NSFocusRingTypeNone;
}

- (void)drawWithFrame:(NSRect)frame inView:(NSView *)controlView {
  [super drawInteriorWithFrame:frame inView:controlView];

  // Focus ring.
  if ([self showsFirstResponder]) {
    NSRect focusRingRect =
        NSInsetRect(frame, kFocusRingLineWidth, kFocusRingLineWidth);
    // TODO(noms): When we are targetting 10.7, we should change this to use
    // -drawFocusRingMaskWithFrame instead.
    [[[NSColor keyboardFocusIndicatorColor] colorWithAlphaComponent:1] set];
    NSBezierPath* path = [NSBezierPath bezierPathWithRect:focusRingRect];
    [path setLineWidth:kFocusRingLineWidth];
    [path stroke];
  }
}

@end

// A custom image view that has a transparent backround.
@interface TransparentBackgroundImageView : NSImageView
@end

@implementation TransparentBackgroundImageView
- (void)drawRect:(NSRect)dirtyRect {
  NSColor* backgroundColor = [NSColor colorWithCalibratedWhite:1 alpha:0.6f];
  [backgroundColor setFill];
  NSRectFillUsingOperation(dirtyRect, NSCompositeSourceAtop);
  [super drawRect:dirtyRect];
}
@end

@interface CustomCircleImageCell : NSButtonCell
@end

@implementation CustomCircleImageCell
- (void)drawWithFrame:(NSRect)frame inView:(NSView *)controlView {
  // Display everything as a circle that spans the entire control.
  NSBezierPath* path = [NSBezierPath bezierPathWithOvalInRect:frame];
  [path addClip];
  [super drawImage:[self image] withFrame:frame inView:controlView];
}
@end

// A custom view with a filled circular background.
@interface BackgroundCircleView : NSView {
 @private
  base::scoped_nsobject<NSColor> fillColor_;
}
@end

@implementation BackgroundCircleView
- (id)initWithFrame:(NSRect)frameRect withFillColor:(NSColor*)fillColor {
  if ((self = [super initWithFrame:frameRect]))
    fillColor_.reset([fillColor retain]);
  return self;
}

- (void)drawRect:(NSRect)dirtyRect {
  [fillColor_ setFill];
  NSBezierPath* circlePath = [NSBezierPath bezierPath];
  [circlePath appendBezierPathWithOvalInRect:[self bounds]];
  [circlePath fill];

  [super drawRect:dirtyRect];
}
@end

// A custom button that allows for setting a background color when hovered over.
@interface BackgroundColorHoverButton : HoverImageButton {
 @private
  base::scoped_nsobject<NSColor> backgroundColor_;
  base::scoped_nsobject<NSColor> hoverColor_;
}

- (void)setTrailingMarginSpacing:(int)trailingMarginSpacing;
@end

@implementation BackgroundColorHoverButton

- (id)initWithFrame:(NSRect)frameRect
  imageTitleSpacing:(int)imageTitleSpacing
    backgroundColor:(NSColor*)backgroundColor {
  if ((self = [super initWithFrame:frameRect])) {
    backgroundColor_.reset([backgroundColor retain]);
    hoverColor_.reset([skia::SkColorToSRGBNSColor(profiles::kHoverColor)
                          retain]);

    [self setBordered:NO];
    [self setFont:[NSFont labelFontOfSize:kTextFontSize]];
    [self setButtonType:NSMomentaryChangeButton];

    base::scoped_nsobject<CustomPaddingImageButtonCell> cell(
        [[CustomPaddingImageButtonCell alloc]
            initWithLeadingMarginSpacing:kHorizontalSpacing
                       imageTitleSpacing:imageTitleSpacing]);
    [cell setLineBreakMode:NSLineBreakByTruncatingTail];
    [cell setHighlightsBy:NSNoCellMask];
    [self setCell:cell.get()];
  }
  return self;
}

- (void)setTrailingMarginSpacing:(int)trailingMarginSpacing {
  [[self cell] setTrailingMarginSpacing:trailingMarginSpacing];
}

- (void)drawRect:(NSRect)dirtyRect {
  if ([self isEnabled]) {
    bool isHighlighted = ([self hoverState] != kHoverStateNone);
    NSColor* backgroundColor = isHighlighted ? hoverColor_ : backgroundColor_;
    [[self cell] setBackgroundColor:backgroundColor];
  }
  [super drawRect:dirtyRect];
}

-(void)keyDown:(NSEvent*)event {
  // Since there is no default button in the bubble, it is safe to activate
  // all buttons on Enter as well, and be consistent with the Windows
  // implementation.
  if ([event keyCode] == kVK_Return || [event keyCode] == kVK_ANSI_KeypadEnter)
    [self performClick:self];
  else
    [super keyDown:event];
}

- (BOOL)canBecomeKeyView {
  return [self isEnabled] ? YES : NO;
}

@end

// A custom view with the given background color.
@interface BackgroundColorView : NSView {
 @private
  base::scoped_nsobject<NSColor> backgroundColor_;
}
@end

@implementation BackgroundColorView
- (id)initWithFrame:(NSRect)frameRect
          withColor:(NSColor*)color {
  if ((self = [super initWithFrame:frameRect]))
    backgroundColor_.reset([color retain]);
  return self;
}

- (void)drawRect:(NSRect)dirtyRect {
  [backgroundColor_ setFill];
  NSRectFill(dirtyRect);
  [super drawRect:dirtyRect];
}
@end

// A custom dummy button that is used to clear focus from the bubble's controls.
@interface DummyWindowFocusButton : NSButton
@end

@implementation DummyWindowFocusButton
// Ignore accessibility, as this is a placeholder button.
- (BOOL)accessibilityIsIgnored {
  return YES;
}

- (id)accessibilityAttributeValue:(NSString*)attribute {
  return nil;
}

- (BOOL)canBecomeKeyView {
  return NO;
}

@end

@interface ProfileChooserController ()
// Adds an horizontal separator to |container| at |yOffset| and returns the
// yOffset corresponding to after the separator.
- (CGFloat)addSeparatorToContainer:(NSView*)container
                         atYOffset:(CGFloat)yOffset;

// Builds the fast user switcher view. This appears as part of the user menu.
// Returns the yOffset corresponding to after the profile switcher buttons.
- (CGFloat)buildFastUserSwitcherViewWithProfiles:(NSArray*)otherProfiles
                                       atYOffset:(CGFloat)yOffset
                                     inContainer:(NSView*)container;

// Builds the regular profile chooser view.
- (void)buildProfileChooserViewWithProfileView:(NSView*)currentProfileView
                                 syncErrorView:(NSView*)syncErrorView
                                 otherProfiles:(NSArray*)otherProfiles
                                     atYOffset:(CGFloat)yOffset
                                   inContainer:(NSView*)container
                                      showLock:(bool)showLock;

// Builds the profile chooser view.
- (NSView*)buildProfileChooserView;

// Builds a header for signin and sync error surfacing on the user menu.
- (NSView*)buildSyncErrorViewIfNeeded;

// Creates the main profile card for the profile |item| at the top of
// the bubble.
- (NSView*)createCurrentProfileView:(const AvatarMenu::Item&)item;

// Creates the possible links for the main profile card with profile |item|.
- (NSView*)createCurrentProfileLinksForItem:(const AvatarMenu::Item&)item
                                       rect:(NSRect)rect;

// Creates the disclaimer text for supervised users, telling them that the
// manager can view their history etc.
- (NSView*)createSupervisedUserDisclaimerView;

// Creates a main profile card for the guest user.
- (NSView*)createGuestProfileView;

// Creates an item for the profile |itemIndex| that is used in the fast profile
// switcher view.
- (NSButton*)createOtherProfileView:(int)itemIndex;

// Creates the following option buttons: lock profile/close all windows, switch
// user/exit guest, and open guest profile.
- (NSView*)createOptionsViewWithFrame:(NSRect)frame showLock:(BOOL)showLock;

// Creates the account management view for the active profile.
- (NSView*)createCurrentProfileAccountsView:(NSRect)rect;

// Creates the list of accounts for the active profile.
- (NSView*)createAccountsListWithRect:(NSRect)rect;

// Creates the Gaia sign-in/add account view.
- (NSView*)buildGaiaEmbeddedView;

// Creates the account removal view.
- (NSView*)buildAccountRemovalView;

// Create a view that shows various options for an upgrade user who is not
// the same person as the currently signed in user.
- (NSView*)buildSwitchUserView;

// Creates a button with |text| and |action|, optionally with an icon given by
// |imageResourceId| or |image|.
- (NSButton*)hoverButtonWithRect:(NSRect)rect
                            text:(NSString*)text
                 imageResourceId:(int)imageResourceId
                          action:(SEL)action;
- (NSButton*)hoverButtonWithRect:(NSRect)rect
                            text:(NSString*)text
                           image:(NSImage*)image
                          action:(SEL)action;
- (BackgroundColorHoverButton*)hoverButtonWithRect:(NSRect)rect
                                              text:(NSString*)text
                                            action:(SEL)action;

// Creates a generic link button with |title| and an |action| positioned at
// |frameOrigin|.
- (NSButton*)linkButtonWithTitle:(NSString*)title
                     frameOrigin:(NSPoint)frameOrigin
                          action:(SEL)action;

// Creates an email account button with |title| and a remove icon. If
// |reauthRequired| is true, the button also displays a warning icon. |tag|
// indicates which account the button refers to.
- (NSButton*)accountButtonWithRect:(NSRect)rect
                         accountId:(const std::string&)accountId
                               tag:(int)tag
                    reauthRequired:(BOOL)reauthRequired;

- (bool)shouldShowGoIncognito;
@end

@implementation ProfileChooserController
- (profiles::BubbleViewMode) viewMode {
  return viewMode_;
}

- (IBAction)editProfile:(id)sender {
  avatarMenu_->EditProfile(avatarMenu_->GetActiveProfileIndex());
  [self postActionPerformed:ProfileMetrics::PROFILE_DESKTOP_MENU_EDIT_IMAGE];
  [self postActionPerformed:ProfileMetrics::PROFILE_DESKTOP_MENU_EDIT_NAME];
}

- (IBAction)switchToProfile:(id)sender {
  // Check the event flags to see if a new window should be created.
  bool alwaysCreate =
      ui::WindowOpenDispositionFromNSEvent([NSApp currentEvent]) ==
      WindowOpenDisposition::NEW_WINDOW;
  avatarMenu_->SwitchToProfile([sender tag], alwaysCreate,
                               ProfileMetrics::SWITCH_PROFILE_ICON);
}

- (IBAction)switchToGuest:(id)sender {
  PrefService* service = g_browser_process->local_state();
  DCHECK(service);
  DCHECK(service->GetBoolean(prefs::kBrowserGuestModeEnabled));
  profiles::SwitchToGuestProfile(ProfileManager::CreateCallback());
}

- (IBAction)showUserManager:(id)sender {
  UserManager::Show(base::FilePath(),
                    profiles::USER_MANAGER_SELECT_PROFILE_NO_ACTION);
  [self postActionPerformed:
      ProfileMetrics::PROFILE_DESKTOP_MENU_OPEN_USER_MANAGER];
}

- (IBAction)exitGuest:(id)sender {
  DCHECK(browser_->profile()->IsGuestSession());
  UserManager::Show(base::FilePath(),
                    profiles::USER_MANAGER_SELECT_PROFILE_NO_ACTION);
  profiles::CloseGuestProfileWindows();
}

- (IBAction)closeAllWindows:(id)sender {
  profiles::CloseProfileWindows(browser_->profile());
}

- (IBAction)goIncognito:(id)sender {
  DCHECK([self shouldShowGoIncognito]);
  chrome::NewIncognitoWindow(browser_);
  [self postActionPerformed:
      ProfileMetrics::PROFILE_DESKTOP_MENU_GO_INCOGNITO];
}

- (IBAction)showAccountManagement:(id)sender {
  [self showMenuWithViewMode:profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT];
}

- (IBAction)hideAccountManagement:(id)sender {
  [self showMenuWithViewMode:profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER];
}

- (IBAction)lockProfile:(id)sender {
  profiles::LockProfile(browser_->profile());
  [self postActionPerformed:ProfileMetrics::PROFILE_DESKTOP_MENU_LOCK];
}

- (void)showSigninUIForMode:(profiles::BubbleViewMode)mode {
  if (SigninViewController::ShouldShowModalSigninForMode(mode)) {
    browser_->signin_view_controller()->ShowModalSignin(mode, browser_,
                                                        accessPoint_);
  } else {
    [self showMenuWithViewMode:mode];
  }
}

- (IBAction)showInlineSigninPage:(id)sender {
  [self showSigninUIForMode:profiles::BUBBLE_VIEW_MODE_GAIA_SIGNIN];
}

- (IBAction)addAccount:(id)sender {
  [self showSigninUIForMode:profiles::BUBBLE_VIEW_MODE_GAIA_ADD_ACCOUNT];
  [self postActionPerformed:ProfileMetrics::PROFILE_DESKTOP_MENU_ADD_ACCT];
}

- (IBAction)navigateBackFromSigninPage:(id)sender {
  std::string primaryAccount = SigninManagerFactory::GetForProfile(
      browser_->profile())->GetAuthenticatedAccountId();
  bool hasAccountManagement =
      !primaryAccount.empty() && signin::IsAccountConsistencyMirrorEnabled();
  [self showMenuWithViewMode:hasAccountManagement
                                 ? profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT
                                 : profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER];
}

- (IBAction)showAccountRemovalView:(id)sender {
  DCHECK(!isGuestSession_);

  // Tag is either |kPrimaryProfileTag| for the primary account, or equal to the
  // index in |currentProfileAccounts_| for a secondary account.
  int tag = [sender tag];
  if (tag == kPrimaryProfileTag) {
    accountIdToRemove_ = SigninManagerFactory::GetForProfile(
        browser_->profile())->GetAuthenticatedAccountId();
  } else {
    DCHECK(base::ContainsKey(currentProfileAccounts_, tag));
    accountIdToRemove_ = currentProfileAccounts_[tag];
  }

  [self showMenuWithViewMode:profiles::BUBBLE_VIEW_MODE_ACCOUNT_REMOVAL];
}

- (IBAction)showSignoutView:(id)sender {
  chrome::ShowSettingsSubPage(browser_, chrome::kSignOutSubPage);
}

- (IBAction)showSignoutSigninView:(id)sender {
  if (ProfileSyncServiceFactory::GetForProfile(browser_->profile()))
    browser_sync::ProfileSyncService::SyncEvent(
        browser_sync::ProfileSyncService::STOP_FROM_OPTIONS);
  SigninManagerFactory::GetForProfile(browser_->profile())
      ->SignOut(signin_metrics::USER_CLICKED_SIGNOUT_SETTINGS,
                signin_metrics::SignoutDelete::IGNORE_METRIC);
  [self showSigninUIForMode:profiles::BUBBLE_VIEW_MODE_GAIA_SIGNIN];
}

- (IBAction)showAccountReauthenticationView:(id)sender {
  DCHECK(!isGuestSession_);
  [self showSigninUIForMode:profiles::BUBBLE_VIEW_MODE_GAIA_REAUTH];
}

- (IBAction)showUpdateChromeView:(id)sender {
  chrome::OpenUpdateChromeDialog(browser_);
}

- (IBAction)showSyncSetupView:(id)sender {
  chrome::ShowSettingsSubPage(browser_, chrome::kSyncSetupSubPage);
}

- (IBAction)removeAccount:(id)sender {
  DCHECK(!accountIdToRemove_.empty());
  ProfileOAuth2TokenServiceFactory::GetForProfile(browser_->profile())
      ->RevokeCredentials(accountIdToRemove_);
  [self postActionPerformed:ProfileMetrics::PROFILE_DESKTOP_MENU_REMOVE_ACCT];
  accountIdToRemove_.clear();

  [self showMenuWithViewMode:profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT];
}

- (IBAction)showSwitchUserView:(id)sender {
  [self showMenuWithViewMode:profiles::BUBBLE_VIEW_MODE_SWITCH_USER];
  ProfileMetrics::LogProfileNewAvatarMenuUpgrade(
      ProfileMetrics::PROFILE_AVATAR_MENU_UPGRADE_NOT_YOU);
}

- (IBAction)showLearnMorePage:(id)sender {
  signin_ui_util::ShowSigninErrorLearnMorePage(browser_->profile());
}

- (IBAction)configureSyncSettings:(id)sender {
  LoginUIServiceFactory::GetForProfile(browser_->profile())->
      SyncConfirmationUIClosed(LoginUIService::CONFIGURE_SYNC_FIRST);
  ProfileMetrics::LogProfileNewAvatarMenuSignin(
      ProfileMetrics::PROFILE_AVATAR_MENU_SIGNIN_SETTINGS);
}

- (IBAction)syncSettingsConfirmed:(id)sender {
  LoginUIServiceFactory::GetForProfile(browser_->profile())->
      SyncConfirmationUIClosed(LoginUIService::SYNC_WITH_DEFAULT_SETTINGS);
  ProfileMetrics::LogProfileNewAvatarMenuSignin(
      ProfileMetrics::PROFILE_AVATAR_MENU_SIGNIN_OK);
  [self showMenuWithViewMode:profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER];
}

- (IBAction)disconnectProfile:(id)sender {
  chrome::ShowSettings(browser_);
  ProfileMetrics::LogProfileNewAvatarMenuNotYou(
      ProfileMetrics::PROFILE_AVATAR_MENU_NOT_YOU_DISCONNECT);
}

- (IBAction)navigateBackFromSwitchUserView:(id)sender {
  [self showMenuWithViewMode:profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER];
  ProfileMetrics::LogProfileNewAvatarMenuNotYou(
      ProfileMetrics::PROFILE_AVATAR_MENU_NOT_YOU_BACK);
}

- (void)windowWillClose:(NSNotification*)notification {
  [super windowWillClose:notification];
}

- (void)moveDown:(id)sender {
  [[self window] selectNextKeyView:self];
}

- (void)moveUp:(id)sender {
  [[self window] selectPreviousKeyView:self];
}

- (void)cleanUpEmbeddedViewContents {
  webContents_.reset();
  webContentsDelegate_.reset();
}

- (id)initWithBrowser:(Browser*)browser
           anchoredAt:(NSPoint)point
             viewMode:(profiles::BubbleViewMode)viewMode
          serviceType:(signin::GAIAServiceType)serviceType
          accessPoint:(signin_metrics::AccessPoint)accessPoint {
  base::scoped_nsobject<InfoBubbleWindow> window([[InfoBubbleWindow alloc]
      initWithContentRect:ui::kWindowSizeDeterminedLater
                styleMask:NSBorderlessWindowMask
                  backing:NSBackingStoreBuffered
                    defer:NO]);

  if ((self = [super initWithWindow:window
                       parentWindow:browser->window()->GetNativeWindow()
                         anchoredAt:point])) {
    browser_ = browser;
    viewMode_ = viewMode;
    observer_.reset(new ActiveProfileObserverBridge(self, browser_));
    serviceType_ = serviceType;
    accessPoint_ = accessPoint;

    avatarMenu_.reset(new AvatarMenu(
        &g_browser_process->profile_manager()->GetProfileAttributesStorage(),
        observer_.get(),
        browser_));
    avatarMenu_->RebuildMenu();

    // Guest profiles do not have a token service.
    isGuestSession_ = browser_->profile()->IsGuestSession();

    // If view mode is PROFILE_CHOOSER but there is an auth error, force
    // ACCOUNT_MANAGEMENT mode.
    if (viewMode_ == profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER &&
        HasAuthError(browser_->profile()) &&
        signin::IsAccountConsistencyMirrorEnabled() &&
        avatarMenu_->GetItemAt(avatarMenu_->GetActiveProfileIndex())
            .signed_in) {
      viewMode_ = profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT;
    }

    [window accessibilitySetOverrideValue:
        l10n_util::GetNSString(IDS_PROFILES_NEW_AVATAR_MENU_ACCESSIBLE_NAME)
                             forAttribute:NSAccessibilityTitleAttribute];
    [window accessibilitySetOverrideValue:
        l10n_util::GetNSString(IDS_PROFILES_NEW_AVATAR_MENU_ACCESSIBLE_NAME)
                             forAttribute:NSAccessibilityHelpAttribute];
    BOOL shouldUseLeadingEdgeForBubble =
        cocoa_l10n_util::ShouldDoExperimentalRTLLayout() &&
        !cocoa_l10n_util::ShouldFlipWindowControlsInRTL();
    [[self bubble]
        setAlignment:shouldUseLeadingEdgeForBubble
                         ? info_bubble::kAlignLeadingEdgeToAnchorEdge
                         : info_bubble::kAlignTrailingEdgeToAnchorEdge];
    [[self bubble] setArrowLocation:info_bubble::kNoArrow];
    [[self bubble] setBackgroundColor:GetDialogBackgroundColor()];
    [self showMenuWithViewMode:viewMode_];
  }

  return self;
}

- (void)showMenuWithViewMode:(profiles::BubbleViewMode)viewToDisplay {
  if (browser_->profile()->IsSupervised() &&
      (viewToDisplay == profiles::BUBBLE_VIEW_MODE_GAIA_ADD_ACCOUNT ||
       viewToDisplay == profiles::BUBBLE_VIEW_MODE_ACCOUNT_REMOVAL)) {
    LOG(WARNING) << "Supervised user attempted to add/remove account";
    return;
  }
  viewMode_ = viewToDisplay;
  NSView* contentView = [[self window] contentView];
  [contentView setSubviews:[NSArray array]];
  NSView* subView;

  switch (viewMode_) {
    case profiles::BUBBLE_VIEW_MODE_GAIA_SIGNIN:
    case profiles::BUBBLE_VIEW_MODE_GAIA_ADD_ACCOUNT:
    case profiles::BUBBLE_VIEW_MODE_GAIA_REAUTH:
      subView = [self buildGaiaEmbeddedView];
      break;
    case profiles::BUBBLE_VIEW_MODE_ACCOUNT_REMOVAL:
      subView = [self buildAccountRemovalView];
      break;
    case profiles::BUBBLE_VIEW_MODE_SWITCH_USER:
      subView = [self buildSwitchUserView];
      break;
    case profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER:
    case profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT:
      subView = [self buildProfileChooserView];
      break;
  }

  // Add a dummy, empty element so that we don't initially display any
  // focus rings.
  NSButton* dummyFocusButton =
     [[[DummyWindowFocusButton alloc] initWithFrame:NSZeroRect] autorelease];
  [dummyFocusButton setNextKeyView:subView];
  [[self window] makeFirstResponder:dummyFocusButton];

  [contentView addSubview:subView];
  [contentView addSubview:dummyFocusButton];
  SetWindowSize([self window],
      NSMakeSize(NSWidth([subView frame]), NSHeight([subView frame])));
}

- (CGFloat)addSeparatorToContainer:(NSView*)container
                         atYOffset:(CGFloat)yOffset {
  NSBox* separator = [self
      horizontalSeparatorWithFrame:NSMakeRect(0, yOffset, kFixedMenuWidth, 0)];
  [container addSubview:separator];
  return NSMaxY([separator frame]);
}

// Builds the fast user switcher view in |container| at |yOffset| and populates
// it with the entries for every profile in |otherProfiles|. Returns the new
// yOffset after adding the elements.
- (CGFloat)buildFastUserSwitcherViewWithProfiles:(NSArray*)otherProfiles
                                       atYOffset:(CGFloat)yOffset
                                     inContainer:(NSView*)container {
  // Other profiles switcher. The profiles have already been sorted
  // by their y-coordinate, so they can be added in the existing order.
  for (NSView* otherProfileView in otherProfiles) {
    [otherProfileView setFrameOrigin:NSMakePoint(0, yOffset)];
    [container addSubview:otherProfileView];
    yOffset = NSMaxY([otherProfileView frame]);
  }

  [container setFrameSize:NSMakeSize(kFixedMenuWidth, yOffset)];
  return yOffset;
}

- (void)buildProfileChooserViewWithProfileView:(NSView*)currentProfileView
                                 syncErrorView:(NSView*)syncErrorView
                                 otherProfiles:(NSArray*)otherProfiles
                                     atYOffset:(CGFloat)yOffset
                                   inContainer:(NSView*)container
                                      showLock:(bool)showLock {
  yOffset += kRelatedControllVerticalSpacing;

  // Option buttons.
  NSRect rect = NSMakeRect(0, yOffset, kFixedMenuWidth, 0);
  NSView* optionsView =
      [self createOptionsViewWithFrame:rect showLock:showLock];
  [container addSubview:optionsView];
  rect.origin.y = NSMaxY([optionsView frame]);
  yOffset = rect.origin.y;

  // Add the fast user switching buttons.
  yOffset = [self buildFastUserSwitcherViewWithProfiles:otherProfiles
                                              atYOffset:yOffset
                                            inContainer:container];
  yOffset += kRelatedControllVerticalSpacing;
  rect.origin.y = yOffset;

  NSBox* separator = [self horizontalSeparatorWithFrame:rect];
  [container addSubview:separator];
  yOffset = NSMaxY([separator frame]);

  // For supervised users, add the disclaimer text.
  if (browser_->profile()->IsSupervised()) {
    yOffset += kVerticalSpacing;
    NSView* disclaimerContainer = [self createSupervisedUserDisclaimerView];
    [disclaimerContainer setFrameOrigin:NSMakePoint(0, yOffset)];
    [container addSubview:disclaimerContainer];
    yOffset = NSMaxY([disclaimerContainer frame]);
  }

  if (viewMode_ == profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT) {
    const AvatarMenu::Item& item =
        avatarMenu_->GetItemAt(avatarMenu_->GetActiveProfileIndex());
    if (item.signed_in) {
      NSView* currentProfileAccountsView = [self
          createCurrentProfileAccountsView:NSMakeRect(0, yOffset,
                                                      kFixedMenuWidth, 0)];
      [container addSubview:currentProfileAccountsView];
      yOffset = NSMaxY([currentProfileAccountsView frame]);

      yOffset = [self addSeparatorToContainer:container atYOffset:yOffset];
    } else {
      // This is the case when the user selects the sign out option in the user
      // menu upon encountering unrecoverable errors. Afterwards, the profile
      // chooser view is shown instead of the account management view.
      viewMode_ = profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER;
    }
  }

  // Active profile card.
  if (currentProfileView) {
    const CGFloat verticalSpacing = kRelatedControllVerticalSpacing;
    yOffset += verticalSpacing;
    [currentProfileView setFrameOrigin:NSMakePoint(0, yOffset)];
    [container addSubview:currentProfileView];
    yOffset = NSMaxY([currentProfileView frame]) + verticalSpacing;
  }

  if (syncErrorView) {
    yOffset = [self addSeparatorToContainer:container atYOffset:yOffset];
    [syncErrorView setFrameOrigin:NSMakePoint(0, yOffset)];
    [container addSubview:syncErrorView];
    yOffset = NSMaxY([syncErrorView frame]);
  }

  [container setFrameSize:NSMakeSize(kFixedMenuWidth, yOffset)];
}

- (NSView*)buildProfileChooserView {
  base::scoped_nsobject<NSView> container(
      [[NSView alloc] initWithFrame:NSZeroRect]);

  NSView* syncErrorView = nil;
  NSView* currentProfileView = nil;
  base::scoped_nsobject<NSMutableArray> otherProfiles(
      [[NSMutableArray alloc] init]);
  // Local and guest profiles cannot lock their profile.
  bool showLock = false;

  // Loop over the profiles in reverse, so that they are sorted by their
  // y-coordinate, and separate them into active and "other" profiles.
  for (int i = avatarMenu_->GetNumberOfItems() - 1; i >= 0; --i) {
    const AvatarMenu::Item& item = avatarMenu_->GetItemAt(i);
    if (item.active) {
      syncErrorView = [self buildSyncErrorViewIfNeeded];
      currentProfileView = [self createCurrentProfileView:item];
      showLock = item.signed_in &&
          profiles::IsLockAvailable(browser_->profile());
    } else {
      [otherProfiles addObject:[self createOtherProfileView:i]];
    }
  }
  firstProfileView_ = [otherProfiles lastObject];
  if (!currentProfileView)  // Guest windows don't have an active profile.
    currentProfileView = [self createGuestProfileView];

  // |yOffset| is the next position at which to draw in |container|
  // coordinates. Add a pixel offset so that the bottom option buttons don't
  // overlap the bubble's rounded corners.
  CGFloat yOffset = 1;

  [self buildProfileChooserViewWithProfileView:currentProfileView
                                 syncErrorView:syncErrorView
                                 otherProfiles:otherProfiles.get()
                                     atYOffset:yOffset
                                   inContainer:container
                                      showLock:showLock];
  return container.autorelease();
}

- (NSView*)buildSyncErrorViewIfNeeded {
  int contentStringId, buttonStringId;
  SEL buttonAction;
  SigninManagerBase* signinManager =
      SigninManagerFactory::GetForProfile(browser_->profile());
  sync_ui_util::AvatarSyncErrorType error =
      sync_ui_util::GetMessagesForAvatarSyncError(
          browser_->profile(), *signinManager, &contentStringId,
          &buttonStringId);
  switch (error) {
    case sync_ui_util::MANAGED_USER_UNRECOVERABLE_ERROR:
      buttonAction = @selector(showSignoutView:);
      break;
    case sync_ui_util::UNRECOVERABLE_ERROR:
      buttonAction = @selector(showSignoutSigninView:);
      break;
    case sync_ui_util::SUPERVISED_USER_AUTH_ERROR:
      buttonAction = nil;
      break;
    case sync_ui_util::AUTH_ERROR:
      buttonAction = @selector(showAccountReauthenticationView:);
      break;
    case sync_ui_util::UPGRADE_CLIENT_ERROR:
      buttonAction = @selector(showUpdateChromeView:);
      break;
    case sync_ui_util::PASSPHRASE_ERROR:
    case sync_ui_util::SETTINGS_UNCONFIRMED_ERROR:
      buttonAction = @selector(showSyncSetupView:);
      break;
    case sync_ui_util::NO_SYNC_ERROR:
      return nil;
    default:
      NOTREACHED();
  }

  base::scoped_nsobject<NSView> container(
      [[NSView alloc] initWithFrame:NSMakeRect(0, 0, kFixedMenuWidth, 0)]);
  CGFloat iconSize = 20.0;
  CGFloat xOffset = kHorizontalSpacing + iconSize + 12.0;
  CGFloat availableWidth = kFixedMenuWidth - xOffset - kHorizontalSpacing;
  CGFloat yOffset = 16.0;

  // Adds an action button for resolving the error at the bottom.
  if (buttonStringId) {
    // If the button string is specified, then the button action needs to be
    // already initialized for the button to be constructed.
    DCHECK(buttonAction);
    base::scoped_nsobject<NSButton> resolveErrorButton(
        [[BlueLabelButton alloc] initWithFrame:NSZeroRect]);
    [resolveErrorButton setTitle:l10n_util::GetNSString(buttonStringId)];
    [resolveErrorButton setTarget:self];
    [resolveErrorButton setAction:buttonAction];
    [resolveErrorButton setAlignment:NSCenterTextAlignment];
    [resolveErrorButton sizeToFit];
    [resolveErrorButton setFrameOrigin:NSMakePoint(xOffset, yOffset + 4.0)];
    [container addSubview:resolveErrorButton];
    yOffset = NSMaxY([resolveErrorButton frame]) + kVerticalSpacing;
  }

  // Adds the error message content.
  NSTextField* contentLabel =
      BuildLabel(l10n_util::GetNSString(contentStringId),
                 NSMakePoint(xOffset, yOffset), nil);
  [contentLabel setFrameSize:NSMakeSize(availableWidth, 0)];
  [GTMUILocalizerAndLayoutTweaker sizeToFitFixedWidthTextField:contentLabel];
  [container addSubview:contentLabel];
  yOffset = NSMaxY([contentLabel frame]) + 4;

  // Adds the title for the error card.
  NSTextField* titleLabel =
      BuildLabel(l10n_util::GetNSString(IDS_SYNC_ERROR_USER_MENU_TITLE),
                 NSMakePoint(xOffset, yOffset),
                 skia::SkColorToCalibratedNSColor(gfx::kGoogleRed700));
  [titleLabel setFrameSize:NSMakeSize(availableWidth, 0)];
  [GTMUILocalizerAndLayoutTweaker sizeToFitFixedWidthTextField:titleLabel];
  [container addSubview:titleLabel];
  yOffset = NSMaxY([titleLabel frame]);

  // Adds the sync problem icon.
  base::scoped_nsobject<NSImageView> syncProblemIcon([[NSImageView alloc]
      initWithFrame:NSMakeRect(kHorizontalSpacing, yOffset - iconSize, iconSize,
                               iconSize)]);
  [syncProblemIcon
      setImage:NSImageFromImageSkia(gfx::CreateVectorIcon(
                   kSyncProblemIcon, iconSize, gfx::kGoogleRed700))];
  [container addSubview:syncProblemIcon];

  [container
      setFrameSize:NSMakeSize(kFixedMenuWidth, yOffset + kVerticalSpacing)];
  return container.autorelease();
}

- (NSView*)createCurrentProfileView:(const AvatarMenu::Item&)item {
  base::scoped_nsobject<NSView> container(
      [[NSView alloc] initWithFrame:NSZeroRect]);

  BOOL isRTL = cocoa_l10n_util::ShouldDoExperimentalRTLLayout();
  CGFloat xOffset = kHorizontalSpacing;
  CGFloat yOffset = 0.0;
  CGFloat cardYOffset = kRelatedControllVerticalSpacing;
  CGFloat availableTextWidth =
      kFixedMenuWidth - 3.0 * kHorizontalSpacing - kMdImageSide;
  CGFloat maxAvailableTextWidth = kFixedMenuWidth - kHorizontalSpacing;

  // Profile options. This can be a link to the accounts view, or a "Sign in"
  // button for local profiles.
  SigninManagerBase* signinManager = SigninManagerFactory::GetForProfile(
      browser_->profile()->GetOriginalProfile());
  NSRect profileLinksBound = NSZeroRect;
  if (item.signed_in && signin::IsAccountConsistencyMirrorEnabled()) {
    profileLinksBound = NSMakeRect(0, 0, kFixedMenuWidth, kVerticalSpacing);
  } else if (!item.signed_in && signinManager->IsSigninAllowed()) {
    profileLinksBound = NSMakeRect(xOffset, kRelatedControllVerticalSpacing,
                                   maxAvailableTextWidth, kVerticalSpacing);
  }
  if (!NSIsEmptyRect(profileLinksBound)) {
    NSView* linksContainer =
        [self createCurrentProfileLinksForItem:item rect:profileLinksBound];
    [container addSubview:linksContainer];
    yOffset = NSMaxY([linksContainer frame]);
  }

  // Profile card button that contains the profile icon, name, and username.
  const base::string16 profileNameString =
      profiles::GetAvatarNameForProfile(browser_->profile()->GetPath());
  NSRect rect =
      NSMakeRect(0, yOffset, kFixedMenuWidth, kMdImageSide + kVerticalSpacing);
  NSButton* profileCard =
      [self hoverButtonWithRect:rect
                           text:[[NSString alloc] init]
                          image:CreateProfileImage(item.icon, kMdImageSide,
                                                   profiles::SHAPE_CIRCLE)
                         action:@selector(editProfile:)];
  [[profileCard cell] setImageDimsWhenDisabled:NO];
  if (item.signed_in) {
    [[profileCard cell]
        accessibilitySetOverrideValue:
            l10n_util::GetNSStringF(
                IDS_PROFILES_EDIT_SIGNED_IN_PROFILE_ACCESSIBLE_NAME,
                profileNameString, item.username)
                         forAttribute:NSAccessibilityTitleAttribute];
  } else {
    [[profileCard cell]
        accessibilitySetOverrideValue:
            l10n_util::GetNSStringF(IDS_PROFILES_EDIT_PROFILE_ACCESSIBLE_NAME,
                                    profileNameString)
                         forAttribute:NSAccessibilityTitleAttribute];
  }
  [container addSubview:profileCard];
  if (isGuestSession_)
    [profileCard setEnabled:NO];

  // Profile badge for supervised account.
  if (browser_->profile()->IsSupervised()) {
    // Draw a circle as the background of the badge icon.
    constexpr int badgeSize = 24;
    constexpr int badgeSpacing = 4;
    NSRect badgeIconCircleFrame =
        NSMakeRect(xOffset + kMdImageSide - badgeSize + badgeSpacing,
                   cardYOffset + kMdImageSide - badgeSize + badgeSpacing,
                   badgeSize, badgeSize);
    if (isRTL) {
      badgeIconCircleFrame.origin.x = NSWidth([profileCard frame]) -
                                      NSWidth(badgeIconCircleFrame) -
                                      NSMinX(badgeIconCircleFrame);
    }
    base::scoped_nsobject<BackgroundCircleView> badgeIconWithCircle([
        [BackgroundCircleView alloc] initWithFrame:badgeIconCircleFrame
                                     withFillColor:GetDialogBackgroundColor()]);
    // Add the badge icon.
    constexpr int borderWidth = 1;
    const int badgeIconSize = badgeSize - borderWidth * 2;
    base::scoped_nsobject<NSImageView> badgeIconView([[NSImageView alloc]
        initWithFrame:NSMakeRect(borderWidth, borderWidth,
                                 badgeIconSize, badgeIconSize)]);
    const gfx::VectorIcon& badgeIcon = browser_->profile()->IsChild()
                                           ? kAccountChildCircleIcon
                                           : kSupervisorAccountCircleIcon;
    [badgeIconView
        setImage:NSImageFromImageSkia(gfx::CreateVectorIcon(
                     badgeIcon, badgeIconSize, gfx::kChromeIconGrey))];
    [badgeIconWithCircle addSubview:badgeIconView];

    [profileCard addSubview:badgeIconWithCircle];
  }
  if (!isRTL)
    xOffset += kMdImageSide + kHorizontalSpacing;
  CGFloat fontSize = kTextFontSize + 1.0;
  NSString* profileNameNSString = base::SysUTF16ToNSString(profileNameString);
  NSTextField* profileName = BuildLabel(profileNameNSString, NSZeroPoint, nil);
  [[profileName cell] setLineBreakMode:NSLineBreakByTruncatingTail];
  [profileName setFont:[NSFont labelFontOfSize:fontSize]];
  [profileName sizeToFit];
  const int profileNameYOffset =
      cardYOffset +
      std::floor((kMdImageSide - NSHeight([profileName frame])) / 2);
  if (profileName.frame.size.width > availableTextWidth) {
    // Add the tooltip only if the profile name is truncated. This method to
    // test if text field is truncated is not ideal (spaces between characters
    // may be reduced to avoid truncation).
    profileName.toolTip = profileNameNSString;
  }
  [profileName
      setFrame:NSMakeRect(xOffset, profileNameYOffset, availableTextWidth,
                          NSHeight([profileName frame]))];
  [profileCard addSubview:profileName];

  // Username, aligned to the leading edge of the  profile icon and
  // below the profile name.
  if (item.signed_in && !signin::IsAccountConsistencyMirrorEnabled()) {
    // Adjust the y-position of profile name to leave space for username.
    cardYOffset += kMdImageSide / 2 - [profileName frame].size.height;
    [profileName setFrameOrigin:NSMakePoint(xOffset, cardYOffset)];

    NSString* elidedEmail =
        ElideEmail(base::UTF16ToUTF8(item.username), availableTextWidth);
    NSTextField* username = BuildLabel(
        elidedEmail, NSZeroPoint, skia::SkColorToSRGBNSColor(SK_ColorGRAY));
    CGFloat usernameOffset =
        isRTL ? NSMaxX([profileName frame]) - NSWidth([username frame])
              : xOffset;
    [username setFrameOrigin:NSMakePoint(usernameOffset,
                                         NSMaxY([profileName frame]))];
    NSString* usernameNSString = base::SysUTF16ToNSString(item.username);
    if (![elidedEmail isEqualToString:usernameNSString]) {
      // Add the tooltip only if the user name is truncated.
      username.toolTip = usernameNSString;
    }
    [profileCard addSubview:username];
  }

  yOffset = NSMaxY([profileCard frame]);
  [container setFrameSize:NSMakeSize(kFixedMenuWidth, yOffset)];
  return container.autorelease();
}

- (NSView*)createCurrentProfileLinksForItem:(const AvatarMenu::Item&)item
                                       rect:(NSRect)rect {
  // The branch is empty in non-account-consistency mode, because in that case,
  // the username would appear in the profile card instead of as a separate link
  // here.
  SigninManagerBase* signinManager = SigninManagerFactory::GetForProfile(
      browser_->profile()->GetOriginalProfile());
  DCHECK((item.signed_in && signin::IsAccountConsistencyMirrorEnabled()) ||
         (!item.signed_in && signinManager->IsSigninAllowed()));

  base::scoped_nsobject<NSView> container([[NSView alloc] initWithFrame:rect]);

  // Don't double-apply the left margin to the sub-views.
  rect.origin.x = 0;

  // Adds right padding.
  const CGFloat kRightPadding = kHorizontalSpacing;
  rect.size.width -= kRightPadding;

  // The available links depend on the type of profile that is active.
  if (item.signed_in) {
    NSButton* link = nil;
    if (signin::IsAccountConsistencyMirrorEnabled()) {
      NSString* linkTitle = l10n_util::GetNSString(
          viewMode_ == profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER ?
              IDS_PROFILES_PROFILE_MANAGE_ACCOUNTS_BUTTON :
              IDS_PROFILES_PROFILE_HIDE_MANAGE_ACCOUNTS_BUTTON);
      SEL linkSelector =
          (viewMode_ == profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER) ?
          @selector(showAccountManagement:) : @selector(hideAccountManagement:);
      rect.size.width += kRightPadding;  // Spans the width of the entire menu.
      link = [self hoverButtonWithRect:NSMakeRect(0, 0, rect.size.width,
                                                  kBlueButtonHeight)
                                  text:linkTitle
                                action:linkSelector];
    }
    if (link) {
      // -linkButtonWithTitle sizeToFit's the link. We can use the height, but
      // need to re-stretch the width so that the link can be centered correctly
      // in the view.
      rect.size.height = [link frame].size.height;
      [link setFrame:rect];
      [container addSubview:link];
      [container setFrameSize:rect.size];
    }
  } else {
    rect.size.height = kBlueButtonHeight;
    NSButton* signinButton = [[BlueLabelButton alloc] initWithFrame:rect];

    // Manually elide the button text so that the contents fit inside the bubble
    // This is needed because the BlueLabelButton cell resets the style on
    // every call to -cellSize, which prevents setting a custom lineBreakMode.
    NSString* elidedButtonText = base::SysUTF16ToNSString(gfx::ElideText(
        l10n_util::GetStringFUTF16(
            IDS_SYNC_START_SYNC_BUTTON_LABEL,
            l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME)),
        gfx::FontList(), rect.size.width, gfx::ELIDE_TAIL));

    [signinButton setTitle:elidedButtonText];
    [signinButton sizeToFit];
    [signinButton setTarget:self];
    [signinButton setAction:@selector(showInlineSigninPage:)];
    if (cocoa_l10n_util::ShouldDoExperimentalRTLLayout()) {
      NSRect buttonFrame = [signinButton frame];
      buttonFrame.origin.x = NSWidth(rect) - NSWidth(buttonFrame);
      [signinButton setFrame:buttonFrame];
    }
    [container addSubview:signinButton];

    // Sign-in promo text.
    NSTextField* promo = BuildLabel(
        l10n_util::GetNSString(IDS_PROFILES_SIGNIN_PROMO),
        NSMakePoint(0, NSMaxY([signinButton frame]) + kVerticalSpacing),
        nil);
    if (kRightPadding >= 8 &&
        !cocoa_l10n_util::ShouldDoExperimentalRTLLayout()) {
      rect.size.width += 8;  // Re-stretch a little bit to fit promo text.
    }
    DCHECK(kRightPadding >= 8);
    [promo setFrameSize:NSMakeSize(rect.size.width, 0)];
    [GTMUILocalizerAndLayoutTweaker sizeToFitFixedWidthTextField:promo];
    [container addSubview:promo];

    [container setFrameSize:NSMakeSize(rect.size.width,
                                       NSMaxY([promo frame]) +
                                           kRelatedControllVerticalSpacing)];
    base::RecordAction(
        base::UserMetricsAction("Signin_Impression_FromAvatarBubbleSignin"));
  }

  return container.autorelease();
}

- (NSView*)createSupervisedUserDisclaimerView {
  base::scoped_nsobject<NSView> container(
      [[NSView alloc] initWithFrame:NSZeroRect]);

  int yOffset = 0;
  int availableTextWidth = kFixedMenuWidth - 2 * kHorizontalSpacing;

  NSTextField* disclaimer = BuildLabel(
      base::SysUTF16ToNSString(avatarMenu_->GetSupervisedUserInformation()),
      NSMakePoint(kHorizontalSpacing, yOffset), nil);
  [disclaimer setFrameSize:NSMakeSize(availableTextWidth, 0)];
  [GTMUILocalizerAndLayoutTweaker sizeToFitFixedWidthTextField:disclaimer];
  yOffset = NSMaxY([disclaimer frame]);

  [container addSubview:disclaimer];
  [container setFrameSize:NSMakeSize(kFixedMenuWidth, yOffset)];
  return container.autorelease();
}

- (NSView*)createGuestProfileView {
  gfx::Image guestIcon =
      ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(
          profiles::GetPlaceholderAvatarIconResourceID());
  AvatarMenu::Item guestItem(std::string::npos, /* menu_index, not used */
                             base::FilePath(), /* profile_path, not used */
                             guestIcon);
  guestItem.active = true;
  guestItem.name = base::SysNSStringToUTF16(
      l10n_util::GetNSString(IDS_PROFILES_GUEST_PROFILE_NAME));

  return [self createCurrentProfileView:guestItem];
}

- (NSButton*)createOtherProfileView:(int)itemIndex {
  const AvatarMenu::Item& item = avatarMenu_->GetItemAt(itemIndex);

  NSRect rect = NSMakeRect(0, 0, kFixedMenuWidth,
                           kBlueButtonHeight + kSmallVerticalSpacing);
  const int imageTitleSpacing = kHorizontalSpacing;
  base::scoped_nsobject<BackgroundColorHoverButton> profileButton(
      [[BackgroundColorHoverButton alloc]
              initWithFrame:rect
          imageTitleSpacing:imageTitleSpacing
            backgroundColor:GetDialogBackgroundColor()]);
  [profileButton setTrailingMarginSpacing:kHorizontalSpacing];

  NSString* title = base::SysUTF16ToNSString(
      profiles::GetProfileSwitcherTextForItem(item));
  [profileButton setTitle:title];

  CGFloat availableWidth;
  [profileButton setDefaultImage:CreateProfileImage(item.icon, kIconImageSide,
                                                    profiles::SHAPE_CIRCLE)];
  availableWidth = rect.size.width - kIconImageSide - imageTitleSpacing -
                   2 * kHorizontalSpacing;

  [profileButton setImagePosition:cocoa_l10n_util::LeadingCellImagePosition()];
  [profileButton setAlignment:NSNaturalTextAlignment];
  [profileButton setBordered:NO];
  [profileButton setTag:itemIndex];
  [profileButton setTarget:self];
  [profileButton setAction:@selector(switchToProfile:)];

  return profileButton.autorelease();
}

- (NSView*)createCurrentProfileAccountsView:(NSRect)rect {
  const CGFloat kAccountButtonHeight = 34;

  const AvatarMenu::Item& item =
      avatarMenu_->GetItemAt(avatarMenu_->GetActiveProfileIndex());
  DCHECK(item.signed_in);

  NSColor* backgroundColor = skia::SkColorToCalibratedNSColor(
      profiles::kAvatarBubbleAccountsBackgroundColor);
  base::scoped_nsobject<NSView> container([[BackgroundColorView alloc]
      initWithFrame:rect
          withColor:backgroundColor]);

  rect.origin.y = 0;
  if (!browser_->profile()->IsSupervised()) {
    // Manually elide the button text so the contents fit inside the bubble.
    // This is needed because the BlueLabelButton cell resets the style on
    // every call to -cellSize, which prevents setting a custom lineBreakMode.
    NSString* elidedButtonText = base::SysUTF16ToNSString(gfx::ElideText(
        l10n_util::GetStringFUTF16(
            IDS_PROFILES_PROFILE_ADD_ACCOUNT_BUTTON, item.name),
        gfx::FontList(), rect.size.width, gfx::ELIDE_TAIL));

    NSButton* addAccountsButton =
        [self linkButtonWithTitle:elidedButtonText
                      frameOrigin:NSMakePoint(
            kHorizontalSpacing, kSmallVerticalSpacing)
                           action:@selector(addAccount:)];
    if (cocoa_l10n_util::ShouldDoExperimentalRTLLayout()) {
      CGRect addAccountsButtonFrame = [addAccountsButton frame];
      addAccountsButtonFrame.origin.x = NSMaxX(rect) -
                                        NSWidth(addAccountsButtonFrame) -
                                        NSMinX(addAccountsButtonFrame);
      [addAccountsButton setFrame:addAccountsButtonFrame];
    }

    [container addSubview:addAccountsButton];
    rect.origin.y += kAccountButtonHeight;
  }

  NSView* accountEmails = [self createAccountsListWithRect:NSMakeRect(
      0, rect.origin.y, rect.size.width, kAccountButtonHeight)];
  [container addSubview:accountEmails];

  [container setFrameSize:NSMakeSize(rect.size.width,
                                     NSMaxY([accountEmails frame]))];
  return container.autorelease();
}

- (NSView*)createOptionsViewWithFrame:(NSRect)rect showLock:(BOOL)showLock {
  NSRect viewRect = NSMakeRect(0, 0, rect.size.width,
                               kBlueButtonHeight + kSmallVerticalSpacing);
  base::scoped_nsobject<NSView> container([[NSView alloc] initWithFrame:rect]);
  const int icon_size = 20;

  // Create a lock profile button when supervised users exist; otherwise, create
  // a button that closes all of the current profile's windows if more than one
  // is open.
  if (showLock) {
    NSButton* lockButton =
        [self hoverButtonWithRect:viewRect
                             text:l10n_util::GetNSString(
                                      IDS_PROFILES_PROFILE_SIGNOUT_BUTTON)
                            image:NSImageFromImageSkia(gfx::CreateVectorIcon(
                                      vector_icons::kLockIcon, icon_size,
                                      gfx::kChromeIconGrey))
                           action:@selector(lockProfile:)];
    [container addSubview:lockButton];
    viewRect.origin.y = NSMaxY([lockButton frame]);
  } else if (!isGuestSession_) {
    int num_browsers = 0;
    for (auto* browser : *BrowserList::GetInstance()) {
      Profile* current_profile = browser_->profile()->GetOriginalProfile();
      if (browser->profile()->GetOriginalProfile() == current_profile)
        num_browsers++;
    }
    if (num_browsers > 1) {
      NSButton* closeAllWindowsButton =
          [self hoverButtonWithRect:viewRect
                               text:l10n_util::GetNSString(
                                        IDS_PROFILES_CLOSE_ALL_WINDOWS_BUTTON)
                              image:NSImageFromImageSkia(gfx::CreateVectorIcon(
                                        kCloseAllIcon, icon_size,
                                        gfx::kChromeIconGrey))
                             action:@selector(closeAllWindows:)];
      [container addSubview:closeAllWindowsButton];
      viewRect.origin.y = NSMaxY([closeAllWindowsButton frame]);
    }
  }

  // Create a manage users/exit guest button.
  NSString* text =
      isGuestSession_
          ? l10n_util::GetNSString(IDS_PROFILES_EXIT_GUEST)
          : l10n_util::GetNSString(IDS_PROFILES_MANAGE_USERS_BUTTON);
  NSImage* icon = NSImageFromImageSkia(
      gfx::CreateVectorIcon(isGuestSession_ ? kCloseAllIcon : kSettingsIcon,
                            icon_size, gfx::kChromeIconGrey));
  SEL action =
      isGuestSession_ ? @selector(exitGuest:) : @selector(showUserManager:);
  NSButton* manageUsersButton =
      [self hoverButtonWithRect:viewRect text:text image:icon action:action];
  viewRect.origin.y = NSMaxY([manageUsersButton frame]);
  [container addSubview:manageUsersButton];

  // Create a guest profile button.
  if (!isGuestSession_ && !browser_->profile()->IsSupervised()) {
    PrefService* service = g_browser_process->local_state();
    DCHECK(service);
    if (service->GetBoolean(prefs::kBrowserGuestModeEnabled)) {
      NSButton* guestProfileButton =
          [self hoverButtonWithRect:viewRect
                               text:l10n_util::GetNSString(
                                        IDS_PROFILES_GUEST_PROFILE_NAME)
                              image:NSImageFromImageSkia(gfx::CreateVectorIcon(
                                        kAccountCircleIcon, icon_size,
                                        gfx::kChromeIconGrey))
                             action:@selector(switchToGuest:)];
      viewRect.origin.y = NSMaxY([guestProfileButton frame]);
      [container addSubview:guestProfileButton];
    }
  }

  [container setFrameSize:NSMakeSize(rect.size.width, viewRect.origin.y)];
  return container.autorelease();
}

- (NSView*)createAccountsListWithRect:(NSRect)rect {
  base::scoped_nsobject<NSView> container([[NSView alloc] initWithFrame:rect]);
  currentProfileAccounts_.clear();

  Profile* profile = browser_->profile();
  std::string primaryAccount =
      SigninManagerFactory::GetForProfile(profile)->GetAuthenticatedAccountId();
  DCHECK(!primaryAccount.empty());
  std::vector<std::string>accounts =
      profiles::GetSecondaryAccountsForProfile(profile, primaryAccount);

  // If there is an account with an authentication error, it needs to be
  // badged with a warning icon.
  std::string errorAccountId = GetAuthErrorAccountId(profile);

  rect.origin.y = 0;
  for (size_t i = 0; i < accounts.size(); ++i) {
    // Save the original email address, as the button text could be elided.
    currentProfileAccounts_[i] = accounts[i];
    NSButton* accountButton =
        [self accountButtonWithRect:rect
                         accountId:accounts[i]
                                tag:i
                     reauthRequired:errorAccountId == accounts[i]];
    [container addSubview:accountButton];
    rect.origin.y = NSMaxY([accountButton frame]);
  }

  // The primary account should always be listed first.
  NSButton* accountButton =
      [self accountButtonWithRect:rect
                        accountId:primaryAccount
                              tag:kPrimaryProfileTag
                   reauthRequired:errorAccountId == primaryAccount];
  [container addSubview:accountButton];
  [container setFrameSize:NSMakeSize(NSWidth([container frame]),
                                     NSMaxY([accountButton frame]))];
  return container.autorelease();
}

- (NSView*)buildGaiaEmbeddedView {
  base::scoped_nsobject<NSView> container(
      [[NSView alloc] initWithFrame:NSZeroRect]);
  CGFloat yOffset = 0;

  int messageId = -1;
  switch (viewMode_) {
    case profiles::BUBBLE_VIEW_MODE_GAIA_SIGNIN:
      messageId = IDS_PROFILES_GAIA_SIGNIN_TITLE;
      break;
    case profiles::BUBBLE_VIEW_MODE_GAIA_ADD_ACCOUNT:
      messageId = IDS_PROFILES_GAIA_ADD_ACCOUNT_TITLE;
      break;
    case profiles::BUBBLE_VIEW_MODE_GAIA_REAUTH:
      DCHECK(HasAuthError(browser_->profile()));
      messageId = IDS_PROFILES_GAIA_REAUTH_TITLE;
      break;
    default:
      NOTREACHED() << "Called with invalid mode=" << viewMode_;
      break;
  }

  webContentsDelegate_.reset(new GaiaWebContentsDelegate());
  webContents_ = SigninViewControllerDelegateMac::CreateGaiaWebContents(
      webContentsDelegate_.get(), viewMode_, browser_->profile(), accessPoint_);

  NSView* webview = webContents_->GetNativeView();

  [container addSubview:webview];
  yOffset = NSMaxY([webview frame]);

  // Adds the title card.
  NSBox* separator = [self horizontalSeparatorWithFrame:
      NSMakeRect(0, yOffset, kFixedGaiaViewWidth, 0)];
  [container addSubview:separator];
  yOffset = NSMaxY([separator frame]) + kVerticalSpacing;

  NSView* titleView = BuildTitleCard(
      NSMakeRect(0, yOffset, kFixedGaiaViewWidth, 0),
      l10n_util::GetStringUTF16(messageId),
      self /* backButtonTarget*/,
      @selector(navigateBackFromSigninPage:) /* backButtonAction */);
  [container addSubview:titleView];
  yOffset = NSMaxY([titleView frame]);

  [container setFrameSize:NSMakeSize(kFixedGaiaViewWidth, yOffset)];
  return container.autorelease();
}

- (NSView*)buildAccountRemovalView {
  DCHECK(!accountIdToRemove_.empty());

  base::scoped_nsobject<NSView> container(
      [[NSView alloc] initWithFrame:NSZeroRect]);
  CGFloat availableWidth =
      kFixedAccountRemovalViewWidth - 2 * kHorizontalSpacing;
  CGFloat yOffset = kVerticalSpacing;

  const std::string& primaryAccount = SigninManagerFactory::GetForProfile(
      browser_->profile())->GetAuthenticatedAccountId();
  bool isPrimaryAccount = primaryAccount == accountIdToRemove_;

  // Adds "remove account" button at the bottom if needed.
  if (!isPrimaryAccount) {
    base::scoped_nsobject<NSButton> removeAccountButton(
        [[BlueLabelButton alloc] initWithFrame:NSZeroRect]);
    [removeAccountButton setTitle:l10n_util::GetNSString(
        IDS_PROFILES_ACCOUNT_REMOVAL_BUTTON)];
    [removeAccountButton setTarget:self];
    [removeAccountButton setAction:@selector(removeAccount:)];
    [removeAccountButton sizeToFit];
    [removeAccountButton setAlignment:NSCenterTextAlignment];
    CGFloat xOffset = (kFixedAccountRemovalViewWidth -
        NSWidth([removeAccountButton frame])) / 2;
    [removeAccountButton setFrameOrigin:NSMakePoint(xOffset, yOffset)];
    [container addSubview:removeAccountButton];

    yOffset = NSMaxY([removeAccountButton frame]) + kVerticalSpacing;
  }

  NSView* contentView;
  NSPoint contentFrameOrigin = NSMakePoint(kHorizontalSpacing, yOffset);
  if (isPrimaryAccount) {
    std::string email = signin_ui_util::GetDisplayEmail(browser_->profile(),
                                                        accountIdToRemove_);
    std::vector<size_t> offsets;
    NSString* contentStr = l10n_util::GetNSStringF(
        IDS_PROFILES_PRIMARY_ACCOUNT_REMOVAL_TEXT,
        base::UTF8ToUTF16(email), base::string16(), &offsets);
    NSString* linkStr = l10n_util::GetNSString(IDS_PROFILES_SETTINGS_LINK);
    contentView = BuildFixedWidthTextViewWithLink(self, contentStr, linkStr,
        offsets[1], contentFrameOrigin, availableWidth);
  } else {
    NSString* contentStr =
        l10n_util::GetNSString(IDS_PROFILES_ACCOUNT_REMOVAL_TEXT);
    NSTextField* contentLabel = BuildLabel(contentStr, contentFrameOrigin, nil);
    [contentLabel setFrameSize:NSMakeSize(availableWidth, 0)];
    [GTMUILocalizerAndLayoutTweaker sizeToFitFixedWidthTextField:contentLabel];
    contentView = contentLabel;
  }
  [container addSubview:contentView];
  yOffset = NSMaxY([contentView frame]) + kVerticalSpacing;

  // Adds the title card.
  NSBox* separator = [self horizontalSeparatorWithFrame:
      NSMakeRect(0, yOffset, kFixedAccountRemovalViewWidth, 0)];
  [container addSubview:separator];
  yOffset = NSMaxY([separator frame]) + kVerticalSpacing;

  NSView* titleView = BuildTitleCard(
      NSMakeRect(0, yOffset, kFixedAccountRemovalViewWidth,0),
      l10n_util::GetStringUTF16(IDS_PROFILES_ACCOUNT_REMOVAL_TITLE),
      self /* backButtonTarget*/,
      @selector(showAccountManagement:) /* backButtonAction */);
  [container addSubview:titleView];
  yOffset = NSMaxY([titleView frame]);

  [container setFrameSize:NSMakeSize(kFixedAccountRemovalViewWidth, yOffset)];
  return container.autorelease();
}

- (NSView*)buildSwitchUserView {
  ProfileMetrics::LogProfileNewAvatarMenuNotYou(
      ProfileMetrics::PROFILE_AVATAR_MENU_NOT_YOU_VIEW);
  base::scoped_nsobject<NSView> container(
      [[NSView alloc] initWithFrame:NSZeroRect]);
  CGFloat availableWidth =
      kFixedSwitchUserViewWidth - 2 * kHorizontalSpacing;
  CGFloat yOffset = 0;
  NSRect viewRect = NSMakeRect(0, yOffset,
                               kFixedSwitchUserViewWidth,
                               kBlueButtonHeight + kSmallVerticalSpacing);

  const AvatarMenu::Item& avatarItem =
      avatarMenu_->GetItemAt(avatarMenu_->GetActiveProfileIndex());

  // Adds "Disconnect your Google Account" button at the bottom.
  NSButton* disconnectButton =
      [self hoverButtonWithRect:viewRect
                           text:l10n_util::GetNSString(
                                    IDS_PROFILES_DISCONNECT_BUTTON)
                imageResourceId:IDR_ICON_PROFILES_MENU_DISCONNECT
                         action:@selector(disconnectProfile:)];
  [container addSubview:disconnectButton];
  yOffset = NSMaxY([disconnectButton frame]);

  NSBox* separator = [self horizontalSeparatorWithFrame:
      NSMakeRect(0, yOffset, kFixedSwitchUserViewWidth, 0)];
  [container addSubview:separator];
  yOffset = NSMaxY([separator frame]);

  // Adds "Add person" button.
  viewRect.origin.y = yOffset;
  NSButton* addPersonButton =
      [self hoverButtonWithRect:viewRect
                           text:l10n_util::GetNSString(
                                    IDS_PROFILES_ADD_PERSON_BUTTON)
                imageResourceId:IDR_ICON_PROFILES_MENU_AVATAR
                         action:@selector(showUserManager:)];
  [container addSubview:addPersonButton];
  yOffset = NSMaxY([addPersonButton frame]);

  separator = [self horizontalSeparatorWithFrame:
      NSMakeRect(0, yOffset, kFixedSwitchUserViewWidth, 0)];
  [container addSubview:separator];
  yOffset = NSMaxY([separator frame]);

  // Adds the content text.
  base::string16 elidedName(gfx::ElideText(
      avatarItem.name, gfx::FontList(), availableWidth, gfx::ELIDE_TAIL));
  NSTextField* contentLabel = BuildLabel(
      l10n_util::GetNSStringF(IDS_PROFILES_NOT_YOU_CONTENT_TEXT, elidedName),
      NSMakePoint(kHorizontalSpacing, yOffset + kVerticalSpacing),
      nil);
  [contentLabel setFrameSize:NSMakeSize(availableWidth, 0)];
  [GTMUILocalizerAndLayoutTweaker sizeToFitFixedWidthTextField:contentLabel];
  [container addSubview:contentLabel];
  yOffset = NSMaxY([contentLabel frame]) + kVerticalSpacing;

  // Adds the title card.
  separator = [self horizontalSeparatorWithFrame:
      NSMakeRect(0, yOffset, kFixedSwitchUserViewWidth, 0)];
  [container addSubview:separator];
  yOffset = NSMaxY([separator frame]) + kVerticalSpacing;

  NSView* titleView = BuildTitleCard(
      NSMakeRect(0, yOffset, kFixedSwitchUserViewWidth,0),
      l10n_util::GetStringFUTF16(IDS_PROFILES_NOT_YOU, avatarItem.name),
      self /* backButtonTarget*/,
      @selector(navigateBackFromSwitchUserView:) /* backButtonAction */);
  [container addSubview:titleView];
  yOffset = NSMaxY([titleView frame]);

  [container setFrameSize:NSMakeSize(kFixedSwitchUserViewWidth, yOffset)];
  return container.autorelease();
}

// Called when clicked on the settings link.
- (BOOL)textView:(NSTextView*)textView
   clickedOnLink:(id)link
         atIndex:(NSUInteger)charIndex {
  chrome::ShowSettings(browser_);
  return YES;
}

- (NSButton*)hoverButtonWithRect:(NSRect)rect
                            text:(NSString*)text
                 imageResourceId:(int)imageResourceId
                          action:(SEL)action {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  NSImage* image = rb.GetNativeImageNamed(imageResourceId).ToNSImage();
  return [self hoverButtonWithRect:rect text:text image:image action:action];
}

- (NSButton*)hoverButtonWithRect:(NSRect)rect
                            text:(NSString*)text
                           image:(NSImage*)image
                          action:(SEL)action {
  BackgroundColorHoverButton* button =
      [self hoverButtonWithRect:rect text:text action:action];
  [button setDefaultImage:image];
  [button setHoverImage:image];
  [button setPressedImage:image];
  [button setImagePosition:cocoa_l10n_util::LeadingCellImagePosition()];

  return button;
}

- (BackgroundColorHoverButton*)hoverButtonWithRect:(NSRect)rect
                                              text:(NSString*)text
                                            action:(SEL)action {
  // The vector icons in hover buttons have small embeded paddings and are
  // therefore given an extra 2px in size to have a consistent look as the
  // profile icons; hence the -2.0 here to left align the hover button texts
  // with those of profile buttons.
  const int image_title_spacing = kHorizontalSpacing - 2.0;

  base::scoped_nsobject<BackgroundColorHoverButton> button(
      [[BackgroundColorHoverButton alloc]
              initWithFrame:rect
          imageTitleSpacing:image_title_spacing
            backgroundColor:GetDialogBackgroundColor()]);

  [button setTitle:text];
  [button setAlignment:NSNaturalTextAlignment];
  [button setBordered:NO];
  [button setTarget:self];
  [button setAction:action];

  return button.autorelease();
}

- (NSButton*)linkButtonWithTitle:(NSString*)title
                     frameOrigin:(NSPoint)frameOrigin
                          action:(SEL)action {
  base::scoped_nsobject<NSButton> link(
      [[HyperlinkButtonCell buttonWithString:title] retain]);

  [[link cell] setTextColor:skia::SkColorToCalibratedNSColor(
      chrome_style::GetLinkColor())];
  [link setTitle:title];
  [link setBordered:NO];
  [link setFont:[NSFont labelFontOfSize:kTextFontSize]];
  [link setTarget:self];
  [link setAction:action];
  [link setFrameOrigin:frameOrigin];
  [link sizeToFit];

  return link.autorelease();
}

- (NSButton*)accountButtonWithRect:(NSRect)rect
                         accountId:(const std::string&)accountId
                               tag:(int)tag
                    reauthRequired:(BOOL)reauthRequired {
  // Get display email address for account.
  std::string email = signin_ui_util::GetDisplayEmail(browser_->profile(),
                                                      accountId);

  ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();
  NSImage* deleteImage = rb->GetNativeImageNamed(IDR_CLOSE_1).ToNSImage();
  CGFloat deleteImageWidth = [deleteImage size].width;
  NSImage* warningImage = reauthRequired ? rb->GetNativeImageNamed(
      IDR_ICON_PROFILES_ACCOUNT_BUTTON_ERROR).ToNSImage() : nil;
  CGFloat warningImageWidth = [warningImage size].width;

  CGFloat availableTextWidth = rect.size.width - kHorizontalSpacing -
      warningImageWidth - deleteImageWidth;
  if (warningImage)
    availableTextWidth -= kHorizontalSpacing;

  NSColor* backgroundColor = skia::SkColorToCalibratedNSColor(
      profiles::kAvatarBubbleAccountsBackgroundColor);
  base::scoped_nsobject<BackgroundColorHoverButton> button(
      [[BackgroundColorHoverButton alloc] initWithFrame:rect
                                      imageTitleSpacing:0
                                        backgroundColor:backgroundColor]);
  [button setTitle:ElideEmail(email, availableTextWidth)];
  [button setAlignment:NSNaturalTextAlignment];
  [button setBordered:NO];
  if (reauthRequired) {
    [button setDefaultImage:warningImage];
    [button setImagePosition:cocoa_l10n_util::LeadingCellImagePosition()];
    [button setTarget:self];
    [button setAction:@selector(showAccountReauthenticationView:)];
    [button setTag:tag];
  }

  // Delete button.
  if (!browser_->profile()->IsSupervised()) {
    NSRect buttonRect;
    NSDivideRect(rect, &buttonRect, &rect,
                 deleteImageWidth + kHorizontalSpacing,
                 cocoa_l10n_util::TrailingEdge());
    buttonRect.origin.y = 0;

    base::scoped_nsobject<HoverImageButton> deleteButton(
        [[HoverImageButton alloc] initWithFrame:buttonRect]);
    [deleteButton setBordered:NO];
    [deleteButton setDefaultImage:deleteImage];
    [deleteButton setHoverImage:rb->GetNativeImageNamed(
        IDR_CLOSE_1_H).ToNSImage()];
    [deleteButton setPressedImage:rb->GetNativeImageNamed(
        IDR_CLOSE_1_P).ToNSImage()];
    [deleteButton setTarget:self];
    [deleteButton setAction:@selector(showAccountRemovalView:)];
    [deleteButton setTag:tag];

    [button addSubview:deleteButton];
  }

  return button.autorelease();
}

- (void)postActionPerformed:(ProfileMetrics::ProfileDesktopMenu)action {
  ProfileMetrics::LogProfileDesktopMenu(action, serviceType_);
  serviceType_ = signin::GAIA_SERVICE_TYPE_NONE;
}

- (bool)shouldShowGoIncognito {
  bool incognitoAvailable =
      IncognitoModePrefs::GetAvailability(browser_->profile()->GetPrefs()) !=
          IncognitoModePrefs::DISABLED;
  return incognitoAvailable && !browser_->profile()->IsGuestSession();
}

- (void)showWindow:(id)sender {
  [super showWindow:sender];
  NSEvent *event = [[NSApplication sharedApplication] currentEvent];
  if (firstProfileView_ && [event type] == NSKeyDown) {
    [[self window] makeFirstResponder:firstProfileView_];
  }
}

@end
