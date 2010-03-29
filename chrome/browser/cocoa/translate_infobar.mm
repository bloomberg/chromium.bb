// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>
#import "chrome/browser/cocoa/translate_infobar.h"

#include "app/l10n_util.h"
#include "base/logging.h"  // for NOTREACHED()
#include "base/mac_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/app/chrome_dll_resource.h"
#import "chrome/browser/cocoa/hover_close_button.h"
#include "chrome/browser/cocoa/infobar.h"
#import "chrome/browser/cocoa/infobar_controller.h"
#import "chrome/browser/cocoa/infobar_gradient_view.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/notification_service.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "third_party/GTM/AppKit/GTMUILocalizerAndLayoutTweaker.h"

// Colors for translate infobar gradient background.
const int kBlueTopColor[] = {0xDA, 0xE7, 0xF9};
const int kBlueBottomColor[] = {0xB3, 0xCA, 0xE7};

#pragma mark Anonymous helper functions.
namespace {

// Move the |toMove| view |spacing| pixels before/after the |anchor| view.
// |after| signifies the side of |anchor| on which to place |toMove|.
void MoveControl(NSView* anchor, NSView* toMove, int spacing, bool after) {
  NSRect anchorFrame = [anchor frame];
  NSRect toMoveFrame = [toMove frame];

  // At the time of this writing, OS X doesn't natively support BiDi UIs, but
  // it doesn't hurt to be forward looking.
  bool toRight = after;

  if (toRight) {
    toMoveFrame.origin.x = NSMaxX(anchorFrame) + spacing;
  } else {
    // Place toMove to theleft of anchor.
    toMoveFrame.origin.x = NSMinX(anchorFrame) -
        spacing - NSWidth(toMoveFrame);
  }
  [toMove setFrame:toMoveFrame];
}

// Vertically center |toMove| in its container.
void VerticallyCenterView(NSView *toMove) {
  NSRect superViewFrame = [[toMove superview] frame];
  NSRect viewFrame = [toMove frame];

  viewFrame.origin.y =
      floor((NSHeight(superViewFrame) - NSHeight(viewFrame))/2.0);
  [toMove setFrame:viewFrame];
}

// Check that the control |before| is ordered visually before the |after|
// control.
// Also, check that there is space between them.
bool VerifyControlOrderAndSpacing(id before, id after) {
  NSRect beforeFrame = [before frame];
  NSRect afterFrame = [after frame];
  NSUInteger spaceBetweenControls = -1;

    spaceBetweenControls = NSMaxX(beforeFrame) - NSMinX(afterFrame);
    // RTL case to be used when we have an RTL version of this UI.
    // spaceBetweenControls = NSMaxX(afterFrame) - NSMinX(beforeFrame);

  return (spaceBetweenControls >= 0);
}

// Creates a label control in the style we need for the translate infobar's
// labels within |bounds|.
NSTextField* CreateLabel(NSRect bounds) {
  NSTextField* ret = [[NSTextField alloc] initWithFrame:bounds];
  [ret setEditable:NO];
  [ret setDrawsBackground:NO];
  [ret setBordered:NO];
  return ret;
}

// Adds an item with the specified properties to |menu|.
void AddMenuItem(NSMenu *menu, id target, NSString* title, int tag,
    bool checked) {
  NSMenuItem* item = [[[NSMenuItem alloc]
      initWithTitle:title
             action:@selector(menuItemSelected:)
      keyEquivalent:@""] autorelease];
  [item setTag:tag];
  [menu addItem:item];
  [item setTarget:target];
  if (checked)
    [item setState:NSOnState];
}

}  // namespace

#pragma mark TranslateInfoBarMenuModel class definition
// Bridge class to handle interfacing with menu controllers from popup
// menus in infobar.
class TranslateInfoBarMenuModel : public menus::SimpleMenuModel::Delegate {
 public:
  TranslateInfoBarMenuModel(TranslateInfoBarDelegate* delegate,
                            TranslateInfoBarController* controller) :
      translate_delegate_(delegate),
      controller_(controller) {}

