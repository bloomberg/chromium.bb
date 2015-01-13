// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/base/metrics/cast_metrics_test_helper.h"

#include "base/logging.h"
#include "base/macros.h"
#include "chromecast/base/metrics/cast_metrics_helper.h"

namespace chromecast {
namespace metrics {

namespace {

class CastMetricsHelperStub : public CastMetricsHelper {
 public:
  CastMetricsHelperStub();
  ~CastMetricsHelperStub() override;

  void UpdateCurrentAppInfo(const std::string& app_id,
                            const std::string& session_id) override;
  void UpdateSDKInfo(const std::string& sdk_version) override;
  void LogMediaPlay() override;
  void LogMediaPause() override;
  void LogTimeToDisplayVideo() override;
  void LogTimeToBufferAv(BufferingType buffering_type,
                         base::TimeDelta time) override;
  void ResetVideoFrameSampling() override;
  void LogFramesPer5Seconds(
      int displayed_frames, int dropped_frames,
      int delayed_frames, int error_frames) override;
  std::string GetMetricsNameWithAppName(
      const std::string& prefix, const std::string& suffix) const override;
  void SetMetricsSink(MetricsSink* delegate) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(CastMetricsHelperStub);
};

bool stub_instance_exists = false;

CastMetricsHelperStub::CastMetricsHelperStub()
    : CastMetricsHelper() {
  DCHECK(!stub_instance_exists);
  stub_instance_exists = true;
}

CastMetricsHelperStub::~CastMetricsHelperStub() {
  DCHECK(stub_instance_exists);
  stub_instance_exists = false;
}

void CastMetricsHelperStub::UpdateCurrentAppInfo(
    const std::string& app_id,
    const std::string& session_id) {
}

void CastMetricsHelperStub::UpdateSDKInfo(const std::string& sdk_version) {
}

void CastMetricsHelperStub::LogMediaPlay() {
}

void CastMetricsHelperStub::LogMediaPause() {
}

void CastMetricsHelperStub::LogTimeToDisplayVideo() {
}

void CastMetricsHelperStub::LogTimeToBufferAv(BufferingType buffering_type,
                                              base::TimeDelta time) {
}

void CastMetricsHelperStub::ResetVideoFrameSampling() {
}

void CastMetricsHelperStub::LogFramesPer5Seconds(int displayed_frames,
                                                 int dropped_frames,
                                                 int delayed_frames,
                                                 int error_frames) {
}

std::string CastMetricsHelperStub::GetMetricsNameWithAppName(
    const std::string& prefix,
    const std::string& suffix) const {
  return "";
}

void CastMetricsHelperStub::SetMetricsSink(MetricsSink* delegate) {
}

}  // namespace

void InitializeMetricsHelperForTesting() {
  if (!stub_instance_exists) {
    new CastMetricsHelperStub();
  }
}

}  // namespace metrics
}  // namespace chromecast
