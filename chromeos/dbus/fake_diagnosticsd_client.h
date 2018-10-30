// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_DIAGNOSTICSD_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_DIAGNOSTICSD_CLIENT_H_

#include "base/macros.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/diagnosticsd_client.h"

namespace chromeos {

class CHROMEOS_EXPORT FakeDiagnosticsdClient final : public DiagnosticsdClient {
 public:
  FakeDiagnosticsdClient();
  ~FakeDiagnosticsdClient() override;

  // DBusClient overrides:
  void Init(dbus::Bus* bus) override;

  // DiagnosticsdClient overrides:
  void WaitForServiceToBeAvailable(
      dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback) override;
  void BootstrapMojoConnection(base::ScopedFD fd,
                               VoidDBusMethodCallback callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeDiagnosticsdClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_DIAGNOSTICSD_CLIENT_H_
