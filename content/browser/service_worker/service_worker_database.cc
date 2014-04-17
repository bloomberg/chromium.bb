// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_database.h"

namespace content {

ServiceWorkerDatabase::RegistrationData::RegistrationData()
    : registration_id(-1),
      version_id(-1),
      is_active(false),
      has_fetch_handler(false) {
}

ServiceWorkerDatabase::RegistrationData::~RegistrationData() {
}

ServiceWorkerDatabase::ServiceWorkerDatabase(const base::FilePath& path)
    : path_(path) {
}

ServiceWorkerDatabase::~ServiceWorkerDatabase() {
}

}  // namespace content
