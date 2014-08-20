// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/translate/translate_bubble_controller.h"

#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/bubble_combobox.h"
#import "chrome/browser/ui/cocoa/info_bubble_view.h"
#import "chrome/browser/ui/cocoa/info_bubble_window.h"
#import "chrome/browser/ui/cocoa/toolbar/toolbar_controller.h"
#include "chrome/browser/ui/translate/language_combobox_model.h"
#include "chrome/browser/ui/translate/translate_bubble_model_impl.h"
#include "components/translate/core/browser/translate_ui_delegate.h"
#include "content/public/browser/browser_context.h"
#include "grit/generated_resources.h"
#import "ui/base/cocoa/controls/hyperlink_button_cell.h"
#import "ui/base/cocoa/window_size_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/combobox_model.h"

// TODO(hajimehoshi): This class is almost same as that of views. Refactor them.
class TranslateDenialComboboxModel : public ui::ComboboxModel {
 public:
  explicit TranslateDenialComboboxModel(
      const base::string16& original_language_name) {
    // Dummy menu item, which is shown on the top of a NSPopUpButton. The top
    // text of the denial pop up menu should be IDS_TRANSLATE_BUBBLE_DENY, while
    // it is impossible to use it here because NSPopUpButtons' addItemWithTitle
    // removes a duplicated menu item. Instead, the title will be set later by
    // NSMenuItem's setTitle.
    items_.push_back(base::string16());

    // Menu items in the drop down menu.
    items_.push_back(l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_DENY));
    items_.push_back(l10n_util::GetStringFUTF16(
        IDS_TRANSLATE_BUBBLE_NEVER_TRANSLATE_LANG,
        original_language_name));
    items_.push_back(l10n_util::GetStringUTF16(
        IDS_TRANSLATE_BUBBLE_NEVER_TRANSLATE_SITE));
  }
  virtual ~TranslateDenialComboboxModel() {}

 private:
  // ComboboxModel:
  virtual int GetItemCount() const OVERRIDE {
    return items_.size();
  }
  virtual base::string16 GetItemAt(int index) OVERRIDE {
    return items_[index];
  }
  virtual bool IsItemSeparatorAt(int index) OVERRIDE {
    return false;
  }
  virtual int GetDefaultIndex() const OVERRIDE {
    return 0;
  }

  std::vector<base::string16> items_;

  DISALLOW_COPY_AND_ASSIGN(TranslateDenialComboboxModel);
};

const CGFloat kWindowWidth = 320;

// Padding between the window frame and content.
const CGFloat kFramePadding = 16;

const CGFloat kRelatedControlHorizontalSpacing = -2;

const CGFloat kRelatedControlVerticalSpacing = 4;
const CGFloat kUnrelatedControlVerticalSpacing = 20;

const CGFloat kContentWidth = kWindowWidth - 2 * kFramePadding;

@interface TranslateBubbleController()

- (void)performLayout;
- (NSView*)newBeforeTranslateView;
- (NSView*)newTranslatingView;
- (NSView*)newAfterTranslateView;
- (NSView*)newErrorView;
- (NSView*)newAdvancedView;
- (void)updateAdvancedView;
- (NSTextField*)addText:(NSString*)text
                 toView:(NSView*)view;
- (NSButton*)addLinkButtonWithText:(NSString*)text
                            action:(SEL)action
                            toView:(NSView*)view;
- (NSButton*)addButton:(NSString*)title
                action:(SEL)action
                toView:(NSView*)view;
- (NSButton*)addCheckbox:(NSString*)title
                  toView:(NSView*)view;
- (NSPopUpButton*)addPopUpButton:(ui::ComboboxModel*)model
                          action:(SEL)action
                          toView:(NSView*)view;
- (void)handleTranslateButtonPressed;
- (void)handleNopeButtonPressed;
- (void)handleDoneButtonPressed;
- (void)handleCancelButtonPressed;
- (void)handleShowOriginalButtonPressed;
- (void)handleAdvancedLinkButtonPressed;
- (void)handleDenialPopUpButtonNopeSelected;
- (void)handleDenialPopUpButtonNeverTranslateLanguageSelected;
- (void)handleDenialPopUpButtonNeverTranslateSiteSelected;
- (void)handleSourceLanguagePopUpButtonSelectedItemChanged:(id)sender;
- (void)handleTargetLanguagePopUpButtonSelectedItemChanged:(id)sender;

