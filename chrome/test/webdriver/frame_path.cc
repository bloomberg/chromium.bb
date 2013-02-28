// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/frame_path.h"

#include "base/strings/string_split.h"

namespace webdriver {

FramePath::FramePath() : path_("") {}

FramePath::FramePath(const FramePath& other) : path_(other.path_) {}

FramePath::FramePath(const std::string& path) : path_(path) {}

FramePath::~FramePath() {}

FramePath& FramePath::operator=(const FramePath& other) {
  path_ = other.path_;
  return *this;
}

bool FramePath::operator==(const FramePath& other) const {
  return path_ == other.path_;
}

FramePath FramePath::Append(const FramePath& path) const {
  return Append(path.path_);
}

FramePath FramePath::Append(const std::string& path) const {
  // An empty path refers to the root frame, so just return it.
  if (path.empty())
    return *this;

  // Don't append a separator if the current path is empty.
  std::string new_path = path_;
  if (path_.length())
    new_path += "\n";
  return FramePath(new_path + path);
}

FramePath FramePath::Parent() const {
  size_t i = path_.find_last_of("\n");
  if (i != std::string::npos)
    return FramePath(path_.substr(0, i));
  return FramePath();
}

FramePath FramePath::BaseName() const {
  size_t i = path_.find_last_of("\n");
  if (i != std::string::npos)
    return FramePath(path_.substr(i + 1));
  return *this;
}

void FramePath::GetComponents(std::vector<std::string>* components) const {
  if (IsSubframe())
    base::SplitString(path_, '\n', components);
}

bool FramePath::IsRootFrame() const {
  return path_.empty();
}

bool FramePath::IsSubframe() const {
  return path_.length();
}

const std::string& FramePath::value() const {
  return path_;
}

}  // namespace webdriver
