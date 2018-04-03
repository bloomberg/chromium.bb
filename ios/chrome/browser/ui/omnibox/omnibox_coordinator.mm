// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/omnibox_coordinator.h"

#include "base/logging.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_text_field_ios.h"
#include "ios/chrome/browser/ui/omnibox/omnibox_view_ios.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_coordinator.h"
#include "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_view_ios.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation OmniboxCoordinator {
  // TODO(crbug.com/818636): use a slimmer subclass of OmniboxView,
  // OmniboxPopupViewSuggestionsDelegate instead of OmniboxViewIOS.
  std::unique_ptr<OmniboxViewIOS> _editView;
}
@synthesize textField = _textField;
@synthesize editController = _editController;
@synthesize browserState = _browserState;
@synthesize dispatcher = _dispatcher;

- (void)start {
  DCHECK(self.textField);
  DCHECK(self.editController);
  // TODO(crbug.com/818637): implement left view provider.
  _editView = std::make_unique<OmniboxViewIOS>(
      self.textField, self.editController, nullptr, self.browserState);
  self.textField.suggestionCommandsEndpoint =
      static_cast<id<OmniboxSuggestionCommands>>(self.dispatcher);
}

- (void)stop {
  _editView.reset();
  self.editController = nil;
  self.textField = nil;
}

- (void)updateOmniboxState {
  _editView->UpdateAppearance();
}

- (void)setNextFocusSourceAsSearchButton {
  OmniboxEditModel* model = _editView->model();
  model->set_focus_source(OmniboxEditModel::FocusSource::SEARCH_BUTTON);
}

- (void)endEditing {
  _editView->HideKeyboardAndEndEditing();
}

- (OmniboxPopupCoordinator*)createPopupCoordinator:
    (id<OmniboxPopupPositioner>)positioner {
  std::unique_ptr<OmniboxPopupViewIOS> popupView =
      std::make_unique<OmniboxPopupViewIOS>(_editView->model(),
                                            _editView.get());

  _editView->model()->set_popup_model(popupView->model());
  _editView->SetPopupProvider(popupView.get());

  OmniboxPopupCoordinator* coordinator =
      [[OmniboxPopupCoordinator alloc] initWithPopupView:std::move(popupView)];
  coordinator.browserState = self.browserState;
  coordinator.positioner = positioner;

  return coordinator;
}

@end
