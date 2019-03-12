// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_ITEMS_COLLECTION_CORE_RENAME_RESULT_H_
#define COMPONENTS_OFFLINE_ITEMS_COLLECTION_CORE_RENAME_RESULT_H_

// The type of download rename dialog that should by shown by Android.
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.offline_items_collection
enum class RenameResult {
  SUCCESS = 0,                   // Rename filename successfully
  FAILURE_NAME_CONFLICT = 1,     // Filename already exists
  FAILURE_NAME_TOO_LONG = 2,     // Illegal file name: too long
  FAILURE_NAME_UNAVIALABLE = 3,  // Name unavailable
  FAILURE_UNKNOWN = 4,           // Unknown
  kMaxValue = FAILURE_UNKNOWN
};

#endif  // COMPONENTS_OFFLINE_ITEMS_COLLECTION_CORE_RENAME_RESULT_H_
