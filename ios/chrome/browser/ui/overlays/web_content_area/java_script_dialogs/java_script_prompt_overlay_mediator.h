// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OVERLAYS_WEB_CONTENT_AREA_JAVA_SCRIPT_DIALOGS_JAVA_SCRIPT_PROMPT_OVERLAY_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_OVERLAYS_WEB_CONTENT_AREA_JAVA_SCRIPT_DIALOGS_JAVA_SCRIPT_PROMPT_OVERLAY_MEDIATOR_H_

#import "ios/chrome/browser/ui/overlays/web_content_area/java_script_dialogs/java_script_dialog_overlay_mediator.h"

class JavaScriptPromptOverlayRequestConfig;
@protocol JavaScriptPromptOverlayMediatorDataSource;

// The accessibility ID for prompt's text field.
extern NSString* const kJavaScriptPromptTextFieldAccessibiltyIdentifier;

// Mediator object that uses a JavaScriptPromptOverlayRequestConfig to set
// up the UI for a JavaScript prompt overlay.
@interface JavaScriptPromptOverlayMediator : JavaScriptDialogOverlayMediator

// The datasource for prompt input values.
@property(nonatomic, weak) id<JavaScriptPromptOverlayMediatorDataSource>
    dataSource;

@end

// Protocol used to provide the text input from the prompt UI to the mediator.
@protocol JavaScriptPromptOverlayMediatorDataSource <NSObject>

// Returns the input value for the prompt UI set up by |mediator|.
- (NSString*)promptInputForMediator:(JavaScriptPromptOverlayMediator*)mediator;

@end

#endif  // IOS_CHROME_BROWSER_UI_OVERLAYS_WEB_CONTENT_AREA_JAVA_SCRIPT_DIALOGS_JAVA_SCRIPT_PROMPT_OVERLAY_MEDIATOR_H_
