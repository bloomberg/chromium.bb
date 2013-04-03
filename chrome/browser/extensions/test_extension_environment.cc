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
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_builder.h"
#include "chrome/common/extensions/value_builder.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

using content::BrowserThread;

TestExtensionEnvironment::TestExtensionEnvironment()
    : ui_thread_(BrowserThread::UI, &loop_),
      file_thread_(BrowserThread::FILE),
      file_blocking_thread_(BrowserThread::FILE_USER_BLOCKING),
      io_thread_(BrowserThread::IO),
      profile_(new TestingProfile),
      extension_service_(NULL) {
  file_thread_.Start();
  file_blocking_thread_.Start();
  io_thread_.StartIOThread();
}

TestExtensionEnvironment::~TestExtensionEnvironment() {
  profile_.reset();
  // Delete the profile, and then cycle the message loop to clear
  // out delayed deletions.
  base::RunLoop run_loop;
  loop_.PostTask(FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}

TestingProfile* TestExtensionEnvironment::profile() const {
  return profile_.get();
}

ExtensionService* TestExtensionEnvironment::GetExtensionService() {
  if (extension_service_ == NULL) {
    TestExtensionSystem* extension_system =
        static_cast<TestExtensionSystem*>(ExtensionSystem::Get(profile()));
    extension_service_ = extension_system->CreateExtensionService(
        CommandLine::ForCurrentProcess(), base::FilePath(), false);
  }
  return extension_service_;
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
  GetExtensionService()->AddExtension(result);
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
