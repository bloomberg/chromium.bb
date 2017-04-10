// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/convert_web_app.h"

#include <stddef.h>
#include <stdint.h>

#include <cmath>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_file_value_serializer.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/web_application_info.h"
#include "crypto/sha2.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/file_util.h"
#include "extensions/common/image_util.h"
#include "extensions/common/manifest_constants.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "url/gurl.h"

namespace extensions {

namespace keys = manifest_keys;

using base::Time;

namespace {

const char kIconsDirName[] = "icons";

// Create the public key for the converted web app.
//
// Web apps are not signed, but the public key for an extension doubles as
// its unique identity, and we need one of those. A web app's unique identity
// is its manifest URL, so we hash that to create a public key. There will be
// no corresponding private key, which means that these extensions cannot be
// auto-updated using ExtensionUpdater.
std::string GenerateKey(const GURL& app_url) {
  char raw[crypto::kSHA256Length] = {0};
  std::string key;
  crypto::SHA256HashString(app_url.spec().c_str(), raw,
                           crypto::kSHA256Length);
  base::Base64Encode(base::StringPiece(raw, crypto::kSHA256Length), &key);
  return key;
}

}  // namespace

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
  double stamp = day_fraction * std::numeric_limits<uint16_t>::max();

  // Ghetto-round, since VC++ doesn't have round().
  stamp = stamp >= (floor(stamp) + 0.5) ? (stamp + 1) : stamp;

  return base::StringPrintf(
      "%i.%i.%i.%i", create_time_exploded.year, create_time_exploded.month,
      create_time_exploded.day_of_month, static_cast<uint16_t>(stamp));
}

scoped_refptr<Extension> ConvertWebAppToExtension(
    const WebApplicationInfo& web_app,
    const Time& create_time,
    const base::FilePath& extensions_dir) {
  base::FilePath install_temp_dir =
      file_util::GetInstallTempDir(extensions_dir);
  if (install_temp_dir.empty()) {
    LOG(ERROR) << "Could not get path to profile temporary directory.";
    return NULL;
  }

  base::ScopedTempDir temp_dir;
  if (!temp_dir.CreateUniqueTempDirUnderPath(install_temp_dir)) {
    LOG(ERROR) << "Could not create temporary directory.";
    return NULL;
  }

  // Create the manifest
  std::unique_ptr<base::DictionaryValue> root(new base::DictionaryValue);
  root->SetString(keys::kPublicKey, GenerateKey(web_app.app_url));
  root->SetString(keys::kName, base::UTF16ToUTF8(web_app.title));
  root->SetString(keys::kVersion, ConvertTimeToExtensionVersion(create_time));
  root->SetString(keys::kDescription, base::UTF16ToUTF8(web_app.description));
  root->SetString(keys::kLaunchWebURL, web_app.app_url.spec());
  if (web_app.generated_icon_color != SK_ColorTRANSPARENT) {
    root->SetString(keys::kAppIconColor, image_util::GenerateHexColorString(
                                             web_app.generated_icon_color));
  }

  // Add the icons and linked icon information.
  base::DictionaryValue* icons = new base::DictionaryValue();
  root->Set(keys::kIcons, icons);
  base::ListValue* linked_icons = new base::ListValue();
  root->Set(keys::kLinkedAppIcons, linked_icons);
  for (const auto& icon : web_app.icons) {
    std::string size = base::StringPrintf("%i", icon.width);
    std::string icon_path = base::StringPrintf("%s/%s.png", kIconsDirName,
                                               size.c_str());
    icons->SetString(size, icon_path);

    if (icon.url.is_valid()) {
      std::unique_ptr<base::DictionaryValue> linked_icon(
          new base::DictionaryValue());
      linked_icon->SetString(keys::kLinkedAppIconURL, icon.url.spec());
      linked_icon->SetInteger(keys::kLinkedAppIconSize, icon.width);
      linked_icons->Append(std::move(linked_icon));
    }
  }

  // Write the manifest.
  base::FilePath manifest_path = temp_dir.GetPath().Append(kManifestFilename);
  JSONFileValueSerializer serializer(manifest_path);
  if (!serializer.Serialize(*root)) {
    LOG(ERROR) << "Could not serialize manifest.";
    return NULL;
  }

  // Write the icon files.
  base::FilePath icons_dir = temp_dir.GetPath().AppendASCII(kIconsDirName);
  if (!base::CreateDirectory(icons_dir)) {
    LOG(ERROR) << "Could not create icons directory.";
    return NULL;
  }
  for (size_t i = 0; i < web_app.icons.size(); ++i) {
    // Skip unfetched bitmaps.
    if (web_app.icons[i].data.colorType() == kUnknown_SkColorType)
      continue;

    base::FilePath icon_file = icons_dir.AppendASCII(
        base::StringPrintf("%i.png", web_app.icons[i].width));
    std::vector<unsigned char> image_data;
    if (!gfx::PNGCodec::EncodeBGRASkBitmap(web_app.icons[i].data,
                                           false,
                                           &image_data)) {
      LOG(ERROR) << "Could not create icon file.";
      return NULL;
    }

    const char* image_data_ptr = reinterpret_cast<const char*>(&image_data[0]);
    int size = base::checked_cast<int>(image_data.size());
    if (base::WriteFile(icon_file, image_data_ptr, size) != size) {
      LOG(ERROR) << "Could not write icon file.";
      return NULL;
    }
  }

  // Finally, create the extension object to represent the unpacked directory.
  std::string error;
  scoped_refptr<Extension> extension =
      Extension::Create(temp_dir.GetPath(), Manifest::INTERNAL, *root,
                        Extension::FROM_BOOKMARK, &error);
  if (!extension.get()) {
    LOG(ERROR) << error;
    return NULL;
  }

  temp_dir.Take();  // The caller takes ownership of the directory.
  return extension;
}

}  // namespace extensions
