// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DIAGNOSTICSD_TESTING_DIAGNOSTICSD_BRIDGE_WRAPPER_H_
#define CHROME_BROWSER_CHROMEOS_DIAGNOSTICSD_TESTING_DIAGNOSTICSD_BRIDGE_WRAPPER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/chromeos/diagnosticsd/diagnosticsd_bridge.h"
#include "chrome/services/diagnosticsd/public/mojom/diagnosticsd.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace chromeos {

// Manages a fake instance of DiagnosticsdBridge for unit tests. Mocks out the
// Mojo communication and provides tools for simulating and handling Mojo
// requests.
class TestingDiagnosticsdBridgeWrapper final {
 public:
  // |mojo_diagnosticsd_service| is an unowned pointer that should be a stub
  // implementation of the DiagnosticsdService Mojo service (which in production
  // is implemented by the wilco_dtc_supportd daemon).
  // |bridge| is an unowned bridge instance that holds the stub diagnosticsd
  // bridge instance created by the TestingDiagnosticsdBridgeWrapper.
  static std::unique_ptr<TestingDiagnosticsdBridgeWrapper> Create(
      diagnosticsd::mojom::DiagnosticsdService* mojo_diagnosticsd_service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loade_factory,
      std::unique_ptr<DiagnosticsdBridge>* bridge);

  ~TestingDiagnosticsdBridgeWrapper();

  // Simulates bootstrapping the Mojo communication between the diagnosticsd
  // daemon and the browser.
  void EstablishFakeMojoConnection();

  // Returns a pointer that allows to simulate Mojo calls to the
  // DiagnosticsdClient mojo service (which in production is implemented by the
  // browser and called by the diagnosticsd daemon).
  //
  // Returns null if EstablishFakeMojoConnection() wasn't called yet.
  diagnosticsd::mojom::DiagnosticsdClient* mojo_diagnosticsd_client() {
    return mojo_diagnosticsd_client_.get();
  }

 private:
  // |mojo_diagnosticsd_service| is an unowned pointer that should be a stub
  // implementation of the DiagnosticsdService Mojo service (which in production
  // is implemented by the diagnosticsd daemon).
  TestingDiagnosticsdBridgeWrapper(
      diagnosticsd::mojom::DiagnosticsdService* mojo_diagnosticsd_service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::unique_ptr<DiagnosticsdBridge>* bridge);

  // Implements the GetService Mojo method of the DiagnosticsdServiceFactory
  // interface. Called during the simulated Mojo boostrapping.
  void HandleMojoGetService(
      diagnosticsd::mojom::DiagnosticsdServiceRequest
          mojo_diagnosticsd_service_request,
      diagnosticsd::mojom::DiagnosticsdClientPtr mojo_diagnosticsd_client);

  // Mojo binding that binds the DiagnosticsdService implementation (passed to
  // the constructor) with the other endpoint owned from |bridge_|.
  mojo::Binding<diagnosticsd::mojom::DiagnosticsdService>
      mojo_diagnosticsd_service_binding_;

  // Mojo pointer that points to the DiagnosticsdClient implementation (owned by
  // |bridge_|).  Is initialized if the Mojo is bootstrapped by
  // EstablishFakeMojoConnection().
  diagnosticsd::mojom::DiagnosticsdClientPtr mojo_diagnosticsd_client_;

  // Temporary callback that allows to deliver the DiagnosticsdServiceRequest
  // value during the Mojo bootstrapping simulation by
  // EstablishFakeMojoConnection().
  base::OnceCallback<void(diagnosticsd::mojom::DiagnosticsdServiceRequest
                              mojo_diagnosticsd_service_request)>
      mojo_get_service_handler_;

  DISALLOW_COPY_AND_ASSIGN(TestingDiagnosticsdBridgeWrapper);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DIAGNOSTICSD_TESTING_DIAGNOSTICSD_BRIDGE_WRAPPER_H_
