// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_SCOPED_BOOL_H_
#define BASE_SCOPED_BOOL_H_

// ScopedBool is useful for setting a flag only during a particular scope.  If
// you have code that has to add "var = false;" at all the exit points of a
// function, for example, you would benefit from using this instead.

class ScopedBool {
 public:
  explicit ScopedBool(bool* scoped_bool) : scoped_bool_(scoped_bool) {
    *scoped_bool_ = true;
  }
  ~ScopedBool() { *scoped_bool_ = false; }

 private:
  bool* scoped_bool_;
  DISALLOW_COPY_AND_ASSIGN(ScopedBool);
};

#endif  // BASE_SCOPED_BOOL_H_
