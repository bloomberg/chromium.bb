// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_WK_WEB_VIEW_SSL_ERROR_UTIL_H_
#define IOS_WEB_WEB_STATE_WK_WEB_VIEW_SSL_ERROR_UTIL_H_

#import <Foundation/Foundation.h>

namespace net {
class SSLInfo;
}

namespace web {

// NSErrorPeerCertificateChainKey from NSError's userInfo dict.
extern NSString* const kNSErrorPeerCertificateChainKey;

// Returns YES if geven error is a SSL error.
BOOL IsWKWebViewSSLError(NSError* error);

// Fills SSLInfo object with information extracted from |error|. Callers are
// responsible to ensure that given |error| is an SSL error by calling
// |web::IsSSLError| function.
void GetSSLInfoFromWKWebViewSSLError(NSError* error, net::SSLInfo* ssl_info);

}  // namespace web

#endif // IOS_WEB_WEB_STATE_WK_WEB_VIEW_SSL_ERROR_UTIL_H_
