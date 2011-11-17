// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/cloud_print/cloud_print_helpers.h"

#include "base/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

void CheckURLs(const GURL& server_base_url) {
  GURL url = CloudPrintHelpers::GetUrlForPrinterRegistration(server_base_url);
  std::string expected_url_base = server_base_url.spec();
  if (expected_url_base[expected_url_base.length() - 1] != '/') {
    expected_url_base += "/";
  }
  std::string expected_url = base::StringPrintf("%sregister",
                                                expected_url_base.c_str());
  EXPECT_EQ(expected_url, url.spec());

  url = CloudPrintHelpers::GetUrlForPrinterUpdate(server_base_url,
                                                  "printeridfoo");
  expected_url = base::StringPrintf("%supdate?printerid=printeridfoo",
                                    expected_url_base.c_str());
  EXPECT_EQ(expected_url, url.spec());

  url = CloudPrintHelpers::GetUrlForPrinterDelete(server_base_url,
                                                  "printeridbar");
  expected_url = base::StringPrintf("%sdelete?printerid=printeridbar",
                                    expected_url_base.c_str());
  EXPECT_EQ(expected_url, url.spec());

  url = CloudPrintHelpers::GetUrlForPrinterList(server_base_url, "demoproxy");
  expected_url = base::StringPrintf("%slist?proxy=demoproxy",
                                    expected_url_base.c_str());
  EXPECT_EQ(expected_url, url.spec());

  url = CloudPrintHelpers::GetUrlForJobFetch(server_base_url,
                                             "myprinter",
                                             "nogoodreason");
  expected_url = base::StringPrintf(
      "%sfetch?printerid=myprinter&deb=nogoodreason",
      expected_url_base.c_str());
  EXPECT_EQ(expected_url, url.spec());

  url = CloudPrintHelpers::GetUrlForJobStatusUpdate(
      server_base_url, "12345678", cloud_print::PRINT_JOB_STATUS_IN_PROGRESS);
  expected_url = base::StringPrintf(
      "%scontrol?jobid=12345678&status=in_progress", expected_url_base.c_str());
  EXPECT_EQ(expected_url, url.spec());

  url = CloudPrintHelpers::GetUrlForJobStatusUpdate(
      server_base_url, "12345678", cloud_print::PRINT_JOB_STATUS_ERROR);
  expected_url = base::StringPrintf("%scontrol?jobid=12345678&status=error",
                                    expected_url_base.c_str());
  EXPECT_EQ(expected_url, url.spec());

  url = CloudPrintHelpers::GetUrlForJobStatusUpdate(
      server_base_url, "12345678", cloud_print::PRINT_JOB_STATUS_COMPLETED);
  expected_url = base::StringPrintf("%scontrol?jobid=12345678&status=done",
                                    expected_url_base.c_str());
  EXPECT_EQ(expected_url, url.spec());

  cloud_print::PrintJobDetails details;
  details.status = cloud_print::PRINT_JOB_STATUS_IN_PROGRESS;
  details.platform_status_flags = 2;
  details.status_message = "Out of Paper";
  details.total_pages = 345;
  details.pages_printed = 47;
  url = CloudPrintHelpers::GetUrlForJobStatusUpdate(server_base_url,
                                                    "87654321", details);
  expected_url = base::StringPrintf(
      "%scontrol?jobid=87654321&status=in_progress&code=2"
      "&message=Out%%20of%%20Paper&numpages=345&pagesprinted=47",
      expected_url_base.c_str());
  EXPECT_EQ(expected_url, url.spec());

  url = CloudPrintHelpers::GetUrlForUserMessage(server_base_url,
                                                "blahmessageid");
  expected_url = base::StringPrintf("%smessage?code=blahmessageid",
                                    expected_url_base.c_str());
  EXPECT_EQ(expected_url, url.spec());

  url = CloudPrintHelpers::GetUrlForGetAuthCode(
      server_base_url,
      "fooclientid.apps.googleusercontent.com",
      "test_proxy");
  expected_url = base::StringPrintf(
      "%screaterobot?oauth_client_id=fooclientid.apps.googleusercontent.com&"
      "proxy=test_proxy", expected_url_base.c_str());
  EXPECT_EQ(expected_url, url.spec());
}

}  // namespace

TEST(CloudPrintHelpersTest, URLGetters) {
  CheckURLs(GURL("https://www.google.com/cloudprint"));
  CheckURLs(GURL("https://www.google.com/cloudprint/"));
  CheckURLs(GURL("http://www.myprinterserver.com"));
  CheckURLs(GURL("http://www.myprinterserver.com/"));
}

