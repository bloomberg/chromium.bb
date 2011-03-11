// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_WEBDRIVER_FRAME_PATH_H_
#define CHROME_TEST_WEBDRIVER_FRAME_PATH_H_
#pragma once

#include <string>

namespace webdriver {

// A path identifying a frame within an HTML window. Recognized by Chrome
// for identifying a frame of the window to execute JavaScript in.
class FramePath {
 public:
  // Creates a frame path that refers to the root frame.
  FramePath();
  FramePath(const FramePath& other);
  explicit FramePath(const std::string& path);
  ~FramePath();
  FramePath& operator=(const FramePath& other);

  bool operator==(const FramePath& other) const;

  // Returns a FramePath appended with the given FramePath.
  FramePath Append(const FramePath& path) const;

  // Returns a FramePath appended with the given path.
  FramePath Append(const std::string& path) const;

  // Gets the parent path.
  FramePath Parent() const;

  // Gets the last path component.
  FramePath BaseName() const;

  // Returns whether the path refers to the root frame.
  bool IsRootFrame() const;

  // Returns whether the path refers to a subframe.
  bool IsSubframe() const;

  // Returns the identifier's string representation.
  const std::string& value() const;

 private:
  // This identifier contains newline-delimited xpath expressions to identify
  // a subframe multiple level deep.
  // Example: '//iframe[2]\n/frameset/frame[1]' identifies the first frame in
  //          the root document's second iframe.
  std::string path_;
};

}  // namespace webdriver

#endif  // CHROME_TEST_WEBDRIVER_FRAME_PATH_H_
