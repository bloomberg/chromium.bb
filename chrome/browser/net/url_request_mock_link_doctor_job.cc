// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/url_request_mock_link_doctor_job.h"

#include "base/path_service.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/common/chrome_paths.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_request_filter.h"

using content::URLRequestMockHTTPJob;

namespace {

// This is the file path leading to the root of the directory to use as the
// root of the http server. This returns a reference that can be assigned to.
FilePath& BasePath() {
  CR_DEFINE_STATIC_LOCAL(FilePath, base_path, ());
  return base_path;
}

}  // namespace

// static
net::URLRequestJob* URLRequestMockLinkDoctorJob::Factory(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate,
    const std::string& scheme) {
  return new URLRequestMockLinkDoctorJob(request, network_delegate);
}

// static
void URLRequestMockLinkDoctorJob::AddUrlHandler(const FilePath& base_path) {
  BasePath() = base_path;

  net::URLRequestFilter* filter = net::URLRequestFilter::GetInstance();
  filter->AddHostnameHandler("http",
                             google_util::LinkDoctorBaseURL().host(),
                             URLRequestMockLinkDoctorJob::Factory);
}

URLRequestMockLinkDoctorJob::URLRequestMockLinkDoctorJob(
    net::URLRequest* request, net::NetworkDelegate* network_delegate)
    : URLRequestMockHTTPJob(
        request,
        network_delegate,
        BasePath().AppendASCII("mock-link-doctor.html")) {
}
