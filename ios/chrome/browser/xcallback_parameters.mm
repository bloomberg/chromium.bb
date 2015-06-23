// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/xcallback_parameters.h"

#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"

namespace {
NSString* const kSourceAppIdKey = @"sourceAppId";
NSString* const kSourceAppNameKey = @"sourceAppName";
NSString* const kSuccessURLKey = @"successURL";
NSString* const kCreateNewTabKey = @"createNewTab";
}

@interface XCallbackParameters () {
  base::scoped_nsobject<NSString> _sourceAppId;
  base::scoped_nsobject<NSString> _sourceAppName;
  GURL _successURL;
  BOOL _createNewTab;
}
@end

@implementation XCallbackParameters

@synthesize successURL = _successURL;
@synthesize createNewTab = _createNewTab;

- (instancetype)initWithSourceAppId:(NSString*)sourceAppId
                      sourceAppName:(NSString*)sourceAppName
                         successURL:(const GURL&)successURL
                       createNewTab:(BOOL)createNewTab {
  self = [super init];
  if (self) {
    _sourceAppId.reset([sourceAppId copy]);
    _sourceAppName.reset([sourceAppName copy]);
    _successURL = successURL;
    _createNewTab = createNewTab;
  }
  return self;
}

- (instancetype)init {
  NOTREACHED();
  return nil;
}

- (NSString*)description {
  return [NSString stringWithFormat:@"SourceApp: %@ (%@)\nSuccessURL: %s\n",
                                    _sourceAppName.get(), _sourceAppId.get(),
                                    _successURL.spec().c_str()];
}

- (NSString*)sourceAppId {
  return _sourceAppId.get();
}

- (NSString*)sourceAppName {
  return _sourceAppName.get();
}

#pragma mark - NSCoding Methods

- (instancetype)initWithCoder:(NSCoder*)aDecoder {
  GURL successURL;
  NSString* successURLStr = [aDecoder decodeObjectForKey:kSuccessURLKey];
  if (successURLStr)
    _successURL = GURL(base::SysNSStringToUTF8(successURLStr));

  return
      [self initWithSourceAppId:[aDecoder decodeObjectForKey:kSourceAppIdKey]
                  sourceAppName:[aDecoder decodeObjectForKey:kSourceAppNameKey]
                     successURL:successURL
                   createNewTab:[aDecoder decodeBoolForKey:kCreateNewTabKey]];
}

- (void)encodeWithCoder:(NSCoder*)aCoder {
  [aCoder encodeObject:_sourceAppId forKey:kSourceAppIdKey];
  [aCoder encodeObject:_sourceAppName forKey:kSourceAppNameKey];
  if (_successURL.is_valid()) {
    NSString* successStr = base::SysUTF8ToNSString(_successURL.spec());
    [aCoder encodeObject:successStr forKey:kSuccessURLKey];
  }
  [aCoder encodeBool:_createNewTab forKey:kCreateNewTabKey];
}

#pragma mark - NSCopying Methods

- (instancetype)copyWithZone:(NSZone*)zone {
  XCallbackParameters* copy =
      [[[self class] allocWithZone:zone] initWithSourceAppId:_sourceAppId
                                               sourceAppName:_sourceAppName
                                                  successURL:_successURL
                                                createNewTab:_createNewTab];
  return copy;
}

@end
