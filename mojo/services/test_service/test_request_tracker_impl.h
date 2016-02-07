// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TEST_SERVICE_TEST_REQUEST_TRACKER_IMPL_H_
#define SERVICES_TEST_SERVICE_TEST_REQUEST_TRACKER_IMPL_H_

#include <stdint.h>

#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/public/cpp/system/macros.h"
#include "mojo/services/test_service/test_request_tracker.mojom.h"

namespace mojo {
namespace test {

typedef std::map<uint64_t, std::vector<ServiceStats> > AllRecordsMap;

// Shared state between all instances of TestRequestTrackerImpl
// and the master TrackedRequestService.
struct TrackingContext {
  TrackingContext();
  ~TrackingContext();
  AllRecordsMap records;
  std::map<uint64_t, std::string> ids_to_names;
  uint64_t next_id;
};

class TestRequestTrackerImpl : public TestRequestTracker {
 public:
  TestRequestTrackerImpl(InterfaceRequest<TestRequestTracker> request,
                         TrackingContext* context);
  ~TestRequestTrackerImpl() override;

  // TestRequestTracker.
  void SetNameAndReturnId(const String& service_name,
                          const Callback<void(uint64_t id)>& callback) override;
  void RecordStats(uint64_t client_id, ServiceStatsPtr stats) override;

 private:
  void UploaderNameCallback(uint64_t id, const mojo::String& name);
  TrackingContext* context_;
  StrongBinding<TestRequestTracker> binding_;
  base::WeakPtrFactory<TestRequestTrackerImpl> weak_factory_;
  MOJO_DISALLOW_COPY_AND_ASSIGN(TestRequestTrackerImpl);
};

class TestTrackedRequestServiceImpl : public TestTrackedRequestService {
 public:
  TestTrackedRequestServiceImpl(
      InterfaceRequest<TestTrackedRequestService> request,
      TrackingContext* context);
  ~TestTrackedRequestServiceImpl() override;

  // |TestTrackedRequestService| implementation.
  void GetReport(const mojo::Callback<void(mojo::Array<ServiceReportPtr>)>&
                     callback) override;

 private:
  TrackingContext* context_;
  StrongBinding<TestTrackedRequestService> binding_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(TestTrackedRequestServiceImpl);
};

}  // namespace test
}  // namespace mojo

#endif  // SERVICES_TEST_SERVICE_TEST_REQUEST_TRACKER_IMPL_H_
