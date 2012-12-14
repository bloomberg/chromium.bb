// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/intents/web_intent_picker_view_controller.h"

#include "base/sys_string_conversions.h"
#import "chrome/browser/ui/chrome_style.h"
#import "chrome/browser/ui/cocoa/event_utils.h"
#import "chrome/browser/ui/cocoa/flipped_view.h"
#import "chrome/browser/ui/cocoa/hover_close_button.h"
#import "chrome/browser/ui/cocoa/intents/web_intent_choose_service_view_controller.h"
#import "chrome/browser/ui/cocoa/intents/web_intent_extension_prompt_view_controller.h"
#import "chrome/browser/ui/cocoa/intents/web_intent_inline_service_view_controller.h"
#import "chrome/browser/ui/cocoa/intents/web_intent_message_view_controller.h"
#import "chrome/browser/ui/cocoa/intents/web_intent_picker_cocoa.h"
#import "chrome/browser/ui/cocoa/intents/web_intent_progress_view_controller.h"
#import "chrome/browser/ui/cocoa/intents/web_intent_service_row_view_controller.h"
#import "chrome/browser/ui/cocoa/key_equivalent_constants.h"
#include "chrome/browser/ui/constrained_window.h"
#include "chrome/browser/ui/intents/web_intent_picker_delegate.h"
#include "chrome/browser/ui/intents/web_intent_inline_disposition_delegate.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "ui/base/cocoa/window_size_constants.h"
#include "ui/base/l10n/l10n_util_mac.h"

@interface WebIntentPickerViewController ()

- (void)performLayoutWithOldViewController:
    (WebIntentViewController*)oldViewController;
// Gets the view controller currently being displayed.
- (WebIntentViewController*)currentViewController;
- (WebIntentPickerState)newPickerState;

// Update the various views to match changes to the picker model.
- (void)updateWaiting;
- (void)updateNoService;
- (void)updateChooseService;
- (void)updateInlineService;
- (void)updateInstallingExtension;
- (void)updateExtensionPrompt;

// Creates a installed service row using the item at the given index.
- (WebIntentServiceRowViewController*)createInstalledServiceAtIndex:(int)index;
// Creates a suggested service row using the item at the given index.
- (WebIntentServiceRowViewController*)createSuggestedServiceAtIndex:(int)index;

- (void)onCloseButton:(id)sender;
- (void)cancelOperation:(id)sender;
- (void)onSelectInstalledService:(id)sender;
- (void)onSelectSuggestedService:(id)sender;
- (void)onShowSuggestedService:(id)sender;
- (void)onShowMoreServices:(id)sender;
- (void)onChooseAnotherService:(id)sender;

@end

@implementation WebIntentPickerViewController

