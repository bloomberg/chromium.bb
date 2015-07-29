// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SIZE_CLASS_SUPPORT_UTIL_H_
#define IOS_CHROME_BROWSER_UI_SIZE_CLASS_SUPPORT_UTIL_H_

#import "ios/chrome/browser/ui/UIView+SizeClassSupport.h"

// Returns the image with |image_name| for |idiom|.  This method assumes that
// images for use with SizeClassIdiom::REGULAR have "_large" as a suffix for the
// image name.
UIImage* GetImageForSizeClass(NSString* image_name, SizeClassIdiom idiom);

#endif  // IOS_CHROME_BROWSER_UI_SIZE_CLASS_SUPPORT_UTIL_H_
