// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_GAIACP_MDM_UTILS_H_
#define CHROME_CREDENTIAL_PROVIDER_GAIACP_MDM_UTILS_H_

namespace credential_provider {

// Mdm registry value key name.

// The url used to register the machine to MDM. If specified and non-empty
// additional user access restrictions will be applied to users associated
// to GCPW that have invalid token handles.
extern const wchar_t kRegMdmUrl[];

// Determines if multiple users can be added to a system managed by MDM.
extern const wchar_t kRegMdmSupportsMultiUser[];

// Checks whether the |kGlobalMdmUrlRegKey| is set on this machine and points
// to a valid URL. Returns false otherwise.
bool MdmEnrollmentEnabled();

}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_GAIACP_MDM_UTILS_H_
