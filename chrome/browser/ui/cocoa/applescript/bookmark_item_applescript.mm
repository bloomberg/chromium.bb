// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/applescript/bookmark_item_applescript.h"

#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/profiles/profile_manager.h"
#import "chrome/browser/ui/cocoa/applescript/error_applescript.h"

@interface BookmarkItemAppleScript()
@property (nonatomic, copy) NSString* tempURL;
@end

@implementation BookmarkItemAppleScript

@synthesize tempURL = tempURL_;

- (id)init {
  if ((self = [super init])) {
    [self setTempURL:@""];
  }
  return self;
}

- (void)dealloc {
  [tempURL_ release];
  [super dealloc];
}

- (void)setBookmarkNode:(const BookmarkNode*)aBookmarkNode {
  [super setBookmarkNode:aBookmarkNode];
  [self setURL:[self tempURL]];
}

- (NSString*)URL {
  if (!bookmarkNode_)
    return tempURL_;

  return base::SysUTF8ToNSString(bookmarkNode_->url().spec());
}

- (void)setURL:(NSString*)aURL {
  // If a scripter sets a URL before the node is added, URL is saved at a
  // temporary location.
  if (!bookmarkNode_) {
    [self setTempURL:aURL];
    return;
  }

  BookmarkModel* model = [self bookmarkModel];
  if (!model)
    return;

  GURL url(base::SysNSStringToUTF8(aURL));
  if (!url.is_valid()) {
    AppleScript::SetError(AppleScript::errInvalidURL);
    return;
  }

  model->SetURL(bookmarkNode_, url);
}

@end
