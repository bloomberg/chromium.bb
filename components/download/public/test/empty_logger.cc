// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/test/empty_logger.h"

#include "base/values.h"

namespace download {
namespace test {

void EmptyLogger::AddObserver(Observer* observer) {}

void EmptyLogger::RemoveObserver(Observer* observer) {}

base::Value EmptyLogger::GetServiceStatus() {
  return base::Value();
}

}  // namespace test
}  // namespace download