  // Overridden from menus::SimpleMenuModel::Delegate:
  virtual bool IsCommandIdChecked(int command_id) const;
  virtual bool IsCommandIdEnabled(int command_id) const;
  virtual bool GetAcceleratorForCommandId(int command_id,
                                          menus::Accelerator* accelerator);
  virtual void ExecuteCommand(int command_id);

 private:
  TranslateInfoBarDelegate* translate_delegate_;  // weak
  TranslateInfoBarController* controller_; // weak
  DISALLOW_COPY_AND_ASSIGN(TranslateInfoBarMenuModel);
};

#pragma mark TranslateNotificationObserverBridge class definition
// Bridge class to allow obj-c TranslateInfoBarController to observe
// notifications.
class TranslateNotificationObserverBridge :
    public NotificationObserver {
 public:
  TranslateNotificationObserverBridge(
      TranslateInfoBarDelegate* delegate,
      TranslateInfoBarController* controller);

  // Overridden from NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  TranslateInfoBarDelegate* translate_delegate_;  // weak
  TranslateInfoBarController* controller_; // weak
  NotificationRegistrar notification_registrar_;
  DISALLOW_COPY_AND_ASSIGN(TranslateNotificationObserverBridge);
};

@interface TranslateInfoBarController (Private)

// Returns the main translate delegate.
- (TranslateInfoBarDelegate*)delegate;

// Make the infobar blue.
- (void)setInfoBarGradientColor;

// Reloads text for all labels for the current state.
- (void)loadLabelText;

// Resizes controls and hides/shows them based on state transition.
// Called before layout;
- (void)resizeAndSetControlVisibility;

// Move all the currently visible views into the correct place for the
// current mode.
- (void)layout;

// Create all the various controls we need for the toolbar.
- (void)constructViews;

// Called when the source or target language selection changes in a menu.
// |newLanguageIdx| is the index of the newly selected item in the appropriate
// menu.
- (void)sourceLanguageModified:(NSInteger)newLanguageIdx;
- (void)targetLanguageModified:(NSInteger)newLanguageIdx;

// Called when the source or target language have changed to update the
// model state and refresh the GUI.
- (void)languageModified;

// Completely rebuild "from" and "to" language menus from the data model.
- (void)populateLanguageMenus;

@end

#pragma mark TranslateInfoBarController class
@implementation TranslateInfoBarController

- (id)initWithDelegate:(InfoBarDelegate*)delegate {
  if ((self = [super initWithDelegate:delegate])) {
      observer_bridge_.reset(
          new TranslateNotificationObserverBridge([self delegate], self));

      original_language_menu_model_.reset(
          new LanguagesMenuModel(menu_model_.get(), [self delegate],
              /*original_language=*/true));

      target_language_menu_model_.reset(
          new LanguagesMenuModel(menu_model_.get(), [self delegate],
              /*original_language=*/false));

      menu_model_.reset(new TranslateInfoBarMenuModel([self delegate], self));
  }
  return self;
}

- (TranslateInfoBarDelegate*)delegate {
  return reinterpret_cast<TranslateInfoBarDelegate*>(delegate_);
}

- (void)constructViews {
  // Using a zero or very large frame causes GTMUILocalizerAndLayoutTweaker
  // to not resize the view properly so we take the bounds of the first label
  // which is contained in the nib.
  NSRect bogusFrame = [label_ frame];
  label2_.reset(CreateLabel(bogusFrame));
  label3_.reset(CreateLabel(bogusFrame));
  translatingLabel_.reset(CreateLabel(bogusFrame));

  optionsPopUp_.reset([[NSPopUpButton alloc] initWithFrame:bogusFrame
                                                 pullsDown:YES]);
  fromLanguagePopUp_.reset([[NSPopUpButton alloc] initWithFrame:bogusFrame
                                                      pullsDown:NO]);
  toLanguagePopUp_.reset([[NSPopUpButton alloc] initWithFrame:bogusFrame
                                                     pullsDown:NO]);
}

