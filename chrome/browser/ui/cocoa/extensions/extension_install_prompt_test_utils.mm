// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/extensions/extension_install_prompt_test_utils.h"

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/json/json_file_value_serializer.h"
#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension.h"

using extensions::Extension;

namespace chrome {

void MockExtensionInstallPromptDelegate::InstallUIProceed() {
  ++proceed_count_;
}

void MockExtensionInstallPromptDelegate::InstallUIAbort(bool user_initiated) {
  ++abort_count_;
}

scoped_refptr<Extension> LoadInstallPromptExtension() {
  scoped_refptr<Extension> extension;

  base::FilePath path;
  PathService::Get(chrome::DIR_TEST_DATA, &path);
  path = path.AppendASCII("extensions")
             .AppendASCII("install_prompt")
             .AppendASCII("extension.json");

  std::string error;
  JSONFileValueSerializer serializer(path);
  scoped_ptr<DictionaryValue> value(static_cast<DictionaryValue*>(
      serializer.Deserialize(NULL, &error)));
  if (!value.get()) {
    LOG(ERROR) << error;
    return extension;
  }

  extension = Extension::Create(
      path.DirName(), extensions::Manifest::INVALID_LOCATION, *value,
      Extension::NO_FLAGS, &error);
  if (!extension.get())
    LOG(ERROR) << error;

  return extension;
}

gfx::Image LoadInstallPromptIcon() {
  base::FilePath path;
  PathService::Get(chrome::DIR_TEST_DATA, &path);
  path = path.AppendASCII("extensions")
             .AppendASCII("install_prompt")
             .AppendASCII("icon.png");

  std::string file_contents;
  file_util::ReadFileToString(path, &file_contents);

  return gfx::Image::CreateFrom1xPNGBytes(
      reinterpret_cast<const unsigned char*>(file_contents.c_str()),
      file_contents.length());
}

ExtensionInstallPrompt::Prompt BuildExtensionInstallPrompt(
    Extension* extension) {
  ExtensionInstallPrompt::Prompt prompt(ExtensionInstallPrompt::INSTALL_PROMPT);
  prompt.set_extension(extension);
  prompt.set_icon(LoadInstallPromptIcon());
  return prompt;
}

ExtensionInstallPrompt::Prompt BuildExtensionPostInstallPermissionsPrompt(
    Extension* extension) {
  ExtensionInstallPrompt::Prompt prompt(
      ExtensionInstallPrompt::POST_INSTALL_PERMISSIONS_PROMPT);
  prompt.set_extension(extension);
  prompt.set_icon(LoadInstallPromptIcon());
  return prompt;
}

}  // namespace chrome
