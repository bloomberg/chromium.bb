// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/content_settings/cookie_details.h"

#include "app/l10n_util_mac.h"
#import "base/i18n/time_formatting.h"
#include "base/sys_string_conversions.h"
#include "grit/generated_resources.h"
#include "chrome/browser/cookies_tree_model.h"
#include "webkit/appcache/appcache_service.h"

#pragma mark Cocoa Cookie Details

@implementation CocoaCookieDetails

@synthesize canEditExpiration = canEditExpiration_;
@synthesize hasExpiration = hasExpiration_;
@synthesize type = type_;

- (BOOL)shouldHideCookieDetailsView {
  return type_ != kCocoaCookieDetailsTypeFolder &&
      type_ != kCocoaCookieDetailsTypeCookie;
}

- (BOOL)shouldShowLocalStorageTreeDetailsView {
  return type_ == kCocoaCookieDetailsTypeTreeLocalStorage;
}

- (BOOL)shouldShowLocalStoragePromptDetailsView {
  return type_ == kCocoaCookieDetailsTypePromptLocalStorage;
}

- (BOOL)shouldShowDatabaseTreeDetailsView {
  return type_ == kCocoaCookieDetailsTypeTreeDatabase;
}

- (BOOL)shouldShowAppCacheTreeDetailsView {
  return type_ == kCocoaCookieDetailsTypeTreeAppCache;
}

- (BOOL)shouldShowDatabasePromptDetailsView {
  return type_ == kCocoaCookieDetailsTypePromptDatabase;
}

- (BOOL)shouldShowAppCachePromptDetailsView {
  return type_ == kCocoaCookieDetailsTypePromptAppCache;
}

- (BOOL)shouldShowIndexedDBTreeDetailsView {
  return type_ == kCocoaCookieDetailsTypeTreeIndexedDB;
}

- (NSString*)name {
  return name_.get();
}

- (NSString*)content {
  return content_.get();
}

- (NSString*)domain {
  return domain_.get();
}

- (NSString*)path {
  return path_.get();
}

- (NSString*)sendFor {
  return sendFor_.get();
}

- (NSString*)created {
  return created_.get();
}

- (NSString*)expires {
  return expires_.get();
}

- (NSString*)fileSize {
  return fileSize_.get();
}

- (NSString*)lastModified {
  return lastModified_.get();
}

- (NSString*)lastAccessed {
  return lastAccessed_.get();
}

- (NSString*)databaseDescription {
  return databaseDescription_.get();
}

- (NSString*)localStorageKey {
  return localStorageKey_.get();
}

- (NSString*)localStorageValue {
  return localStorageValue_.get();
}

- (NSString*)manifestURL {
  return manifestURL_.get();
}

- (id)initAsFolder {
  if ((self = [super init])) {
    type_ = kCocoaCookieDetailsTypeFolder;
  }
  return self;
}

- (id)initWithCookie:(const net::CookieMonster::CanonicalCookie*)cookie
              origin:(NSString*)origin
   canEditExpiration:(BOOL)canEditExpiration {
  if ((self = [super init])) {
    type_ = kCocoaCookieDetailsTypeCookie;
    hasExpiration_ = cookie->DoesExpire();
    canEditExpiration_ = canEditExpiration && hasExpiration_;
    name_.reset([base::SysUTF8ToNSString(cookie->Name()) retain]);
    content_.reset([base::SysUTF8ToNSString(cookie->Value()) retain]);
    path_.reset([base::SysUTF8ToNSString(cookie->Path()) retain]);
    domain_.reset([origin retain]);

    if (cookie->DoesExpire()) {
      expires_.reset([base::SysUTF16ToNSString(
          base::TimeFormatFriendlyDateAndTime(cookie->ExpiryDate())) retain]);
    } else {
      expires_.reset([l10n_util::GetNSStringWithFixup(
          IDS_COOKIES_COOKIE_EXPIRES_SESSION) retain]);
    }

    created_.reset([base::SysUTF16ToNSString(
        base::TimeFormatFriendlyDateAndTime(cookie->CreationDate())) retain]);

    if (cookie->IsSecure()) {
      sendFor_.reset([l10n_util::GetNSStringWithFixup(
          IDS_COOKIES_COOKIE_SENDFOR_SECURE) retain]);
    } else {
      sendFor_.reset([l10n_util::GetNSStringWithFixup(
          IDS_COOKIES_COOKIE_SENDFOR_ANY) retain]);
    }
  }
  return self;
}

- (id)initWithDatabase:(const BrowsingDataDatabaseHelper::DatabaseInfo*)
    databaseInfo {
  if ((self = [super init])) {
    type_ = kCocoaCookieDetailsTypeTreeDatabase;
    canEditExpiration_ = NO;
    databaseDescription_.reset([base::SysUTF8ToNSString(
        databaseInfo->description) retain]);
    fileSize_.reset([base::SysUTF16ToNSString(FormatBytes(databaseInfo->size,
        GetByteDisplayUnits(databaseInfo->size), true)) retain]);
    lastModified_.reset([base::SysUTF16ToNSString(
        base::TimeFormatFriendlyDateAndTime(
            databaseInfo->last_modified)) retain]);
  }
  return self;
}

- (id)initWithLocalStorage:(
    const BrowsingDataLocalStorageHelper::LocalStorageInfo*)storageInfo {
  if ((self = [super init])) {
    type_ = kCocoaCookieDetailsTypeTreeLocalStorage;
    canEditExpiration_ = NO;
    domain_.reset([base::SysUTF8ToNSString(storageInfo->origin) retain]);
    fileSize_.reset([base::SysUTF16ToNSString(FormatBytes(storageInfo->size,
        GetByteDisplayUnits(storageInfo->size), true)) retain]);
    lastModified_.reset([base::SysUTF16ToNSString(
        base::TimeFormatFriendlyDateAndTime(
            storageInfo->last_modified)) retain]);
  }
  return self;
}

