// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/printing/printer_configuration.h"

#include <string>

#include "base/guid.h"

namespace chromeos {

Printer::Printer() {
  id_ = base::GenerateGUID();
}

Printer::Printer(const std::string& id) : id_(id) {
  if (id_.empty())
    id_ = base::GenerateGUID();
}

Printer::Printer(const Printer& other) = default;

Printer& Printer::operator=(const Printer& other) = default;

Printer::~Printer() {}

}  // namespace chromeos
