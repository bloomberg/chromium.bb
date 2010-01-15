// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef CHROME_COMMON_DOM_STORAGE_COMMON_H_
#define CHROME_COMMON_DOM_STORAGE_COMMON_H_

const int64 kLocalStorageNamespaceId = 0;
const int64 kInvalidSessionStorageNamespaceId = kLocalStorageNamespaceId;

enum DOMStorageType {
  DOM_STORAGE_LOCAL = 0,
  DOM_STORAGE_SESSION
};

#endif  // CHROME_COMMON_DOM_STORAGE_COMMON_H_