- (void)sourceLanguageModified:(NSInteger)newLanguageIdx {
  DCHECK_GT(newLanguageIdx, 0);

  if (newLanguageIdx == [self delegate]->original_lang_index())
    return;

  [self delegate]->ModifyOriginalLanguage(newLanguageIdx);
  [fromLanguagePopUp_ selectItemAtIndex:newLanguageIdx];

  [self languageModified];
}

- (void)targetLanguageModified:(NSInteger)newLanguageIdx {
  DCHECK_GT(newLanguageIdx, 0);
  if (newLanguageIdx == [self delegate]->target_lang_index())
    return;

  [self delegate]->ModifyTargetLanguage(newLanguageIdx);
  [toLanguagePopUp_ selectItemAtIndex:newLanguageIdx];
  [self languageModified];
}

- (void)languageModified {
  // If necessary, update state and translate.
  [self rebuildOptionsMenu];

  // Selecting an item from the "from language" menu in the before translate
  // phase shouldn't trigger translation - http://crbug.com/36666
  if ([self delegate]->state() == TranslateInfoBarDelegate::kAfterTranslate) {
    [self delegate]->Translate();
    [self updateState];
  }
}

- (void)updateState {
  // Fixup our GUI.
  [self loadLabelText];

  [self resizeAndSetControlVisibility];
  [self layout];
}

- (void)setInfoBarGradientColor {
  // Use blue gradient for the infobars.
  NSColor* startingColor =
    [NSColor colorWithCalibratedRed:kBlueTopColor[0] / 255.0
                              green:kBlueTopColor[1] / 255.0
                               blue:kBlueTopColor[2] / 255.0
                              alpha:1.0];
  NSColor* endingColor =
      [NSColor colorWithCalibratedRed:kBlueBottomColor[0] / 255.0
                                green:kBlueBottomColor[1] / 255.0
                                 blue:kBlueBottomColor[2] / 255.0
                                alpha:1.0];
  NSGradient* translateInfoBarGradient =
      [[[NSGradient alloc] initWithStartingColor:startingColor
                                     endingColor:endingColor] autorelease];

  [infoBarView_ setGradient:translateInfoBarGradient];
}

- (void)resizeAndSetControlVisibility {
  TranslateInfoBarDelegate::TranslateState state = [self delegate]->state();

  // Step 1: remove all controls from the infobar so we have a clean slate.
  NSArray *allControls = [NSArray arrayWithObjects:label2_.get(), label3_.get(),
      translatingLabel_.get(), fromLanguagePopUp_.get(), toLanguagePopUp_.get(),
      nil];

  for (NSControl* control in allControls) {
    if ([control superview])
      [control removeFromSuperview];
  }

  // OK & Cancel buttons are only visible in "before translate" mode.
  if (state != TranslateInfoBarDelegate::kBeforeTranslate) {
    // Removing okButton_ & cancelButton_ from the view may cause them
    // to be released and since we can still access them from other areas
    // in the code later, we need them to be nil when this happens.
    [okButton_ removeFromSuperview];
    okButton_ = nil;
    [cancelButton_ removeFromSuperview];
    cancelButton_ = nil;

  }

  // Step 2: Resize all visible controls and add them to the infobar.
  NSArray *visibleControls = nil;
  NSArray *visibleMenus = nil;

  switch (state) {
    case TranslateInfoBarDelegate::kBeforeTranslate:
      visibleControls = [NSArray arrayWithObjects:label_, okButton_,
          cancelButton_, label2_.get(), fromLanguagePopUp_.get(), nil];
      visibleMenus =  [NSArray arrayWithObjects:fromLanguagePopUp_.get(), nil];
      break;
    case TranslateInfoBarDelegate::kTranslating:
      visibleControls = [NSArray arrayWithObjects:label_, label2_.get(),
          translatingLabel_.get(), nil];
      visibleMenus =  [NSArray arrayWithObjects:fromLanguagePopUp_.get(), nil];
      break;
    case TranslateInfoBarDelegate::kAfterTranslate:
      visibleControls = [NSArray arrayWithObjects:label_, label2_.get(),
          fromLanguagePopUp_.get(), toLanguagePopUp_.get(), nil];
      visibleMenus =  [NSArray arrayWithObjects:fromLanguagePopUp_.get(),
          toLanguagePopUp_.get(), nil];
      break;
    default:
      NOTREACHED() << "Invalid translate infobar state";
      break;
  }

  if (numLabelsDisplayed_ >= 3) {
    visibleControls = [visibleControls arrayByAddingObject:label3_.get()];
  }

  for (NSControl* control in visibleControls) {
    [GTMUILocalizerAndLayoutTweaker sizeToFitView:control];
    [control setAutoresizingMask:NSViewMaxXMargin | NSViewMinYMargin |
        NSViewMaxYMargin];

    // Need to check if a view is already attached since |label_| is always
    // parented and we don't want to add it again.
    if (![control superview])
      [infoBarView_ addSubview:control];
  }

  // Make "from" and "to" language popup menus the same size as the options
  // menu.
  // We don't autosize since some languages names are really long causing
  // the toolbar to overflow.
  NSRect optionsFrame = [optionsPopUp_ frame];
  for (NSPopUpButton* menuButton in visibleMenus) {
    [menuButton setFrame:optionsFrame];
    [menuButton setAutoresizingMask:NSViewMaxXMargin |
        NSViewMinYMargin | NSViewMaxYMargin];
    [infoBarView_ addSubview:menuButton];
  }
}

