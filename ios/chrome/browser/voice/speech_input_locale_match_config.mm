// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/voice/speech_input_locale_match_config.h"

#import "base/mac/foundation_util.h"
#import "base/mac/scoped_nsobject.h"

namespace {

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

// Loads |_matches| from config file |plistFileName|.
- (void)loadConfigFile:(NSString*)plistFileName;

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
  self = [super init];
  if (self) {
    [self loadConfigFile:@"SpeechInputLocaleMatches"];
  }
  return self;
}

#pragma mark Accessors

- (NSArray*)matches {
  return _matches.get();
}

#pragma mark - Private

- (void)loadConfigFile:(NSString*)plistFileName {
  NSString* path = [[NSBundle mainBundle] pathForResource:plistFileName
                                                   ofType:@"plist"
                                              inDirectory:@"gm-config/ANY"];
  NSArray* configData = [NSArray arrayWithContentsOfFile:path];
  NSMutableArray* matches = [NSMutableArray array];
  for (id item in configData) {
    NSDictionary* matchDict = base::mac::ObjCCastStrict<NSDictionary>(item);
    base::scoped_nsobject<SpeechInputLocaleMatch> match(
        [[SpeechInputLocaleMatch alloc] initWithDictionary:matchDict]);
    [matches addObject:match];
  }
  _matches.reset([matches copy]);
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
