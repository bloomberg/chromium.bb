// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

namespace url_util {

// A dummy version of the url_util function called by url_constants.cc for all
// process types. For some processes, we don't want to link against googleurl.
// rather than having complicated versions of that library (32 and 64 bit ones
// on Windows) we just instead link this file in cases where googleurl isn't
// otherwise necessary.
void AddStandardScheme(const char* new_scheme) {
}

void LockStandardSchemes() {
}

}  // namespace url_util
