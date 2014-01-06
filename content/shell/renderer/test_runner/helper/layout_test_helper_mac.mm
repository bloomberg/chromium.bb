// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2012 Apple Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import <AvailabilityMacros.h>
#import <AppKit/AppKit.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

// This is a simple helper app that changes the color sync profile to the
// generic profile and back when done.  This program is managed by the layout
// test script, so it can do the job for multiple DumpRenderTree while they are
// running layout tests.

namespace {

#if defined(MAC_OS_X_VERSION_10_7) && \
    MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_7

CFURLRef user_color_profile_url;

void InstallLayoutTestColorProfile() {
  // To make sure we get consistent colors (not dependent on the chosen color
  // space of the main display), we force the generic RGB color profile.
  // This causes a change the user can see.

  CFUUIDRef main_display_id =
      CGDisplayCreateUUIDFromDisplayID(CGMainDisplayID());

  if (!user_color_profile_url) {
    CFDictionaryRef device_info = ColorSyncDeviceCopyDeviceInfo(
        kColorSyncDisplayDeviceClass, main_display_id);

    if (!device_info) {
      NSLog(@"No display attached to system; not setting main display's color "
             "profile.");
      CFRelease(main_display_id);
      return;
    }

    CFDictionaryRef profile_info = (CFDictionaryRef)CFDictionaryGetValue(
        device_info, kColorSyncCustomProfiles);
    if (profile_info) {
      user_color_profile_url =
          (CFURLRef)CFDictionaryGetValue(profile_info, CFSTR("1"));
      CFRetain(user_color_profile_url);
    } else {
      profile_info = (CFDictionaryRef)CFDictionaryGetValue(
          device_info, kColorSyncFactoryProfiles);
      CFDictionaryRef factory_profile =
          (CFDictionaryRef)CFDictionaryGetValue(profile_info, CFSTR("1"));
      user_color_profile_url = (CFURLRef)CFDictionaryGetValue(
          factory_profile, kColorSyncDeviceProfileURL);
      CFRetain(user_color_profile_url);
    }

    CFRelease(device_info);
  }

  ColorSyncProfileRef generic_rgb_profile =
      ColorSyncProfileCreateWithName(kColorSyncGenericRGBProfile);
  CFErrorRef error;
  CFURLRef profile_url = ColorSyncProfileGetURL(generic_rgb_profile, &error);
  if (!profile_url) {
    NSLog(@"Failed to get URL of Generic RGB color profile! Many pixel tests "
           "may fail as a result. Error: %@",
          error);

    if (user_color_profile_url) {
      CFRelease(user_color_profile_url);
      user_color_profile_url = 0;
    }

    CFRelease(generic_rgb_profile);
    CFRelease(main_display_id);
    return;
  }

  CFMutableDictionaryRef profile_info =
      CFDictionaryCreateMutable(kCFAllocatorDefault,
                                0,
                                &kCFTypeDictionaryKeyCallBacks,
                                &kCFTypeDictionaryValueCallBacks);
  CFDictionarySetValue(
      profile_info, kColorSyncDeviceDefaultProfileID, profile_url);

  if (!ColorSyncDeviceSetCustomProfiles(
           kColorSyncDisplayDeviceClass, main_display_id, profile_info)) {
    NSLog(@"Failed to set color profile for main display! Many pixel tests may "
           "fail as a result.");

    if (user_color_profile_url) {
      CFRelease(user_color_profile_url);
      user_color_profile_url = 0;
    }
  }

  CFRelease(profile_info);
  CFRelease(generic_rgb_profile);
  CFRelease(main_display_id);
}

void RestoreUserColorProfile() {
  // This is used as a signal handler, and thus the calls into ColorSync are
  // unsafe.
  // But we might as well try to restore the user's color profile, we're going
  // down anyway...

  if (!user_color_profile_url)
    return;

  CFUUIDRef main_display_id =
      CGDisplayCreateUUIDFromDisplayID(CGMainDisplayID());
  CFMutableDictionaryRef profile_info =
      CFDictionaryCreateMutable(kCFAllocatorDefault,
                                0,
                                &kCFTypeDictionaryKeyCallBacks,
                                &kCFTypeDictionaryValueCallBacks);
  CFDictionarySetValue(
      profile_info, kColorSyncDeviceDefaultProfileID, user_color_profile_url);
  ColorSyncDeviceSetCustomProfiles(
      kColorSyncDisplayDeviceClass, main_display_id, profile_info);
  CFRelease(main_display_id);
  CFRelease(profile_info);
}

#else  // For Snow Leopard and before, use older CM* API.

const char color_profile_path[] =
    "/System/Library/ColorSync/Profiles/Generic RGB Profile.icc";

// The locType field is initialized to 0 which is the same as cmNoProfileBase.
CMProfileLocation initial_color_profile_location;

void InstallLayoutTestColorProfile() {
  // To make sure we get consistent colors (not dependent on the Main display),
  // we force the generic rgb color profile.  This cases a change the user can
  // see.
  const CMDeviceScope scope = {kCFPreferencesCurrentUser,
                               kCFPreferencesCurrentHost};

  CMProfileRef profile = 0;
  int error =
      CMGetProfileByAVID((CMDisplayIDType)kCGDirectMainDisplay, &profile);
  if (!error) {
    UInt32 size = sizeof(initial_color_profile_location);
    error = NCMGetProfileLocation(profile, &initial_color_profile_location, &size);
    CMCloseProfile(profile);
  }
  if (error) {
    NSLog(@"failed to get the current color profile, pixmaps won't match. "
           "Error: %d",
          (int)error);
    initial_color_profile_location.locType = cmNoProfileBase;
    return;
  }

  CMProfileLocation location;
  location.locType = cmPathBasedProfile;
  strncpy(location.u.pathLoc.path,
          color_profile_path,
          sizeof(location.u.pathLoc.path));
  error = CMSetDeviceProfile(cmDisplayDeviceClass,
                             (CMDeviceID)kCGDirectMainDisplay,
                             &scope,
                             cmDefaultProfileID,
                             &location);
  if (error) {
    NSLog(@"failed install the generic color profile, pixmaps won't match. "
           "Error: %d",
          (int)error);
    initial_color_profile_location.locType = cmNoProfileBase;
  }
}

void RestoreUserColorProfile() {
  // This is used as a signal handler, and thus the calls into ColorSync are
  // unsafe.
  // But we might as well try to restore the user's color profile, we're going
  // down anyway...
  if (initial_color_profile_location.locType != cmNoProfileBase) {
    const CMDeviceScope scope = {kCFPreferencesCurrentUser,
                                 kCFPreferencesCurrentHost};
    int error = CMSetDeviceProfile(cmDisplayDeviceClass,
                                   (CMDeviceID)kCGDirectMainDisplay,
                                   &scope,
                                   cmDefaultProfileID,
                                   &initial_color_profile_location);
    if (error) {
      NSLog(@"Failed to restore color profile, use System Preferences -> "
             "Displays -> Color to reset. Error: %d",
            (int)error);
    }
    initial_color_profile_location.locType = cmNoProfileBase;
  }
}

#endif

void SimpleSignalHandler(int sig) {
  // Try to restore the color profile and try to go down cleanly.
  RestoreUserColorProfile();
  exit(128 + sig);
}

}  // namespace

int main(int argc, char* argv[]) {
  NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];

  // Hooks the ways we might get told to clean up...
  signal(SIGINT, SimpleSignalHandler);
  signal(SIGHUP, SimpleSignalHandler);
  signal(SIGTERM, SimpleSignalHandler);

  // Save off the current profile, and then install the layout test profile.
  InstallLayoutTestColorProfile();

  // Let the script know we're ready.
  printf("ready\n");
  fflush(stdout);

  // Wait for any key (or signal).
  getchar();

  // Restore the profile.
  RestoreUserColorProfile();

  [pool release];
  return 0;
}