- (id)initWithPicker:(WebIntentPickerCocoa*)picker {
  if ((self = [super init])) {
    picker_ = picker;

    scoped_nsobject<NSView> view(
        [[FlippedView alloc] initWithFrame:ui::kWindowSizeDeterminedLater]);
    [view setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    [self setView:view];

    closeButton_.reset(
        [[WebUIHoverCloseButton alloc] initWithFrame:NSZeroRect]);
    [closeButton_ setTarget:self];
    [closeButton_ setAction:@selector(onCloseButton:)];
    [[closeButton_ cell] setKeyEquivalent:kKeyEquivalentEscape];
    [[self view] addSubview:closeButton_];

    chooseServiceViewController_.reset(
        [[WebIntentChooseServiceViewController alloc] init]);
    [[chooseServiceViewController_ showMoreServicesButton] setTarget:self];
    [[chooseServiceViewController_ showMoreServicesButton]
        setAction:@selector(onShowMoreServices:)];

    inlineServiceViewController_.reset(
        [[WebIntentInlineServiceViewController alloc] initWithPicker:picker_]);
    [[inlineServiceViewController_ chooseServiceButton] setTarget:self];
    [[inlineServiceViewController_ chooseServiceButton]
        setAction:@selector(onChooseAnotherService:)];

    messageViewController_.reset(
        [[WebIntentMessageViewController alloc] init]);
    progressViewController_.reset(
        [[WebIntentProgressViewController alloc] init]);
    extensionPromptViewController_.reset(
        [[WebIntentExtensionPromptViewController alloc] init]);
  }
  return self;
}

- (NSButton*)closeButton {
  return closeButton_.get();
}

- (gfx::Size)minimumInlineWebViewSize {
  NSSize size = [inlineServiceViewController_ minimumInlineWebViewSizeForFrame:
      [WebIntentViewController minimumInnerFrame]];
  return gfx::Size(NSSizeToCGSize(size));
}

- (WebIntentPickerState)state {
  return state_;
}

- (WebIntentChooseServiceViewController*)chooseServiceViewController {
  return chooseServiceViewController_;
}

- (WebIntentInlineServiceViewController*)inlineServiceViewController {
  return inlineServiceViewController_;
}

- (WebIntentMessageViewController*)messageViewController {
  return messageViewController_;
}

- (WebIntentProgressViewController*)progressViewController {
  return progressViewController_;
}

- (WebIntentExtensionPromptViewController*)extensionPromptViewController {
  return extensionPromptViewController_;
}

- (void)update {
  // The model may be NULL between the time the dialog is closed and this object
  // is deleted.
  if (!picker_->model())
    return;

  WebIntentPickerState newState = [self newPickerState];
  WebIntentViewController* oldViewController = [self currentViewController];
  if (state_ != newState || ![[oldViewController view] superview]) {
    state_ = newState;
    [[self view] addSubview:[[self currentViewController] view]];

    // Ensure that the close button is topmost.
    [closeButton_ removeFromSuperview];
    [[self view] addSubview:closeButton_];
  }

  switch (state_) {
    case PICKER_STATE_WAITING:
      [self updateWaiting];
      break;
    case PICKER_STATE_NO_SERVICE:
      [self updateNoService];
      break;
    case PICKER_STATE_CHOOSE_SERVICE:
      [self updateChooseService];
      break;
    case PICKER_STATE_INLINE_SERVICE:
      [self updateInlineService];
      break;
    case PICKER_STATE_INSTALLING_EXTENSION:
      [self updateInstallingExtension];
      break;
    case PICKER_STATE_EXTENSION_PROMPT:
      [self updateExtensionPrompt];
      break;
  }

  [self performLayoutWithOldViewController:oldViewController];
}

- (void)performLayoutWithOldViewController:
    (WebIntentViewController*)oldViewController {
  WebIntentViewController* viewController = [self currentViewController];
  [viewController sizeToFitAndLayout];
  [[viewController view] setFrameOrigin:NSZeroPoint];

  NSRect bounds = [[viewController view] bounds];
  NSWindow* window = [[self view] window];
  NSRect windowFrame = [window frameRectForContentRect:bounds];

  NSRect closeFrame;
  closeFrame.size.width = chrome_style::GetCloseButtonSize();
  closeFrame.size.height = chrome_style::GetCloseButtonSize();
  closeFrame.origin.x = NSMaxX(bounds) - NSWidth(closeFrame) -
      chrome_style::kCloseButtonPadding;
  closeFrame.origin.y = chrome_style::kCloseButtonPadding;

  if (oldViewController) {
    scoped_nsobject<NSMutableArray> array([[NSMutableArray alloc] init]);
    if (oldViewController && ![oldViewController isEqual:viewController]) {
      [array addObject:@{
          NSViewAnimationTargetKey: [viewController view],
          NSViewAnimationEffectKey: NSViewAnimationFadeInEffect
      }];
      [array addObject:@{
          NSViewAnimationTargetKey: [oldViewController view],
          NSViewAnimationEffectKey: NSViewAnimationFadeOutEffect
      }];
    }
    [array addObject:@{
        NSViewAnimationTargetKey:   window,
        NSViewAnimationEndFrameKey: [NSValue valueWithRect:windowFrame]
    }];
    [array addObject:@{
        NSViewAnimationTargetKey:   closeButton_,
        NSViewAnimationEndFrameKey: [NSValue valueWithRect:closeFrame]
    }];

    scoped_nsobject<NSViewAnimation> animation(
        [[NSViewAnimation alloc] initWithViewAnimations:array]);
    [animation setAnimationBlockingMode:NSAnimationBlocking];
    [animation setDuration:0.2];
    [animation startAnimation];
  } else {
    [window setFrame:windowFrame display:YES];
    [[self view] setFrame:bounds];
    [closeButton_ setFrame:closeFrame];
  }

  if (oldViewController && ![oldViewController isEqual:viewController]) {
    [[oldViewController view] removeFromSuperview];
    [oldViewController viewRemovedFromSuperview];
  }
}

- (WebIntentViewController*)currentViewController {
  switch (state_) {
    case PICKER_STATE_WAITING:
      return progressViewController_;
    case PICKER_STATE_NO_SERVICE:
      return messageViewController_;
    case PICKER_STATE_CHOOSE_SERVICE:
      return chooseServiceViewController_;
    case PICKER_STATE_INLINE_SERVICE:
      return inlineServiceViewController_;
    case PICKER_STATE_INSTALLING_EXTENSION:
      return progressViewController_;
    case PICKER_STATE_EXTENSION_PROMPT:
      return extensionPromptViewController_;
  }
  return nil;
}

- (WebIntentPickerState)newPickerState {
  WebIntentPickerModel* model = picker_->model();
  if (model->pending_extension_install_delegate() &&
      model->pending_extension_install_prompt()) {
    return PICKER_STATE_EXTENSION_PROMPT;
  }
  if (!model->pending_extension_install_id().empty())
    return PICKER_STATE_INSTALLING_EXTENSION;
  if (model->IsInlineDisposition())
    return PICKER_STATE_INLINE_SERVICE;
  if (model->GetSuggestedExtensionCount() || model->GetInstalledServiceCount())
    return PICKER_STATE_CHOOSE_SERVICE;
  if (model->IsWaitingForSuggestions())
    return PICKER_STATE_WAITING;
  return PICKER_STATE_NO_SERVICE;
}

- (void)updateWaiting {
  NSString* message = l10n_util::GetNSStringWithFixup(
      IDS_INTENT_PICKER_WAIT_FOR_CWS);
  [progressViewController_ setMessage:message];
  [progressViewController_ setPercentDone:-1];
}

- (void)updateNoService {
  [messageViewController_ setTitle:l10n_util::GetNSStringWithFixup(
      IDS_INTENT_PICKER_NO_SERVICES_TITLE)];
  [messageViewController_ setMessage:l10n_util::GetNSStringWithFixup(
      IDS_INTENT_PICKER_NO_SERVICES)];
}

- (void)updateChooseService {
  WebIntentPickerModel* model = picker_->model();

  if (model->GetInstalledServiceCount()) {
    [chooseServiceViewController_ setTitle:base::SysUTF16ToNSString(
        WebIntentPicker::GetDisplayStringForIntentAction(model->action()))];
    [chooseServiceViewController_ setMessage:nil];
  } else {
    [chooseServiceViewController_ setTitle:l10n_util::GetNSStringWithFixup(
        IDS_INTENT_PICKER_CHOOSE_SERVICES_NONE_INSTALLED_TITLE)];
    [chooseServiceViewController_ setMessage:l10n_util::GetNSStringWithFixup(
        IDS_INTENT_PICKER_CHOOSE_SERVICES_NONE_INSTALLED_MESSAGE)];
  }

  scoped_nsobject<NSMutableArray> rows([[NSMutableArray alloc] init]);
  for (size_t i = 0; i < model->GetInstalledServiceCount(); ++i)
    [rows addObject:[self createInstalledServiceAtIndex:i]];
  for (size_t i = 0; i < model->GetSuggestedExtensionCount(); ++i) {
    if ([rows count] >= WebIntentPicker::kMaxServicesToShow)
      break;
    [rows addObject:[self createSuggestedServiceAtIndex:i]];
  }
  [chooseServiceViewController_ setRows:rows];
}

- (void)updateInlineService {
  const WebIntentPickerModel::InstalledService* service =
      picker_->model()->GetInstalledServiceWithURL(
          picker_->model()->inline_disposition_url());
  if (!service)
    return;

  [inlineServiceViewController_ setServiceName:
      base::SysUTF16ToNSString(service->title)];
  if (service->favicon.IsEmpty())
    [inlineServiceViewController_ setServiceIcon:nil];
  else
    [inlineServiceViewController_ setServiceIcon:service->favicon.ToNSImage()];
  [inlineServiceViewController_ setServiceURL:service->url];
  [inlineServiceViewController_ setChooseServiceButtonHidden:
      !picker_->model()->show_use_another_service()];
}

- (void)updateInstallingExtension {
  WebIntentPickerModel* model = picker_->model();
  const WebIntentPickerModel::SuggestedExtension* extension =
      model->GetSuggestedExtensionWithId(
          model->pending_extension_install_id());
  if (!extension)
    return;
  [progressViewController_ setTitle:
      base::SysUTF16ToNSString(extension->title)];
  [progressViewController_ setMessage:base::SysUTF16ToNSString(
      model->pending_extension_install_status_string())];
  [progressViewController_ setPercentDone:
      model->pending_extension_install_download_progress()];
}

- (void)updateExtensionPrompt {
  ExtensionInstallPrompt::Delegate* delegate =
      picker_->model()->pending_extension_install_delegate();
  const ExtensionInstallPrompt::Prompt* prompt =
      picker_->model()->pending_extension_install_prompt();
  if (!delegate || !prompt)
    return;
  [extensionPromptViewController_ setNavigator:picker_->web_contents()
                                      delegate:delegate
                                        prompt:*prompt];
}

- (WebIntentServiceRowViewController*)createInstalledServiceAtIndex:(int)index {
  const WebIntentPickerModel::InstalledService& service =
      picker_->model()->GetInstalledServiceAt(index);
  NSString* title = base::SysUTF16ToNSString(service.title);
  NSImage* icon = nil;
  if (!service.favicon.IsEmpty())
    icon = service.favicon.ToNSImage();
  WebIntentServiceRowViewController* row =
      [[[WebIntentServiceRowViewController alloc]
          initInstalledServiceRowWithTitle:title
                                      icon:icon] autorelease];
  [[row selectButton] setTag:index];
  [[row selectButton] setTarget:self];
  [[row selectButton] setAction:@selector(onSelectInstalledService:)];
  return row;
}

- (WebIntentServiceRowViewController*)createSuggestedServiceAtIndex:(int)index {
  const WebIntentPickerModel::SuggestedExtension& service =
      picker_->model()->GetSuggestedExtensionAt(index);
  NSString* title = base::SysUTF16ToNSString(service.title);
  NSImage* icon = nil;
  if (!service.icon.IsEmpty())
    icon = service.icon.ToNSImage();
  WebIntentServiceRowViewController* row =
      [[[WebIntentServiceRowViewController alloc]
          initSuggestedServiceRowWithTitle:title
                                      icon:icon
                                    rating:service.average_rating] autorelease];
  [[row selectButton] setTag:index];
  [[row selectButton] setTarget:self];
  [[row selectButton] setAction:@selector(onSelectSuggestedService:)];
  [[row titleLinkButton] setTag:index];
  [[row titleLinkButton] setTarget:self];
  [[row titleLinkButton] setAction:@selector(onShowSuggestedService:)];
  return row;
}

- (void)onCloseButton:(id)sender {
  picker_->delegate()->OnUserCancelledPickerDialog();
}

// Handle default OSX dialog cancel mechanisms. (Cmd-.)
- (void)cancelOperation:(id)sender {
  [self onCloseButton:sender];
}

- (void)onSelectInstalledService:(id)sender {
  const WebIntentPickerModel::InstalledService& service =
      picker_->model()->GetInstalledServiceAt([sender tag]);
  picker_->delegate()->OnServiceChosen(
      service.url, service.disposition,
      WebIntentPickerDelegate::kEnableDefaults);
}

- (void)onSelectSuggestedService:(id)sender {
  const WebIntentPickerModel::SuggestedExtension& service =
      picker_->model()->GetSuggestedExtensionAt([sender tag]);
  picker_->delegate()->OnExtensionInstallRequested(service.id);
}

- (void)onShowSuggestedService:(id)sender {
  WindowOpenDisposition disposition =
      event_utils::WindowOpenDispositionFromNSEvent([NSApp currentEvent]);
  const WebIntentPickerModel::SuggestedExtension& service =
      picker_->model()->GetSuggestedExtensionAt([sender tag]);
  picker_->delegate()->OnExtensionLinkClicked(service.id, disposition);
}

- (void)onShowMoreServices:(id)sender {
  WindowOpenDisposition disposition =
      event_utils::WindowOpenDispositionFromNSEvent([NSApp currentEvent]);
  picker_->delegate()->OnSuggestionsLinkClicked(disposition);
}

- (void)onChooseAnotherService:(id)sender {
  picker_->delegate()->OnChooseAnotherService();
}

@end
