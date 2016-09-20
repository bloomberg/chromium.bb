// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/printing/printer_configuration.h"

#include <string>

namespace chromeos {

Printer::Printer() {}

Printer::Printer(const std::string& id) : id_(id) {}

Printer::Printer(const Printer& other) = default;

Printer& Printer::operator=(const Printer& other) = default;

Printer::~Printer() {}

void Printer::SetPPD(std::unique_ptr<Printer::PPDFile> ppd) {
  ppd_ = *(ppd.get());
}

}  // namespace chromeos
