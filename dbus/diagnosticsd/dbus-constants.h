// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants for the D-Bus API exposed by the diagnosticsd daemon. Normally the
// consumer of this API is the browser.

#ifndef SYSTEM_API_DBUS_DIAGNOSTICSD_DBUS_CONSTANTS_H_
#define SYSTEM_API_DBUS_DIAGNOSTICSD_DBUS_CONSTANTS_H_

namespace diagnostics {

constexpr char kDiagnosticsdServiceInterface[] =
    "org.chromium.DiagnosticsdInterface";
constexpr char kDiagnosticsdServicePath[] = "/org/chromium/Diagnosticsd";
constexpr char kDiagnosticsdServiceName[] = "org.chromium.Diagnosticsd";

constexpr char kDiagnosticsdBootstrapMojoConnectionMethod[] =
    "BootstrapMojoConnection";

// Token used for the Mojo connection pipe.
constexpr char kDiagnosticsdMojoConnectionChannelToken[] = "diagnosticsd";

}  // namespace diagnostics

#endif  // SYSTEM_API_DBUS_DIAGNOSTICSD_DBUS_CONSTANTS_H_
