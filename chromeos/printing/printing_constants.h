// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PRINTING_PRINTING_CONSTANTS_H_
#define CHROMEOS_PRINTING_PRINTING_CONSTANTS_H_

namespace chromeos {
namespace printing {

// Maximum size of a PPD file that we will accept, currently 250k.  This number
// is relatively
// arbitrary, we just don't want to try to handle ridiculously huge files.
constexpr size_t kMaxPpdSizeBytes = 250 * 1024;

}  // namespace printing
}  // namespace chromeos

#endif  // CHROMEOS_PRINTING_PRINTING_CONSTANTS_H_
