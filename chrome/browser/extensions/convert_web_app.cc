// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/convert_web_app.h"

#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include "base/base64.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/memory/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "crypto/sha2.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_file_util.h"
#include "chrome/common/web_apps.h"
#include "content/common/json_value_serializer.h"
#include "googleurl/src/gurl.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"

namespace keys = extension_manifest_keys;

using base::Time;

namespace {

const char kIconsDirName[] = "icons";

// Create the public key for the converted web app.
//
// Web apps are not signed, but the public key for an extension doubles as
// its unique identity, and we need one of those. A web app's unique identity
// is its manifest URL, so we hash that to create a public key. There will be
// no corresponding private key, which means that these extensions cannot be
// auto-updated using ExtensionUpdater. But Chrome does notice updates to the
// manifest and regenerates these extensions.
std::string GenerateKey(const GURL& manifest_url) {
  char raw[crypto::SHA256_LENGTH] = {0};
  std::string key;
  crypto::SHA256HashString(manifest_url.spec().c_str(),
                           raw,
                           crypto::SHA256_LENGTH);
  base::Base64Encode(std::string(raw, crypto::SHA256_LENGTH), &key);
  return key;
}

}


// Generates a version for the converted app using the current date. This isn't
// really needed, but it seems like useful information.
std::string ConvertTimeToExtensionVersion(const Time& create_time) {
  Time::Exploded create_time_exploded;
  create_time.UTCExplode(&create_time_exploded);

  double micros = static_cast<double>(
      (create_time_exploded.millisecond * Time::kMicrosecondsPerMillisecond) +
      (create_time_exploded.second * Time::kMicrosecondsPerSecond) +
      (create_time_exploded.minute * Time::kMicrosecondsPerMinute) +
      (create_time_exploded.hour * Time::kMicrosecondsPerHour));
  double day_fraction = micros / Time::kMicrosecondsPerDay;
  double stamp = day_fraction * std::numeric_limits<uint16>::max();

  // Ghetto-round, since VC++ doesn't have round().
  stamp = stamp >= (floor(stamp) + 0.5) ? (stamp + 1) : stamp;

  return base::StringPrintf("%i.%i.%i.%i",
                            create_time_exploded.year,
                            create_time_exploded.month,
                            create_time_exploded.day_of_month,
                            static_cast<uint16>(stamp));
}

scoped_refptr<Extension> ConvertWebAppToExtension(
    const WebApplicationInfo& web_app,
    const Time& create_time) {
  FilePath user_data_temp_dir = extension_file_util::GetUserDataTempDir();
  if (user_data_temp_dir.empty()) {
    LOG(ERROR) << "Could not get path to profile temporary directory.";
    return NULL;
  }

  ScopedTempDir temp_dir;
  if (!temp_dir.CreateUniqueTempDirUnderPath(user_data_temp_dir)) {
    LOG(ERROR) << "Could not create temporary directory.";
    return NULL;
  }

  // Create the manifest
  scoped_ptr<DictionaryValue> root(new DictionaryValue);
  root->SetString(keys::kPublicKey, GenerateKey(web_app.manifest_url));
  root->SetString(keys::kName, UTF16ToUTF8(web_app.title));
  root->SetString(keys::kVersion, ConvertTimeToExtensionVersion(create_time));
  root->SetString(keys::kDescription, UTF16ToUTF8(web_app.description));
  root->SetString(keys::kLaunchWebURL, web_app.app_url.spec());

  if (!web_app.launch_container.empty())
    root->SetString(keys::kLaunchContainer, web_app.launch_container);

  // Add the icons.
  DictionaryValue* icons = new DictionaryValue();
  root->Set(keys::kIcons, icons);
  for (size_t i = 0; i < web_app.icons.size(); ++i) {
    std::string size = StringPrintf("%i", web_app.icons[i].width);
    std::string icon_path = StringPrintf("%s/%s.png", kIconsDirName,
                                         size.c_str());
    icons->SetString(size, icon_path);
  }

  // Add the permissions.
  ListValue* permissions = new ListValue();
  root->Set(keys::kPermissions, permissions);
  for (size_t i = 0; i < web_app.permissions.size(); ++i) {
    permissions->Append(Value::CreateStringValue(web_app.permissions[i]));
  }

  // Add the URLs.
  ListValue* urls = new ListValue();
  root->Set(keys::kWebURLs, urls);
  for (size_t i = 0; i < web_app.urls.size(); ++i) {
    urls->Append(Value::CreateStringValue(web_app.urls[i].spec()));
  }

  // Write the manifest.
  FilePath manifest_path = temp_dir.path().Append(
      Extension::kManifestFilename);
  JSONFileValueSerializer serializer(manifest_path);
  if (!serializer.Serialize(*root)) {
    LOG(ERROR) << "Could not serialize manifest.";
    return NULL;
  }

  // Write the icon files.
  FilePath icons_dir = temp_dir.path().AppendASCII(kIconsDirName);
  if (!file_util::CreateDirectory(icons_dir)) {
    LOG(ERROR) << "Could not create icons directory.";
    return NULL;
  }
  for (size_t i = 0; i < web_app.icons.size(); ++i) {
    FilePath icon_file = icons_dir.AppendASCII(
        StringPrintf("%i.png", web_app.icons[i].width));
    std::vector<unsigned char> image_data;
    if (!gfx::PNGCodec::EncodeBGRASkBitmap(web_app.icons[i].data,
                                           false,
                                           &image_data)) {
      LOG(ERROR) << "Could not create icon file.";
      return NULL;
    }

    const char* image_data_ptr = reinterpret_cast<const char*>(&image_data[0]);
    if (!file_util::WriteFile(icon_file, image_data_ptr, image_data.size())) {
      LOG(ERROR) << "Could not write icon file.";
      return NULL;
    }
  }

  // Finally, create the extension object to represent the unpacked directory.
  std::string error;
  scoped_refptr<Extension> extension = Extension::Create(
      temp_dir.path(),
      Extension::INTERNAL,
      *root,
      Extension::STRICT_ERROR_CHECKS,
      &error);
  if (!extension) {
    LOG(ERROR) << error;
    return NULL;
  }

  temp_dir.Take();  // The caller takes ownership of the directory.
  return extension;
}
