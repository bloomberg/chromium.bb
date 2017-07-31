// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_ZUCCHINI_ZUCCHINI_H_
#define CHROME_INSTALLER_ZUCCHINI_ZUCCHINI_H_

// Definitions, structures, and interfaces for the Zucchini library.

namespace zucchini {

namespace status {

// Zucchini status code, which can also be used as process exit code. Therefore
// success is explicitly 0.
enum Code {
  kStatusSuccess = 0,
  kStatusInvalidParam = 1,
  kStatusFileReadError = 2,
  kStatusFileWriteError = 3,
};

}  // namespace status

}  // namespace zucchini

#endif  // CHROME_INSTALLER_ZUCCHINI_ZUCCHINI_H_
