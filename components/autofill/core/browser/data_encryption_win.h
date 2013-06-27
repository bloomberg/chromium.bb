// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_ENCRYPTION_WIN_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_ENCRYPTION_WIN_H_

#include <string>
#include <vector>

#include "base/basictypes.h"

namespace autofill {

std::vector<uint8> EncryptData(const std::string& data);
bool DecryptData(const std::vector<uint8>& in_data, std::string* out_data);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_ENCRYPTION_WIN_H_
