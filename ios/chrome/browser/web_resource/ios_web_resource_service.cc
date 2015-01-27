// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/web_resource/ios_web_resource_service.h"

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/values.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/web/public/web_thread.h"

namespace {

const char kInvalidDataTypeError[] =
    "Data from web resource server is missing or not valid JSON.";

const char kUnexpectedJSONFormatError[] =
    "Data from web resource server does not have expected format.";

// Helper method to run a closure.
void RunClosure(const base::Closure& closure) {
  closure.Run();
}

}  // namespace

IOSWebResourceService::IOSWebResourceService(
    PrefService* prefs,
    const GURL& web_resource_server,
    bool apply_locale_to_url,
    const char* last_update_time_pref_name,
    int start_fetch_delay_ms,
    int cache_update_delay_ms)
    : web_resource::WebResourceService(
          prefs,
          web_resource_server,
          apply_locale_to_url ? GetApplicationContext()->GetApplicationLocale()
                              : std::string(),
          last_update_time_pref_name,
          start_fetch_delay_ms,
          cache_update_delay_ms,
          GetApplicationContext()->GetSystemURLRequestContext(),
          nullptr) {
}

IOSWebResourceService::~IOSWebResourceService() {
}

// static
base::Closure IOSWebResourceService::ParseJSONOnBackgroundThread(
    const std::string& data,
    const SuccessCallback& success_callback,
    const ErrorCallback& error_callback) {
  if (data.empty())
    return base::Bind(error_callback, std::string(kInvalidDataTypeError));

  scoped_ptr<base::Value> value(base::JSONReader::Read(data));
  if (!value.get()) {
    // Page information not properly read, or corrupted.
    return base::Bind(error_callback, std::string(kInvalidDataTypeError));
  }

  if (!value->IsType(base::Value::TYPE_DICTIONARY))
    return base::Bind(error_callback, std::string(kUnexpectedJSONFormatError));

  return base::Bind(success_callback, base::Passed(&value));
}

void IOSWebResourceService::ParseJSON(const std::string& data,
                                      const SuccessCallback& success_callback,
                                      const ErrorCallback& error_callback) {
  base::PostTaskAndReplyWithResult(
      web::WebThread::GetBlockingPool(), FROM_HERE,
      base::Bind(&IOSWebResourceService::ParseJSONOnBackgroundThread, data,
                 success_callback, error_callback),
      base::Bind(&RunClosure));
}
