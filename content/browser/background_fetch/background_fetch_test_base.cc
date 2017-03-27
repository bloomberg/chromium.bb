// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_test_base.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "content/browser/background_fetch/background_fetch_registration_id.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "url/gurl.h"

namespace content {

namespace {

const char kTestOrigin[] = "https://example.com/";
const char kTestScriptUrl[] = "https://example.com/sw.js";

void DidRegisterServiceWorker(int64_t* out_service_worker_registration_id,
                              base::Closure quit_closure,
                              ServiceWorkerStatusCode status,
                              const std::string& status_message,
                              int64_t service_worker_registration_id) {
  DCHECK(out_service_worker_registration_id);
  EXPECT_EQ(SERVICE_WORKER_OK, status) << status_message;

  *out_service_worker_registration_id = service_worker_registration_id;

  quit_closure.Run();
}

void DidFindServiceWorkerRegistration(
    scoped_refptr<ServiceWorkerRegistration>* out_service_worker_registration,
    base::Closure quit_closure,
    ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> service_worker_registration) {
  DCHECK(out_service_worker_registration);
  EXPECT_EQ(SERVICE_WORKER_OK, status) << ServiceWorkerStatusToString(status);

  *out_service_worker_registration = service_worker_registration;

  quit_closure.Run();
}

}  // namespace

BackgroundFetchTestBase::BackgroundFetchTestBase()
    : thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP),
      origin_(GURL(kTestOrigin)) {}

BackgroundFetchTestBase::~BackgroundFetchTestBase() {
  DCHECK(set_up_called_);
  DCHECK(tear_down_called_);
}

void BackgroundFetchTestBase::SetUp() {
  set_up_called_ = true;
}

void BackgroundFetchTestBase::TearDown() {
  service_worker_registrations_.clear();
  tear_down_called_ = true;
}

bool BackgroundFetchTestBase::CreateRegistrationId(
    const std::string& tag,
    BackgroundFetchRegistrationId* registration_id) {
  DCHECK(registration_id);
  DCHECK(registration_id->is_null());

  GURL script_url(kTestScriptUrl);

  int64_t service_worker_registration_id = kInvalidServiceWorkerRegistrationId;

  {
    base::RunLoop run_loop;
    embedded_worker_test_helper_.context()->RegisterServiceWorker(
        origin_.GetURL(), script_url, nullptr /* provider_host */,
        base::Bind(&DidRegisterServiceWorker, &service_worker_registration_id,
                   run_loop.QuitClosure()));

    run_loop.Run();
  }

  if (service_worker_registration_id == kInvalidServiceWorkerRegistrationId) {
    ADD_FAILURE() << "Could not obtain a valid Service Worker registration";
    return false;
  }

  scoped_refptr<ServiceWorkerRegistration> service_worker_registration;

  {
    base::RunLoop run_loop;
    embedded_worker_test_helper_.context()->storage()->FindRegistrationForId(
        service_worker_registration_id, origin_.GetURL(),
        base::Bind(&DidFindServiceWorkerRegistration,
                   &service_worker_registration, run_loop.QuitClosure()));

    run_loop.Run();
  }

  // Wait for the worker to be activated.
  base::RunLoop().RunUntilIdle();

  if (!service_worker_registration) {
    ADD_FAILURE() << "Could not find the new Service Worker registration.";
    return false;
  }

  *registration_id = BackgroundFetchRegistrationId(
      service_worker_registration->id(), origin_, tag);

  service_worker_registrations_.push_back(
      std::move(service_worker_registration));
  return true;
}

}  // namespace content
