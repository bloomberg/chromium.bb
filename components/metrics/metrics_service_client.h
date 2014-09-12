// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_METRICS_SERVICE_CLIENT_H_
#define COMPONENTS_METRICS_METRICS_SERVICE_CLIENT_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "components/metrics/proto/system_profile.pb.h"

namespace metrics {

class MetricsLogUploader;

// An abstraction of operations that depend on the embedder's (e.g. Chrome)
// environment.
class MetricsServiceClient {
 public:
  virtual ~MetricsServiceClient() {}

  // Registers the client id with other services (e.g. crash reporting), called
  // when metrics recording gets enabled.
  virtual void SetMetricsClientId(const std::string& client_id) = 0;

  // Whether there's an "off the record" (aka "Incognito") session active.
  virtual bool IsOffTheRecordSessionActive() = 0;

  // Returns the current application locale (e.g. "en-US").
  virtual std::string GetApplicationLocale() = 0;

  // Retrieves the brand code string associated with the install, returning
  // false if no brand code is available.
  virtual bool GetBrand(std::string* brand_code) = 0;

  // Returns the release channel (e.g. stable, beta, etc) of the application.
  virtual SystemProfileProto::Channel GetChannel() = 0;

  // Returns the version of the application as a string.
  virtual std::string GetVersionString() = 0;

  // Called by the metrics service when a log has been uploaded.
  virtual void OnLogUploadComplete() = 0;

  // Starts gathering metrics, calling |done_callback| when initial metrics
  // gathering is complete.
  virtual void StartGatheringMetrics(const base::Closure& done_callback) = 0;

  // Called prior to a metrics log being closed, allowing the client to collect
  // extra histograms that will go in that log. Asynchronous API - the client
  // implementation should call |done_callback| when complete.
  virtual void CollectFinalMetrics(const base::Closure& done_callback) = 0;

  // Creates a MetricsLogUploader with the specified parameters (see comments on
  // MetricsLogUploader for details).
  virtual scoped_ptr<MetricsLogUploader> CreateUploader(
      const std::string& server_url,
      const std::string& mime_type,
      const base::Callback<void(int)>& on_upload_complete) = 0;

  // Returns the name of a key under HKEY_CURRENT_USER that can be used to store
  // backups of metrics data. Unused except on Windows.
  virtual base::string16 GetRegistryBackupKey();
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_METRICS_SERVICE_CLIENT_H_
