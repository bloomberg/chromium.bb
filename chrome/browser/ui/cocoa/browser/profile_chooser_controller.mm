// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#import "chrome/browser/ui/cocoa/browser/profile_chooser_controller.h"

#include "base/mac/bundle_locations.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/avatar_menu.h"
#include "chrome/browser/profiles/avatar_menu_observer.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_info_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/mutable_profile_oauth2_token_service.h"
#include "chrome/browser/signin/profile_oauth2_token_service.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_style.h"
#import "chrome/browser/ui/cocoa/info_bubble_view.h"
#import "chrome/browser/ui/cocoa/info_bubble_window.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "skia/ext/skia_utils_mac.h"
#import "ui/base/cocoa/cocoa_event_utils.h"
#import "ui/base/cocoa/controls/blue_label_button.h"
#import "ui/base/cocoa/controls/hyperlink_button_cell.h"
#import "ui/base/cocoa/hover_image_button.h"
#include "ui/base/cocoa/window_size_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/text_elider.h"

namespace {

// Constants taken from the Windows/Views implementation at:
// chrome/browser/ui/views/profile_chooser_view.cc
const int kLargeImageSide = 64;
const int kSmallImageSide = 32;

const CGFloat kMinMenuWidth = 250;
const CGFloat kVerticalSpacing = 20.0;
const CGFloat kSmallVerticalSpacing = 10.0;
const CGFloat kHorizontalSpacing = 20.0;
const CGFloat kTitleFontSize = 15.0;
const CGFloat kTextFontSize = 12.0;
const CGFloat kProfileButtonHeight = 30;
const int kOverlayHeight = 20;  // Height of the "Change" avatar photo overlay.
const int kBezelThickness = 3;  // Width of the bezel on an NSButton.

// Minimum size for embedded sign in pages as defined in Gaia.
const CGFloat kMinGaiaViewWidth = 320;
const CGFloat kMinGaiaViewHeight = 440;

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

}  // namespace

// Class that listens to changes to the OAuth2Tokens for the active profile, or
// changes to the avatar menu model.
// TODO(noms): The avatar menu model changes can only be triggered by modifying
// the name or avatar for the active profile, but this is not currently
// implemented.
class ActiveProfileObserverBridge : public AvatarMenuObserver,
                                    public OAuth2TokenService::Observer {
 public:
  explicit ActiveProfileObserverBridge(ProfileChooserController* controller)
      : controller_(controller) {
  }

  virtual ~ActiveProfileObserverBridge() {
  }

  // OAuth2TokenService::Observer:
  virtual void OnRefreshTokenAvailable(const std::string& account_id) OVERRIDE {
    // Tokens can only be added by adding an account through the inline flow,
    // which is started from the account management view. Refresh it to show the
    // update.
    BubbleViewMode viewMode = [controller_ viewMode];
    if (viewMode == ACCOUNT_MANAGEMENT_VIEW ||
        viewMode == GAIA_SIGNIN_VIEW ||
        viewMode == GAIA_ADD_ACCOUNT_VIEW) {
      [controller_ initMenuContentsWithView:ACCOUNT_MANAGEMENT_VIEW];
    }
  }

  virtual void OnRefreshTokenRevoked(const std::string& account_id) OVERRIDE {
    // Tokens can only be removed from the account management view. Refresh it
    // to show the update.
    if ([controller_ viewMode] == ACCOUNT_MANAGEMENT_VIEW)
      [controller_ initMenuContentsWithView:ACCOUNT_MANAGEMENT_VIEW];
  }

  // AvatarMenuObserver:
  virtual void OnAvatarMenuChanged(AvatarMenu* avatar_menu) OVERRIDE {
    // While the bubble is open, the avatar menu can only change from the
    // profile chooser view by modifying the current profile's photo or name.
    [controller_ initMenuContentsWithView:PROFILE_CHOOSER_VIEW];
  }

 private:
  ProfileChooserController* controller_;  // Weak; owns this.

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
  NSString* newProfileName = [profileNameTextField_ stringValue];
  if ([newProfileName length] > 0) {
    profiles::UpdateProfileName(profile_,
        base::SysNSStringToUTF16(newProfileName));
    [self setTitle:newProfileName];
  }
  [profileNameTextField_ setHidden:YES];
}

- (void)showEditableView:(id)sender {
  [profileNameTextField_ setHidden:NO];
  [profileNameTextField_ becomeFirstResponder];
}

- (NSString*)profileName {
  return [self title];
}
@end

@interface ProfileChooserController (Private)
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

// Creates the Guest / Add person / View all persons buttons.
- (NSView*)createOptionsViewWithRect:(NSRect)rect;

