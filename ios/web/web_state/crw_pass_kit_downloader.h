// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_CRW_PASS_KIT_DOWNLOADER_H_
#define IOS_WEB_WEB_STATE_CRW_PASS_KIT_DOWNLOADER_H_

#import <Foundation/Foundation.h>

class GURL;

namespace net {
class URLRequestContextGetter;
}  // namespace net

namespace web {

// Completion handler called when the download is complete. |data| should be the
// data from a PassKit file, but this is not guaranteed, and the handler is
// responsible for error handling non PassKit data. If the download does not
// successfully complete, |data| will be nil.
typedef void (^PassKitCompletionHandler)(NSData*);

}  // namespace web

// CRWPassKitDownloader downloads PassKit data and passes it to a completion
// handler.
@interface CRWPassKitDownloader : NSObject

// Initializes the CRWPassKitDownloader. |getter| must not be null and
// |handler| must not be null.
- (instancetype)initWithContextGetter:(net::URLRequestContextGetter*)getter
                    completionHandler:(web::PassKitCompletionHandler)handler
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Returns whether the given string indicates a MIME type for a PassKit file.
- (BOOL)isMIMETypePassKitType:(NSString*)MIMEType;

// Downloads a PassKit file and passes the data on to the completion
// handler.
// |URL| is the URL of the PassKit file. The MIME type associated with the
// URL should be for PassKit. If this method is called before a pending
// download completes, the pending download will be cancelled.
- (void)downloadPassKitFileWithURL:(const GURL&)URL;

// Cancels currently pending download. This method has no effect if called when
// there is no pending download.
- (void)cancelPendingDownload;

@end

#endif  // IOS_WEB_WEB_STATE_CRW_PASS_KIT_DOWNLOADER_H_