- (void)layout {
  TranslateInfoBarDelegate::TranslateState state = [self delegate]->state();

  if (state != TranslateInfoBarDelegate::kAfterTranslate) {
      // 3rd label is only displayed in some locales, but should never be
      // visible in this stage.
      // If it ever is visible then we need to move it into position here.
      DCHECK(numLabelsDisplayed_ < 3);
  }

  switch (state) {
    case  TranslateInfoBarDelegate::kBeforeTranslate:
      MoveControl(label_, fromLanguagePopUp_, 0, true);
      MoveControl(fromLanguagePopUp_, label2_, 0, true);
      MoveControl(label2_, okButton_, spaceBetweenControls_, true);
      MoveControl(okButton_, cancelButton_, spaceBetweenControls_, true);
      VerticallyCenterView(fromLanguagePopUp_);
      break;

    case TranslateInfoBarDelegate::kTranslating:
      MoveControl(label_, fromLanguagePopUp_, 0, true);
      MoveControl(fromLanguagePopUp_, label2_, 0, true);
      MoveControl(label2_, translatingLabel_, spaceBetweenControls_ * 2, true);
      VerticallyCenterView(fromLanguagePopUp_);
      break;

    case TranslateInfoBarDelegate::kAfterTranslate:
      MoveControl(label_, fromLanguagePopUp_, 0, true);
      MoveControl(fromLanguagePopUp_, label2_, 0, true);
      MoveControl(label2_, toLanguagePopUp_, 0, true);
      if (numLabelsDisplayed_ == 3)
        MoveControl(toLanguagePopUp_, label3_, 0, true);
      VerticallyCenterView(fromLanguagePopUp_);
      VerticallyCenterView(toLanguagePopUp_);
      break;

    default:
      NOTREACHED() << "Invalid translate infobar state";
      break;
  }
}