@end

@implementation TranslateBubbleController

- (id)initWithParentWindow:(BrowserWindowController*)controller
                     model:(scoped_ptr<TranslateBubbleModel>)model
               webContents:(content::WebContents*)webContents {
  NSWindow* parentWindow = [controller window];

  // Use an arbitrary size; it will be changed in performLayout.
  NSRect contentRect = ui::kWindowSizeDeterminedLater;
  base::scoped_nsobject<InfoBubbleWindow> window(
      [[InfoBubbleWindow alloc] initWithContentRect:contentRect
                                          styleMask:NSBorderlessWindowMask
                                            backing:NSBackingStoreBuffered
                                              defer:NO]);

  if ((self = [super initWithWindow:window
                       parentWindow:parentWindow
                         anchoredAt:NSZeroPoint])) {
    webContents_ = webContents;
    model_ = model.Pass();
    if (model_->GetViewState() !=
        TranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE) {
      translateExecuted_ = YES;
    }

    views_.reset([@{
        @(TranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE):
            [self newBeforeTranslateView],
        @(TranslateBubbleModel::VIEW_STATE_TRANSLATING):
            [self newTranslatingView],
        @(TranslateBubbleModel::VIEW_STATE_AFTER_TRANSLATE):
            [self newAfterTranslateView],
        @(TranslateBubbleModel::VIEW_STATE_ERROR):
            [self newErrorView],
        @(TranslateBubbleModel::VIEW_STATE_ADVANCED):
            [self newAdvancedView],
    } retain]);

    [self performLayout];
  }
  return self;
}

@synthesize webContents = webContents_;

- (NSView*)currentView {
  NSNumber* key = @(model_->GetViewState());
  NSView* view = [views_ objectForKey:key];
  DCHECK(view);
  return view;
}

- (const TranslateBubbleModel*)model {
  return model_.get();
}

- (void)showWindow:(id)sender {
  BrowserWindowController* controller = [[self parentWindow] windowController];
  NSPoint anchorPoint = [[controller toolbarController] translateBubblePoint];
  anchorPoint = [[self parentWindow] convertBaseToScreen:anchorPoint];
  [self setAnchorPoint:anchorPoint];
  [super showWindow:sender];
}

- (void)switchView:(TranslateBubbleModel::ViewState)viewState {
  if (model_->GetViewState() == viewState)
    return;

  model_->SetViewState(viewState);
  [self performLayout];
}

- (void)switchToErrorView:(translate::TranslateErrors::Type)errorType {
  // FIXME: Implement this.
}

- (void)performLayout {
  NSWindow* window = [self window];
  [[window contentView] setSubviews:@[ [self currentView] ]];

  CGFloat height = NSHeight([[self currentView] frame]) +
      2 * kFramePadding + info_bubble::kBubbleArrowHeight;

  NSRect windowFrame = [window contentRectForFrameRect:[[self window] frame]];
  NSRect newWindowFrame = [window frameRectForContentRect:NSMakeRect(
      NSMinX(windowFrame), NSMaxY(windowFrame) - height, kWindowWidth, height)];
  [window setFrame:newWindowFrame
           display:YES
           animate:[[self window] isVisible]];
}