// Creates the account management view for the active profile.
- (NSView*)createCurrentProfileAccountsView:(NSRect)rect;

// Creates the list of accounts for the active profile.
- (NSView*)createAccountsListWithRect:(NSRect)rect;

// Creates the Gaia sign-in/add account view.
- (NSView*)createGaiaEmbeddedView;

// Creates a generic button with text given by |textResourceId|, an icon
// given by |imageResourceId| and with |action|.
- (NSButton*)buttonWithRect:(NSRect)rect
             textResourceId:(int)textResourceId
            imageResourceId:(int)imageResourceId
                     action:(SEL)action;

// Creates a generic link button with |title| and an |action| positioned at
// |frameOrigin|.
- (NSButton*)linkButtonWithTitle:(NSString*)title
                     frameOrigin:(NSPoint)frameOrigin
                          action:(SEL)action;

// Creates an email account button with |title|. If |canBeDeleted| is YES, then
// the button is clickable and has a remove icon.
- (NSButton*)makeAccountButtonWithRect:(NSRect)rect
                                 title:(const std::string&)title
                          canBeDeleted:(BOOL)canBeDeleted;

- (void)dealloc;

@end

@implementation ProfileChooserController
- (BubbleViewMode) viewMode {
  return viewMode_;
}

- (IBAction)addNewProfile:(id)sender {
  profiles::CreateAndSwitchToNewProfile(
      browser_->host_desktop_type(),
      profiles::ProfileSwitchingDoneCallback());
}

- (IBAction)switchToProfile:(id)sender {
  // Check the event flags to see if a new window should be created.
  bool always_create = ui::WindowOpenDispositionFromNSEvent(
      [NSApp currentEvent]) == NEW_WINDOW;
  avatarMenu_->SwitchToProfile([sender tag], always_create);
}

- (IBAction)showUserManager:(id)sender {
  // Only non-guest users appear in the User Manager.
  base::FilePath profile_path;
  if (!isGuestSession_) {
    size_t active_index = avatarMenu_->GetActiveProfileIndex();
    profile_path = avatarMenu_->GetItemAt(active_index).profile_path;
  }
  chrome::ShowUserManager(profile_path);
}

- (IBAction)switchToGuestProfile:(id)sender {
  profiles::SwitchToGuestProfile(browser_->host_desktop_type(),
                                 profiles::ProfileSwitchingDoneCallback());
}

- (IBAction)exitGuestProfile:(id)sender {
  profiles::CloseGuestProfileWindows();
}

- (IBAction)showAccountManagement:(id)sender {
  [self initMenuContentsWithView:ACCOUNT_MANAGEMENT_VIEW];
}

- (IBAction)lockProfile:(id)sender {
  profiles::LockProfile(browser_->profile());
}

- (IBAction)showSigninPage:(id)sender {
  [self initMenuContentsWithView:GAIA_SIGNIN_VIEW];
}

- (IBAction)addAccount:(id)sender {
  [self initMenuContentsWithView:GAIA_ADD_ACCOUNT_VIEW];
}

- (IBAction)removeAccount:(id)sender {
  DCHECK(!isGuestSession_);
  DCHECK_GE([sender tag], 0);  // Should not be called for the primary account.
  DCHECK(ContainsKey(currentProfileAccounts_, [sender tag]));
  std::string account = currentProfileAccounts_[[sender tag]];
  ProfileOAuth2TokenServiceFactory::GetPlatformSpecificForProfile(
      browser_->profile())->RevokeCredentials(account);
}

- (id)initWithBrowser:(Browser*)browser anchoredAt:(NSPoint)point {
  base::scoped_nsobject<InfoBubbleWindow> window([[InfoBubbleWindow alloc]
      initWithContentRect:ui::kWindowSizeDeterminedLater
                styleMask:NSBorderlessWindowMask
                  backing:NSBackingStoreBuffered
                    defer:NO]);

  if ((self = [super initWithWindow:window
                       parentWindow:browser->window()->GetNativeWindow()
                         anchoredAt:point])) {
    browser_ = browser;
    viewMode_ = PROFILE_CHOOSER_VIEW;
    observer_.reset(new ActiveProfileObserverBridge(self));

    avatarMenu_.reset(new AvatarMenu(
        &g_browser_process->profile_manager()->GetProfileInfoCache(),
        observer_.get(),
        browser_));
    avatarMenu_->RebuildMenu();

    // Guest profiles do not have a token service.
    isGuestSession_ = browser_->profile()->IsGuestSession();
    if (!isGuestSession_) {
      ProfileOAuth2TokenService* oauth2TokenService =
          ProfileOAuth2TokenServiceFactory::GetForProfile(browser_->profile());
      DCHECK(oauth2TokenService);
      oauth2TokenService->AddObserver(observer_.get());
    }

    [[self bubble] setArrowLocation:info_bubble::kTopRight];
    [self initMenuContentsWithView:viewMode_];
  }

  return self;
}

