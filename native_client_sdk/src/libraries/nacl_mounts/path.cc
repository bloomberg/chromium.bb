/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdio.h>
#include <string.h>
#include <string>

#include "nacl_mounts/path.h"

Path::Path() {}

Path::Path(const Path& path) {
  paths_ = path.paths_;
}

Path::Path(const std::string& path) {
  Set(path);
}

Path::~Path() {}

bool Path::IsAbsolute() const {
  return !paths_.empty() && paths_[0] == "/";
}

const std::string& Path::Part(size_t index) const {
  return paths_[index];
}

size_t Path::Size() const {
  return paths_.size();
}

Path& Path::Append(const std::string& path) {
  StringArray_t paths = Split(path);

  for (size_t index = 0; index < paths.size(); index++) {
    // Skip ROOT
    if (paths_.size() && index == 0 && paths[0] == "/") continue;
    paths_.push_back(paths[index]);
  }

  paths_ = Normalize(paths_);
  return *this;
}

Path& Path::Prepend(const std::string& path) {
  StringArray_t paths = Split(path);

  for (size_t index = 0; index < paths_.size(); index++) {
    // Skip ROOT
    if (index == 0 && paths_[0] == "/") continue;
    paths.push_back(paths[index]);
  }
  paths_ = Normalize(paths);
  return *this;
}

Path& Path::Set(const std::string& path) {
  StringArray_t paths = Split(path);
  paths_ = Normalize(paths);
  return *this;
}

Path Path::Parent() const {
  Path out;
  out.paths_ = paths_;
  if (out.paths_.size()) out.paths_.pop_back();
  return out;
}

std::string Path::Basename() const {
  if (paths_.size()) return paths_.back();
  return std::string();
}

std::string Path::Join() const {
  return Range(paths_, 0, paths_.size());
}

std::string Path::Range(size_t start, size_t end) const {
  return Range(paths_, start, end);
}

StringArray_t Path::Split() const {
  return paths_;
}

// static
StringArray_t Path::Normalize(const StringArray_t& paths) {
  StringArray_t path_out;
  int back = 0;

  for (size_t index = 0; index < paths.size(); index++) {
    const std::string &curr = paths[index];

    // Check if '/' was used excessively in the path.
    // For example, in cd Desktop/////
    if (curr == "/" && index != 0) continue;

    // Check for '.' in the path and remove it
    if (curr == ".") continue;

    // Check for '..'
    if (curr == "..") {
      // If the path is empty, or "..", then add ".."
      if (path_out.empty() || path_out.back() == "..") {
        path_out.push_back(curr);
        continue;
      }

      // If the path is at root, "/.." = "/"
      if (path_out.back() == "/") {
        continue;
      }

      // if we are already at root, then stay there (root/.. -> root)
      if (path_out.back() == "/") {
        continue;
      }

      // otherwise, pop off the top path component
      path_out.pop_back();
      continue;
    }

    // By now, we should have handled end cases so just append.
    path_out.push_back(curr);
  }

  // If the path was valid, but now it's empty, return self
  if (path_out.size() == 0) path_out.push_back(".");

  return path_out;
}

// static
std::string Path::Join(const StringArray_t& paths) {
  return Range(paths, 0, paths.size());
}

// static
std::string Path::Range(const StringArray_t& paths, size_t start, size_t end) {
  std::string out_path;
  size_t index = start;

  if (end > paths.size()) end = paths.size();

  if (start == 0 && end > 0 && paths[0] == "/") {
    if (end == 1) return std::string("/");
    index++;
  }

  for (; index < end; index++) {
    if (index) out_path += "/";
    out_path += paths[index];
  }

  return out_path;
}

// static
StringArray_t Path::Split(const std::string& path) {
  StringArray_t components;
  size_t offs = 0;
  size_t next = 0;
  ssize_t cnt  = 0;

  if (path[0] == '/') {
    offs = 1;
    components.push_back("/");
  }

  while (next != std::string::npos) {
    next = path.find('/', offs);

    // Remove extra seperators
    if (next == offs) {
      ++offs;
      continue;
    }

    std::string part = path.substr(offs, next - offs);
    if (!part.empty()) components.push_back(part);
    offs = next + 1;
  }
  return components;
}

Path& Path::operator =(const Path& p) {
  paths_ = p.paths_;
  return *this;
}

Path& Path::operator =(const std::string& p) {
  return Set(p);
}