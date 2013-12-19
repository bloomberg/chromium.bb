// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/component_updater_utils.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/guid.h"
#include "base/strings/stringprintf.h"
#include "base/sys_info.h"
#include "base/win/windows_version.h"
#include "chrome/browser/component_updater/crx_update_item.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/omaha_query_params/omaha_query_params.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"

namespace component_updater {

std::string BuildProtocolRequest(const std::string& request_body,
                                 const std::string& additional_attributes) {
  const std::string prod_id(chrome::OmahaQueryParams::GetProdIdString(
      chrome::OmahaQueryParams::CHROME));
  const chrome::VersionInfo chrome_version_info;
  const std::string chrome_version(chrome_version_info.Version());

  std::string request(
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
      "<request protocol=\"3.0\" ");

  if (!additional_attributes.empty())
     base::StringAppendF(&request, "%s ", additional_attributes.c_str());

  // Chrome version and platform information.
  base::StringAppendF(
      &request,
      "version=\"%s-%s\" prodversion=\"%s\" "
      "requestid=\"{%s}\" updaterchannel=\"%s\" arch=\"%s\" nacl_arch=\"%s\"",
      prod_id.c_str(), chrome_version.c_str(),        // "version"
      chrome_version.c_str(),                         // "prodversion"
      base::GenerateGUID().c_str(),                   // "requestid"
      chrome::OmahaQueryParams::GetChannelString(),   // "updaterchannel"
      chrome::OmahaQueryParams::getArch(),            // "arch"
      chrome::OmahaQueryParams::getNaclArch());       // "nacl_arch"
#if defined(OS_WIN)
    const bool is_wow64(
        base::win::OSInfo::GetInstance()->wow64_status() ==
        base::win::OSInfo::WOW64_ENABLED);
    if (is_wow64)
      base::StringAppendF(&request, " wow64=\"1\"");
#endif
    base::StringAppendF(&request, ">");

  // OS version and platform information.
  base::StringAppendF(
      &request,
      "<os platform=\"%s\" version=\"%s\" arch=\"%s\"/>",
      chrome::VersionInfo().OSType().c_str(),                  // "platform"
      base::SysInfo().OperatingSystemVersion().c_str(),        // "version"
      base::SysInfo().OperatingSystemArchitecture().c_str());  // "arch"

  // The actual payload of the request.
  base::StringAppendF(&request, "%s</request>", request_body.c_str());

  return request;
}

net::URLFetcher* SendProtocolRequest(
    const GURL& url,
    const std::string& protocol_request,
    net::URLFetcherDelegate* url_fetcher_delegate,
    net::URLRequestContextGetter* url_request_context_getter) {
  net::URLFetcher* url_fetcher(
      net::URLFetcher::Create(0,
                              url,
                              net::URLFetcher::POST,
                              url_fetcher_delegate));

  url_fetcher->SetUploadData("application/xml", protocol_request);
  url_fetcher->SetRequestContext(url_request_context_getter);
  url_fetcher->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
                            net::LOAD_DO_NOT_SAVE_COOKIES |
                            net::LOAD_DISABLE_CACHE);
  url_fetcher->SetAutomaticallyRetryOn5xx(false);
  url_fetcher->Start();

  return url_fetcher;
}

bool FetchSuccess(const net::URLFetcher& fetcher) {
  return GetFetchError(fetcher) == 0;
}

int GetFetchError(const net::URLFetcher& fetcher) {
  const net::URLRequestStatus::Status status(fetcher.GetStatus().status());
  switch (status) {
    case net::URLRequestStatus::IO_PENDING:
    case net::URLRequestStatus::CANCELED:
      // Network status is a small positive number.
      return status;

    case net::URLRequestStatus::SUCCESS: {
      // Response codes are positive numbers, greater than 100.
      const int response_code(fetcher.GetResponseCode());
      if (response_code == 200)
        return 0;
      else
        return response_code ? response_code : -1;
    }

    case net::URLRequestStatus::FAILED: {
      // Network errors are small negative numbers.
      const int error = fetcher.GetStatus().error();
      return error ? error : -1;
    }

    default:
      return -1;
  }
}

bool HasDiffUpdate(const CrxUpdateItem* update_item) {
  return !update_item->crx_diffurls.empty();
}

bool IsHttpServerError(int status_code) {
  return 500 <= status_code && status_code < 600;
}

bool DeleteFileAndEmptyParentDirectory(const base::FilePath& filepath) {
  if (!base::DeleteFile(filepath, false))
    return false;

  const base::FilePath dirname(filepath.DirName());
  if (!base::IsDirectoryEmpty(dirname))
    return true;

  return base::DeleteFile(dirname, false);
}

}  // namespace component_updater

