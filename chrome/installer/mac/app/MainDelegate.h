// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_MAC_APP_MAINDELEGATE_H_
#define CHROME_INSTALLER_MAC_APP_MAINDELEGATE_H_

#import <Foundation/Foundation.h>

#import "Downloader.h"
#import "OmahaCommunication.h"

// TODO: move this into the unpacking file when created
@protocol UnpackDelegate
- (void)onUnpackSuccess;
@end

@interface MainDelegate
    : NSObject<OmahaCommunicationDelegate, DownloaderDelegate, UnpackDelegate>
- (void)runApplication;
@end

#endif  // CHROME_INSTALLER_MAC_APP_MAINDELEGATE_H_
