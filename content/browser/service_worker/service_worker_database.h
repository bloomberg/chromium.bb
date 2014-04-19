// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_DATABASE_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_DATABASE_H_

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "url/gurl.h"

namespace content {

// Class to persist serviceworker regitration data in a database.
// Should NOT be used on the IO thread since this does blocking
// file io. The ServiceWorkerStorage class owns this class and
// is responsible for only calling it serially on background
// nonIO threads (ala SequencedWorkerPool).
class ServiceWorkerDatabase {
 public:
  // We do leveldb stuff in |path| or in memory if |path| is empty.
  explicit ServiceWorkerDatabase(const base::FilePath& path);
  ~ServiceWorkerDatabase();

  struct RegistrationData {
    // These values are immutable for the life of a registration.
    int64 registration_id;
    GURL scope;
    GURL script;

    // Versions are first stored once they successfully install and become
    // the waiting version. Then transition to the active version. The stored
    // version may be in the ACTIVE state or in the INSTALLED state.
    int64 version_id;
    bool is_active;
    bool has_fetch_handler;
    base::Time last_update_check;

    ServiceWorkerVersion::Status GetVersionStatus() const {
      if (is_active)
        return ServiceWorkerVersion::ACTIVE;
      return ServiceWorkerVersion::INSTALLED;
    }

    RegistrationData();
    ~RegistrationData();
  };

 private:
  base::FilePath path_;
  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerDatabase);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_DATABASE_H_