- (void)dealloc {
  if (!isGuestSession_) {
    ProfileOAuth2TokenService* oauth2TokenService =
        ProfileOAuth2TokenServiceFactory::GetForProfile(browser_->profile());
    DCHECK(oauth2TokenService);
    oauth2TokenService->RemoveObserver(observer_.get());
  }
  [super dealloc];
}

- (void)initMenuContentsWithView:(BubbleViewMode)viewToDisplay {
  viewMode_ = viewToDisplay;
  NSView* contentView = [[self window] contentView];
  [contentView setSubviews:[NSArray array]];

  if (viewMode_ == GAIA_SIGNIN_VIEW || viewMode_ == GAIA_ADD_ACCOUNT_VIEW) {
    [contentView addSubview:[self createGaiaEmbeddedView]];
    SetWindowSize([self window],
                  NSMakeSize(kMinGaiaViewWidth, kMinGaiaViewHeight));
    return;
  }

  NSView* currentProfileView = nil;
  base::scoped_nsobject<NSMutableArray> otherProfiles(
      [[NSMutableArray alloc] init]);

  // Separate items into active and other profiles.
  for (size_t i = 0; i < avatarMenu_->GetNumberOfItems(); ++i) {
    const AvatarMenu::Item& item = avatarMenu_->GetItemAt(i);
    if (item.active) {
      currentProfileView = [self createCurrentProfileView:item];
    } else {
      [otherProfiles addObject:[self createOtherProfileView:i]];
    }
  }
  if (!currentProfileView)  // Guest windows don't have an active profile.
    currentProfileView = [self createGuestProfileView];

  CGFloat contentsWidth =
      std::max(kMinMenuWidth, NSWidth([currentProfileView frame]));
  CGFloat contentsWidthWithSpacing = contentsWidth + 2 * kHorizontalSpacing;

  // |yOffset| is the next position at which to draw in |contentView|
  // coordinates.
  CGFloat yOffset = kVerticalSpacing;
  NSRect viewRect = NSMakeRect(kHorizontalSpacing, yOffset, contentsWidth, 0);

  // Guest / Add Person / View All Persons buttons.
  NSView* optionsView = [self createOptionsViewWithRect:viewRect];
  [contentView addSubview:optionsView];
  yOffset = NSMaxY([optionsView frame]) + kVerticalSpacing;

  NSBox* separator = [self separatorWithFrame:NSMakeRect(
      0, yOffset, contentsWidthWithSpacing, 0)];
  [contentView addSubview:separator];
  yOffset = NSMaxY([separator frame]) + kVerticalSpacing;

  if (viewToDisplay == PROFILE_CHOOSER_VIEW) {
    // Other profiles switcher.
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
  } else if (viewToDisplay == ACCOUNT_MANAGEMENT_VIEW) {
    // Reuse the same view area as for the option buttons.
    viewRect.origin.y = yOffset;
    NSView* currentProfileAccountsView =
        [self createCurrentProfileAccountsView:viewRect];
    [contentView addSubview:currentProfileAccountsView];
    yOffset = NSMaxY([currentProfileAccountsView frame]) + kVerticalSpacing;

    NSBox* accountsSeparator = [self separatorWithFrame:NSMakeRect(
        0, yOffset, contentsWidthWithSpacing, 0)];
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

  SetWindowSize([self window], NSMakeSize(contentsWidthWithSpacing, yOffset));
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
  CGFloat maxXLinksContainer = 0;
  if (!isGuestSession_ && viewMode_ == PROFILE_CHOOSER_VIEW) {
    NSView* linksContainer =
        [self createCurrentProfileLinksForItem:item withXOffset:xOffset];
    [container addSubview:linksContainer];
    yOffset = NSMaxY([linksContainer frame]);
    maxXLinksContainer = NSMaxX([linksContainer frame]);
  }

  // TODO(noms): At the moment, this means that long profile names will
  // not be displayed correctly in the bubble. This is related to
  // crbug.com/334761, which will also fix this issue.
  CGFloat maxX = std::max(maxXLinksContainer, kMinMenuWidth);

  // Profile name.
  base::scoped_nsobject<EditableProfileNameButton> profileName(
      [[EditableProfileNameButton alloc]
          initWithFrame:NSMakeRect(xOffset, yOffset,
                                   maxX - xOffset, /* Width of the column */
                                   kProfileButtonHeight)
                profile:browser_->profile()
            profileName:base::SysUTF16ToNSString(item.name)
         editingAllowed:!isGuestSession_]);

  [container addSubview:profileName];
  [container setFrameSize:NSMakeSize(maxX, NSHeight([iconView frame]))];
  return container.autorelease();
}

- (NSView*)createCurrentProfileLinksForItem:(const AvatarMenu::Item&)item
                                withXOffset:(CGFloat)xOffset {
  base::scoped_nsobject<NSView> container([[NSView alloc]
      initWithFrame:NSZeroRect]);
  CGFloat maxX = 0;  // Ensure the container is wide enough for all the links.
  CGFloat yOffset;

  // The available links depend on the type of profile that is active.
  if (item.signed_in) {
    yOffset = 0;
    // We need to display 2 links instead of 1, so make the padding in between
    // the links even smaller to fit.
    const CGFloat kLinkSpacing = kSmallVerticalSpacing / 2;
    NSButton* manageAccountsLink =
        [self linkButtonWithTitle:l10n_util::GetNSString(
            IDS_PROFILES_PROFILE_MANAGE_ACCOUNTS_BUTTON)
                      frameOrigin:NSMakePoint(xOffset, yOffset)
                           action:@selector(showAccountManagement:)];
    yOffset = NSMaxY([manageAccountsLink frame]) + kLinkSpacing;

    NSButton* signOutLink =
        [self linkButtonWithTitle:l10n_util::GetNSString(
            IDS_PROFILES_PROFILE_SIGNOUT_BUTTON)
                      frameOrigin:NSMakePoint(xOffset, yOffset)
                           action:@selector(lockProfile:)];
    yOffset = NSMaxY([signOutLink frame]);

    maxX = std::max(NSMaxX([manageAccountsLink frame]),
                                 NSMaxX([signOutLink frame]));
    [container addSubview:manageAccountsLink];
    [container addSubview:signOutLink];
  } else {
    yOffset = kSmallVerticalSpacing;
    NSButton* signInLink =
        [self linkButtonWithTitle:l10n_util::GetNSStringFWithFixup(
            IDS_SYNC_START_SYNC_BUTTON_LABEL,
            l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME))
                      frameOrigin:NSMakePoint(xOffset, yOffset)
                           action:@selector(showSigninPage:)];
    yOffset = NSMaxY([signInLink frame]) + kSmallVerticalSpacing;
    maxX = NSMaxX([signInLink frame]);

    [container addSubview:signInLink];
  }

  [container setFrameSize:NSMakeSize(maxX, yOffset)];
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

  // TODO(noms): Increase the spacing between the icon and the text to 10px;
  [profileButton setTitle:base::SysUTF16ToNSString(item.name)];
  [profileButton setImage:CreateProfileImage(
      item.icon, kSmallImageSide).ToNSImage()];
  [profileButton setImagePosition:NSImageLeft];
  [profileButton setBordered:NO];
  [profileButton setFont:[NSFont labelFontOfSize:kTitleFontSize]];
  [profileButton setTag:itemIndex];
  [profileButton setTarget:self];
  [profileButton setAction:@selector(switchToProfile:)];
  [profileButton sizeToFit];

  return profileButton.autorelease();
}

