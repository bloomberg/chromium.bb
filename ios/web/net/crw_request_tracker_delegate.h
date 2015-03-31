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

// Returns |YES| of all the requests are static file requests and returns |NO|
// if all the requests are network requests. Note it is not allowed for a
// |CRWRequestTrackerDelegate| to send both static file requests and network
// requests.
- (BOOL)isForStaticFileRequests;

// The tracker calls this method every time there is a change in the SSL status
// of a page. The info is whatever object was passed to TrimToURL().
- (void)updatedSSLStatus:(const web::SSLStatus&)sslStatus
              forPageUrl:(const GURL&)url
                userInfo:(id)userInfo;

// The tracker calls this method when it receives response headers.
- (void)handleResponseHeaders:(net::HttpResponseHeaders*)headers
                   requestUrl:(const GURL&)requestUrl;

// This method is called when a network request has an issue with the SSL
// connection to present it to the user. The user will decide if the request
// should continue or not and the callback should be invoked to let the backend
// know.
// If the callback is not called the request will be cancelled on the next call
// to TrimToURL().
// The callback is safe to call until the requestTracker it originated from
// is deleted.
typedef void (^SSLErrorCallback)(BOOL);
- (void)presentSSLError:(const net::SSLInfo&)info
           forSSLStatus:(const web::SSLStatus&)status
                  onUrl:(const GURL&)url
            recoverable:(BOOL)recoverable
               callback:(SSLErrorCallback)shouldContinue;

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
