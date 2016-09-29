// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ntp_tiles/ios_popular_sites_factory.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "components/ntp_tiles/popular_sites.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_paths.h"
#include "ios/chrome/browser/search_engines/template_url_service_factory.h"
#include "ios/web/public/web_thread.h"

namespace {

// Mimics SafeJsonParser API, but parses unsafely for iOS.
class JsonUnsafeParser {
 public:
  using SuccessCallback = base::Callback<void(std::unique_ptr<base::Value>)>;
  using ErrorCallback = base::Callback<void(const std::string&)>;

  // As with SafeJsonParser, runs either success_callback or error_callback on
  // the calling thread, but not before the call returns.
  static void Parse(const std::string& unsafe_json,
                    const SuccessCallback& success_callback,
                    const ErrorCallback& error_callback) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(DoParse, unsafe_json, success_callback, error_callback));
  }

  JsonUnsafeParser() = delete;

 private:
  static void DoParse(const std::string& unsafe_json,
                      const SuccessCallback& success_callback,
                      const ErrorCallback& error_callback) {
    std::string error_msg;
    int error_line, error_column;
    std::unique_ptr<base::Value> value = base::JSONReader::ReadAndReturnError(
        unsafe_json, base::JSON_ALLOW_TRAILING_COMMAS, nullptr, &error_msg,
        &error_line, &error_column);
    if (value) {
      success_callback.Run(std::move(value));
    } else {
      error_callback.Run(base::StringPrintf("%s (%d:%d)", error_msg.c_str(),
                                            error_line, error_column));
    }
  }
};

}  // namespace

std::unique_ptr<ntp_tiles::PopularSites>
IOSPopularSitesFactory::NewForBrowserState(
    ios::ChromeBrowserState* browser_state) {
  base::FilePath popular_sites_path;
  base::PathService::Get(ios::DIR_USER_DATA, &popular_sites_path);
  return base::MakeUnique<ntp_tiles::PopularSites>(
      web::WebThread::GetBlockingPool(), browser_state->GetPrefs(),
      ios::TemplateURLServiceFactory::GetForBrowserState(browser_state),
      GetApplicationContext()->GetVariationsService(),
      browser_state->GetRequestContext(), popular_sites_path,
      base::Bind(JsonUnsafeParser::Parse));
}
