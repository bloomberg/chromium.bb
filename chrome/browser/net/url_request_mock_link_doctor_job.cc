// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/url_request_mock_link_doctor_job.h"

#include "base/path_service.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/common/chrome_paths.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_request_filter.h"

namespace {

FilePath GetMockFilePath() {
  FilePath test_dir;
  bool success = PathService::Get(chrome::DIR_TEST_DATA, &test_dir);
  DCHECK(success);
  return test_dir.AppendASCII("mock-link-doctor.html");
}

}  // namespace

// static
net::URLRequestJob* URLRequestMockLinkDoctorJob::Factory(
    net::URLRequest* request,
    const std::string& scheme) {
  return new URLRequestMockLinkDoctorJob(request);
}

// static
void URLRequestMockLinkDoctorJob::AddUrlHandler() {
  net::URLRequestFilter* filter = net::URLRequestFilter::GetInstance();
  filter->AddHostnameHandler("http",
                             GURL(google_util::kLinkDoctorBaseURL).host(),
                             URLRequestMockLinkDoctorJob::Factory);
}

URLRequestMockLinkDoctorJob::URLRequestMockLinkDoctorJob(
    net::URLRequest* request)
    : URLRequestMockHTTPJob(request, GetMockFilePath()) {
}
