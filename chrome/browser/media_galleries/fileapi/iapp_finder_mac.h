// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_IAPP_FINDER_MAC_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_IAPP_FINDER_MAC_H_

#include "chrome/browser/media_galleries/fileapi/iapp_finder.h"
#include "chrome/browser/storage_monitor/storage_info.h"

#if defined(__OBJC__)
#import <Foundation/Foundation.h>
#else  // __OBJC__
#include <CoreFoundation/CoreFoundation.h>
class NSString;
#endif  // __OBJC__

namespace iapps {

class IAppFinderMac : public IAppFinder {
 public:
  virtual ~IAppFinderMac();

 protected:
  IAppFinderMac(StorageInfo::Type type, CFStringRef recent_databases_key,
                const IAppFinderCallback& callback);

  virtual base::FilePath ExtractPath(NSString* path_ns) const = 0;

 private:
  virtual void FindIAppOnFileThread() OVERRIDE;

  CFStringRef recent_databases_key_;

  DISALLOW_COPY_AND_ASSIGN(IAppFinderMac);
};

}  // namespace iapps

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_ITUNES_FINDER_MAC_H_
