// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DIAGNOSTICSD_DIAGNOSTICSD_MESSAGING_H_
#define CHROME_BROWSER_CHROMEOS_DIAGNOSTICSD_DIAGNOSTICSD_MESSAGING_H_

#include <memory>

namespace extensions {
class NativeMessageHost;
}  // namespace extensions

namespace chromeos {

extern const char kDiagnosticsdUiMessageHost[];

extern const char kDiagnosticsdUiMessageTooBigExtensionsError[];
extern const char kDiagnosticsdUiExtraMessagesExtensionsError[];

extern const int kDiagnosticsdUiMessageMaxSize;

// Creates an extensions native message host that talks to the
// diagnostics_processor daemon. This should be used when the communication is
// initiated by the extension (i.e., not the daemon).
std::unique_ptr<extensions::NativeMessageHost>
CreateExtensionOwnedDiagnosticsdMessageHost();

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DIAGNOSTICSD_DIAGNOSTICSD_MESSAGING_H_
