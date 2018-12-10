// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_NETWORK_ENROLLMENT_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_ASH_NETWORK_ENROLLMENT_DIALOG_VIEW_H_

#include <string>

namespace chromeos {
namespace enrollment {

// Creates and shows the dialog for certificate-based network enrollment.
bool CreateEnrollmentDialog(const std::string& network_id);

}  // namespace enrollment
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_ASH_NETWORK_ENROLLMENT_DIALOG_VIEW_H_
