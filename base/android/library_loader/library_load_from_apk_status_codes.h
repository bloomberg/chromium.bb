// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ANDROID_LIBRARY_LOAD_FROM_APK_STATUS_CODES_H_
#define BASE_ANDROID_LIBRARY_LOAD_FROM_APK_STATUS_CODES_H_

namespace base {
namespace android {

namespace {

// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.base.library_loader
enum LibraryLoadFromApkStatusCodes {
  // The loader was unable to determine whether the functionality is supported.
  LIBRARY_LOAD_FROM_APK_STATUS_CODES_UNKNOWN = 0,

  // The device does not support loading a library directly from the APK file.
  LIBRARY_LOAD_FROM_APK_STATUS_CODES_NOT_SUPPORTED = 1,

  // The device supports loading a library directly from the APK file.
  LIBRARY_LOAD_FROM_APK_STATUS_CODES_SUPPORTED = 2,

  // A library was successfully loaded directly from the APK file.
  LIBRARY_LOAD_FROM_APK_STATUS_CODES_SUCCESSFUL = 3,

  // End sentinel.
  LIBRARY_LOAD_FROM_APK_STATUS_CODES_MAX = 4,
};

}  // namespace

}  // namespace android
}  // namespace base

#endif  // BASE_ANDROID_LIBRARY_LOAD_FROM_APK_STATUS_CODES_H_