- (void) rebuildOptionsMenu {
  // The options model doesn't know how to handle state transitions, so rebuild
  // it each time through here.
  options_menu_model_.reset(
              new OptionsMenuModel(menu_model_.get(), [self delegate]));

  [optionsPopUp_ removeAllItems];
  // Set title.
  NSString* optionsLabel =
      l10n_util::GetNSString(IDS_TRANSLATE_INFOBAR_OPTIONS);
  [optionsPopUp_ addItemWithTitle:optionsLabel];

   // Populate options menu.
  NSMenu* optionsMenu = [optionsPopUp_ menu];
  for (int i = 0; i < options_menu_model_->GetItemCount(); ++i) {
    NSString* title = base::SysUTF16ToNSString(
        options_menu_model_->GetLabelAt(i));
    int cmd = options_menu_model_->GetCommandIdAt(i);
    bool checked = options_menu_model_->IsItemCheckedAt(i);
    AddMenuItem(optionsMenu, self, title, cmd, checked);
  }
}

- (void)populateLanguageMenus {
  NSMenu* originalLanguageMenu = [fromLanguagePopUp_ menu];
  int selectedIndex = [self delegate]->original_lang_index();
  for (int i = 0; i < original_language_menu_model_->GetItemCount(); ++i) {
    NSString* title = base::SysUTF16ToNSString(
        original_language_menu_model_->GetLabelAt(i));
    int cmd = original_language_menu_model_->GetCommandIdAt(i);
    bool checked = i == selectedIndex;
    AddMenuItem(originalLanguageMenu, self, title, cmd, checked);
  }
  [fromLanguagePopUp_ selectItemAtIndex:selectedIndex];

  NSMenu* targetLanguageMenu = [toLanguagePopUp_ menu];
  selectedIndex = [self delegate]->target_lang_index();
  for (int i = 0; i < target_language_menu_model_->GetItemCount(); ++i) {
    NSString* title = base::SysUTF16ToNSString(
        target_language_menu_model_->GetLabelAt(i));
    int cmd = target_language_menu_model_->GetCommandIdAt(i);
    bool checked = i == selectedIndex;
    if (checked)
      selectedIndex = i;
    AddMenuItem(targetLanguageMenu, self, title, cmd, checked);
  }
  [toLanguagePopUp_ selectItemAtIndex:selectedIndex];
}

- (void)loadLabelText {
  string16 message_text_utf16;
  std::vector<size_t> offsets;
  [self delegate]->GetMessageText([self delegate]->state(), &message_text_utf16,
      &offsets, &swappedLanguagePlaceholders_);

  NSString* message_text = base::SysUTF16ToNSString(message_text_utf16);
  NSRange label1Range = NSMakeRange(0, offsets[0]);
  NSString* label1Text = [message_text substringWithRange:label1Range];
  NSRange label2Range = NSMakeRange(offsets[0],
      offsets[1] - offsets[0]);
  NSString* label2Text = [message_text substringWithRange:label2Range];
  [self setLabelToMessage:label1Text];
  [label2_ setStringValue:label2Text];

  numLabelsDisplayed_ = 2;

  // If this locale requires a 3rd label for the status message.
  if (offsets.size() == 3) {
    NSRange label3Range = NSMakeRange(offsets[1],
        offsets[2] - offsets[1]);
    NSString* label3Text = [message_text substringWithRange:label3Range];
    [label3_ setStringValue:label3Text];
    numLabelsDisplayed_ = 3;
  }
}

- (void)addAdditionalControls {
  using l10n_util::GetNSString;
  using l10n_util::GetNSStringWithFixup;

  // Get layout information from the NIB.
  NSRect okButtonFrame = [okButton_ frame];
  NSRect cancelButtonFrame = [cancelButton_ frame];
  spaceBetweenControls_ = NSMinX(cancelButtonFrame) - NSMaxX(okButtonFrame);

  // Set infobar background color.
  [self setInfoBarGradientColor];

  // Instantiate additional controls.
  [self constructViews];

  [self populateLanguageMenus];

  // Set OK & Cancel text.
  [okButton_ setTitle:GetNSStringWithFixup(IDS_TRANSLATE_INFOBAR_ACCEPT)];
  [cancelButton_ setTitle:GetNSStringWithFixup(IDS_TRANSLATE_INFOBAR_DENY)];
  [translatingLabel_
      setStringValue:GetNSString(IDS_TRANSLATE_INFOBAR_TRANSLATING)];

  // Add and configure controls that are visible in all modes.
  [optionsPopUp_ setAutoresizingMask:NSViewMinXMargin | NSViewMinYMargin |
          NSViewMaxYMargin];
  // Add "options" popup z-ordered below all other controls so when we
  // resize the toolbar it doesn't hide them.
  [infoBarView_ addSubview:optionsPopUp_
                positioned:NSWindowBelow
                relativeTo:nil];
  [GTMUILocalizerAndLayoutTweaker sizeToFitView:optionsPopUp_];
  MoveControl(closeButton_, optionsPopUp_, spaceBetweenControls_, false);
  VerticallyCenterView(optionsPopUp_);

  // Show and place GUI elements.
  [self updateState];
}

