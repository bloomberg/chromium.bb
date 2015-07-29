// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/size_class_support_util.h"

#include "base/logging.h"

UIImage* GetImageForSizeClass(NSString* image_name, SizeClassIdiom idiom) {
  DCHECK(image_name.length);
  NSString* name = idiom == REGULAR
                       ? [NSString stringWithFormat:@"%@_large", image_name]
                       : image_name;
  return [UIImage imageNamed:name];
}
