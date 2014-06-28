// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_RAPPOR_SAMPLING_H_
#define CHROME_BROWSER_METRICS_RAPPOR_SAMPLING_H_

#include <string>

class GURL;

namespace rappor {

// Extract the domain and registry for a sample from a GURL.
// For file:// urls this will just return "file://" and for other special
// schemes like chrome-extension will return the scheme and host.
std::string GetDomainAndRegistrySampleFromGURL(const GURL& gurl);

// Records the domain and registry of a url to a Rappor metric.
// If the Rappor service is NULL, such as during tests, this call does nothing.
void SampleDomainAndRegistryFromGURL(const std::string& metric,
                                     const GURL& gurl);

}  // namespace rappor

#endif  // CHROME_BROWSER_METRICS_RAPPOR_SAMPLING_H_
