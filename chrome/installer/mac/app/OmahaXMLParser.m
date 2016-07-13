// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "OmahaXMLParser.h"

@implementation OmahaXMLParser

- (NSMutableArray*)chromeIncompleteDownloadURLs {
  return chromeIncompleteDownloadURLs_;
}

- (NSString*)chromeImageFilename {
  return chromeImageFilename_;
}

// Sets up instance of NSXMLParser and calls on delegate methods to do actual
// parsing work.
- (NSArray*)parseXML:(NSData*)omahaResponseXML error:(NSError**)error {
  NSXMLParser* parser = [[NSXMLParser alloc] initWithData:omahaResponseXML];
  [parser setDelegate:self];
  BOOL success = [parser parse];
  if (!success) {
    *error = [parser parserError];
  }

  return chromeIncompleteDownloadURLs_;
}

// Method implementation for XMLParserDelegate.
// Searches the XML data for the tag "URL" and the subsequent "codebase"
// attribute that indicates a URL follows. Copies each URL into an array.
// Note that the URLs in the XML file are incomplete. They need the filename
// appended to end. The second if statement checks for the tag "package" which
// contains the filename we need to complete the URLs.
- (void)parser:(NSXMLParser*)parser
    didStartElement:(NSString*)elementName
       namespaceURI:(NSString*)namespaceURI
      qualifiedName:(NSString*)qName
         attributes:(NSDictionary*)attributeDict {
  if ([elementName isEqualToString:@"url"]) {
    if (!chromeIncompleteDownloadURLs_) {
      chromeIncompleteDownloadURLs_ = [[NSMutableArray alloc] init];
    }
    NSString* extractedURL = [attributeDict objectForKey:@"codebase"];
    [chromeIncompleteDownloadURLs_ addObject:extractedURL];
  }
  if ([elementName isEqualToString:@"package"]) {
    chromeImageFilename_ = [[NSString alloc]
        initWithFormat:@"%@", [attributeDict objectForKey:@"name"]];
  }
}

@end
