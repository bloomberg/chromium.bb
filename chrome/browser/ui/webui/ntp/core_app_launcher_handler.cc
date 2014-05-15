// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/core_app_launcher_handler.h"

#include "base/bind.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ntp/app_launcher_handler.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "content/public/browser/web_ui.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
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

// static
void CoreAppLauncherHandler::RecordAppListSearchLaunch(
    const extensions::Extension* extension) {
  extension_misc::AppLaunchBucket bucket =
      extension_misc::APP_LAUNCH_APP_LIST_SEARCH;
  if (extension->id() == extension_misc::kWebStoreAppId)
    bucket = extension_misc::APP_LAUNCH_APP_LIST_SEARCH_WEBSTORE;
  else if (extension->id() == extension_misc::kChromeAppId)
    bucket = extension_misc::APP_LAUNCH_APP_LIST_SEARCH_CHROME;
  RecordAppLaunchType(bucket, extension->GetType());
}

// static
void CoreAppLauncherHandler::RecordAppListMainLaunch(
    const extensions::Extension* extension) {
  extension_misc::AppLaunchBucket bucket =
      extension_misc::APP_LAUNCH_APP_LIST_MAIN;
  if (extension->id() == extension_misc::kWebStoreAppId)
    bucket = extension_misc::APP_LAUNCH_APP_LIST_MAIN_WEBSTORE;
  else if (extension->id() == extension_misc::kChromeAppId)
    bucket = extension_misc::APP_LAUNCH_APP_LIST_MAIN_CHROME;
  RecordAppLaunchType(bucket, extension->GetType());
}

// static
void CoreAppLauncherHandler::RecordWebStoreLaunch() {
  RecordAppLaunchType(extension_misc::APP_LAUNCH_NTP_WEBSTORE,
                      extensions::Manifest::TYPE_HOSTED_APP);
}

// static
void CoreAppLauncherHandler::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(prefs::kNtpAppPageNames,
                             user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
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
  if (!extensions::ExtensionRegistry::Get(profile)
          ->enabled_extensions().GetAppByURL(url)) {
    return;
  }

  RecordAppLaunchType(bucket, extensions::Manifest::TYPE_HOSTED_APP);
}

void CoreAppLauncherHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("recordAppLaunchByURL",
      base::Bind(&CoreAppLauncherHandler::HandleRecordAppLaunchByUrl,
                 base::Unretained(this)));
}