- (NSView*)newBeforeTranslateView {
  NSRect contentFrame = NSMakeRect(
      kFramePadding,
      kFramePadding,
      kContentWidth,
      0);
  NSView* view = [[NSView alloc] initWithFrame:contentFrame];

  NSString* message =
      l10n_util::GetNSStringWithFixup(IDS_TRANSLATE_BUBBLE_BEFORE_TRANSLATE);
  NSTextField* textLabel = [self addText:message
                                  toView:view];
  message = l10n_util::GetNSStringWithFixup(IDS_TRANSLATE_BUBBLE_ADVANCED);
  NSButton* advancedLinkButton =
      [self addLinkButtonWithText:message
                           action:@selector(handleAdvancedLinkButtonPressed)
                           toView:view];

  NSString* title =
      l10n_util::GetNSStringWithFixup(IDS_TRANSLATE_BUBBLE_ACCEPT);
  NSButton* translateButton =
      [self addButton:title
               action:@selector(handleTranslateButtonPressed)
               toView:view];

  base::string16 originalLanguageName =
      model_->GetLanguageNameAt(model_->GetOriginalLanguageIndex());
  // TODO(hajimehoshi): When TranslateDenialComboboxModel is factored out as a
  // common model, ui::MenuModel will be used here.
  translateDenialComboboxModel_.reset(
      new TranslateDenialComboboxModel(originalLanguageName));
  NSPopUpButton* denyPopUpButton =
      [self addPopUpButton:translateDenialComboboxModel_.get()
                    action:nil
                    toView:view];
  [denyPopUpButton setPullsDown:YES];
  [[denyPopUpButton itemAtIndex:1] setTarget:self];
  [[denyPopUpButton itemAtIndex:1]
    setAction:@selector(handleDenialPopUpButtonNopeSelected)];
  [[denyPopUpButton itemAtIndex:2] setTarget:self];
  [[denyPopUpButton itemAtIndex:2]
    setAction:@selector(handleDenialPopUpButtonNeverTranslateLanguageSelected)];
  [[denyPopUpButton itemAtIndex:3] setTarget:self];
  [[denyPopUpButton itemAtIndex:3]
    setAction:@selector(handleDenialPopUpButtonNeverTranslateSiteSelected)];

  title = base::SysUTF16ToNSString(
      l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_DENY));
  [[denyPopUpButton itemAtIndex:0] setTitle:title];

  // Adjust width for the first item.
  base::scoped_nsobject<NSMenu> originalMenu([[denyPopUpButton menu] copy]);
  [denyPopUpButton removeAllItems];
  [denyPopUpButton addItemWithTitle:[[originalMenu itemAtIndex:0] title]];
  [denyPopUpButton sizeToFit];
  [denyPopUpButton setMenu:originalMenu];

  // Layout
  CGFloat yPos = 0;

  [translateButton setFrameOrigin:NSMakePoint(
      kContentWidth - NSWidth([translateButton frame]), yPos)];

  NSRect denyPopUpButtonFrame = [denyPopUpButton frame];
  CGFloat diffY = [[denyPopUpButton cell]
    titleRectForBounds:[denyPopUpButton bounds]].origin.y;
  [denyPopUpButton setFrameOrigin:NSMakePoint(
      NSMinX([translateButton frame]) - denyPopUpButtonFrame.size.width
      - kRelatedControlHorizontalSpacing,
      yPos + diffY)];

  yPos += NSHeight([translateButton frame]) +
      kUnrelatedControlVerticalSpacing;

  [textLabel setFrameOrigin:NSMakePoint(0, yPos)];
  [advancedLinkButton setFrameOrigin:NSMakePoint(
      NSWidth([textLabel frame]), yPos)];

  [view setFrameSize:NSMakeSize(kContentWidth, NSMaxY([textLabel frame]))];

  return view;
}

- (NSView*)newTranslatingView {
  NSRect contentFrame = NSMakeRect(
      kFramePadding,
      kFramePadding,
      kContentWidth,
      0);
  NSView* view = [[NSView alloc] initWithFrame:contentFrame];

  NSString* message =
      l10n_util::GetNSStringWithFixup(IDS_TRANSLATE_BUBBLE_TRANSLATING);
  NSTextField* textLabel = [self addText:message
                                  toView:view];
  NSString* title =
      l10n_util::GetNSStringWithFixup(IDS_TRANSLATE_BUBBLE_REVERT);
  NSButton* showOriginalButton =
      [self addButton:title
               action:@selector(handleShowOriginalButtonPressed)
               toView:view];
  [showOriginalButton setEnabled:NO];

  // Layout
  // TODO(hajimehoshi): Use l10n_util::VerticallyReflowGroup.
  CGFloat yPos = 0;

  [showOriginalButton setFrameOrigin:NSMakePoint(
      kContentWidth - NSWidth([showOriginalButton frame]), yPos)];

  yPos += NSHeight([showOriginalButton frame]) +
      kUnrelatedControlVerticalSpacing;

  [textLabel setFrameOrigin:NSMakePoint(0, yPos)];

  [view setFrameSize:NSMakeSize(kContentWidth, NSMaxY([textLabel frame]))];

  return view;
}

