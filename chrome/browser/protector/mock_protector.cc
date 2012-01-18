// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/protector/mock_protector.h"

#include "googleurl/src/gurl.h"

namespace protector {

MockProtector::MockProtector(Profile* profile) : Protector(profile) {
}

MockProtector::~MockProtector() {
}

}  // namespace protector
