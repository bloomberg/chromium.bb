// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cloud_devices/common/cloud_devices_urls.h"

#include "base/command_line.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "components/cloud_devices/common/cloud_devices_switches.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/url_util.h"

namespace cloud_devices {

const char kCloudPrintAuthScope[] =
    "https://www.googleapis.com/auth/cloudprint";

const char kCloudDevicesAuthScope[] =
    "https://www.googleapis.com/auth/clouddevices";

const char kCloudPrintLearnMoreURL[] =
    "https://www.google.com/support/cloudprint";

const char kCloudPrintTestPageURL[] =
    "http://www.google.com/landing/cloudprint/enable.html?print=true";

namespace {

// Url must not be matched by "urls" section of
// cloud_print_app/manifest.json. If it's matched, print driver dialog will
// open sign-in page in separate window.
const char kCloudPrintURL[] = "https://www.google.com/cloudprint";

const char kCloudDevicesUrl[] = "https://www.googleapis.com/clouddevices/v1";

}

// Returns the root service URL for the cloud print service.  The default is to
// point at the Google Cloud Print service.  This can be overridden by the
// command line or by the user preferences.
GURL GetCloudPrintURL() {
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  GURL cloud_print_url(
      command_line->GetSwitchValueASCII(switches::kCloudPrintURL));
  if (cloud_print_url.is_empty())
    cloud_print_url = GURL(kCloudPrintURL);
  return cloud_print_url;
}

GURL GetCloudPrintRelativeURL(const std::string& relative_path) {
  GURL url = GetCloudPrintURL();
  std::string path;
  static const char kURLPathSeparator[] = "/";
  base::TrimString(url.path(), kURLPathSeparator, &path);
  std::string trimmed_path;
  base::TrimString(relative_path, kURLPathSeparator, &trimmed_path);
  path += kURLPathSeparator;
  path += trimmed_path;
  GURL::Replacements replacements;
  replacements.SetPathStr(path);
  return url.ReplaceComponents(replacements);
}

GURL GetCloudPrintSigninURL() {
  GURL url(GaiaUrls::GetInstance()->service_login_url());
  url = net::AppendQueryParameter(url, "service", "cloudprint");
  url = net::AppendQueryParameter(url, "sarp", "1");
  std::string continue_str = GetCloudPrintURL().spec();
  url = net::AppendQueryParameter(url, "continue", continue_str);
  return url;
}

GURL GetCloudPrintAddAccountURL() {
  GURL url(GaiaUrls::GetInstance()->add_account_url());
  url = net::AppendQueryParameter(url, "service", "cloudprint");
  url = net::AppendQueryParameter(url, "sarp", "1");
  std::string continue_str = GetCloudPrintURL().spec();
  url = net::AppendQueryParameter(url, "continue", continue_str);
  return url;
}

GURL GetCloudPrintEnableURL(const std::string& proxy_id) {
  GURL url = GetCloudPrintRelativeURL("enable_chrome_connector/enable.html");
  url = net::AppendQueryParameter(url, "proxy", proxy_id);
  return url;
}

GURL GetCloudPrintEnableWithSigninURL(const std::string& proxy_id) {
  GURL url(GaiaUrls::GetInstance()->service_login_url());
  url = net::AppendQueryParameter(url, "service", "cloudprint");
  url = net::AppendQueryParameter(url, "sarp", "1");
  std::string continue_str = GetCloudPrintEnableURL(proxy_id).spec();
  return net::AppendQueryParameter(url, "continue", continue_str);
}

GURL GetCloudPrintManageDeviceURL(const std::string& device_id) {
  std::string ref = "printers/" + device_id;
  GURL::Replacements replacements;
  replacements.SetRefStr(ref);
  return GetCloudPrintURL().ReplaceComponents(replacements);
}

GURL GetCloudDevicesURL() {
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  GURL cloud_print_url(
      command_line->GetSwitchValueASCII(switches::kCloudDevicesURL));
  if (cloud_print_url.is_empty())
    cloud_print_url = GURL(kCloudDevicesUrl);
  return cloud_print_url;
}

GURL GetCloudDevicesRelativeURL(const std::string& relative_path) {
  GURL url = GetCloudDevicesURL();
  std::string path;
  const char kURLPathSeparator[] = "/";
  base::TrimString(url.path(), kURLPathSeparator, &path);
  std::string trimmed_path;
  base::TrimString(relative_path, kURLPathSeparator, &trimmed_path);
  path += kURLPathSeparator;
  path += trimmed_path;
  GURL::Replacements replacements;
  replacements.SetPathStr(path);
  return url.ReplaceComponents(replacements);
}

}  // namespace cloud_devices
