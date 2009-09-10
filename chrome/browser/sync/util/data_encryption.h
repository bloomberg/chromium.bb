// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_UTIL_DATA_ENCRYPTION_H_
#define CHROME_BROWSER_SYNC_UTIL_DATA_ENCRYPTION_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/logging.h"
#include "chrome/browser/sync/util/sync_types.h"

using std::string;
using std::vector;

vector<uint8> EncryptData(const string& data);
bool DecryptData(const vector<uint8>& in_data, string* out_data);

#endif  // CHROME_BROWSER_SYNC_UTIL_DATA_ENCRYPTION_H_
