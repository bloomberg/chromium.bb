// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/net/crw_ssl_status_updater.h"

#import "base/ios/weak_nsobject.h"
#import "base/mac/scoped_nsobject.h"
#import "base/strings/sys_string_conversions.h"
#include "ios/web/public/cert_store.h"
#import "ios/web/public/navigation_item.h"
#import "ios/web/public/navigation_manager.h"
#include "ios/web/public/ssl_status.h"
#import "ios/web/web_state/wk_web_view_security_util.h"
#include "net/cert/x509_certificate.h"
#include "url/gurl.h"

using net::CertStatus;
using web::SecurityStyle;

@interface CRWSSLStatusUpdater () {
  // DataSource for CRWSSLStatusUpdater.
  base::WeakNSProtocol<id<CRWSSLStatusUpdaterDataSource>> _dataSource;
  // Backs up property of the same name.
  base::WeakNSProtocol<id<CRWSSLStatusUpdaterDelegate>> _delegate;
}

// Unowned pointer to web::NavigationManager.
@property(nonatomic, readonly) web::NavigationManager* navigationManager;

// Identifier used for storing and retrieving certificates.
@property(nonatomic, readonly) int certGroupID;

// Updates |security_style| and |cert_status| for the NavigationItem with ID
// |navigationItemID|, if URL and certificate chain still match |host| and
// |certChain|.
- (void)updateSSLStatusForNavigationItemWithID:(int)navigationItemID
                                     certChain:(NSArray*)chain
                                          host:(NSString*)host
                             withSecurityStyle:(SecurityStyle)style
                                    certStatus:(CertStatus)certStatus;

// Asynchronously obtains SSL status from given |certChain| and |host| and
// updates current navigation item. Before scheduling update changes SSLStatus'
// cert_status and security_style to default.
- (void)scheduleSSLStatusUpdateUsingCertChain:(NSArray*)chain
                                         host:(NSString*)host;

// Notifies delegate about SSLStatus change.
- (void)didChangeSSLStatusForNavigationItem:(web::NavigationItem*)navItem;

@end

@implementation CRWSSLStatusUpdater
@synthesize navigationManager = _navigationManager;
@synthesize certGroupID = _certGroupID;

#pragma mark - Public

- (instancetype)initWithDataSource:(id<CRWSSLStatusUpdaterDataSource>)dataSource
                 navigationManager:(web::NavigationManager*)navigationManager
                       certGroupID:(int)certGroupID {
  DCHECK(dataSource);
  DCHECK(navigationManager);
  DCHECK(certGroupID);
  if (self = [super init]) {
    _dataSource.reset(dataSource);
    _navigationManager = navigationManager;
    _certGroupID = certGroupID;
  }
  return self;
}

- (id<CRWSSLStatusUpdaterDelegate>)delegate {
  return _delegate.get();
}

- (void)setDelegate:(id<CRWSSLStatusUpdaterDelegate>)delegate {
  _delegate.reset(delegate);
}