- (NSView*)createOptionsViewWithRect:(NSRect)rect {
  CGFloat yOffset = 0;
  base::scoped_nsobject<NSView> container([[NSView alloc] initWithFrame:rect]);

  NSButton* allUsersButton =
      [self buttonWithRect:NSMakeRect(0, yOffset, 0, 0)
            textResourceId:IDS_PROFILES_ALL_PEOPLE_BUTTON
           imageResourceId:IDR_ICON_PROFILES_ADD_USER
                    action:@selector(showUserManager:)];
  yOffset = NSMaxY([allUsersButton frame]) + kSmallVerticalSpacing;

  NSButton* addUserButton =
      [self buttonWithRect:NSMakeRect(0, yOffset, 0, 0)
            textResourceId:IDS_PROFILES_ADD_PERSON_BUTTON
           imageResourceId:IDR_ICON_PROFILES_ADD_USER
                    action:@selector(addNewProfile:)];
  yOffset = NSMaxY([addUserButton frame]) + kSmallVerticalSpacing;

  int guestButtonText = isGuestSession_ ? IDS_PROFILES_EXIT_GUEST_BUTTON :
                                          IDS_PROFILES_GUEST_BUTTON;
  SEL guestButtonAction = isGuestSession_ ? @selector(exitGuestProfile:) :
                                            @selector(switchToGuestProfile:);
  NSButton* guestButton =
      [self buttonWithRect:NSMakeRect(0, yOffset, 0, 0)
            textResourceId:guestButtonText
           imageResourceId:IDR_ICON_PROFILES_BROWSE_GUEST
                    action:guestButtonAction];
  yOffset = NSMaxY([guestButton frame]);

  [container setSubviews:@[allUsersButton, addUserButton, guestButton]];
  [container setFrameSize:NSMakeSize(NSWidth([container frame]), yOffset)];
  return container.autorelease();
}

