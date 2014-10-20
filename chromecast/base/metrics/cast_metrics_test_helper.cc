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
  virtual ~CastMetricsHelperStub();

  virtual void TagAppStart(const std::string& arg_app_name) override;
  virtual void LogMediaPlay() override;
  virtual void LogMediaPause() override;
  virtual void LogTimeToDisplayVideo() override;
  virtual void LogTimeToBufferAv(BufferingType buffering_type,
                                 base::TimeDelta time) override;
  virtual void ResetVideoFrameSampling() override;
  virtual void LogFramesPer5Seconds(
      int displayed_frames, int dropped_frames,
      int delayed_frames, int error_frames) override;
  virtual std::string GetMetricsNameWithAppName(
      const std::string& prefix, const std::string& suffix) const override;
  virtual void SetMetricsSink(MetricsSink* delegate) override;

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

void CastMetricsHelperStub::TagAppStart(const std::string& arg_app_name) {
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
