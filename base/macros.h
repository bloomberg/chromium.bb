// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MACROS_H_
#define BASE_MACROS_H_

#define DISALLOW_COPY(ClassName) ClassName(const ClassName&) = delete
#define DISALLOW_ASSIGN(ClassName) \
  ClassName& operator=(const ClassName&) = delete
#define DISALLOW_COPY_AND_ASSIGN(ClassName) \
  DISALLOW_COPY(ClassName);                 \
  DISALLOW_ASSIGN(ClassName)

#endif  // BASE_MACROS_H_