- (NSView*)newAfterTranslateView {
  NSRect contentFrame = NSMakeRect(
      kFramePadding,
      kFramePadding,
      kContentWidth,
      0);
  NSView* view = [[NSView alloc] initWithFrame:contentFrame];

  NSString* message =
      l10n_util::GetNSStringWithFixup(IDS_TRANSLATE_BUBBLE_TRANSLATED);
  NSTextField* textLabel = [self addText:message
                                  toView:view];
  message = l10n_util::GetNSStringWithFixup(IDS_TRANSLATE_BUBBLE_ADVANCED);
  NSButton* advancedLinkButton =
      [self addLinkButtonWithText:message
                           action:@selector(handleAdvancedLinkButtonPressed)
                           toView:view];
  NSString* title =
      l10n_util::GetNSStringWithFixup(IDS_TRANSLATE_BUBBLE_REVERT);
  NSButton* showOriginalButton =
      [self addButton:title
               action:@selector(handleShowOriginalButtonPressed)
               toView:view];

  // Layout
  CGFloat yPos = 0;

  [showOriginalButton setFrameOrigin:NSMakePoint(
      kContentWidth - NSWidth([showOriginalButton frame]), yPos)];

  yPos += NSHeight([showOriginalButton frame]) +
      kUnrelatedControlVerticalSpacing;

  [textLabel setFrameOrigin:NSMakePoint(0, yPos)];
  [advancedLinkButton setFrameOrigin:NSMakePoint(
      NSMaxX([textLabel frame]), yPos)];

  [view setFrameSize:NSMakeSize(kContentWidth, NSMaxY([textLabel frame]))];

  return view;
}

- (NSView*)newErrorView {
  NSRect contentFrame = NSMakeRect(
      kFramePadding,
      kFramePadding,
      kContentWidth,
      0);
  NSView* view = [[NSView alloc] initWithFrame:contentFrame];

  // TODO(hajimehoshi): Implement this.

  return view;
}

