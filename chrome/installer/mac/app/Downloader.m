// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "Downloader.h"

#include <assert.h>

#import "NetworkCommunication.h"
#import "OmahaXMLParser.h"

@implementation Downloader

@synthesize delegate = delegate_;

// TODO: make this overrideable with commandline argument? or enviro variable
+ (NSString*)getDownloadsFilePath {
  NSArray* downloadPaths = NSSearchPathForDirectoriesInDomains(
      NSDownloadsDirectory, NSUserDomainMask, YES);
  NSString* filePathToDownloads = [downloadPaths objectAtIndex:0];
  NSArray* filenameComposition = @[ filePathToDownloads, @"GoogleChrome.dmg" ];
  NSString* completeFilePath =
      [NSString pathWithComponents:filenameComposition];
  return completeFilePath;
}

// The URLs extracted from parseXML are incomplete and need the filename (which
// is the same for all the links) appended to the end. Iterates through
// chromeIncompleteDownloadURLs_ and appends chromeImageFilename_ to each URL.
- (NSMutableArray*)appendFilename:(NSString*)filename
                           toURLs:(NSArray*)incompleteURLs {
  NSMutableArray* completeURLs = [[NSMutableArray alloc] init];

  for (NSString* URL in incompleteURLs) {
    [completeURLs addObject:[NSURL URLWithString:filename
                                   relativeToURL:[NSURL URLWithString:URL]]];
  }
  return completeURLs;
}

// Extract URLs and the filename from the omahaResponseXML. Then complete the
// URLs by appending filename and returns the first complete URL.
- (NSURL*)getChromeImageURLFromOmahaResponse:(NSData*)omahaResponseXML {
  NSError* err = nil;

  OmahaXMLParser* parser = [[OmahaXMLParser alloc] init];
  NSArray* incompleteURLs = [parser parseXML:omahaResponseXML error:&err];

  if ([incompleteURLs count] < 1) {
    // TODO: Error handling and implement way to verify URL is working. Error
    // information saved in "err".
  }

  NSMutableArray* completeURLs =
      [self appendFilename:parser.chromeImageFilename toURLs:incompleteURLs];

  NSURL* chromeURL = [completeURLs firstObject];
  return chromeURL;
}

// Downloads contents of chromeURL to downloads folders and delegates the work
// to the DownloadDelegate class.
- (BOOL)writeChromeImageToDownloadsDirectory:(NSURL*)chromeURL {
  NetworkCommunication* downloadTask =
      [[NetworkCommunication alloc] initWithDelegate:self];

  // TODO: What if file already exists?
  [downloadTask createRequestWithUrlAsString:[chromeURL absoluteString]
                                  andXMLBody:nil];
  [downloadTask sendDownloadRequest];

  NSFileManager* manager = [[NSFileManager alloc] init];
  return [manager fileExistsAtPath:[Downloader getDownloadsFilePath]];
}

// Pieces together the getting the URL portion and downloading the contents of
// URL portion.
- (BOOL)downloadChromeImageToDownloadsDirectory:(NSData*)omahaResponseXML {
  NSURL* chromeURL = [self getChromeImageURLFromOmahaResponse:omahaResponseXML];
  BOOL writeWasSuccessful =
      [self writeChromeImageToDownloadsDirectory:chromeURL];
  return writeWasSuccessful;
}

// Skeleton of delegate method to provide download progress updates.
// TODO: Make use of (totalBytesWritten/totalBytesExpectedToWrite)*100
// to generate download progress percentage.
- (void)URLSession:(NSURLSession*)session
                 downloadTask:(NSURLSessionDownloadTask*)downloadTask
                 didWriteData:(int64_t)bytesWritten
            totalBytesWritten:(int64_t)totalBytesWritten
    totalBytesExpectedToWrite:(int64_t)totalBytesExpectedToWrite {
}

// Delegate method to move downloaded disk image to user's Download directory.
- (void)URLSession:(NSURLSession*)session
                 downloadTask:(NSURLSessionDownloadTask*)downloadTask
    didFinishDownloadingToURL:(NSURL*)location {
  assert([location isFileURL]);
  NSFileManager* manager = [[NSFileManager alloc] init];
  NSURL* downloadsDirectory =
      [[NSURL alloc] initFileURLWithPath:[Downloader getDownloadsFilePath]];
  if ([manager fileExistsAtPath:location.path]) {
    [manager moveItemAtURL:location toURL:downloadsDirectory error:nil];
  } else {
    // TODO: Error Handling
  }
}

- (void)URLSession:(NSURLSession*)session
                    task:(NSURLSessionTask*)task
    didCompleteWithError:(NSError*)error {
  // TODO: Error Handling

  [delegate_ onDownloadSuccess];
}

@end
