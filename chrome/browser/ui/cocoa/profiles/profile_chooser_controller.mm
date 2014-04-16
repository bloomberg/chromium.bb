// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#import "chrome/browser/ui/cocoa/profiles/profile_chooser_controller.h"

#include "base/mac/bundle_locations.h"
#include "base/prefs/pref_service.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/avatar_menu.h"
#include "chrome/browser/profiles/avatar_menu_observer.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/chrome_style.h"
#import "chrome/browser/ui/cocoa/hyperlink_text_view.h"
#import "chrome/browser/ui/cocoa/info_bubble_view.h"
#import "chrome/browser/ui/cocoa/info_bubble_window.h"
#import "chrome/browser/ui/cocoa/profiles/user_manager_mac.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "components/signin/core/browser/mutable_profile_oauth2_token_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "skia/ext/skia_utils_mac.h"
#import "third_party/google_toolbox_for_mac/src/AppKit/GTMUILocalizerAndLayoutTweaker.h"
#import "ui/base/cocoa/cocoa_base_utils.h"
#import "ui/base/cocoa/controls/blue_label_button.h"
#import "ui/base/cocoa/controls/hyperlink_button_cell.h"
#import "ui/base/cocoa/hover_image_button.h"
#include "ui/base/cocoa/window_size_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/text_elider.h"
#include "ui/native_theme/native_theme.h"

