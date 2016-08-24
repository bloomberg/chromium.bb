// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "AppDelegate.h"

#import "InstallerWindowController.h"
#import "NSError+ChromeInstallerAdditions.h"
#import "NSAlert+ChromeInstallerAdditions.h"
#import "AuthorizedInstall.h"

@interface NSAlert ()
- (void)beginSheetModalForWindow:(NSWindow*)sheetWindow
               completionHandler:
                   (void (^__nullable)(NSModalResponse returnCode))handler;
@end

@interface AppDelegate ()<OmahaCommunicationDelegate, DownloaderDelegate> {
  InstallerWindowController* installerWindowController_;
  AuthorizedInstall* authorizedInstall_;
}
@property(strong) NSWindow* window;
@end

@implementation AppDelegate
@synthesize window = window_;

// Sets up the main window and begins the downloading process.
- (void)applicationDidFinishLaunching:(NSNotification*)aNotification {
  // TODO: fix UI not loading until after asking for authorization.
  installerWindowController_ =
      [[InstallerWindowController alloc] initWithWindow:window_];
  authorizedInstall_ = [[AuthorizedInstall alloc] init];
  if ([authorizedInstall_ loadInstallationTool]) {
    [self startDownload];
  } else {
    [self onLoadInstallationToolFailure];
  }
}

- (void)applicationWillTerminate:(NSNotification*)aNotification {
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)sender {
  return YES;
}

- (void)startDownload {
  [installerWindowController_ updateStatusDescription:@"Initializing..."];
  OmahaCommunication* omahaMessenger = [[OmahaCommunication alloc] init];
  omahaMessenger.delegate = self;

  [omahaMessenger fetchDownloadURLs];
}

- (void)onOmahaSuccessWithURLs:(NSArray*)URLs {
  [installerWindowController_ updateStatusDescription:@"Downloading..."];
  Downloader* download = [[Downloader alloc] init];
  download.delegate = self;
  [download downloadChromeImageToDownloadsDirectory:[URLs firstObject]];
}

- (void)onOmahaFailureWithError:(NSError*)error {
  NSError* networkError =
      [NSError errorForAlerts:@"Network Error"
              withDescription:@"Could not connect to Chrome server."
                isRecoverable:YES];
  [self displayError:networkError];
}

// Bridge method from Downloader to InstallerWindowController. Allows Downloader
// to update the progressbar without having direct access to any UI obejcts.
- (void)didDownloadData:(double)downloadProgressPercentage {
  [installerWindowController_
      updateDownloadProgress:(double)downloadProgressPercentage];
}

- (void)downloader:(Downloader*)download
    onDownloadSuccess:(NSURL*)diskImagePath {
  [installerWindowController_ updateStatusDescription:@"Done."];
  [installerWindowController_ enableLaunchButton];
  // TODO: Add unpacking step here and pass the path to the app bundle inside
  // the mounted disk image path to startInstall. Currently passing hardcoded
  // path to preunpacked app bundle.
  //[authorizedInstall_
  //    startInstall:@"$HOME/Downloads/Google Chrome.app"];
}

- (void)downloader:(Downloader*)download
    onDownloadFailureWithError:(NSError*)error {
  NSError* downloadError =
      [NSError errorForAlerts:@"Download Failure"
              withDescription:@"Unable to download Google Chrome."
                isRecoverable:NO];
  [self displayError:downloadError];
}

- (void)onLoadInstallationToolFailure {
  NSError* loadToolError = [NSError
       errorForAlerts:@"Could not load installion tool"
      withDescription:
          @"Your Chrome Installer may be corrupted. Download and try again."
        isRecoverable:NO];
  [self displayError:loadToolError];
}

// Displays an alert on the main window using the contents of the passed in
// error.
- (void)displayError:(NSError*)error {
  NSAlert* alertForUser = [NSAlert alertWithError:error];

  dispatch_async(dispatch_get_main_queue(), ^{
    [alertForUser beginSheetModalForWindow:window_
                         completionHandler:^(NSModalResponse returnCode) {
                           if (returnCode != [alertForUser quitButton]) {
                             [self startDownload];
                           } else {
                             [NSApp terminate:nil];
                           }
                         }];
  });
}

@end
