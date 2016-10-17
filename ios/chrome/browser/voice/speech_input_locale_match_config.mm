// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/voice/speech_input_locale_match_config.h"

#import "base/mac/scoped_nsobject.h"

namespace {

// Name of plist file containing locale matches.
NSString* const kLocaleMatchesFilename = @"SpeechInputLocaleMatches.plist";

// Keys used in SpeechInputLocaleMatches.plist:
NSString* const kMatchedLocaleKey = @"Locale";
NSString* const kMatchingLocalesKey = @"MatchingLocales";
NSString* const kMatchingLanguagesKey = @"MatchingLanguages";

}  // namespace

#pragma mark - SpeechInputLocaleMatchConfig

@interface SpeechInputLocaleMatchConfig () {
  // Backing object for the property of the same name.
  base::scoped_nsobject<NSArray> _matches;
}

@end

@implementation SpeechInputLocaleMatchConfig

+ (instancetype)sharedInstance {
  static SpeechInputLocaleMatchConfig* matchConfig;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    matchConfig = [[SpeechInputLocaleMatchConfig alloc] init];
  });
  return matchConfig;
}

- (instancetype)init {
  self = [super initWithAppId:nil version:nil plist:kLocaleMatchesFilename];
  if (self)
    self.stopsUpdateChecksOnAppTermination = YES;
  return self;
}

#pragma mark Accessors

- (NSArray*)matches {
  if (!_matches) {
    NSMutableArray* matches = [NSMutableArray array];
    for (NSDictionary* matchDict in [self arrayFromConfig]) {
      SpeechInputLocaleMatch* match = [[[SpeechInputLocaleMatch alloc]
          initWithDictionary:matchDict] autorelease];
      [matches addObject:match];
    }
    _matches.reset([matches copy]);
  }
  return _matches;
}

#pragma mark UpdatableConfigBase

- (void)resourceDidUpdate:(NSNotification*)notification {
  [super resourceDidUpdate:notification];
  _matches.reset();
}

@end

#pragma mark - SpeechInputLocaleMatch

@interface SpeechInputLocaleMatch () {
  // Backing objects for properties of the same name.
  base::scoped_nsobject<NSString> _matchedLocaleCode;
  base::scoped_nsobject<NSArray> _matchingLocaleCodes;
  base::scoped_nsobject<NSArray> _matchingLanguages;
}

@end

@implementation SpeechInputLocaleMatch

- (instancetype)initWithDictionary:(NSDictionary*)matchDict {
  if ((self = [super init])) {
    _matchedLocaleCode.reset([matchDict[kMatchedLocaleKey] copy]);
    _matchingLocaleCodes.reset([matchDict[kMatchingLocalesKey] copy]);
    _matchingLanguages.reset([matchDict[kMatchingLanguagesKey] copy]);
  }
  return self;
}

#pragma mark Accessors

- (NSString*)matchedLocaleCode {
  return _matchedLocaleCode;
}

- (NSArray*)matchingLocaleCodes {
  return _matchingLocaleCodes;
}

- (NSArray*)matchingLanguages {
  return _matchingLanguages;
}

@end