- (NSView*)newAdvancedView {
  NSRect contentFrame = NSMakeRect(
      kFramePadding,
      kFramePadding,
      kContentWidth,
      0);
  NSView* view = [[NSView alloc] initWithFrame:contentFrame];

  NSString* title = l10n_util::GetNSStringWithFixup(
      IDS_TRANSLATE_BUBBLE_PAGE_LANGUAGE);
  NSTextField* sourceLanguageLabel = [self addText:title
                                            toView:view];
  title = l10n_util::GetNSStringWithFixup(
      IDS_TRANSLATE_BUBBLE_TRANSLATION_LANGUAGE);
  NSTextField* targetLanguageLabel = [self addText:title
                                            toView:view];

  // combobox
  int sourceDefaultIndex = model_->GetOriginalLanguageIndex();
  int targetDefaultIndex = model_->GetTargetLanguageIndex();
  sourceLanguageComboboxModel_.reset(
      new LanguageComboboxModel(sourceDefaultIndex, model_.get()));
  targetLanguageComboboxModel_.reset(
      new LanguageComboboxModel(targetDefaultIndex, model_.get()));
  SEL action = @selector(handleSourceLanguagePopUpButtonSelectedItemChanged:);
  NSPopUpButton* sourcePopUpButton =
      [self addPopUpButton:sourceLanguageComboboxModel_.get()
                    action:action
                    toView:view];
  action = @selector(handleTargetLanguagePopUpButtonSelectedItemChanged:);
  NSPopUpButton* targetPopUpButton =
      [self addPopUpButton:targetLanguageComboboxModel_.get()
                    action:action
                    toView:view];

  // 'Always translate' checkbox
  BOOL isIncognitoWindow = webContents_ ?
      webContents_->GetBrowserContext()->IsOffTheRecord() : NO;
  if (!isIncognitoWindow) {
    NSString* title =
        l10n_util::GetNSStringWithFixup(IDS_TRANSLATE_BUBBLE_ALWAYS);
    alwaysTranslateCheckbox_ = [self addCheckbox:title
                                          toView:view];
  }

  // Buttons
  advancedDoneButton_ =
      [self addButton:l10n_util::GetNSStringWithFixup(IDS_DONE)
               action:@selector(handleDoneButtonPressed)
               toView:view];
  advancedCancelButton_ =
      [self addButton:l10n_util::GetNSStringWithFixup(IDS_CANCEL)
               action:@selector(handleCancelButtonPressed)
               toView:view];

  // Layout
  CGFloat textLabelWidth = NSWidth([sourceLanguageLabel frame]);
  if (textLabelWidth < NSWidth([targetLanguageLabel frame]))
    textLabelWidth = NSWidth([targetLanguageLabel frame]);

  CGFloat yPos = 0;

  [advancedDoneButton_ setFrameOrigin:NSMakePoint(0, yPos)];
  [advancedCancelButton_ setFrameOrigin:NSMakePoint(0, yPos)];

  yPos += NSHeight([advancedDoneButton_ frame]) +
      kUnrelatedControlVerticalSpacing;

  if (alwaysTranslateCheckbox_) {
    [alwaysTranslateCheckbox_ setFrameOrigin:NSMakePoint(textLabelWidth, yPos)];

    yPos += NSHeight([alwaysTranslateCheckbox_ frame]) +
        kRelatedControlVerticalSpacing;
  }

  CGFloat diffY = [[sourcePopUpButton cell]
                   titleRectForBounds:[sourcePopUpButton bounds]].origin.y;

  [targetLanguageLabel setFrameOrigin:NSMakePoint(
      textLabelWidth - NSWidth([targetLanguageLabel frame]), yPos + diffY)];

  NSRect frame = [targetPopUpButton frame];
  frame.origin = NSMakePoint(textLabelWidth, yPos);
  frame.size.width = (kWindowWidth - 2 * kFramePadding) - textLabelWidth;
  [targetPopUpButton setFrame:frame];

  yPos += NSHeight([targetPopUpButton frame]) +
      kRelatedControlVerticalSpacing;

  [sourceLanguageLabel setFrameOrigin:NSMakePoint(
      textLabelWidth - NSWidth([sourceLanguageLabel frame]), yPos + diffY)];

  frame = [sourcePopUpButton frame];
  frame.origin = NSMakePoint(textLabelWidth, yPos);
  frame.size.width = NSWidth([targetPopUpButton frame]);
  [sourcePopUpButton setFrame:frame];

  [view setFrameSize:NSMakeSize(kContentWidth,
                                NSMaxY([sourcePopUpButton frame]))];

  [self updateAdvancedView];

  return view;
}

- (void)updateAdvancedView {
  NSInteger state = model_->ShouldAlwaysTranslate() ? NSOnState : NSOffState;
  [alwaysTranslateCheckbox_ setState:state];

  NSString* title;
  if (model_->IsPageTranslatedInCurrentLanguages())
    title = l10n_util::GetNSStringWithFixup(IDS_DONE);
  else
    title = l10n_util::GetNSStringWithFixup(IDS_TRANSLATE_BUBBLE_ACCEPT);
  [advancedDoneButton_ setTitle:title];
  [advancedDoneButton_ sizeToFit];

  NSRect frame = [advancedDoneButton_ frame];
  frame.origin.x = (kWindowWidth - 2 * kFramePadding) - NSWidth(frame);
  [advancedDoneButton_ setFrameOrigin:frame.origin];

  frame = [advancedCancelButton_ frame];
  frame.origin.x = NSMinX([advancedDoneButton_ frame]) - NSWidth(frame)
      - kRelatedControlHorizontalSpacing;
  [advancedCancelButton_ setFrameOrigin:frame.origin];
}

- (NSTextField*)addText:(NSString*)text
                 toView:(NSView*)view {
  base::scoped_nsobject<NSTextField> textField(
      [[NSTextField alloc] initWithFrame:NSZeroRect]);
  [textField setEditable:NO];
  [textField setSelectable:YES];
  [textField setDrawsBackground:NO];
  [textField setBezeled:NO];
  [textField setStringValue:text];
  NSFont* font = [NSFont systemFontOfSize:[NSFont smallSystemFontSize]];
  [textField setFont:font];
  [textField setAutoresizingMask:NSViewWidthSizable];
  [view addSubview:textField.get()];

  [textField sizeToFit];
  return textField.get();
}

