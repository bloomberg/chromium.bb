// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MACROS_H_
#define BASE_MACROS_H_

// Use this when declaring/defining noexcept move constructors, to work around a
// bug on older versions of g++.
//
// TODO(issues/40): Delete this macro once the g++ version is upgraded on the
// bots.
#if defined(__GNUC__) && __GNUC__ < 6
#define MAYBE_NOEXCEPT
#else
#define MAYBE_NOEXCEPT noexcept
#endif

#define DISALLOW_COPY(ClassName) ClassName(const ClassName&) = delete
#define DISALLOW_ASSIGN(ClassName) \
  ClassName& operator=(const ClassName&) = delete
#define DISALLOW_COPY_AND_ASSIGN(ClassName) \
  DISALLOW_COPY(ClassName);                 \
  DISALLOW_ASSIGN(ClassName)
#define DISALLOW_IMPLICIT_CONSTRUCTORS(ClassName) \
  ClassName() = delete;                           \
  DISALLOW_COPY_AND_ASSIGN(ClassName)

#endif  // BASE_MACROS_H_