- (void)updateSSLStatusForNavigationItem:(web::NavigationItem*)item
                            withCertHost:(NSString*)host
                               certChain:(NSArray*)chain
                    hasOnlySecureContent:(BOOL)hasOnlySecureContent {
  web::SSLStatus previousSSLStatus = item->GetSSL();

  // Starting from iOS9 WKWebView blocks active mixed content, so if
  // |hasOnlySecureContent| returns NO it means passive content.
  item->GetSSL().content_status =
      hasOnlySecureContent ? web::SSLStatus::NORMAL_CONTENT
                           : web::SSLStatus::DISPLAYED_INSECURE_CONTENT;

  // Try updating SSLStatus for current NavigationItem asynchronously.
  scoped_refptr<net::X509Certificate> cert;
  if (item->GetURL().SchemeIsCryptographic()) {
    cert = web::CreateCertFromChain(chain);
    if (cert) {
      int oldCertID = item->GetSSL().cert_id;
      std::string oldHost = item->GetSSL().cert_status_host;
      item->GetSSL().cert_id = web::CertStore::GetInstance()->StoreCert(
          cert.get(), self.certGroupID);
      item->GetSSL().cert_status_host = base::SysNSStringToUTF8(host);
      // Only recompute the SSLStatus information if the certificate or host has
      // since changed. Host can be changed in case of redirect.
      if (oldCertID != item->GetSSL().cert_id ||
          oldHost != item->GetSSL().cert_status_host) {
        // Real SSL status is unknown, reset cert status and security style.
        // They will be asynchronously updated in
        // |scheduleSSLStatusUpdateUsingCertChain|.
        item->GetSSL().cert_status = CertStatus();
        item->GetSSL().security_style = web::SECURITY_STYLE_UNKNOWN;

        [self scheduleSSLStatusUpdateUsingCertChain:chain host:host];
      }
    }
  }

  if (!cert) {
    item->GetSSL().cert_id = 0;
    if (!item->GetURL().SchemeIsCryptographic()) {
      // HTTP or other non-secure connection.
      item->GetSSL().security_style = web::SECURITY_STYLE_UNAUTHENTICATED;
    } else {
      // HTTPS, no certificate (this use-case has not been observed).
      item->GetSSL().security_style = web::SECURITY_STYLE_UNKNOWN;
    }
  }

  if (!previousSSLStatus.Equals(item->GetSSL())) {
    [self didChangeSSLStatusForNavigationItem:item];
  }
}

#pragma mark - Private

- (void)updateSSLStatusForNavigationItemWithID:(int)navigationItemID
                                     certChain:(NSArray*)chain
                                          host:(NSString*)host
                             withSecurityStyle:(SecurityStyle)style
                                    certStatus:(CertStatus)certStatus {
  // The searched item almost always be the last one, so walk backward rather
  // than forward.
  for (int i = _navigationManager->GetItemCount() - 1; 0 <= i; i--) {
    web::NavigationItem* item = _navigationManager->GetItemAtIndex(i);
    if (item->GetUniqueID() != navigationItemID)
      continue;

    // NavigationItem's UniqueID is preserved even after redirects, so
    // checking that cert and URL match is necessary.
    scoped_refptr<net::X509Certificate> cert(web::CreateCertFromChain(chain));
    int certID =
        web::CertStore::GetInstance()->StoreCert(cert.get(), self.certGroupID);
    std::string GURLHost = base::SysNSStringToUTF8(host);
    web::SSLStatus& SSLStatus = item->GetSSL();
    if (item->GetURL().SchemeIsCryptographic() && SSLStatus.cert_id == certID &&
        item->GetURL().host() == GURLHost) {
      web::SSLStatus previousSSLStatus = item->GetSSL();
      SSLStatus.cert_status = certStatus;
      SSLStatus.security_style = style;
      if (!previousSSLStatus.Equals(SSLStatus)) {
        [self didChangeSSLStatusForNavigationItem:item];
      }
    }
    return;
  }
}

- (void)scheduleSSLStatusUpdateUsingCertChain:(NSArray*)chain
                                         host:(NSString*)host {
  // Use Navigation Item's unique ID to locate requested item after
  // obtaining cert status asynchronously.
  int itemID = _navigationManager->GetLastCommittedItem()->GetUniqueID();

  DCHECK(_dataSource);
  base::WeakNSObject<CRWSSLStatusUpdater> weakSelf(self);
  [_dataSource SSLStatusUpdater:self
      querySSLStatusForCertChain:chain
                            host:host
               completionHandler:^(SecurityStyle style, CertStatus certStatus) {
                 [weakSelf updateSSLStatusForNavigationItemWithID:itemID
                                                        certChain:chain
                                                             host:host
                                                withSecurityStyle:style
                                                       certStatus:certStatus];
               }];
}

- (void)didChangeSSLStatusForNavigationItem:(web::NavigationItem*)navItem {
  if ([_delegate respondsToSelector:
          @selector(SSLStatusUpdater:didChangeSSLStatusForNavigationItem:)]) {
    [_delegate SSLStatusUpdater:self
        didChangeSSLStatusForNavigationItem:navItem];
  }
}

@end