// Called when "Translate" button is clicked.
- (IBAction)ok:(id)sender {
  DCHECK(
      [self delegate]->state() == TranslateInfoBarDelegate::kBeforeTranslate);
  [self delegate]->Translate();
  [self updateState];
}

// Called when someone clicks on the "Nope" button.
- (IBAction)cancel:(id)sender {
  DCHECK(
      [self delegate]->state() == TranslateInfoBarDelegate::kBeforeTranslate);
  [self delegate]->TranslationDeclined();
  [super dismiss:nil];
}

- (void)menuItemSelected:(id)item {
  if ([item respondsToSelector:@selector(tag)]) {
    int cmd = [item tag];
    menu_model_->ExecuteCommand(cmd);
  } else {
    NOTREACHED();
  }
}

#pragma mark TestingAPI
- (NSMenu*)optionsMenu {
  return [optionsPopUp_ menu];
}

- (bool)verifyLayout:(TranslateInfoBarDelegate::TranslateState)state {
  NSArray* allControls = [NSArray arrayWithObjects:label_, label2_.get(),
      label3_.get(), translatingLabel_.get(), fromLanguagePopUp_.get(),
      toLanguagePopUp_.get(), optionsPopUp_.get(), closeButton_, nil];

  // Array of all visible controls ordered from start -> end.
  NSArray* visibleControls = nil;

  switch (state) {
    case TranslateInfoBarDelegate::kBeforeTranslate:
      visibleControls = [NSArray arrayWithObjects:label_,
          fromLanguagePopUp_.get(), label2_.get(), optionsPopUp_.get(),
          closeButton_, nil];
      break;
    case TranslateInfoBarDelegate::kTranslating:
      visibleControls = [NSArray arrayWithObjects:label_,
          fromLanguagePopUp_.get(), label2_.get(), translatingLabel_.get(),
          optionsPopUp_.get(), closeButton_, nil];
      break;
    case TranslateInfoBarDelegate::kAfterTranslate:
      visibleControls = [NSArray arrayWithObjects:label_,
          fromLanguagePopUp_.get(), label2_.get(), toLanguagePopUp_.get(),
          optionsPopUp_.get(), closeButton_, nil];
      break;
    default:
      NOTREACHED() << "Unknown state";
      return false;
  }

  // Step 1: Make sure control visibility is what we expect.
  for (NSUInteger i = 0; i < [allControls count]; ++i) {
    id control = [allControls objectAtIndex:i];
    bool hasSuperView = [control superview];
    bool expectedVisibility = [visibleControls containsObject:control];
    if (expectedVisibility != hasSuperView) {
      LOG(ERROR) <<
          "Control @" << i << (hasSuperView ? " has" : " doesn't have") <<
          "a superview" << base::SysNSStringToUTF8([control description]);
      return false;
    }
  }

  // Step 2: Check that controls are ordered correctly with no overlap.
  id previousControl = nil;
  for (NSUInteger i = 0; i < [allControls count]; ++i) {
    id control = [allControls objectAtIndex:i];
    if (!VerifyControlOrderAndSpacing(previousControl, control)) {
      LOG(ERROR) <<
          "Control @" << i << " not ordered correctly: " <<
          base::SysNSStringToUTF8([control description]);
      return false;
    }
    previousControl = control;
  }
  return true;
}

