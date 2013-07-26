// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/core_app_launcher_handler.h"

#include "base/bind.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ntp/app_launcher_handler.h"
#include "content/public/browser/web_ui.h"
#include "net/base/escape.h"

namespace {
const net::UnescapeRule::Type kUnescapeRules =
    net::UnescapeRule::NORMAL | net::UnescapeRule::URL_SPECIAL_CHARS;
}

CoreAppLauncherHandler::CoreAppLauncherHandler() {}

CoreAppLauncherHandler::~CoreAppLauncherHandler() {}

// static
void CoreAppLauncherHandler::RecordAppLaunchType(
    extension_misc::AppLaunchBucket bucket,
    extensions::Manifest::Type app_type) {
  DCHECK_LT(bucket, extension_misc::APP_LAUNCH_BUCKET_BOUNDARY);
  if (app_type == extensions::Manifest::TYPE_PLATFORM_APP) {
    UMA_HISTOGRAM_ENUMERATION(extension_misc::kPlatformAppLaunchHistogram,
                              bucket,
                              extension_misc::APP_LAUNCH_BUCKET_BOUNDARY);
  } else {
    UMA_HISTOGRAM_ENUMERATION(extension_misc::kAppLaunchHistogram,
                              bucket,
                              extension_misc::APP_LAUNCH_BUCKET_BOUNDARY);
  }
}

void CoreAppLauncherHandler::HandleRecordAppLaunchByUrl(
    const base::ListValue* args) {
  std::string url;
  CHECK(args->GetString(0, &url));
  double source;
  CHECK(args->GetDouble(1, &source));

  extension_misc::AppLaunchBucket bucket =
      static_cast<extension_misc::AppLaunchBucket>(static_cast<int>(source));
  CHECK(source < extension_misc::APP_LAUNCH_BUCKET_BOUNDARY);

  RecordAppLaunchByUrl(Profile::FromWebUI(web_ui()), url, bucket);
}

void CoreAppLauncherHandler::RecordAppLaunchByUrl(
    Profile* profile,
    std::string escaped_url,
    extension_misc::AppLaunchBucket bucket) {
  CHECK(bucket != extension_misc::APP_LAUNCH_BUCKET_INVALID);

  GURL url(net::UnescapeURLComponent(escaped_url, kUnescapeRules));
  DCHECK(profile->GetExtensionService());
  if (!profile->GetExtensionService()->IsInstalledApp(url))
    return;

  RecordAppLaunchType(bucket, extensions::Manifest::TYPE_HOSTED_APP);
}

void CoreAppLauncherHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("recordAppLaunchByURL",
      base::Bind(&CoreAppLauncherHandler::HandleRecordAppLaunchByUrl,
                 base::Unretained(this)));
}
