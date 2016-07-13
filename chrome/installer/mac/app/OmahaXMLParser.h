// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_MAC_APP_OMAHAXMLPARSER_H_
#define CHROME_INSTALLER_MAC_APP_OMAHAXMLPARSER_H_

#import <Foundation/Foundation.h>

@interface OmahaXMLParser : NSObject<NSXMLParserDelegate> {
  NSMutableArray* chromeIncompleteDownloadURLs_;
  NSString* chromeImageFilename_;
}

- (NSMutableArray*)chromeIncompleteDownloadURLs;

- (NSString*)chromeImageFilename;

// Parses an XML document and extracts all the URL's it finds as well as the
// filename. Adds each URL into the array chromeIncompleteDownloadURLs_.
- (NSArray*)parseXML:(NSData*)omahaResponseXML error:(NSError**)error;

@end

#endif  // CHROME_INSTALLER_MAC_APP_OMAHAXMLPARSER_H_