@end

#pragma mark CreateInfoBar implementation.
InfoBar* TranslateInfoBarDelegate::CreateInfoBar() {
  TranslateInfoBarController* controller =
      [[TranslateInfoBarController alloc] initWithDelegate:this];
  return new InfoBar(controller);
}

#pragma mark menus::SimpleMenuModel::Delegates

bool TranslateInfoBarMenuModel::IsCommandIdChecked(int command_id) const {
  switch (command_id) {
    case IDC_TRANSLATE_OPTIONS_NEVER_TRANSLATE_LANG :
      return translate_delegate_->IsLanguageBlacklisted();

    case IDC_TRANSLATE_OPTIONS_NEVER_TRANSLATE_SITE :
      return translate_delegate_->IsSiteBlacklisted();

    case IDC_TRANSLATE_OPTIONS_ALWAYS :
      return translate_delegate_->ShouldAlwaysTranslate();

    default:
      NOTREACHED() << "Invalid command_id from menu";
      break;
  }
  return false;
}

bool TranslateInfoBarMenuModel::IsCommandIdEnabled(int command_id) const {
  return true;
}

bool TranslateInfoBarMenuModel::GetAcceleratorForCommandId(int command_id,
    menus::Accelerator* accelerator) {
  return false;
}

void TranslateInfoBarMenuModel::ExecuteCommand(int command_id) {
  if (command_id >= IDC_TRANSLATE_TARGET_LANGUAGE_BASE) {
    int language_command_id =
        command_id - IDC_TRANSLATE_TARGET_LANGUAGE_BASE;
    [controller_
        targetLanguageModified:language_command_id];
  } else if (command_id >= IDC_TRANSLATE_ORIGINAL_LANGUAGE_BASE) {
    int language_command_id =
        command_id - IDC_TRANSLATE_ORIGINAL_LANGUAGE_BASE;
    [controller_
        sourceLanguageModified:language_command_id];
  } else {
    switch (command_id) {
      case IDC_TRANSLATE_OPTIONS_NEVER_TRANSLATE_LANG:
        translate_delegate_->ToggleLanguageBlacklist();
        break;

      case IDC_TRANSLATE_OPTIONS_NEVER_TRANSLATE_SITE:
        translate_delegate_->ToggleSiteBlacklist();
        break;

      case IDC_TRANSLATE_OPTIONS_ALWAYS:
        translate_delegate_->ToggleAlwaysTranslate();
        break;

      case IDC_TRANSLATE_OPTIONS_ABOUT: {
        TabContents* tab_contents = translate_delegate_->tab_contents();
        if (tab_contents) {
          string16 url = l10n_util::GetStringUTF16(
              IDS_ABOUT_GOOGLE_TRANSLATE_URL);
          tab_contents->OpenURL(GURL(url), GURL(), NEW_FOREGROUND_TAB,
              PageTransition::LINK);
        }
        break;
      }

      default:
        NOTREACHED() << "Invalid command id from menu.";
        break;
    }
  }
}

# pragma mark TranslateInfoBarNotificationObserverBridge

TranslateNotificationObserverBridge::TranslateNotificationObserverBridge(
    TranslateInfoBarDelegate* delegate,
    TranslateInfoBarController* controller) :
        translate_delegate_(delegate),
        controller_(controller) {
  // Register for PAGE_TRANSLATED notification.
  notification_registrar_.Add(this, NotificationType::PAGE_TRANSLATED,
      Source<TabContents>(translate_delegate_->tab_contents()));
};

void TranslateNotificationObserverBridge::Observe(NotificationType type,
      const NotificationSource& source, const NotificationDetails& details) {
  if (type.value != NotificationType::PAGE_TRANSLATED)
    return;
  TabContents* tab = Source<TabContents>(source).ptr();
  if (tab != translate_delegate_->tab_contents())
    return;
  [controller_ updateState];

}
