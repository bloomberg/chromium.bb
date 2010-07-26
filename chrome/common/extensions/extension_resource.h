// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_EXTENSION_RESOURCE_H_
#define CHROME_COMMON_EXTENSIONS_EXTENSION_RESOURCE_H_
#pragma once

#include <string>

#include "base/file_path.h"
#include "base/platform_thread.h"

// Represents a resource inside an extension. For example, an image, or a
// JavaScript file. This is more complicated than just a simple FilePath
// because extension resources can come from multiple physical file locations
// depending on locale.
class ExtensionResource {
 public:
  ExtensionResource();

  ExtensionResource(const std::string& extension_id,
                    const FilePath& extension_root,
                    const FilePath& relative_path);

  // Returns actual path to the resource (default or locale specific). In the
  // browser process, this will DCHECK if not called on the file thread. To
  // easily load extension images on the UI thread, see ImageLoadingTracker.
  const FilePath& GetFilePath() const;

  // Gets the physical file path for the extension resource, taking into account
  // localization. In the browser process, this will DCHECK if not called on the
  // file thread. To easily load extension images on the UI thread, see
  // ImageLoadingTracker.
  static FilePath GetFilePath(const FilePath& extension_root,
                              const FilePath& relative_path);

  // Gets the file path on any thread. Unlike GetFilePath(), these can be called
  // from any thread without DCHECKing.
  //
  // In the browser process, calling this method is almost always wrong. Use
  // GetFilePath() on the file thread instead.
  const FilePath& GetFilePathOnAnyThreadHack() const;
  static FilePath GetFilePathOnAnyThreadHack(const FilePath& extension_root,
                                             const FilePath& relative_path);

  // Setter for the proper thread to run file tasks on.
  static void set_file_thread_id(PlatformThreadId thread_id) {
    file_thread_id_ = thread_id;
    check_for_file_thread_ = true;
  }

  // Checks whether we are running on the file thread and DCHECKs if not. Relies
  // on set_file_thread_id being called first, otherwise, it will not DCHECK.
  static void CheckFileAccessFromFileThread();

  // Getters
  const std::string& extension_id() const { return extension_id_; }
  const FilePath& extension_root() const { return extension_root_; }
  const FilePath& relative_path() const { return relative_path_; }

  bool empty() { return extension_root().empty(); }

  // Unit test helpers.
  FilePath::StringType NormalizeSeperators(FilePath::StringType path) const;
  bool ComparePathWithDefault(const FilePath& path) const;

 private:
  // The id of the extension that this resource is associated with.
  std::string extension_id_;

  // Extension root.
  FilePath extension_root_;

  // Relative path to resource.
  FilePath relative_path_;

  // Full path to extension resource. Starts empty.
  mutable FilePath full_resource_path_;

  // The thread id for the file thread. If set, GetFilePath() and related
  // methods will DCHECK that they are on this thread when called.
  static PlatformThreadId file_thread_id_;

  // Whether to check for file thread. See |file_thread_id_|. If set,
  // GetFilePath() and related methods will DCHECK that they are on this thread
  // when called.
  static bool check_for_file_thread_;
};

#endif  // CHROME_COMMON_EXTENSIONS_EXTENSION_RESOURCE_H_
