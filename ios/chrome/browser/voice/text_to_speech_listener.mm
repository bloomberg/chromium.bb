// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/voice/text_to_speech_listener.h"

#include <memory>

#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/ptr_util.h"
#include "ios/web/public/navigation_manager.h"
#include "ios/web/public/web_state/web_state.h"
#include "ios/web/public/web_state/web_state_observer.h"
#import "ios/chrome/browser/voice/text_to_speech_parser.h"
#import "ios/chrome/browser/voice/voice_search_url_rewriter.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#pragma mark - TextToSpeechListener Private Interface

class TextToSpeechWebStateObserver;

@interface TextToSpeechListener ()

// The TextToSpeechListenerDelegate passed on initialization.
@property(weak, nonatomic, readonly) id<TextToSpeechListenerDelegate> delegate;

@end

#pragma mark - TextToSpeechWebStateObserver

class TextToSpeechWebStateObserver : public web::WebStateObserver {
 public:
  TextToSpeechWebStateObserver(web::WebState* web_state,
                               TextToSpeechListener* listener);
  ~TextToSpeechWebStateObserver() override;

  // web::WebStateObserver implementation:
  void PageLoaded(
      web::WebState* web_state,
      web::PageLoadCompletionStatus load_completion_status) override;
  void WebStateDestroyed(web::WebState* web_state) override;

 private:
  TextToSpeechListener* listener_;
};

TextToSpeechWebStateObserver::TextToSpeechWebStateObserver(
    web::WebState* web_state,
    TextToSpeechListener* listener)
    : web::WebStateObserver(web_state), listener_(listener) {
  DCHECK(web_state);
  DCHECK(listener);
  // Rewrite the next loaded URL to have the voice search flags.
  web_state->GetNavigationManager()->AddTransientURLRewriter(
      &VoiceSearchURLRewriter);
}

TextToSpeechWebStateObserver::~TextToSpeechWebStateObserver() {}

void TextToSpeechWebStateObserver::PageLoaded(
    web::WebState* web_state,
    web::PageLoadCompletionStatus load_completion_status) {
  const GURL& url = web_state->GetLastCommittedURL();
  BOOL shouldParse = [listener_.delegate shouldTextToSpeechListener:listener_
                                                   parseDataFromURL:url];
  if (shouldParse) {
    __weak TextToSpeechListener* weakListener = listener_;
    ExtractVoiceSearchAudioDataFromWebState(web_state, ^(NSData* audioData) {
      [[weakListener delegate] textToSpeechListener:weakListener
                                   didReceiveResult:audioData];
    });
  } else {
    [listener_.delegate textToSpeechListener:listener_ didReceiveResult:nil];
  }
}

void TextToSpeechWebStateObserver::WebStateDestroyed(web::WebState* web_state) {
  [listener_.delegate textToSpeechListenerWebStateWasDestroyed:listener_];
}

#pragma mark - TextToSpeechListener
@implementation TextToSpeechListener {
  // The TextToSpeechWebStateObserver that listens for Text-To-Speech data.
  std::unique_ptr<TextToSpeechWebStateObserver> _webStateObserver;
}
@synthesize delegate = _delegate;

- (instancetype)initWithWebState:(web::WebState*)webState
                        delegate:(id<TextToSpeechListenerDelegate>)delegate {
  if ((self = [super init])) {
    DCHECK(webState);
    DCHECK(delegate);
    _webStateObserver =
        base::MakeUnique<TextToSpeechWebStateObserver>(webState, self);
    _delegate = delegate;
  }
  return self;
}

#pragma mark Accessors

- (web::WebState*)webState {
  return _webStateObserver->web_state();
}

@end
