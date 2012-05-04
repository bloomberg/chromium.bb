/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef PACKAGES_LIBRARIES_NACL_MOUNTS_PATH_H_
#define PACKAGES_LIBRARIES_NACL_MOUNTS_PATH_H_

#include <stdint.h>
#include <string>
#include <vector>

#include "macros.h"

typedef std::vector<std::string> StringArray_t;

class Path {
 public:
  Path();
  Path(const Path& path);

  // This constructor splits path by '/' as a starting point for this Path.
  // If the path begins with the character '/', the path is considered
  // to be absolute.
  explicit Path(const std::string& path);
  ~Path();


  // Return true of the first path item is '/'.
  bool IsAbsolute() const;

  // Return a part of the path
  const std::string& Part(size_t index) const;

  // Return the number of path parts/
  size_t Size() const;

  // Update the path.
  Path& Append(const std::string& path);
  Path& Prepend(const std::string& path);
  Path& Set(const std::string& path);

  // Return the parent path.
  Path Parent() const;
  std::string Basename() const;

  std::string Join() const;
  std::string Range(size_t start, size_t end) const;
  StringArray_t Split() const;

  // Collapse the string list removing extraneous '.', '..' path components
  static StringArray_t Normalize(const StringArray_t& paths);
  static std::string Join(const StringArray_t& paths);
  static std::string Range(const StringArray_t& paths, size_t start,
                           size_t end);
  static StringArray_t Split(const std::string& paths);
  // Operator versions
  Path& operator=(const Path& p);
  Path& operator=(const std::string& str);

 private:
  // Internal representation of the path stored an array of string representing
  // the directory traversal.  The first string is a "/" if this is an abolute
  // path.
  StringArray_t paths_;
};

#endif  // PACKAGES_LIBRARIES_NACL_MOUNTS_PATH_H_

