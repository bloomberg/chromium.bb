// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/resource_bundle.h"

#import <Foundation/Foundation.h>

#include "base/file_path.h"
#include "base/logging.h"
#include "base/mac_util.h"
#include "skia/ext/skia_utils_mac.h"

namespace {

FilePath GetResourcesPakFilePath(NSString* name) {
  NSString *resource_path = [mac_util::MainAppBundle() pathForResource:name
                                                                ofType:@"pak"];
  if (!resource_path)
    return FilePath();
  return FilePath([resource_path fileSystemRepresentation]);
}

}  // namespace

// static
FilePath ResourceBundle::GetResourcesFilePath() {
  return GetResourcesPakFilePath(@"chrome");
}

// static
FilePath ResourceBundle::GetLocaleFilePath(const std::string& app_locale) {
  DLOG_IF(WARNING, !app_locale.empty())
      << "ignoring requested locale in favor of NSBundle's selection";
  return GetResourcesPakFilePath(@"locale");
}

NSImage* ResourceBundle::GetNSImageNamed(int resource_id) {
  // Currently this doesn't make a cache holding these as NSImages because
  // GetBitmapNamed has a cache, and we don't want to double cache.
  SkBitmap* bitmap = GetBitmapNamed(resource_id);
  if (!bitmap)
    return nil;

  NSImage* nsimage = gfx::SkBitmapToNSImage(*bitmap);
  return nsimage;
}
