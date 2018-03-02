// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/model/system_tray_model.h"

#include "ash/system/model/enterprise_domain_model.h"

namespace ash {

SystemTrayModel::SystemTrayModel()
    : enterprise_domain_(std::make_unique<EnterpriseDomainModel>()) {}

SystemTrayModel::~SystemTrayModel() = default;

}  // namespace ash
