// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_GCAPI_GCAPI_OMAHA_EXPERIMENT_H_
#define CHROME_INSTALLER_GCAPI_GCAPI_OMAHA_EXPERIMENT_H_

// Writes a reactivation brand code experiment label in the Chrome product and
// binaries registry keys for |brand_code|. This experiment label will have a
// expiration date of now plus one year. If |shell_mode| is set to
// GCAPI_INVOKED_UAC_ELEVATION, the value will be written to HKLM, otherwise
// HKCU.
bool SetReactivationExperimentLabels(const wchar_t* brand_code, int shell_mode);

#endif  // CHROME_INSTALLER_GCAPI_GCAPI_OMAHA_EXPERIMENT_H_
