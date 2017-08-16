// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CHILD_PROCESS_IMPORTANCE_H_
#define CONTENT_BROWSER_CHILD_PROCESS_IMPORTANCE_H_

namespace content {

// Importance of a child process. For renderer processes, the importance is
// independent from visibility of its WebContents.
// Values are listed in increasing importance.
//
// Note this is only used by and implemented on Android which exposes this API
// through public java code. If this is useful on other platforms, then this
// enum and related methods on WebContentsImpl should be moved to the public
// interface.
//
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.content_public.browser
enum class ChildProcessImportance {
  // NORMAL is the default value.
  NORMAL = 0,
  MODERATE,
  IMPORTANT,
  // Place holder to represent number of values.
  COUNT,
};

}  // namespace content

#endif  // CONTENT_BROWSER_CHILD_PROCESS_IMPORTANCE_H_
