// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/component_updater_utils.h"

#include <cmath>

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/guid.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/sys_info.h"
#include "base/win/windows_version.h"
#include "chrome/browser/component_updater/component_updater_configurator.h"
#include "chrome/browser/component_updater/crx_update_item.h"
#include "components/omaha_query_params/omaha_query_params.h"
#include "extensions/common/extension.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"

using omaha_query_params::OmahaQueryParams;

namespace component_updater {

namespace {

// Returns the amount of physical memory in GB, rounded to the nearest GB.
int GetPhysicalMemoryGB() {
  const double kOneGB = 1024 * 1024 * 1024;
  const int64 phys_mem = base::SysInfo::AmountOfPhysicalMemory();
  return static_cast<int>(std::floor(0.5 + phys_mem / kOneGB));
}

}  // namespace

std::string BuildProtocolRequest(const std::string& browser_version,
                                 const std::string& channel,
                                 const std::string& lang,
                                 const std::string& os_long_name,
                                 const std::string& request_body,
                                 const std::string& additional_attributes) {
  const std::string prod_id(
      OmahaQueryParams::GetProdIdString(OmahaQueryParams::CHROME));

  std::string request(
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
      "<request protocol=\"3.0\" ");

  if (!additional_attributes.empty())
    base::StringAppendF(&request, "%s ", additional_attributes.c_str());

  // Chrome version and platform information.
  base::StringAppendF(
      &request,
      "version=\"%s-%s\" prodversion=\"%s\" "
      "requestid=\"{%s}\" lang=\"%s\" updaterchannel=\"%s\" prodchannel=\"%s\" "
      "os=\"%s\" arch=\"%s\" nacl_arch=\"%s\"",
      prod_id.c_str(),
      browser_version.c_str(),           // "version"
      browser_version.c_str(),           // "prodversion"
      base::GenerateGUID().c_str(),      // "requestid"
      lang.c_str(),                      // "lang",
      channel.c_str(),                   // "updaterchannel"
      channel.c_str(),                   // "prodchannel"
      OmahaQueryParams::GetOS(),         // "os"
      OmahaQueryParams::GetArch(),       // "arch"
      OmahaQueryParams::GetNaclArch());  // "nacl_arch"
#if defined(OS_WIN)
  const bool is_wow64(base::win::OSInfo::GetInstance()->wow64_status() ==
                      base::win::OSInfo::WOW64_ENABLED);
  if (is_wow64)
    base::StringAppendF(&request, " wow64=\"1\"");
#endif
  base::StringAppendF(&request, ">");

  // HW platform information.
  base::StringAppendF(&request,
                      "<hw physmemory=\"%d\"/>",
                      GetPhysicalMemoryGB());  // "physmem" in GB.

  // OS version and platform information.
  base::StringAppendF(
      &request,
      "<os platform=\"%s\" version=\"%s\" arch=\"%s\"/>",
      os_long_name.c_str(),                                    // "platform"
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
  net::URLFetcher* url_fetcher(net::URLFetcher::Create(
      0, url, net::URLFetcher::POST, url_fetcher_delegate));

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

// Produces an extension-like friendly id.
std::string HexStringToID(const std::string& hexstr) {
  std::string id;
  for (size_t i = 0; i < hexstr.size(); ++i) {
    int val = 0;
    if (base::HexStringToInt(
            base::StringPiece(hexstr.begin() + i, hexstr.begin() + i + 1),
            &val)) {
      id.append(1, val + 'a');
    } else {
      id.append(1, 'a');
    }
  }
  DCHECK(extensions::Extension::IdIsValid(id));
  return id;
}

std::string GetCrxComponentID(const CrxComponent& component) {
  return HexStringToID(base::StringToLowerASCII(
      base::HexEncode(&component.pk_hash[0], component.pk_hash.size() / 2)));
}

}  // namespace component_updater
