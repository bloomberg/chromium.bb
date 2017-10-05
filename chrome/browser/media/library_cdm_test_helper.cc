// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/library_cdm_test_helper.h"

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/native_library.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/public/browser/cdm_registry.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/common/cdm_info.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/webplugininfo.h"
#include "media/base/media_switches.h"
#include "media/cdm/cdm_paths.h"

void RegisterExternalClearKey(base::CommandLine* command_line,
                              bool expect_cdm_exists) {
  base::FilePath cdm_path;
  base::PathService::Get(base::DIR_MODULE, &cdm_path);
  cdm_path = cdm_path
                 .Append(media::GetPlatformSpecificDirectory(
                     media::kClearKeyCdmBaseDirectory))
                 .AppendASCII(base::GetNativeLibraryName(
                     media::kClearKeyCdmLibraryName));
  DCHECK_EQ(expect_cdm_exists, base::PathExists(cdm_path))
      << cdm_path.MaybeAsASCII();

  // Append the switch to register the Clear Key CDM path.
  command_line->AppendSwitchNative(switches::kClearKeyCdmPathForTesting,
                                   cdm_path.value());
}

bool IsLibraryCdmRegistered(const std::string& cdm_guid) {
  std::vector<content::CdmInfo> cdm_info_vector =
      content::CdmRegistry::GetInstance()->GetAllRegisteredCdms();
  for (const auto& cdm_info : cdm_info_vector) {
    if (cdm_info.guid == cdm_guid) {
      DVLOG(2) << "CDM registered for " << cdm_guid << " with path "
               << cdm_info.path.value();
      return true;
    }
  }

  return false;
}

base::FilePath GetPepperCdmPath(const std::string& adapter_base_dir,
                                const std::string& adapter_file_name) {
  base::FilePath adapter_path;
  PathService::Get(base::DIR_MODULE, &adapter_path);
  adapter_path = adapter_path.Append(
      media::GetPlatformSpecificDirectory(adapter_base_dir));
  adapter_path = adapter_path.AppendASCII(adapter_file_name);
  return adapter_path;
}

base::FilePath::StringType BuildPepperCdmRegistration(
    const std::string& adapter_base_dir,
    const std::string& adapter_file_name,
    const std::string& display_name,
    const std::string& mime_type,
    bool expect_adapter_exists) {
  base::FilePath adapter_path =
      GetPepperCdmPath(adapter_base_dir, adapter_file_name);
  DCHECK_EQ(expect_adapter_exists, base::PathExists(adapter_path))
      << adapter_path.MaybeAsASCII();

  base::FilePath::StringType pepper_cdm_registration = adapter_path.value();

  std::string string_to_append = "#";
  string_to_append.append(display_name);
  string_to_append.append("#CDM#0.1.0.0;");
  string_to_append.append(mime_type);

#if defined(OS_WIN)
  pepper_cdm_registration.append(base::ASCIIToUTF16(string_to_append));
#else
  pepper_cdm_registration.append(string_to_append);
#endif

  return pepper_cdm_registration;
}

void RegisterPepperCdm(base::CommandLine* command_line,
                       const std::string& adapter_base_dir,
                       const std::string& adapter_file_name,
                       const std::string& display_name,
                       const std::string& mime_type,
                       bool expect_adapter_exists) {
  base::FilePath::StringType pepper_cdm_registration =
      BuildPepperCdmRegistration(adapter_base_dir, adapter_file_name,
                                 display_name, mime_type,
                                 expect_adapter_exists);

  // Append the switch to register the CDM Adapter.
  command_line->AppendSwitchNative(switches::kRegisterPepperPlugins,
                                   pepper_cdm_registration);
}

bool IsPepperCdmRegistered(const std::string& mime_type) {
  std::vector<content::WebPluginInfo> plugins;
  content::PluginService::GetInstance()->GetInternalPlugins(&plugins);
  for (const auto& plugin : plugins) {
    for (const auto& plugin_mime_type : plugin.mime_types) {
      if (plugin_mime_type.mime_type == mime_type)
        return true;
    }
  }
  return false;
}