namespace {

// Constants taken from the Windows/Views implementation at:
// chrome/browser/ui/views/profile_chooser_view.cc
const int kLargeImageSide = 64;
const int kSmallImageSide = 32;
const CGFloat kFixedMenuWidth = 250;

const CGFloat kVerticalSpacing = 20.0;
const CGFloat kSmallVerticalSpacing = 10.0;
const CGFloat kHorizontalSpacing = 20.0;
const CGFloat kTitleFontSize = 15.0;
const CGFloat kTextFontSize = 12.0;
const CGFloat kProfileButtonHeight = 30;
const int kOverlayHeight = 20;  // Height of the "Change" avatar photo overlay.
const int kBezelThickness = 3;  // Width of the bezel on an NSButton.
const int kImageTitleSpacing = 10;
const int kBlueButtonHeight = 30;

// Fixed size for embedded sign in pages as defined in Gaia.
const CGFloat kFixedGaiaViewWidth = 360;
const CGFloat kFixedGaiaViewHeight = 400;

// Fixed size for the account removal view.
const CGFloat kFixedAccountRemovalViewWidth = 280;

// Maximum number of times to show the tutorial in the profile avatar bubble.
const int kProfileAvatarTutorialShowMax = 5;

// The tag number for the primary account.
const int kPrimaryProfileTag = -1;

gfx::Image CreateProfileImage(const gfx::Image& icon, int imageSize) {
  return profiles::GetSizedAvatarIconWithBorder(
      icon, true /* image is a square */,
      imageSize + profiles::kAvatarIconPadding,
      imageSize + profiles::kAvatarIconPadding);
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
  base::string16 elidedEmail = gfx::ElideEmail(
      base::UTF8ToUTF16(email),
      ui::ResourceBundle::GetSharedInstance().GetFontList(
          ui::ResourceBundle::BaseFont),
      width);
  return base::SysUTF16ToNSString(elidedEmail);
}

// Builds a label with the given |title| anchored at |frame_origin|. Sets the
// text color and background color to the given colors if not null.
NSTextField* BuildLabel(NSString* title,
                        NSPoint frame_origin,
                        NSColor* background_color,
                        NSColor* text_color) {
  base::scoped_nsobject<NSTextField> label(
      [[NSTextField alloc] initWithFrame:NSZeroRect]);
  [label setStringValue:title];
  [label setEditable:NO];
  [label setAlignment:NSLeftTextAlignment];
  [label setBezeled:NO];
  [label setFont:[NSFont labelFontOfSize:kTextFontSize]];
  [label setFrameOrigin:frame_origin];
  [label sizeToFit];

  if (background_color) {
    DCHECK(text_color);
    [[label cell] setBackgroundColor:background_color];
    [[label cell] setTextColor:text_color];
  }
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
  NSColor* link_color = gfx::SkColorToCalibratedNSColor(
      chrome_style::GetLinkColor());
  [text_view setMessageAndLink:message
                      withLink:link
                      atOffset:link_offset
                          font:[NSFont labelFontOfSize:kTextFontSize]
                  messageColor:[NSColor blackColor]
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

// Builds a title card with one back button right aligned and one label center
// aligned.
NSView* BuildTitleCard(NSRect frame_rect,
                       int message_id,
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

  NSTextField* title_label =
      BuildLabel(l10n_util::GetNSString(message_id), NSZeroPoint,
                 nil /* background_color */, nil /* text_color */);
  [title_label setAlignment:NSCenterTextAlignment];
  [title_label setFont:[NSFont labelFontOfSize:kTitleFontSize]];
  [title_label sizeToFit];
  CGFloat x_offset = (frame_rect.size.width - NSWidth([title_label frame])) / 2;
  [title_label setFrameOrigin:NSMakePoint(x_offset, 0)];

  [container addSubview:button];
  [container addSubview:title_label];
  CGFloat height = std::max(NSMaxY([title_label frame]),
                            NSMaxY([button frame])) + kSmallVerticalSpacing;
  [container setFrameSize:NSMakeSize(NSWidth([container frame]), height)];

  return container.autorelease();
}

}  // namespace

// Class that listens to changes to the OAuth2Tokens for the active profile,
// changes to the avatar menu model or browser close notifications.
class ActiveProfileObserverBridge : public AvatarMenuObserver,
                                    public content::NotificationObserver,
                                    public OAuth2TokenService::Observer {
 public:
  ActiveProfileObserverBridge(ProfileChooserController* controller,
                              Browser* browser)
      : controller_(controller),
        browser_(browser),
        token_observer_registered_(false) {
    registrar_.Add(this, chrome::NOTIFICATION_BROWSER_CLOSING,
                   content::NotificationService::AllSources());
    if (!browser_->profile()->IsGuestSession())
      AddTokenServiceObserver();
  }

  virtual ~ActiveProfileObserverBridge() {
    RemoveTokenServiceObserver();
  }

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
  virtual void OnRefreshTokenAvailable(const std::string& account_id) OVERRIDE {
    // Tokens can only be added by adding an account through the inline flow,
    // which is started from the account management view. Refresh it to show the
    // update.
    BubbleViewMode viewMode = [controller_ viewMode];
    if (viewMode == BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT ||
        viewMode == BUBBLE_VIEW_MODE_GAIA_SIGNIN ||
        viewMode == BUBBLE_VIEW_MODE_GAIA_ADD_ACCOUNT) {
      [controller_
          initMenuContentsWithView:BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT];
    }
  }

  virtual void OnRefreshTokenRevoked(const std::string& account_id) OVERRIDE {
    // Tokens can only be removed from the account management view. Refresh it
    // to show the update.
    if ([controller_ viewMode] == BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT)
      [controller_
          initMenuContentsWithView:BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT];
  }

  // AvatarMenuObserver:
  virtual void OnAvatarMenuChanged(AvatarMenu* avatar_menu) OVERRIDE {
    // While the bubble is open, the avatar menu can only change from the
    // profile chooser view by modifying the current profile's photo or name.
    [controller_ initMenuContentsWithView:BUBBLE_VIEW_MODE_PROFILE_CHOOSER];
  }

  // content::NotificationObserver:
  virtual void Observe(
      int type,
      const content::NotificationSource& source,
      const content::NotificationDetails& details) OVERRIDE {
    DCHECK_EQ(chrome::NOTIFICATION_BROWSER_CLOSING, type);
    if (browser_ == content::Source<Browser>(source).ptr()) {
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
  content::NotificationRegistrar registrar_;

  // The observer can be removed both when closing the browser, and by just
  // closing the avatar bubble. However, in the case of closing the browser,
  // the avatar bubble will also be closed afterwards, resulting in a second
  // attempt to remove the observer. This ensures the observer is only
  // removed once.
  bool token_observer_registered_;

  DISALLOW_COPY_AND_ASSIGN(ActiveProfileObserverBridge);
};

// A custom button that has a transparent backround.
@interface TransparentBackgroundButton : NSButton
@end

@implementation TransparentBackgroundButton
- (id)initWithFrame:(NSRect)frameRect {
  if ((self = [super initWithFrame:frameRect])) {
    [self setBordered:NO];
    [self setFont:[NSFont labelFontOfSize:kTextFontSize]];
    [self setButtonType:NSMomentaryChangeButton];
  }
  return self;
}

- (void)drawRect:(NSRect)dirtyRect {
  NSColor* backgroundColor = [NSColor colorWithCalibratedWhite:0 alpha:0.5f];
  [backgroundColor setFill];
  NSRectFillUsingOperation(dirtyRect, NSCompositeSourceAtop);
  [super drawRect:dirtyRect];
}
@end

// A custom image control that shows a "Change" button when moused over.
@interface EditableProfilePhoto : NSImageView {
 @private
  AvatarMenu* avatarMenu_;  // Weak; Owned by ProfileChooserController.
  base::scoped_nsobject<TransparentBackgroundButton> changePhotoButton_;
  // Used to display the "Change" button on hover.
  ui::ScopedCrTrackingArea trackingArea_;
}

- (id)initWithFrame:(NSRect)frameRect
         avatarMenu:(AvatarMenu*)avatarMenu
        profileIcon:(const gfx::Image&)profileIcon
     editingAllowed:(BOOL)editingAllowed;

// Called when the "Change" button is clicked.
- (void)editPhoto:(id)sender;

// When hovering over the profile photo, show the "Change" button.
- (void)mouseEntered:(NSEvent*)event;

// When hovering away from the profile photo, hide the "Change" button.
- (void)mouseExited:(NSEvent*)event;
@end

@interface EditableProfilePhoto (Private)
// Create the "Change" avatar photo button.
- (TransparentBackgroundButton*)changePhotoButtonWithRect:(NSRect)rect;
@end

@implementation EditableProfilePhoto
- (id)initWithFrame:(NSRect)frameRect
         avatarMenu:(AvatarMenu*)avatarMenu
        profileIcon:(const gfx::Image&)profileIcon
     editingAllowed:(BOOL)editingAllowed {
  if ((self = [super initWithFrame:frameRect])) {
    avatarMenu_ = avatarMenu;
    [self setImage:CreateProfileImage(
        profileIcon, kLargeImageSide).ToNSImage()];

    // Add a tracking area so that we can show/hide the button when hovering.
    trackingArea_.reset([[CrTrackingArea alloc]
        initWithRect:[self bounds]
             options:NSTrackingMouseEnteredAndExited | NSTrackingActiveAlways
               owner:self
            userInfo:nil]);
    [self addTrackingArea:trackingArea_.get()];

    if (editingAllowed) {
      // The avatar photo uses a frame of width profiles::kAvatarIconPadding,
      // which we must subtract from the button's bounds.
      changePhotoButton_.reset([self changePhotoButtonWithRect:NSMakeRect(
          profiles::kAvatarIconPadding, profiles::kAvatarIconPadding,
          kLargeImageSide - 2 * profiles::kAvatarIconPadding,
          kOverlayHeight)]);
      [self addSubview:changePhotoButton_];

      // Hide the button until the image is hovered over.
      [changePhotoButton_ setHidden:YES];
    }
  }
  return self;
}

- (void)editPhoto:(id)sender {
  avatarMenu_->EditProfile(avatarMenu_->GetActiveProfileIndex());
}

- (void)mouseEntered:(NSEvent*)event {
  [changePhotoButton_ setHidden:NO];
}

- (void)mouseExited:(NSEvent*)event {
  [changePhotoButton_ setHidden:YES];
}

- (TransparentBackgroundButton*)changePhotoButtonWithRect:(NSRect)rect {
  TransparentBackgroundButton* button =
      [[TransparentBackgroundButton alloc] initWithFrame:rect];

  // The button has a centered white text and a transparent background.
  base::scoped_nsobject<NSMutableParagraphStyle> textStyle(
      [[NSMutableParagraphStyle alloc] init]);
  [textStyle setAlignment:NSCenterTextAlignment];
  NSDictionary* titleAttributes = @{
      NSParagraphStyleAttributeName : textStyle,
      NSForegroundColorAttributeName : [NSColor whiteColor]
  };
  NSString* buttonTitle = l10n_util::GetNSString(
      IDS_PROFILES_PROFILE_CHANGE_PHOTO_BUTTON);
  base::scoped_nsobject<NSAttributedString> attributedTitle(
      [[NSAttributedString alloc] initWithString:buttonTitle
                                      attributes:titleAttributes]);
  [button setAttributedTitle:attributedTitle];
  [button setTarget:self];
  [button setAction:@selector(editPhoto:)];
  return button;
}
@end

// A custom text control that turns into a textfield for editing when clicked.
@interface EditableProfileNameButton : HoverImageButton<NSTextFieldDelegate> {
 @private
  base::scoped_nsobject<NSTextField> profileNameTextField_;
  Profile* profile_;  // Weak.
}

- (id)initWithFrame:(NSRect)frameRect
            profile:(Profile*)profile
        profileName:(NSString*)profileName
     editingAllowed:(BOOL)editingAllowed;

// Called when the button is clicked.
- (void)showEditableView:(id)sender;

// Called when the user presses "Enter" in the textfield.
- (void)controlTextDidEndEditing:(NSNotification *)obj;
@end

@implementation EditableProfileNameButton
- (id)initWithFrame:(NSRect)frameRect
            profile:(Profile*)profile
        profileName:(NSString*)profileName
     editingAllowed:(BOOL)editingAllowed {
  if ((self = [super initWithFrame:frameRect])) {
    profile_ = profile;

    [self setBordered:NO];
    [self setFont:[NSFont labelFontOfSize:kTitleFontSize]];
    [self setAlignment:NSLeftTextAlignment];
    [[self cell] setLineBreakMode:NSLineBreakByTruncatingTail];
    [self setTitle:profileName];

    if (editingAllowed) {
      // Show an "edit" pencil icon when hovering over.
      ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();
      [self setHoverImage:
          rb->GetNativeImageNamed(IDR_ICON_PROFILES_EDIT_HOVER).AsNSImage()];
      [self setAlternateImage:
          rb->GetNativeImageNamed(IDR_ICON_PROFILES_EDIT_PRESSED).AsNSImage()];
      [self setImagePosition:NSImageRight];
      [self setTarget:self];
      [self setAction:@selector(showEditableView:)];

      // We need to subtract the width of the bezel from the frame rect, so that
      // the textfield can take the exact same space as the button.
      frameRect.size.height -= 2 * kBezelThickness;
      frameRect.origin = NSMakePoint(0, kBezelThickness);
      profileNameTextField_.reset(
          [[NSTextField alloc] initWithFrame:frameRect]);
      [profileNameTextField_ setStringValue:profileName];
      [profileNameTextField_ setFont:[NSFont labelFontOfSize:kTitleFontSize]];
      [profileNameTextField_ setEditable:YES];
      [profileNameTextField_ setDrawsBackground:YES];
      [profileNameTextField_ setBezeled:YES];
      [[profileNameTextField_ cell] setWraps:NO];
      [[profileNameTextField_ cell] setLineBreakMode:
          NSLineBreakByTruncatingTail];
      [profileNameTextField_ setDelegate:self];
      [self addSubview:profileNameTextField_];

      // Hide the textfield until the user clicks on the button.
      [profileNameTextField_ setHidden:YES];
    }
  }
  return self;
}

// NSTextField objects send an NSNotification to a delegate if
// it implements this method:
- (void)controlTextDidEndEditing:(NSNotification *)obj {
  NSString* text = [profileNameTextField_ stringValue];
  // Empty profile names are not allowed, and are treated as a cancel.
  if ([text length] > 0) {
    profiles::UpdateProfileName(profile_, base::SysNSStringToUTF16(text));
    [self setTitle:text];
  }
  [profileNameTextField_ setHidden:YES];
  [profileNameTextField_ resignFirstResponder];
}

- (void)showEditableView:(id)sender {
  [profileNameTextField_ setHidden:NO];
  [profileNameTextField_ becomeFirstResponder];
}

@end

// Custom button cell that adds a left padding before the button image, and
// a custom spacing between the button image and title.
@interface CustomPaddingImageButtonCell : NSButtonCell {
 @private
  // Padding between the left margin of the button and the cell image.
  int leftMarginSpacing_;
  // Spacing between the cell image and title.
  int imageTitleSpacing_;
}

- (id)initWithLeftMarginSpacing:(int)leftMarginSpacing
              imageTitleSpacing:(int)imageTitleSpacing;
@end

@implementation CustomPaddingImageButtonCell
- (id)initWithLeftMarginSpacing:(int)leftMarginSpacing
              imageTitleSpacing:(int)imageTitleSpacing {
  if ((self = [super init])) {
    leftMarginSpacing_ = leftMarginSpacing;
    imageTitleSpacing_ = imageTitleSpacing;
  }
  return self;
}

- (NSRect)drawTitle:(NSAttributedString*)title
          withFrame:(NSRect)frame
             inView:(NSView*)controlView {
  // The title frame origin isn't aware of the left margin spacing added
  // in -drawImage, so it must be added when drawing the title as well.
  frame.origin.x += leftMarginSpacing_ + imageTitleSpacing_;
  frame.size.width -= (imageTitleSpacing_ + leftMarginSpacing_);
  return [super drawTitle:title withFrame:frame inView:controlView];
}

- (void)drawImage:(NSImage*)image
       withFrame:(NSRect)frame
          inView:(NSView*)controlView {
  frame.origin.x = leftMarginSpacing_;
  [super drawImage:image withFrame:frame inView:controlView];
}

- (NSSize)cellSize {
  NSSize buttonSize = [super cellSize];
  buttonSize.width += leftMarginSpacing_ + imageTitleSpacing_;
  return buttonSize;
}

@end

// A custom button that allows for setting a background color when hovered over.
@interface BackgroundColorHoverButton : HoverImageButton
@end

@implementation BackgroundColorHoverButton
- (id)initWithFrame:(NSRect)frameRect {
  if ((self = [super initWithFrame:frameRect])) {
    [self setBordered:NO];
    [self setFont:[NSFont labelFontOfSize:kTextFontSize]];
    [self setButtonType:NSMomentaryChangeButton];

    base::scoped_nsobject<CustomPaddingImageButtonCell> cell(
        [[CustomPaddingImageButtonCell alloc]
            initWithLeftMarginSpacing:kHorizontalSpacing
                    imageTitleSpacing:kImageTitleSpacing]);
    [cell setLineBreakMode:NSLineBreakByTruncatingTail];
    [self setCell:cell.get()];
  }
  return self;
}

- (void)setHoverState:(HoverState)state {
  [super setHoverState:state];
  bool isHighlighted = ([self hoverState] != kHoverStateNone);

  NSColor* backgroundColor = gfx::SkColorToCalibratedNSColor(
      ui::NativeTheme::instance()->GetSystemColor(isHighlighted?
          ui::NativeTheme::kColorId_MenuSeparatorColor :
          ui::NativeTheme::kColorId_DialogBackground));

  [[self cell] setBackgroundColor:backgroundColor];
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

@interface ProfileChooserController ()
// Creates a tutorial card for the profile |avatar_item| if needed.
- (NSView*)createTutorialViewIfNeeded:(const AvatarMenu::Item&)item;

// Creates the main profile card for the profile |item| at the top of
// the bubble.
- (NSView*)createCurrentProfileView:(const AvatarMenu::Item&)item;

// Creates the possible links for the main profile card with profile |item|.
- (NSView*)createCurrentProfileLinksForItem:(const AvatarMenu::Item&)item
                                withXOffset:(CGFloat)xOffset;

// Creates a main profile card for the guest user.
- (NSView*)createGuestProfileView;

// Creates an item for the profile |itemIndex| that is used in the fast profile
// switcher in the middle of the bubble.
- (NSButton*)createOtherProfileView:(int)itemIndex;

// Creates the "Not you" and Lock option buttons.
- (NSView*)createOptionsViewWithRect:(NSRect)rect
                          enableLock:(BOOL)enableLock;

// Creates the account management view for the active profile.
- (NSView*)createCurrentProfileAccountsView:(NSRect)rect;

// Creates the list of accounts for the active profile.
- (NSView*)createAccountsListWithRect:(NSRect)rect;

// Creates the Gaia sign-in/add account view.
- (NSView*)buildGaiaEmbeddedView;

// Creates the account removal view.
- (NSView*)buildAccountRemovalView;

// Creates a button with |text|, an icon given by |imageResourceId| and with
// |action|. The icon |alternateImageResourceId| is displayed in the button's
// hovered and pressed states.
- (NSButton*)hoverButtonWithRect:(NSRect)rect
                            text:(NSString*)text
                 imageResourceId:(int)imageResourceId
        alternateImageResourceId:(int)alternateImageResourceId
                          action:(SEL)action;

// Creates a generic link button with |title| and an |action| positioned at
// |frameOrigin|.
- (NSButton*)linkButtonWithTitle:(NSString*)title
                     frameOrigin:(NSPoint)frameOrigin
                          action:(SEL)action;

// Creates an email account button with |title| and a remove icon.
- (NSButton*)accountButtonWithRect:(NSRect)rect
                             title:(const std::string&)title;

@end

@implementation ProfileChooserController
- (BubbleViewMode) viewMode {
  return viewMode_;
}

- (IBAction)switchToProfile:(id)sender {
  // Check the event flags to see if a new window should be created.
  bool alwaysCreate = ui::WindowOpenDispositionFromNSEvent(
      [NSApp currentEvent]) == NEW_WINDOW;
  avatarMenu_->SwitchToProfile([sender tag], alwaysCreate,
                               ProfileMetrics::SWITCH_PROFILE_ICON);
}

- (IBAction)showUserManager:(id)sender {
  profiles::ShowUserManagerMaybeWithTutorial(browser_->profile());
}

- (IBAction)showAccountManagement:(id)sender {
  [self initMenuContentsWithView:BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT];
}

- (IBAction)lockProfile:(id)sender {
  profiles::LockProfile(browser_->profile());
}

- (IBAction)showSigninPage:(id)sender {
  [self initMenuContentsWithView:BUBBLE_VIEW_MODE_GAIA_SIGNIN];
}

- (IBAction)addAccount:(id)sender {
  [self initMenuContentsWithView:BUBBLE_VIEW_MODE_GAIA_ADD_ACCOUNT];
}

- (IBAction)navigateBackFromSigninPage:(id)sender {
  std::string primaryAccount = SigninManagerFactory::GetForProfile(
      browser_->profile())->GetAuthenticatedUsername();
  [self initMenuContentsWithView:primaryAccount.empty() ?
      BUBBLE_VIEW_MODE_PROFILE_CHOOSER : BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT];
}

- (IBAction)showAccountRemovalView:(id)sender {
  DCHECK(!isGuestSession_);

  // Tag is either |kPrimaryProfileTag| for the primary account, or equal to the
  // index in |currentProfileAccounts_| for a secondary account.
  int tag = [sender tag];
  if (tag == kPrimaryProfileTag) {
    accountIdToRemove_ = SigninManagerFactory::GetForProfile(
        browser_->profile())->GetAuthenticatedUsername();
  } else {
    DCHECK(ContainsKey(currentProfileAccounts_, tag));
    accountIdToRemove_ = currentProfileAccounts_[tag];
  }

  [self initMenuContentsWithView:BUBBLE_VIEW_MODE_ACCOUNT_REMOVAL];
}

- (IBAction)removeAccountAndRelaunch:(id)sender {
  DCHECK(!accountIdToRemove_.empty());
  ProfileOAuth2TokenServiceFactory::GetPlatformSpecificForProfile(
      browser_->profile())->RevokeCredentials(accountIdToRemove_);
  accountIdToRemove_.clear();
  chrome::AttemptRestart();
}

- (IBAction)openTutorialLearnMoreURL:(id)sender {
  // TODO(guohui): update |learnMoreUrl| once it is decided.
  const GURL learnMoreUrl("https://support.google.com/chrome/?hl=en#to");
  chrome::NavigateParams params(browser_->profile(), learnMoreUrl,
                                content::PAGE_TRANSITION_LINK);
  params.disposition = NEW_FOREGROUND_TAB;
  chrome::Navigate(&params);
}

- (IBAction)dismissTutorial:(id)sender {
  // If the user manually dismissed the tutorial, never show it again by setting
  // the number of times shown to the maximum plus 1, so that later we could
  // distinguish between the dismiss case and the case when the tutorial is
  // indeed shown for the maximum number of times.
  browser_->profile()->GetPrefs()->SetInteger(
      prefs::kProfileAvatarTutorialShown, kProfileAvatarTutorialShowMax + 1);
  [self initMenuContentsWithView:BUBBLE_VIEW_MODE_PROFILE_CHOOSER];
}

- (void)cleanUpEmbeddedViewContents {
  webContents_.reset();
}

- (id)initWithBrowser:(Browser*)browser
           anchoredAt:(NSPoint)point
             withMode:(BubbleViewMode)mode {
  base::scoped_nsobject<InfoBubbleWindow> window([[InfoBubbleWindow alloc]
      initWithContentRect:ui::kWindowSizeDeterminedLater
                styleMask:NSBorderlessWindowMask
                  backing:NSBackingStoreBuffered
                    defer:NO]);

  if ((self = [super initWithWindow:window
                       parentWindow:browser->window()->GetNativeWindow()
                         anchoredAt:point])) {
    browser_ = browser;
    viewMode_ = mode;
    tutorialShowing_ = false;
    observer_.reset(new ActiveProfileObserverBridge(self, browser_));

    avatarMenu_.reset(new AvatarMenu(
        &g_browser_process->profile_manager()->GetProfileInfoCache(),
        observer_.get(),
        browser_));
    avatarMenu_->RebuildMenu();

    // Guest profiles do not have a token service.
    isGuestSession_ = browser_->profile()->IsGuestSession();

    ui::NativeTheme* nativeTheme = ui::NativeTheme::instance();
    [[self bubble] setAlignment:info_bubble::kAlignRightEdgeToAnchorEdge];
    [[self bubble] setArrowLocation:info_bubble::kNoArrow];
    [[self bubble] setBackgroundColor:
        gfx::SkColorToCalibratedNSColor(nativeTheme->GetSystemColor(
            ui::NativeTheme::kColorId_DialogBackground))];
    [self initMenuContentsWithView:viewMode_];
  }

  return self;
}

- (void)initMenuContentsWithView:(BubbleViewMode)viewToDisplay {
  viewMode_ = viewToDisplay;
  NSView* contentView = [[self window] contentView];
  [contentView setSubviews:[NSArray array]];

  if (viewMode_ == BUBBLE_VIEW_MODE_GAIA_SIGNIN ||
      viewMode_ == BUBBLE_VIEW_MODE_GAIA_ADD_ACCOUNT ||
      viewMode_ == BUBBLE_VIEW_MODE_ACCOUNT_REMOVAL) {
    bool isRemovalView = viewMode_ == BUBBLE_VIEW_MODE_ACCOUNT_REMOVAL;
    NSView* subView = isRemovalView ?
        [self buildAccountRemovalView] : [self buildGaiaEmbeddedView];
    [contentView addSubview:subView];
    SetWindowSize([self window],
        NSMakeSize(NSWidth([subView frame]), NSHeight([subView frame])));
    return;
  }

  NSView* tutorialView = nil;
  NSView* currentProfileView = nil;
  base::scoped_nsobject<NSMutableArray> otherProfiles(
      [[NSMutableArray alloc] init]);
  // Local and guest profiles cannot lock their profile.
  bool enableLock = false;

  // Loop over the profiles in reverse, so that they are sorted by their
  // y-coordinate, and separate them into active and "other" profiles.
  for (int i = avatarMenu_->GetNumberOfItems() - 1; i >= 0; --i) {
    const AvatarMenu::Item& item = avatarMenu_->GetItemAt(i);
    if (item.active) {
      if (viewMode_ == BUBBLE_VIEW_MODE_PROFILE_CHOOSER)
        tutorialView = [self createTutorialViewIfNeeded:item];
      currentProfileView = [self createCurrentProfileView:item];
      enableLock = item.signed_in;
    } else {
      [otherProfiles addObject:[self createOtherProfileView:i]];
    }
  }
  if (!currentProfileView)  // Guest windows don't have an active profile.
    currentProfileView = [self createGuestProfileView];

  // |yOffset| is the next position at which to draw in |contentView|
  // coordinates.
  CGFloat yOffset = kSmallVerticalSpacing;

  // Option buttons.
  NSView* optionsView = [self createOptionsViewWithRect:
      NSMakeRect(0, yOffset, kFixedMenuWidth, 0)
                                        enableLock:enableLock];
  [contentView addSubview:optionsView];
  yOffset = NSMaxY([optionsView frame]) + kSmallVerticalSpacing;

  NSBox* separator = [self separatorWithFrame:
      NSMakeRect(0, yOffset, kFixedMenuWidth, 0)];
  [contentView addSubview:separator];
  yOffset = NSMaxY([separator frame]) + kVerticalSpacing;

  if (viewToDisplay == BUBBLE_VIEW_MODE_PROFILE_CHOOSER &&
      switches::IsFastUserSwitching()) {
    // Other profiles switcher. The profiles have already been sorted
    // by their y-coordinate, so they can be added in the existing order.
    for (NSView *otherProfileView in otherProfiles.get()) {
      [otherProfileView setFrameOrigin:NSMakePoint(kHorizontalSpacing,
                                                   yOffset)];
      [contentView addSubview:otherProfileView];
      yOffset = NSMaxY([otherProfileView frame]) + kSmallVerticalSpacing;
    }

    // If we displayed other profiles, ensure the spacing between the last item
    // and the active profile card is the same as the spacing between the active
    // profile card and the bottom of the bubble.
    if ([otherProfiles.get() count] > 0)
      yOffset += kSmallVerticalSpacing;
  } else if (viewToDisplay == BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT) {
    NSView* currentProfileAccountsView = [self createCurrentProfileAccountsView:
        NSMakeRect(kHorizontalSpacing,
                   yOffset,
                   kFixedMenuWidth - 2 * kHorizontalSpacing,
                   0)];
    [contentView addSubview:currentProfileAccountsView];
    yOffset = NSMaxY([currentProfileAccountsView frame]) + kVerticalSpacing;

    NSBox* accountsSeparator = [self separatorWithFrame:
        NSMakeRect(0, yOffset, kFixedMenuWidth, 0)];
    [contentView addSubview:accountsSeparator];
    yOffset = NSMaxY([accountsSeparator frame]) + kVerticalSpacing;
  }

  // Active profile card.
  if (currentProfileView) {
    [currentProfileView setFrameOrigin:NSMakePoint(kHorizontalSpacing,
                                                   yOffset)];
    [contentView addSubview:currentProfileView];
    yOffset = NSMaxY([currentProfileView frame]) + kVerticalSpacing;
  }

  if (tutorialView) {
    [tutorialView setFrameOrigin:NSMakePoint(0, yOffset)];
    [contentView addSubview:tutorialView];
    yOffset = NSMaxY([tutorialView frame]);
  } else {
    tutorialShowing_ = false;
  }

  SetWindowSize([self window], NSMakeSize(kFixedMenuWidth, yOffset));
}

- (NSView*)createTutorialViewIfNeeded:(const AvatarMenu::Item&)item {
  if (!item.signed_in)
    return nil;

  Profile* profile = browser_->profile();
  const int showCount = profile->GetPrefs()->GetInteger(
      prefs::kProfileAvatarTutorialShown);
  // Do not show the tutorial if user has dismissed it.
  if (showCount > kProfileAvatarTutorialShowMax)
    return nil;

  if (!tutorialShowing_) {
    if (showCount == kProfileAvatarTutorialShowMax)
      return nil;
    profile->GetPrefs()->SetInteger(
      prefs::kProfileAvatarTutorialShown, showCount + 1);
    tutorialShowing_ = true;
  }

  NSColor* tutorialBackgroundColor =
      gfx::SkColorToSRGBNSColor(profiles::kAvatarTutorialBackgroundColor);
  base::scoped_nsobject<NSView> container([[BackgroundColorView alloc]
      initWithFrame:NSMakeRect(0, 0, kFixedMenuWidth, 0)
          withColor:tutorialBackgroundColor]);
  CGFloat availableWidth = kFixedMenuWidth - 2 * kHorizontalSpacing;
  CGFloat yOffset = kVerticalSpacing;

  // Adds links and buttons at the bottom.
  NSButton* learnMoreLink =
      [self linkButtonWithTitle:
          l10n_util::GetNSString(IDS_PROFILES_PROFILE_TUTORIAL_LEARN_MORE)
                    frameOrigin:NSMakePoint(kHorizontalSpacing, yOffset)
                         action:@selector(openTutorialLearnMoreURL:)];
  [[learnMoreLink cell] setTextColor:[NSColor whiteColor]];
  [container addSubview:learnMoreLink];

  base::scoped_nsobject<NSButton> tutorialOkButton([[HoverButton alloc]
      initWithFrame:NSZeroRect]);
  [tutorialOkButton setTitle:l10n_util::GetNSString(
      IDS_PROFILES_TUTORIAL_OK_BUTTON)];
  [tutorialOkButton setTarget:self];
  [tutorialOkButton setAction:@selector(dismissTutorial:)];
  [tutorialOkButton sizeToFit];
  NSSize buttonSize = [tutorialOkButton frame].size;
  const CGFloat kTopBottomTextPadding = 6;
  const CGFloat kLeftRightTextPadding = 15;
  buttonSize.width += 2 * kLeftRightTextPadding;
  buttonSize.height += 2 * kTopBottomTextPadding;
  [tutorialOkButton setFrameSize:buttonSize];
  [tutorialOkButton setAlignment:NSCenterTextAlignment];
  [tutorialOkButton setFrameOrigin:NSMakePoint(
      kFixedMenuWidth - NSWidth([tutorialOkButton frame]) - kHorizontalSpacing,
      yOffset)];
  [container addSubview:tutorialOkButton];

  yOffset = std::max(NSMaxY([learnMoreLink frame]),
                     NSMaxY([tutorialOkButton frame])) + kVerticalSpacing;

  // Adds body content.
  NSTextField* contentLabel = BuildLabel(
      l10n_util::GetNSString(
          IDS_PROFILES_PREVIEW_ENABLED_TUTORIAL_CONTENT_TEXT),
      NSMakePoint(kHorizontalSpacing, yOffset),
      tutorialBackgroundColor,
      gfx::SkColorToSRGBNSColor(profiles::kAvatarTutorialContentTextColor));
  [contentLabel setFrameSize:NSMakeSize(availableWidth, 0)];
  [GTMUILocalizerAndLayoutTweaker sizeToFitFixedWidthTextField:contentLabel];
  [container addSubview:contentLabel];
  yOffset = NSMaxY([contentLabel frame]) + kSmallVerticalSpacing;

  // Adds title.
  NSTextField* titleLabel =
      BuildLabel(
          l10n_util::GetNSString(IDS_PROFILES_PREVIEW_ENABLED_TUTORIAL_TITLE),
          NSMakePoint(kHorizontalSpacing, yOffset),
          tutorialBackgroundColor,
          [NSColor whiteColor] /* text_color */);
  [titleLabel setFont:[NSFont labelFontOfSize:kTitleFontSize]];
  [titleLabel sizeToFit];
  [titleLabel setFrameSize:
      NSMakeSize(availableWidth, NSHeight([titleLabel frame]))];
  [container addSubview:titleLabel];
  yOffset = NSMaxY([titleLabel frame]) + kVerticalSpacing;

  [container setFrameSize:NSMakeSize(kFixedMenuWidth, yOffset)];

  // Adds caret at the bottom.
  NSImage* caretImage = ui::ResourceBundle::GetSharedInstance().
      GetNativeImageNamed(IDR_ICON_PROFILES_MENU_CARET).AsNSImage();
  base::scoped_nsobject<NSImageView> caretView(
      [[NSImageView alloc] initWithFrame:NSMakeRect(
          kHorizontalSpacing, 0, caretImage.size.width,
          caretImage.size.height)]);
  [caretView setImage:caretImage];

  base::scoped_nsobject<NSView> containerWithCaret([[NSView alloc]
      initWithFrame:NSMakeRect(0, 0, kFixedMenuWidth, 0)]);
  [containerWithCaret addSubview:caretView];

  [container setFrameOrigin:NSMakePoint(0, caretImage.size.height)];
  [containerWithCaret addSubview:container];

  [containerWithCaret setFrameSize:
      NSMakeSize(kFixedMenuWidth, NSMaxY([container frame]))];
  return containerWithCaret.autorelease();
}

- (NSView*)createCurrentProfileView:(const AvatarMenu::Item&)item {
  base::scoped_nsobject<NSView> container([[NSView alloc]
      initWithFrame:NSZeroRect]);

  // Profile icon.
  base::scoped_nsobject<EditableProfilePhoto> iconView(
      [[EditableProfilePhoto alloc]
          initWithFrame:NSMakeRect(0, 0, kLargeImageSide, kLargeImageSide)
             avatarMenu:avatarMenu_.get()
            profileIcon:item.icon
         editingAllowed:!isGuestSession_]);

  [container addSubview:iconView];

  CGFloat xOffset = NSMaxX([iconView frame]) + kHorizontalSpacing;
  CGFloat yOffset = kVerticalSpacing;
  if (!isGuestSession_ && viewMode_ == BUBBLE_VIEW_MODE_PROFILE_CHOOSER) {
    NSView* linksContainer =
        [self createCurrentProfileLinksForItem:item withXOffset:xOffset];
    [container addSubview:linksContainer];
    yOffset = NSMaxY([linksContainer frame]);
  }

  // Profile name.
  CGFloat availableTextWidth =
      kFixedMenuWidth - xOffset - 2 * kHorizontalSpacing;
  base::scoped_nsobject<EditableProfileNameButton> profileName(
      [[EditableProfileNameButton alloc]
          initWithFrame:NSMakeRect(xOffset, yOffset,
                                   availableTextWidth,
                                   kProfileButtonHeight)
                profile:browser_->profile()
            profileName:base::SysUTF16ToNSString(
                profiles::GetAvatarNameForProfile(browser_->profile()))
         editingAllowed:!isGuestSession_]);

  [container addSubview:profileName];
  [container setFrameSize:NSMakeSize(kFixedMenuWidth,
                                     NSHeight([iconView frame]))];
  return container.autorelease();
}

- (NSView*)createCurrentProfileLinksForItem:(const AvatarMenu::Item&)item
                                withXOffset:(CGFloat)xOffset {
  base::scoped_nsobject<NSView> container([[NSView alloc]
      initWithFrame:NSZeroRect]);

  NSButton* link;
  NSPoint frameOrigin = NSMakePoint(xOffset, kSmallVerticalSpacing);
  // The available links depend on the type of profile that is active.
  if (item.signed_in) {
    link = [self linkButtonWithTitle:l10n_util::GetNSString(
        IDS_PROFILES_PROFILE_MANAGE_ACCOUNTS_BUTTON)
                         frameOrigin:frameOrigin
                              action:@selector(showAccountManagement:)];
  } else {
    link = [self linkButtonWithTitle:l10n_util::GetNSStringFWithFixup(
        IDS_SYNC_START_SYNC_BUTTON_LABEL,
        l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME))
                         frameOrigin:frameOrigin
                              action:@selector(showSigninPage:)];
  }

  [container addSubview:link];
  [container setFrameSize:NSMakeSize(
      NSMaxX([link frame]), NSMaxY([link frame]) + kSmallVerticalSpacing)];
  return container.autorelease();
}

- (NSView*)createGuestProfileView {
  gfx::Image guestIcon =
      ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(
          IDR_LOGIN_GUEST);
  AvatarMenu::Item guestItem(std::string::npos, /* menu_index, not used */
                             std::string::npos, /* profile_index, not used */
                             guestIcon);
  guestItem.active = true;
  guestItem.name = base::SysNSStringToUTF16(
      l10n_util::GetNSString(IDS_PROFILES_GUEST_PROFILE_NAME));

  return [self createCurrentProfileView:guestItem];
}

- (NSButton*)createOtherProfileView:(int)itemIndex {
  const AvatarMenu::Item& item = avatarMenu_->GetItemAt(itemIndex);
  base::scoped_nsobject<NSButton> profileButton([[NSButton alloc]
      initWithFrame:NSZeroRect]);
  base::scoped_nsobject<CustomPaddingImageButtonCell> cell(
  [[CustomPaddingImageButtonCell alloc]
      initWithLeftMarginSpacing:0
              imageTitleSpacing:kImageTitleSpacing]);
  [profileButton setCell:cell.get()];

  [[profileButton cell] setLineBreakMode:NSLineBreakByTruncatingTail];
  [profileButton setTitle:base::SysUTF16ToNSString(item.name)];
  [profileButton setImage:CreateProfileImage(
      item.icon, kSmallImageSide).ToNSImage()];
  [profileButton setImagePosition:NSImageLeft];
  [profileButton setAlignment:NSLeftTextAlignment];
  [profileButton setBordered:NO];
  [profileButton setFont:[NSFont labelFontOfSize:kTitleFontSize]];
  [profileButton setTag:itemIndex];
  [profileButton setTarget:self];
  [profileButton setAction:@selector(switchToProfile:)];

  // Since the bubble is fixed width, we need to calculate the width available
  // for the profile name, as longer names will have to be elided.
  CGFloat availableTextWidth = kFixedMenuWidth - 2 * kHorizontalSpacing;
  [profileButton sizeToFit];
  [profileButton setFrameSize:NSMakeSize(availableTextWidth,
                                         NSHeight([profileButton frame]))];

  return profileButton.autorelease();
}

- (NSView*)createOptionsViewWithRect:(NSRect)rect
                          enableLock:(BOOL)enableLock {
  int widthOfLockButton = enableLock? 2 * kHorizontalSpacing + 12 : 0;
  NSRect viewRect = NSMakeRect(0, 0,
                               rect.size.width - widthOfLockButton,
                               kBlueButtonHeight);
  NSButton* notYouButton =
      [self hoverButtonWithRect:viewRect
                           text:l10n_util::GetNSStringF(
          IDS_PROFILES_NOT_YOU_BUTTON,
          profiles::GetAvatarNameForProfile(browser_->profile()))
                imageResourceId:IDR_ICON_PROFILES_MENU_AVATAR
       alternateImageResourceId:IDR_ICON_PROFILES_MENU_AVATAR
                         action:@selector(showUserManager:)];

  rect.size.height = NSMaxY([notYouButton frame]);
  base::scoped_nsobject<NSView> container([[NSView alloc]
      initWithFrame:rect]);
  [container addSubview:notYouButton];

  if (enableLock) {
    viewRect.origin.x = NSMaxX([notYouButton frame]);
    viewRect.size.width = widthOfLockButton;
    NSButton* lockButton =
        [self hoverButtonWithRect:viewRect
                             text:@""
                  imageResourceId:IDR_ICON_PROFILES_MENU_LOCK
         alternateImageResourceId:IDR_ICON_PROFILES_MENU_LOCK
                           action:@selector(lockProfile:)];
    [container addSubview:lockButton];
  }

  return container.autorelease();
}

- (NSView*)createCurrentProfileAccountsView:(NSRect)rect {
  const CGFloat kAccountButtonHeight = 15;

  const AvatarMenu::Item& item =
      avatarMenu_->GetItemAt(avatarMenu_->GetActiveProfileIndex());
  DCHECK(item.signed_in);

  base::scoped_nsobject<NSView> container([[NSView alloc] initWithFrame:rect]);

  NSRect viewRect = NSMakeRect(0, 0, rect.size.width, kBlueButtonHeight);
  base::scoped_nsobject<NSButton> addAccountsButton([[BlueLabelButton alloc]
      initWithFrame:viewRect]);

  // Manually elide the button text so that the contents fit inside the bubble.
  // This is needed because the BlueLabelButton cell resets the style on
  // every call to -cellSize, which prevents setting a custom lineBreakMode.
  NSString* elidedButtonText = base::SysUTF16ToNSString(gfx::ElideText(
      l10n_util::GetStringFUTF16(
          IDS_PROFILES_PROFILE_ADD_ACCOUNT_BUTTON, item.name),
      ui::ResourceBundle::GetSharedInstance().GetFontList(
          ui::ResourceBundle::BaseFont),
      rect.size.width,
      gfx::ELIDE_AT_END));

  [addAccountsButton setTitle:elidedButtonText];
  [addAccountsButton setTarget:self];
  [addAccountsButton setAction:@selector(addAccount:)];
  [container addSubview:addAccountsButton];

  // Update the height of the email account buttons. This is needed so that the
  // all the buttons span the entire width of the bubble.
  viewRect.origin.y = NSMaxY([addAccountsButton frame]) + kVerticalSpacing;
  viewRect.size.height = kAccountButtonHeight;

  NSView* accountEmails = [self createAccountsListWithRect:viewRect];
  [container addSubview:accountEmails];
  [container setFrameSize:NSMakeSize(
      NSWidth([container frame]), NSMaxY([accountEmails frame]))];
  return container.autorelease();
}

- (NSView*)createAccountsListWithRect:(NSRect)rect {
  base::scoped_nsobject<NSView> container([[NSView alloc] initWithFrame:rect]);
  currentProfileAccounts_.clear();

  Profile* profile = browser_->profile();
  std::string primaryAccount =
      SigninManagerFactory::GetForProfile(profile)->GetAuthenticatedUsername();
  DCHECK(!primaryAccount.empty());
  std::vector<std::string>accounts =
      profiles::GetSecondaryAccountsForProfile(profile, primaryAccount);

  rect.origin.y = 0;
  for (size_t i = 0; i < accounts.size(); ++i) {
    // Save the original email address, as the button text could be elided.
    currentProfileAccounts_[i] = accounts[i];
    NSButton* accountButton = [self accountButtonWithRect:rect
                                                    title:accounts[i]];
    [accountButton setTag:i];
    [container addSubview:accountButton];
    rect.origin.y = NSMaxY([accountButton frame]) + kSmallVerticalSpacing;
  }

  // The primary account should always be listed first.
  NSButton* accountButton = [self accountButtonWithRect:rect
                                                  title:primaryAccount];
  [container addSubview:accountButton];
  [container setFrameSize:NSMakeSize(NSWidth([container frame]),
                                     NSMaxY([accountButton frame]))];
  [accountButton setTag:kPrimaryProfileTag];
  return container.autorelease();
}

- (NSView*)buildGaiaEmbeddedView {
  base::scoped_nsobject<NSView> container(
      [[NSView alloc] initWithFrame:NSZeroRect]);
  CGFloat yOffset = 0;

  bool addSecondaryAccount = viewMode_ == BUBBLE_VIEW_MODE_GAIA_ADD_ACCOUNT;
  signin::Source source = addSecondaryAccount ?
      signin::SOURCE_AVATAR_BUBBLE_ADD_ACCOUNT :
      signin::SOURCE_AVATAR_BUBBLE_SIGN_IN;

  webContents_.reset(content::WebContents::Create(
      content::WebContents::CreateParams(browser_->profile())));
  webContents_->GetController().LoadURL(
      signin::GetPromoURL(
          source, false /* auto_close */, true /* is_constrained */),
      content::Referrer(),
      content::PAGE_TRANSITION_AUTO_TOPLEVEL,
      std::string());
  NSView* webview = webContents_->GetView()->GetNativeView();
  [webview setFrameSize:NSMakeSize(kFixedGaiaViewWidth, kFixedGaiaViewHeight)];
  [container addSubview:webview];
  yOffset = NSMaxY([webview frame]);

  // Adds the title card.
  NSBox* separator = [self separatorWithFrame:
      NSMakeRect(0, yOffset, kFixedGaiaViewWidth, 0)];
  [container addSubview:separator];
  yOffset = NSMaxY([separator frame]) + kSmallVerticalSpacing;

  NSView* titleView = BuildTitleCard(
      NSMakeRect(0, yOffset, kFixedGaiaViewWidth,0),
      addSecondaryAccount ? IDS_PROFILES_GAIA_ADD_ACCOUNT_TITLE :
                            IDS_PROFILES_GAIA_SIGNIN_TITLE,
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
      browser_->profile())->GetAuthenticatedUsername();
  bool isPrimaryAccount = primaryAccount == accountIdToRemove_;

  // Adds "remove and relaunch" button at the bottom if needed.
  if (!isPrimaryAccount) {
    base::scoped_nsobject<NSButton> removeAndRelaunchButton(
        [[BlueLabelButton alloc] initWithFrame:NSZeroRect]);
    [removeAndRelaunchButton setTitle:l10n_util::GetNSString(
        IDS_PROFILES_ACCOUNT_REMOVAL_BUTTON)];
    [removeAndRelaunchButton setTarget:self];
    [removeAndRelaunchButton setAction:@selector(removeAccountAndRelaunch:)];
    [removeAndRelaunchButton sizeToFit];
    [removeAndRelaunchButton setAlignment:NSCenterTextAlignment];
    CGFloat xOffset = (kFixedAccountRemovalViewWidth -
        NSWidth([removeAndRelaunchButton frame])) / 2;
    [removeAndRelaunchButton setFrameOrigin:NSMakePoint(xOffset, yOffset)];
    [container addSubview:removeAndRelaunchButton];

    yOffset = NSMaxY([removeAndRelaunchButton frame]) + kVerticalSpacing;
  }

  NSView* contentView;
  NSPoint contentFrameOrigin = NSMakePoint(kHorizontalSpacing, yOffset);
  if (isPrimaryAccount) {
    std::vector<size_t> offsets;
    NSString* contentStr = l10n_util::GetNSStringF(
        IDS_PROFILES_PRIMARY_ACCOUNT_REMOVAL_TEXT,
        base::UTF8ToUTF16(accountIdToRemove_), base::string16(), &offsets);
    NSString* linkStr = l10n_util::GetNSString(IDS_PROFILES_SETTINGS_LINK);
    contentView = BuildFixedWidthTextViewWithLink(self, contentStr, linkStr,
        offsets[1], contentFrameOrigin, availableWidth);
  } else {
    NSString* contentStr = isPrimaryAccount ?
    l10n_util::GetNSStringF(IDS_PROFILES_PRIMARY_ACCOUNT_REMOVAL_TEXT,
                            base::UTF8ToUTF16(accountIdToRemove_)) :
    l10n_util::GetNSString(IDS_PROFILES_ACCOUNT_REMOVAL_TEXT);
    NSTextField* contentLabel = BuildLabel(contentStr, contentFrameOrigin,
        nil /* background_color */, nil /* text_color */);
    [contentLabel setFrameSize:NSMakeSize(availableWidth, 0)];
    [GTMUILocalizerAndLayoutTweaker sizeToFitFixedWidthTextField:contentLabel];
    contentView = contentLabel;
  }
  [container addSubview:contentView];
  yOffset = NSMaxY([contentView frame]) + kVerticalSpacing;

  // Adds the title card.
  NSBox* separator = [self separatorWithFrame:
      NSMakeRect(0, yOffset, kFixedAccountRemovalViewWidth, 0)];
  [container addSubview:separator];
  yOffset = NSMaxY([separator frame]) + kSmallVerticalSpacing;

  NSView* titleView = BuildTitleCard(
      NSMakeRect(0, yOffset, kFixedAccountRemovalViewWidth,0),
      IDS_PROFILES_ACCOUNT_REMOVAL_TITLE,
      self /* backButtonTarget*/,
      @selector(showAccountManagement:) /* backButtonAction */);
  [container addSubview:titleView];
  yOffset = NSMaxY([titleView frame]);

  [container setFrameSize:NSMakeSize(kFixedAccountRemovalViewWidth, yOffset)];
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
        alternateImageResourceId:(int)alternateImageResourceId
                          action:(SEL)action  {
  base::scoped_nsobject<BackgroundColorHoverButton> button(
      [[BackgroundColorHoverButton alloc] initWithFrame:rect]);

  [button setTitle:text];
  ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();
  NSImage* alternateImage = rb->GetNativeImageNamed(
      alternateImageResourceId).ToNSImage();
  [button setDefaultImage:rb->GetNativeImageNamed(imageResourceId).ToNSImage()];
  [button setHoverImage:alternateImage];
  [button setPressedImage:alternateImage];
  [button setImagePosition:NSImageLeft];
  [button setAlignment:NSLeftTextAlignment];
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

  [[link cell] setShouldUnderline:NO];
  [[link cell] setTextColor:gfx::SkColorToCalibratedNSColor(
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
                             title:(const std::string&)title {
  base::scoped_nsobject<NSButton> button([[NSButton alloc] initWithFrame:rect]);
  [button setTitle:ElideEmail(title, rect.size.width)];
  [button setAlignment:NSLeftTextAlignment];
  [button setBordered:NO];

  [button setImage:ui::ResourceBundle::GetSharedInstance().
      GetNativeImageNamed(IDR_CLOSE_1).ToNSImage()];
  [button setImagePosition:NSImageRight];
  [button setTarget:self];
  [button setAction:@selector(showAccountRemovalView:)];

  return button.autorelease();
}

@end
