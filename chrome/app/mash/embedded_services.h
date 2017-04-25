// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_APP_MASH_EMBEDDED_SERVICES_H_
#define CHROME_APP_MASH_EMBEDDED_SERVICES_H_

#include <memory>

#include "services/service_manager/public/cpp/service.h"

// Starts one of Mash's embedded services.
std::unique_ptr<service_manager::Service> CreateEmbeddedMashService(
    const std::string& service_name);

#endif  // CHROME_APP_MASH_EMBEDDED_SERVICES_H_
