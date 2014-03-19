// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#import "chrome/browser/ui/cocoa/browser/profile_chooser_controller.h"

#include "base/mac/bundle_locations.h"
#include "base/prefs/pref_service.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/avatar_menu.h"
#include "chrome/browser/profiles/avatar_menu_observer.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_info_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/profiles/profiles_state.h"
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
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/signin/core/mutable_profile_oauth2_token_service.h"
#include "components/signin/core/profile_oauth2_token_service.h"
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

// Minimum size for embedded sign in pages as defined in Gaia.
const CGFloat kMinGaiaViewWidth = 320;
const CGFloat kMinGaiaViewHeight = 440;

// Maximum number of times to show the tutorial in the profile avatar bubble.
const int kProfileAvatarTutorialShowMax = 5;

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
    [self setCell:cell.get()];
  }
  return self;
}

- (void)setHoverState:(HoverState)state {
  [super setHoverState:state];
  bool isHighlighted = ([self hoverState] != kHoverStateNone);

  NSColor* backgroundColor = gfx::SkColorToCalibratedNSColor(
      ui::NativeTheme::instance()->GetSystemColor(isHighlighted?
          ui::NativeTheme::kColorId_FocusedMenuItemBackgroundColor :
          ui::NativeTheme::kColorId_MenuBackgroundColor));

  [[self cell] setBackgroundColor:backgroundColor];

  // When hovered, the button text should be white.
  NSColor* textColor =
      isHighlighted ? [NSColor whiteColor] : [NSColor blackColor];
  base::scoped_nsobject<NSMutableParagraphStyle> textStyle(
      [[NSMutableParagraphStyle alloc] init]);
  [textStyle setAlignment:NSLeftTextAlignment];

  base::scoped_nsobject<NSAttributedString> attributedTitle(
      [[NSAttributedString alloc]
          initWithString:[self title]
              attributes:@{ NSParagraphStyleAttributeName : textStyle,
                            NSForegroundColorAttributeName : textColor }]);
  [self setAttributedTitle:attributedTitle];
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

// Creates the Guest / Add person / View all persons buttons.
- (NSView*)createOptionsViewWithRect:(NSRect)rect;

// Creates the account management view for the active profile.
- (NSView*)createCurrentProfileAccountsView:(NSRect)rect;

// Creates the list of accounts for the active profile.
- (NSView*)createAccountsListWithRect:(NSRect)rect;

// Creates the Gaia sign-in/add account view.
- (NSView*)createGaiaEmbeddedView;

// Creates a button with text given by |textResourceId|, an icon given by
// |imageResourceId| and with |action|. The icon |alternateImageResourceId| is
// displayed in the button's hovered and pressed states.
- (NSButton*)hoverButtonWithRect:(NSRect)rect
                  textResourceId:(int)textResourceId
                 imageResourceId:(int)imageResourceId
        alternateImageResourceId:(int)alternateImageResourceId
                          action:(SEL)action;

// Creates a generic link button with |title| and an |action| positioned at
// |frameOrigin|.
- (NSButton*)linkButtonWithTitle:(NSString*)title
                     frameOrigin:(NSPoint)frameOrigin
                          action:(SEL)action;

// Creates an email account button with |title|. If |canBeDeleted| is YES, then
// the button is clickable and has a remove icon.
- (NSButton*)accountButtonWithRect:(NSRect)rect
                             title:(const std::string&)title
                      canBeDeleted:(BOOL)canBeDeleted;

- (NSTextField*)labelWithTitle:(NSString*)title
                   frameOrigin:(NSPoint)frameOrigin;

@end

@implementation ProfileChooserController
- (BubbleViewMode) viewMode {
  return viewMode_;
}

- (IBAction)addNewProfile:(id)sender {
  profiles::CreateAndSwitchToNewProfile(
      browser_->host_desktop_type(),
      profiles::ProfileSwitchingDoneCallback(),
      ProfileMetrics::ADD_NEW_USER_ICON);
}

- (IBAction)switchToProfile:(id)sender {
  // Check the event flags to see if a new window should be created.
  bool always_create = ui::WindowOpenDispositionFromNSEvent(
      [NSApp currentEvent]) == NEW_WINDOW;
  avatarMenu_->SwitchToProfile([sender tag], always_create,
                               ProfileMetrics::SWITCH_PROFILE_ICON);
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
  [self initMenuContentsWithView:PROFILE_CHOOSER_VIEW];
}

- (void)cleanUpEmbeddedViewContents {
  webContents_.reset();
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
    tutorialShowing_ = false;
    observer_.reset(new ActiveProfileObserverBridge(self, browser_));

    avatarMenu_.reset(new AvatarMenu(
        &g_browser_process->profile_manager()->GetProfileInfoCache(),
        observer_.get(),
        browser_));
    avatarMenu_->RebuildMenu();

    // Guest profiles do not have a token service.
    isGuestSession_ = browser_->profile()->IsGuestSession();

    [[self bubble] setArrowLocation:info_bubble::kTopRight];
    [self initMenuContentsWithView:viewMode_];
  }

  return self;
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

  NSView* tutorialView = nil;
  NSView* currentProfileView = nil;
  base::scoped_nsobject<NSMutableArray> otherProfiles(
      [[NSMutableArray alloc] init]);

  // Loop over the profiles in reverse, so that they are sorted by their
  // y-coordinate, and separate them into active and "other" profiles.
  for (int i = avatarMenu_->GetNumberOfItems() - 1; i >= 0; --i) {
    const AvatarMenu::Item& item = avatarMenu_->GetItemAt(i);
    if (item.active) {
      if (viewMode_ == PROFILE_CHOOSER_VIEW)
        tutorialView = [self createTutorialViewIfNeeded:item];
      currentProfileView = [self createCurrentProfileView:item];
    } else {
      [otherProfiles addObject:[self createOtherProfileView:i]];
    }
  }
  if (!currentProfileView)  // Guest windows don't have an active profile.
    currentProfileView = [self createGuestProfileView];

  // |yOffset| is the next position at which to draw in |contentView|
  // coordinates.
  CGFloat yOffset = kSmallVerticalSpacing;

  // Guest / Add Person / View All Persons buttons.
  NSView* optionsView = [self createOptionsViewWithRect:
      NSMakeRect(0, yOffset, kFixedMenuWidth, 0)];
  [contentView addSubview:optionsView];
  yOffset = NSMaxY([optionsView frame]) + kSmallVerticalSpacing;

  NSBox* separator = [self separatorWithFrame:
      NSMakeRect(0, yOffset, kFixedMenuWidth, 0)];
  [contentView addSubview:separator];
  yOffset = NSMaxY([separator frame]) + kVerticalSpacing;

  if (viewToDisplay == PROFILE_CHOOSER_VIEW) {
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
  } else if (viewToDisplay == ACCOUNT_MANAGEMENT_VIEW) {
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
    NSBox* accountsSeparator = [self separatorWithFrame:
        NSMakeRect(0, yOffset, kFixedMenuWidth, 0)];
    [contentView addSubview:accountsSeparator];
    yOffset = NSMaxY([accountsSeparator frame]) + kVerticalSpacing;

    [tutorialView setFrameOrigin:NSMakePoint(kHorizontalSpacing,
                                             yOffset)];
    [contentView addSubview:tutorialView];
    yOffset = NSMaxY([tutorialView frame]) + kVerticalSpacing;
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

  CGFloat availableWidth = kFixedMenuWidth - 2 * kHorizontalSpacing;
  base::scoped_nsobject<NSView> container([[NSView alloc]
      initWithFrame:NSMakeRect(0, 0, availableWidth, 0)]);
  CGFloat yOffset = 0;

  // Adds links and buttons at the bottom.
  NSButton* learnMoreLink =
      [self linkButtonWithTitle:l10n_util::GetNSString(IDS_LEARN_MORE)
                    frameOrigin:NSMakePoint(0, yOffset)
                         action:@selector(openTutorialLearnMoreURL:)];
  [container addSubview:learnMoreLink];

  base::scoped_nsobject<NSButton> tutorialOkButton([[BlueLabelButton alloc]
      initWithFrame:NSZeroRect]);
  [tutorialOkButton setTitle:l10n_util::GetNSString(
      IDS_PROFILES_SIGNIN_TUTORIAL_OK_BUTTON)];
  [tutorialOkButton setTarget:self];
  [tutorialOkButton setAction:@selector(dismissTutorial:)];
  [tutorialOkButton sizeToFit];
  [tutorialOkButton setFrameOrigin:NSMakePoint(
      availableWidth - NSWidth([tutorialOkButton frame]), yOffset)];
  [container addSubview:tutorialOkButton];

  yOffset = std::max(NSMaxY([learnMoreLink frame]),
                     NSMaxY([tutorialOkButton frame])) + kVerticalSpacing;

  // Adds body content consisting of three bulleted lines.
  const int kTextHorizIndentation = 10;
  NSTextField* bulletLabel =
      [self labelWithTitle:l10n_util::GetNSString(
          IDS_PROFILES_SIGNIN_TUTORIAL_CONTENT_TEXT)
               frameOrigin:NSMakePoint(kTextHorizIndentation, yOffset)];
  [bulletLabel setFrameSize:NSMakeSize(availableWidth,
      NSHeight([bulletLabel frame]))];
  [container addSubview:bulletLabel];
  yOffset = NSMaxY([bulletLabel frame]) + kSmallVerticalSpacing;

  // Adds body header.
  NSTextField* contentHeaderLabel =
      [self labelWithTitle:l10n_util::GetNSString(
          IDS_PROFILES_SIGNIN_TUTORIAL_CONTENT_HEADER)
               frameOrigin:NSMakePoint(0, yOffset)];
  [contentHeaderLabel setFrameSize:NSMakeSize(availableWidth, 0)];
  [GTMUILocalizerAndLayoutTweaker sizeToFitFixedWidthTextField:
      contentHeaderLabel];
  [container addSubview:contentHeaderLabel];
  yOffset = NSMaxY([contentHeaderLabel frame]) + kSmallVerticalSpacing;

  // Adds title.
  NSTextField* titleLabel =
      [self labelWithTitle:l10n_util::GetNSStringF(
          IDS_PROFILES_SIGNIN_TUTORIAL_TITLE,
          profiles::GetAvatarNameForProfile(profile))
               frameOrigin:NSMakePoint(0, yOffset)];
  [titleLabel setFont:[NSFont labelFontOfSize:kTitleFontSize]];
  [[titleLabel cell] setTextColor:
      gfx::SkColorToCalibratedNSColor(chrome_style::GetLinkColor())];
  [titleLabel sizeToFit];
  [titleLabel setFrameSize:
      NSMakeSize(availableWidth, NSHeight([titleLabel frame]))];
  [container addSubview:titleLabel];
  yOffset = NSMaxY([titleLabel frame]);

  [container setFrameSize:NSMakeSize(kFixedMenuWidth, yOffset)];
  return container.autorelease();
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
  if (!isGuestSession_ && viewMode_ == PROFILE_CHOOSER_VIEW) {
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

- (NSView*)createOptionsViewWithRect:(NSRect)rect {
  NSRect viewRect = NSMakeRect(0, 0, rect.size.width, kBlueButtonHeight);
  NSButton* allUsersButton =
      [self hoverButtonWithRect:viewRect
                 textResourceId:IDS_PROFILES_ALL_PEOPLE_BUTTON
                imageResourceId:IDR_ICON_PROFILES_ADD_USER
       alternateImageResourceId:IDR_ICON_PROFILES_ADD_USER_WHITE
                         action:@selector(showUserManager:)];
  viewRect.origin.y = NSMaxY([allUsersButton frame]);

  NSButton* addUserButton =
      [self hoverButtonWithRect:viewRect
                 textResourceId:IDS_PROFILES_ADD_PERSON_BUTTON
                imageResourceId:IDR_ICON_PROFILES_ADD_USER
       alternateImageResourceId:IDR_ICON_PROFILES_ADD_USER_WHITE
                         action:@selector(addNewProfile:)];
  viewRect.origin.y = NSMaxY([addUserButton frame]);

  int guestButtonText = isGuestSession_ ? IDS_PROFILES_EXIT_GUEST_BUTTON :
                                          IDS_PROFILES_GUEST_BUTTON;
  SEL guestButtonAction = isGuestSession_ ? @selector(exitGuestProfile:) :
                                            @selector(switchToGuestProfile:);
  NSButton* guestButton =
      [self hoverButtonWithRect:viewRect
                 textResourceId:guestButtonText
                imageResourceId:IDR_ICON_PROFILES_BROWSE_GUEST
       alternateImageResourceId:IDR_ICON_PROFILES_BROWSE_GUEST_WHITE
                         action:guestButtonAction];
  rect.size.height = NSMaxY([guestButton frame]);
  base::scoped_nsobject<NSView> container([[NSView alloc]
      initWithFrame:rect]);
  [container setSubviews:@[allUsersButton, addUserButton, guestButton]];
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
  NSButton* accountButton = [self accountButtonWithRect:rect
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
      signin::GetPromoURL(
          source, false /* auto_close */, true /* is_constrained */),
      content::Referrer(),
      content::PAGE_TRANSITION_AUTO_TOPLEVEL,
      std::string());
  NSView* webview = webContents_->GetView()->GetNativeView();
  [webview setFrameSize:NSMakeSize(kMinGaiaViewWidth, kMinGaiaViewHeight)];
  return webview;
}

- (NSButton*)hoverButtonWithRect:(NSRect)rect
                  textResourceId:(int)textResourceId
                 imageResourceId:(int)imageResourceId
        alternateImageResourceId:(int)alternateImageResourceId
                          action:(SEL)action  {
  base::scoped_nsobject<BackgroundColorHoverButton> button(
      [[BackgroundColorHoverButton alloc] initWithFrame:rect]);

  [button setTitle:l10n_util::GetNSString(textResourceId)];
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

- (NSTextField*)labelWithTitle:(NSString*)title
                   frameOrigin:(NSPoint)frameOrigin {
  base::scoped_nsobject<NSTextField> label(
      [[NSTextField alloc] initWithFrame:NSZeroRect]);
  [label setStringValue:title];
  [label setEditable:NO];
  [label setAlignment:NSLeftTextAlignment];
  [label setBezeled:NO];
  [label setFont:[NSFont labelFontOfSize:kTextFontSize]];
  [label setFrameOrigin:frameOrigin];
  [label sizeToFit];

  return label.autorelease();
}
@end

