// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#if !defined(NDEBUG)
#import "ios/web/web_state/ui/crw_debug_web_view.h"

#include "base/mac/scoped_nsobject.h"

// These categories define private API in iOS, in reality part of WebKit.
@interface NSObject (CRWDebugWebView_WebScriptCallFrame_methods)
- (id)functionName;
- (id)caller;
- (id)exception;
@end

@interface NSObject (CRWDebugWebView_WebView_methods)
- (void)setScriptDebugDelegate:
    (id<CRWDebugWebView_WebViewScriptDelegate>)delegate;
@end

@interface NSObject (CRWDebugWebView_WebScriptObject_methods)
- (id)callWebScriptMethod:(NSString*)name withArguments:(NSArray*)args;
@end

#pragma mark -

// The debug implementation of the webscript delegate.
@interface CRWDebugWebDelegate :
NSObject<CRWDebugWebView_WebViewScriptDelegate> {
  base::scoped_nsobject<NSMutableDictionary> _sidsToURLs;
}
@end

@implementation CRWDebugWebDelegate

- (id)init {
  if ((self = [super init])) {
    _sidsToURLs.reset([[NSMutableDictionary alloc] init]);
  }
  return self;
}

- (void)webView:(WebView*)webView addMessageToConsole:(NSDictionary*)dict {
  NSLog(@"JS: %@", dict);
}

- (void)webView:(WebView*)webView
    didParseSource:(NSString*)source
    baseLineNumber:(NSUInteger)lineNumber
           fromURL:(NSURL*)url
          sourceId:(int)sid
       forWebFrame:(WebFrame*)webFrame {
  if (url && sid)
    [_sidsToURLs setObject:url forKey:[NSNumber numberWithInt:sid]];
}

- (void)webView:(WebView*)webView
    failedToParseSource:(NSString*)source
         baseLineNumber:(unsigned)lineNumber
                fromURL:(NSURL*)url
              withError:(NSError*)error
            forWebFrame:(WebFrame*)webFrame {
  NSLog(@"JS Failed to parse: url=%@ line=%d error=%@\nsource=%@",
        url, lineNumber, error, source);
}

- (void)webView:(WebView*)webView
    exceptionWasRaised:(WebScriptCallFrame*)frame
              sourceId:(int)sid
                  line:(int)lineno
           forWebFrame:(WebFrame*)webFrame {
  NSURL* url = [_sidsToURLs objectForKey:[NSNumber numberWithInt:sid]];
  id representation = [frame exception];
  if ([representation isKindOfClass:NSClassFromString(@"WebScriptObject")]) {
    representation =
        [representation callWebScriptMethod:@"toString" withArguments:nil];
  }
  base::scoped_nsobject<NSMutableArray> message([[NSMutableArray alloc] init]);

  [message addObject:
      [NSString stringWithFormat:@"JS Exception: sid=%d url=%@ exception=%@",
          sid, url, representation]];

  if ([frame respondsToSelector:@selector(caller)]) {
    while (frame) {
      frame = [frame caller];
      [message addObject:[NSString stringWithFormat:@"line=%d function=%@",
          lineno, [frame functionName]]];
    }
  }

  NSLog(@"%@", [message componentsJoinedByString:@"\n\t"]);
}

@end

#pragma mark -

@implementation CRWDebugWebView {
  base::scoped_nsobject<CRWDebugWebDelegate> _webDelegate;
}

- (void)webView:(id)sender
    didClearWindowObject:(id)windowObject
                forFrame:(WebFrame*)frame {
  if (!_webDelegate)
    _webDelegate.reset([[CRWDebugWebDelegate alloc] init]);
  [sender setScriptDebugDelegate:_webDelegate];
}

@end

#endif  // !defined(NDEBUG)