- (NSButton*)addLinkButtonWithText:(NSString*)text
                            action:(SEL)action
                            toView:(NSView*)view {
  base::scoped_nsobject<NSButton> button(
      [[HyperlinkButtonCell buttonWithString:text] retain]);

  [button setButtonType:NSMomentaryPushInButton];
  [button setBezelStyle:NSRegularSquareBezelStyle];
  [button setFont:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]];
  [button sizeToFit];
  [button setTarget:self];
  [button setAction:action];

  [view addSubview:button.get()];

  return button.get();
}

- (NSButton*)addButton:(NSString*)title
                action:(SEL)action
                toView:(NSView*)view {
  base::scoped_nsobject<NSButton> button(
      [[NSButton alloc] initWithFrame:NSZeroRect]);
  [button setFont:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]];
  [button setTitle:title];
  [button setBezelStyle:NSRoundedBezelStyle];
  [[button cell] setControlSize:NSSmallControlSize];
  [button sizeToFit];
  [button setTarget:self];
  [button setAction:action];

  [view addSubview:button.get()];

  return button.get();
}

- (NSButton*)addCheckbox:(NSString*)title
                  toView:(NSView*)view {
  base::scoped_nsobject<NSButton> button(
      [[NSButton alloc] initWithFrame:NSZeroRect]);
  [button setFont:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]];
  [button setTitle:title];
  [[button cell] setControlSize:NSSmallControlSize];
  [button setButtonType:NSSwitchButton];
  [button sizeToFit];

  [view addSubview:button.get()];

  return button.get();
}

- (NSPopUpButton*)addPopUpButton:(ui::ComboboxModel*)model
                          action:(SEL)action
                          toView:(NSView*)view {
  base::scoped_nsobject<NSPopUpButton> button(
      [[BubbleCombobox alloc] initWithFrame:NSZeroRect
                                  pullsDown:NO
                                      model:model]);
  [button setTarget:self];
  [button setAction:action];
  [button sizeToFit];
  [view addSubview:button.get()];
  return button.get();
}

- (void)handleTranslateButtonPressed {
  translateExecuted_ = YES;
  model_->Translate();
}

- (void)handleNopeButtonPressed {
  [self close];
}

- (void)handleDoneButtonPressed {
  if (alwaysTranslateCheckbox_) {
    model_->SetAlwaysTranslate(
        [alwaysTranslateCheckbox_ state] == NSOnState);
  }
  if (model_->IsPageTranslatedInCurrentLanguages()) {
    model_->GoBackFromAdvanced();
    [self performLayout];
  } else {
    translateExecuted_ = true;
    model_->Translate();
    [self switchView:TranslateBubbleModel::VIEW_STATE_TRANSLATING];
  }
}

- (void)handleCancelButtonPressed {
  model_->GoBackFromAdvanced();
  [self performLayout];
}

- (void)handleShowOriginalButtonPressed {
  model_->RevertTranslation();
  [self close];
}

- (void)handleAdvancedLinkButtonPressed {
  [self switchView:TranslateBubbleModel::VIEW_STATE_ADVANCED];
}

- (void)handleDenialPopUpButtonNopeSelected {
  [self close];
}

- (void)handleDenialPopUpButtonNeverTranslateLanguageSelected {
  model_->SetNeverTranslateLanguage(true);
  [self close];
}

- (void)handleDenialPopUpButtonNeverTranslateSiteSelected {
  model_->SetNeverTranslateSite(true);
  [self close];
}

- (void)handleSourceLanguagePopUpButtonSelectedItemChanged:(id)sender {
  NSPopUpButton* button = base::mac::ObjCCastStrict<NSPopUpButton>(sender);
  model_->UpdateOriginalLanguageIndex([button indexOfSelectedItem]);
  [self updateAdvancedView];
}

- (void)handleTargetLanguagePopUpButtonSelectedItemChanged:(id)sender {
  NSPopUpButton* button = base::mac::ObjCCastStrict<NSPopUpButton>(sender);
  model_->UpdateTargetLanguageIndex([button indexOfSelectedItem]);
  [self updateAdvancedView];
}

@end
