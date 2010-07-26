// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APP_DOWNLOAD_FILE_INTERFACE_H_
#define APP_DOWNLOAD_FILE_INTERFACE_H_
#pragma once

#include "build/build_config.h"

#include "base/basictypes.h"
#include "base/ref_counted.h"

#if defined(OS_WIN)
#include <objidl.h>
#endif

class FilePath;

// Defines the interface to observe the status of file download.
class DownloadFileObserver
    : public base::RefCountedThreadSafe<DownloadFileObserver> {
 public:
  virtual void OnDownloadCompleted(const FilePath& file_path) = 0;
  virtual void OnDownloadAborted() = 0;

 protected:
  friend class base::RefCountedThreadSafe<DownloadFileObserver>;
  virtual ~DownloadFileObserver() {}
};

// Defines the interface to control how a file is downloaded.
class DownloadFileProvider
    : public base::RefCountedThreadSafe<DownloadFileProvider> {
 public:
  virtual bool Start(DownloadFileObserver* observer) = 0;
  virtual void Stop() = 0;
#if defined(OS_WIN)
  virtual IStream* GetStream() = 0;
#endif

 protected:
  friend class base::RefCountedThreadSafe<DownloadFileProvider>;
  virtual ~DownloadFileProvider() {}
};

#endif  // APP_DOWNLOAD_FILE_INTERFACE_H_
