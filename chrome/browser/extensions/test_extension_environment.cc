// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/test_extension_environment.h"

#include "base/command_line.h"
#include "base/json/json_writer.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/value_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(USE_AURA)
#include "ui/aura/env.h"
#endif

namespace extensions {

using content::BrowserThread;

TestExtensionEnvironment::TestExtensionEnvironment()
    : profile_(new TestingProfile),
      extension_service_(NULL),
      extension_prefs_(NULL) {
#if defined(USE_AURA)
  aura::Env::CreateInstance(true);
#endif
}

TestExtensionEnvironment::~TestExtensionEnvironment() {
#if defined(USE_AURA)
  aura::Env::DeleteInstance();
#endif
}

TestingProfile* TestExtensionEnvironment::profile() const {
  return profile_.get();
}

TestExtensionSystem* TestExtensionEnvironment::GetExtensionSystem() {
  return static_cast<TestExtensionSystem*>(ExtensionSystem::Get(profile()));
}

ExtensionService* TestExtensionEnvironment::GetExtensionService() {
  if (extension_service_ == NULL) {
    extension_service_ = GetExtensionSystem()->CreateExtensionService(
        CommandLine::ForCurrentProcess(), base::FilePath(), false);
  }
  return extension_service_;
}

ExtensionPrefs* TestExtensionEnvironment::GetExtensionPrefs() {
  if (extension_prefs_ == NULL) {
    extension_prefs_ = GetExtensionSystem()->CreateExtensionPrefs(
        CommandLine::ForCurrentProcess(), base::FilePath());
  }
  return extension_prefs_;
}

const Extension* TestExtensionEnvironment::MakeExtension(
    const base::Value& manifest_extra) {
  scoped_ptr<base::DictionaryValue> manifest = DictionaryBuilder()
      .Set("name", "Extension")
      .Set("version", "1.0")
      .Set("manifest_version", 2)
      .Build();
  const base::DictionaryValue* manifest_extra_dict;
  if (manifest_extra.GetAsDictionary(&manifest_extra_dict)) {
    manifest->MergeDictionary(manifest_extra_dict);
  } else {
    std::string manifest_json;
    base::JSONWriter::Write(&manifest_extra, &manifest_json);
    ADD_FAILURE() << "Expected dictionary; got \"" << manifest_json << "\"";
  }

  scoped_refptr<Extension> result =
      ExtensionBuilder().SetManifest(manifest.Pass()).Build();
  GetExtensionService()->AddExtension(result.get());
  return result.get();
}

scoped_ptr<content::WebContents> TestExtensionEnvironment::MakeTab() const {
  scoped_ptr<content::WebContents> contents(
      content::WebContentsTester::CreateTestWebContents(profile(), NULL));
  // Create a tab id.
  SessionTabHelper::CreateForWebContents(contents.get());
  return contents.Pass();
}

}  // namespace extensions