- (NSView*)createCurrentProfileAccountsView:(NSRect)rect {
  const CGFloat kBlueButtonHeight = 30;
  const CGFloat kAccountButtonHeight = 15;

  const AvatarMenu::Item& item =
      avatarMenu_->GetItemAt(avatarMenu_->GetActiveProfileIndex());
  DCHECK(item.signed_in);

  base::scoped_nsobject<NSView> container([[NSView alloc] initWithFrame:rect]);

  NSRect viewRect = NSMakeRect(0, 0, rect.size.width, kBlueButtonHeight);
  base::scoped_nsobject<NSButton> addAccountsButton([[BlueLabelButton alloc]
      initWithFrame:viewRect]);
  [addAccountsButton setTitle:l10n_util::GetNSStringFWithFixup(
      IDS_PROFILES_PROFILE_ADD_ACCOUNT_BUTTON, item.name)];
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
    NSButton* accountButton = [self makeAccountButtonWithRect:rect
                                                        title:accounts[i]
                                                 canBeDeleted:true];
    [accountButton setTag:i];
    [container addSubview:accountButton];
    rect.origin.y = NSMaxY([accountButton frame]) + kSmallVerticalSpacing;
  }

  // The primary account should always be listed first. It doesn't need a tag,
  // as it cannot be removed.
  // TODO(rogerta): we still need to further differentiate the primary account
  // from the others in the UI, so more work is likely required here:
  // crbug.com/311124.
  NSButton* accountButton = [self makeAccountButtonWithRect:rect
                                                      title:primaryAccount
                                               canBeDeleted:false];
  [container addSubview:accountButton];
  [container setFrameSize:NSMakeSize(NSWidth([container frame]),
                                     NSMaxY([accountButton frame]))];
  return container.autorelease();
}

- (NSView*) createGaiaEmbeddedView {
  signin::Source source = (viewMode_ == GAIA_SIGNIN_VIEW) ?
      signin::SOURCE_AVATAR_BUBBLE_SIGN_IN :
      signin::SOURCE_AVATAR_BUBBLE_ADD_ACCOUNT;

  webContents_.reset(content::WebContents::Create(
      content::WebContents::CreateParams(browser_->profile())));
  webContents_->GetController().LoadURL(
      signin::GetPromoURL(source, false),
      content::Referrer(),
      content::PAGE_TRANSITION_AUTO_TOPLEVEL,
      std::string());
  NSView* webview = webContents_->GetView()->GetNativeView();
  [webview setFrameSize:NSMakeSize(kMinGaiaViewWidth, kMinGaiaViewHeight)];
  return webview;
}

- (NSButton*)buttonWithRect:(NSRect)rect
             textResourceId:(int)textResourceId
            imageResourceId:(int)imageResourceId
                     action:(SEL)action  {
  base::scoped_nsobject<NSButton> button([[NSButton alloc] initWithFrame:rect]);

  // TODO(noms): Increase the spacing between the icon and the text to 10px;
  [button setTitle:l10n_util::GetNSString(textResourceId)];
  [button setImage:ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(
      imageResourceId).ToNSImage()];
  [button setImagePosition:NSImageLeft];
  [button setBordered:NO];
  [button setTarget:self];
  [button setAction:action];
  [button sizeToFit];

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

- (NSButton*)makeAccountButtonWithRect:(NSRect)rect
                                 title:(const std::string&)title
                          canBeDeleted:(BOOL)canBeDeleted {
  base::scoped_nsobject<NSButton> button([[NSButton alloc] initWithFrame:rect]);
  [button setTitle:ElideEmail(title, rect.size.width)];
  [button setAlignment:NSLeftTextAlignment];
  [button setBordered:NO];

  if (canBeDeleted) {
    [button setImage:ui::ResourceBundle::GetSharedInstance().
        GetNativeImageNamed(IDR_CLOSE_1).ToNSImage()];
    [button setImagePosition:NSImageRight];
    [button setTarget:self];
    [button setAction:@selector(removeAccount:)];
  }

  return button.autorelease();
}

@end

