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
#include "chrome/browser/translate/translate_infobars_delegates.h"
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

// Main function to update the toolbar graphic state and data model after
// the state has changed.
// Controls are moved around as needed and visibility changed to match the
// current state.
// |newState| is the state we're transitioning to.
// |initialDisplay| is true if we're being called when initially displaying
// and not because of a state transition.
- (void)updateState:(TranslateInfoBarDelegate::TranslateState)newState
     initialDisplay:(bool)initialDisplay;

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

// Teardown and rebuild the options menu.
- (void)rebuildOptionsMenu;

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
  if ([self delegate]->state() == TranslateInfoBarDelegate::kAfterTranslate)
    [self delegate]->Translate();
}

- (void)updateState:(TranslateInfoBarDelegate::TranslateState)newState
     initialDisplay:(bool)initialDisplay {
  // If this isn't a call from the constructor, only procede if the state
  // has changed.
  if (!initialDisplay && newState == [self delegate]->state())
    return;

  // Update the data model.
  [self delegate]->UpdateState(newState);

  // Fixup our GUI.
  TranslateInfoBarDelegate::TranslateState state = [self delegate]->state();

  if (state != TranslateInfoBarDelegate::kTranslating) {
    [self loadLabelText];
    [self rebuildOptionsMenu];
  }

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
  switch ([self delegate]->state()) {
    case TranslateInfoBarDelegate::kBeforeTranslate: {
      // Size to fit and set resize attributes for all visible controls.
      NSArray *controls = [NSArray arrayWithObjects:okButton_, cancelButton_,
          label_, label2_.get(), optionsPopUp_.get(), nil];
      for (NSControl* control in controls) {
        [GTMUILocalizerAndLayoutTweaker sizeToFitView:control];
        [control setAutoresizingMask:NSViewMaxXMargin | NSViewMinYMargin |
            NSViewMaxYMargin];
      }
      [fromLanguagePopUp_ setAutoresizingMask:NSViewMaxXMargin |
          NSViewMinYMargin | NSViewMaxYMargin];
      [optionsPopUp_ setAutoresizingMask:NSViewMinXMargin | NSViewMinYMargin |
          NSViewMaxYMargin];

      // Make "from language" popup menu the same size as the options menu.
      // We don't autosize since some languages names are really long causing
      // the toolbar to overflow.
      NSRect optionsFrame = [optionsPopUp_ frame];
      [fromLanguagePopUp_ setFrame:optionsFrame];

      [infoBarView_ addSubview:label2_];
      [infoBarView_ addSubview:fromLanguagePopUp_];

      // Add "options" popup z-ordered below all other controls so when we
      // resize the toolbar it doesn't hide them.
      [infoBarView_ addSubview:optionsPopUp_
                    positioned:NSWindowBelow
                    relativeTo:nil];
      break;
    }

    case TranslateInfoBarDelegate::kTranslating:
      [okButton_ removeFromSuperview];
      [cancelButton_ removeFromSuperview];
      [infoBarView_ addSubview:translatingLabel_];
      [GTMUILocalizerAndLayoutTweaker sizeToFitView:translatingLabel_];
      [translatingLabel_ setAutoresizingMask:NSViewMaxXMargin |
          NSViewMinYMargin | NSViewMaxYMargin];
      break;

    case TranslateInfoBarDelegate::kAfterTranslate: {
      [translatingLabel_ removeFromSuperview];

      [infoBarView_ addSubview:toLanguagePopUp_];
      if (numLabelsDisplayed_ == 3)
        [infoBarView_ addSubview:label3_];

      // Label contents may have changed.
      NSArray *controls = [NSArray arrayWithObjects:label_, label2_.get(),
          label3_.get(), nil];
      for (NSControl* control in controls) {
        [GTMUILocalizerAndLayoutTweaker sizeToFitView:control];
        [control setAutoresizingMask:NSViewMaxXMargin | NSViewMinYMargin |
            NSViewMaxYMargin];
      }

      // See note on sizeing fromLanguagePopUp_ above.
      NSRect optionsFrame = [optionsPopUp_ frame];
      [toLanguagePopUp_ setFrame:optionsFrame];
      break;
    }

    default:
      NOTREACHED() << "Invalid translate state change";
      break;
  }
}

- (void)layout {
  // This method assumes that the toolbar starts out in |kBeforeTranslate|
  // translate state and progresses unidirectionally and contiguously through
  // the other states.
  // Under these assumptions, the changes we make can be cumulative rather than
  // repositioning elements multiple times.
  switch ([self delegate]->state()) {
    case TranslateInfoBarDelegate::kBeforeTranslate:
      MoveControl(label_, fromLanguagePopUp_, 0, true);
      MoveControl(fromLanguagePopUp_, label2_, 0, true);
      MoveControl(label2_, okButton_, spaceBetweenControls_, true);
      MoveControl(okButton_, cancelButton_, spaceBetweenControls_, true);

      // 3rd label is only displayed in some locales, but should never be
      // visible in the before translate stage.
      // If it ever is visible then we need to move it into position here.
      DCHECK(numLabelsDisplayed_ < 3);

      MoveControl(closeButton_, optionsPopUp_, spaceBetweenControls_, false);

      // Vertically center popup menus.
      VerticallyCenterView(fromLanguagePopUp_);
      VerticallyCenterView(optionsPopUp_);
      break;

    case TranslateInfoBarDelegate::kTranslating:
      MoveControl(label2_, translatingLabel_, spaceBetweenControls_ * 2, true);
      break;


    case TranslateInfoBarDelegate::kAfterTranslate:
      MoveControl(label_, fromLanguagePopUp_, 0, true);
      MoveControl(fromLanguagePopUp_, label2_, 0, true);
      MoveControl(label2_, toLanguagePopUp_, 0, true);
      if (numLabelsDisplayed_ == 3)
        MoveControl(toLanguagePopUp_, label3_, 0, true);
      VerticallyCenterView(toLanguagePopUp_);
      break;

    default:
      NOTREACHED() << "Invalid translate state change";
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
  [self delegate]->GetMessageText(&message_text_utf16, &offsets,
      &swappedLanguagePlaceholders_);

  NSString* message_text = base::SysUTF16ToNSString(message_text_utf16);
  NSRange label1Range = NSMakeRange(0, offsets[0]);
  NSString* label1Text = [message_text substringWithRange:label1Range];
  NSRange label2Range = NSMakeRange(offsets[0],
      offsets[1] - offsets[0]);
  NSString* label2Text = [message_text substringWithRange:label2Range];
  [label_ setStringValue:label1Text];
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

  // Show and place GUI elements.
  [self updateState:TranslateInfoBarDelegate::kBeforeTranslate
     initialDisplay:true];
}

// Called when "Translate" button is clicked.
- (IBAction)ok:(id)sender {
  DCHECK(
      [self delegate]->state() == TranslateInfoBarDelegate::kBeforeTranslate);
  [self updateState:TranslateInfoBarDelegate::kTranslating
     initialDisplay:false];
  [self delegate]->Translate();
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
  [controller_ updateState:TranslateInfoBarDelegate::kAfterTranslate
            initialDisplay:false];

}
