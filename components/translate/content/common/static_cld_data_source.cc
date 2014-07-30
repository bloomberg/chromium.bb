// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cld_data_source.h"

namespace translate {

std::string CldDataSource::GetName() {
  return "static";
}

bool CldDataSource::ShouldRegisterForComponentUpdates() {
  return false;
}

bool CldDataSource::ShouldUseStandaloneDataFile() {
  return false;
}

}  // namespace translate