- (id)initWithAppCacheInfo:(const appcache::AppCacheInfo*)appcacheInfo {
  if ((self = [super init])) {
    type_ = kCocoaCookieDetailsTypeTreeAppCache;
    canEditExpiration_ = NO;
    manifestURL_.reset([base::SysUTF8ToNSString(
        appcacheInfo->manifest_url.spec()) retain]);
    fileSize_.reset([base::SysUTF16ToNSString(FormatBytes(appcacheInfo->size,
        GetByteDisplayUnits(appcacheInfo->size), true)) retain]);
    created_.reset([base::SysUTF16ToNSString(
        base::TimeFormatFriendlyDateAndTime(
            appcacheInfo->creation_time)) retain]);
    lastAccessed_.reset([base::SysUTF16ToNSString(
        base::TimeFormatFriendlyDateAndTime(
            appcacheInfo->last_access_time)) retain]);
  }
  return self;
}

- (id)initWithDatabase:(const std::string&)domain
          databaseName:(const string16&)databaseName
   databaseDescription:(const string16&)databaseDescription
              fileSize:(unsigned long)fileSize {
  if ((self = [super init])) {
    type_ = kCocoaCookieDetailsTypePromptDatabase;
    canEditExpiration_ = NO;
    name_.reset([base::SysUTF16ToNSString(databaseName) retain]);
    domain_.reset([base::SysUTF8ToNSString(domain) retain]);
    databaseDescription_.reset(
        [base::SysUTF16ToNSString(databaseDescription) retain]);
    fileSize_.reset([base::SysUTF16ToNSString(FormatBytes(fileSize,
        GetByteDisplayUnits(fileSize), true)) retain]);
  }
  return self;
}

- (id)initWithLocalStorage:(const std::string&)domain
                       key:(const string16&)key
                     value:(const string16&)value {
  if ((self = [super init])) {
    type_ = kCocoaCookieDetailsTypePromptLocalStorage;
    canEditExpiration_ = NO;
    domain_.reset([base::SysUTF8ToNSString(domain) retain]);
    localStorageKey_.reset([base::SysUTF16ToNSString(key) retain]);
    localStorageValue_.reset([base::SysUTF16ToNSString(value) retain]);
  }
  return self;
}

- (id)initWithAppCacheManifestURL:(const std::string&)manifestURL {
  if ((self = [super init])) {
    type_ = kCocoaCookieDetailsTypePromptAppCache;
    canEditExpiration_ = NO;
    manifestURL_.reset([base::SysUTF8ToNSString(manifestURL) retain]);
  }
  return self;
}

- (id)initWithIndexedDBInfo:
    (const BrowsingDataIndexedDBHelper::IndexedDBInfo*)indexedDBInfo {
  if ((self = [super init])) {
    type_ = kCocoaCookieDetailsTypeTreeIndexedDB;
    canEditExpiration_ = NO;
    domain_.reset([base::SysUTF8ToNSString(indexedDBInfo->origin) retain]);
    fileSize_.reset([base::SysUTF16ToNSString(FormatBytes(indexedDBInfo->size,
        GetByteDisplayUnits(indexedDBInfo->size), true)) retain]);
    lastModified_.reset([base::SysUTF16ToNSString(
        base::TimeFormatFriendlyDateAndTime(
            indexedDBInfo->last_modified)) retain]);
  }
  return self;
}

+ (CocoaCookieDetails*)createFromCookieTreeNode:(CookieTreeNode*)treeNode {
  CookieTreeNode::DetailedInfo info = treeNode->GetDetailedInfo();
  CookieTreeNode::DetailedInfo::NodeType nodeType = info.node_type;
  NSString* origin;
  switch (nodeType) {
    case CookieTreeNode::DetailedInfo::TYPE_COOKIE:
      origin = base::SysWideToNSString(info.origin.c_str());
      return [[[CocoaCookieDetails alloc] initWithCookie:info.cookie
                                                  origin:origin
                                       canEditExpiration:NO] autorelease];
    case CookieTreeNode::DetailedInfo::TYPE_DATABASE:
      return [[[CocoaCookieDetails alloc]
          initWithDatabase:info.database_info] autorelease];
    case CookieTreeNode::DetailedInfo::TYPE_LOCAL_STORAGE:
      return [[[CocoaCookieDetails alloc]
          initWithLocalStorage:info.local_storage_info] autorelease];
    case CookieTreeNode::DetailedInfo::TYPE_APPCACHE:
      return [[[CocoaCookieDetails alloc]
          initWithAppCacheInfo:info.appcache_info] autorelease];
    case CookieTreeNode::DetailedInfo::TYPE_INDEXED_DB:
      return [[[CocoaCookieDetails alloc]
          initWithIndexedDBInfo:info.indexed_db_info] autorelease];
    default:
      return [[[CocoaCookieDetails alloc] initAsFolder] autorelease];
  }
}

@end

#pragma mark Content Object Adapter

@implementation CookiePromptContentDetailsAdapter

- (id)initWithDetails:(CocoaCookieDetails*)details {
  if ((self = [super init])) {
    details_.reset([details retain]);
  }
  return self;
}

- (CocoaCookieDetails*)details {
  return details_.get();
}

@end
