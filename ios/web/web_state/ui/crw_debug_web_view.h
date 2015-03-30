// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef IOS_WEB_WEB_STATE_UI_CRW_DEBUG_WEB_VIEW_H_
#define IOS_WEB_WEB_STATE_UI_CRW_DEBUG_WEB_VIEW_H_

// This class is only available in debug mode. It uses private API.
#if !defined(NDEBUG)

#import <UIKit/UIKit.h>

// All part of webkit API, but it is private on iOS.
@class WebFrame;
@class WebScriptCallFrame;
@class WebView;

@protocol CRWDebugWebView_WebViewScriptDelegate
@optional
// Called when a javascript statement want to write on the console.
- (void)webView:(WebView*)webView addMessageToConsole:(NSDictionary*)dict;

// Some source was parsed, establishing a "source ID" (>= 0) for future
// reference
- (void)webView:(WebView*)webView
    didParseSource:(NSString*)source
    baseLineNumber:(NSUInteger)lineNumber
           fromURL:(NSURL*)url
          sourceId:(int)sid
       forWebFrame:(WebFrame*)webFrame;

// Called if a loaded javascript file fail to parse.
- (void)webView:(WebView*)webView
    failedToParseSource:(NSString*)source
         baseLineNumber:(unsigned)lineNumber
                fromURL:(NSURL*)url
              withError:(NSError*)error
            forWebFrame:(WebFrame*)webFrame;

// Called if an exception is raised in Javascript.
- (void)webView:(WebView*)webView
    exceptionWasRaised:(WebScriptCallFrame*)frame
              sourceId:(int)sid
                  line:(int)lineno
           forWebFrame:(WebFrame*)webFrame;

@end

// Simply use like a regular UIWebView. It just logs javascript information on
// the console.
@interface CRWDebugWebView : UIWebView

// Webview delegate API, which the superclass is. Used to set the script
// delegate on the same webview the superclass is delegate of.
- (void)webView:(id)sender
    didClearWindowObject:(id)windowObject
                forFrame:(WebFrame*)frame;

@end

#endif  // !defined(NDEBUG)
#endif  // IOS_WEB_WEB_STATE_UI_CRW_DEBUG_WEB_VIEW_H_
