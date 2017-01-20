// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NET_CRW_REQUEST_TRACKER_DELEGATE_H_
#define IOS_WEB_NET_CRW_REQUEST_TRACKER_DELEGATE_H_

#include <vector>

#include "net/cert/cert_status_flags.h"

class GURL;

namespace net {
class HttpResponseHeaders;
class SSLInfo;
class X509Certificate;
}

namespace web {
struct SSLStatus;
}

// All the methods in this protocol must be sent on the main thread.
@protocol CRWRequestTrackerDelegate

// The tracker calls this method every time there is a change in the SSL status
// of a page. The info is whatever object was passed to TrimToURL().
- (void)updatedSSLStatus:(const web::SSLStatus&)sslStatus
              forPageUrl:(const GURL&)url
                userInfo:(id)userInfo;

// The tracker calls this method when it receives response headers.
- (void)handleResponseHeaders:(net::HttpResponseHeaders*)headers
                   requestUrl:(const GURL&)requestUrl;

// Update the progress.
- (void)updatedProgress:(float)progress;

// This method is called when a certificate with an error is in use.
- (void)certificateUsed:(net::X509Certificate*)certificate
                forHost:(const std::string&)host
                 status:(net::CertStatus)status;

// Called when all the active allowed certificates need to be cleared. This
// happens during the TrimToURL(), which corresponds to a navigation.
- (void)clearCertificates;
@end

#endif  // IOS_WEB_NET_CRW_REQUEST_TRACKER_DELEGATE_H_
