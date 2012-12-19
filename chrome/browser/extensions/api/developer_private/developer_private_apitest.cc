// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>

#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/chrome_switches.h"

using extensions::Extension;
using extensions::Manifest;

class DeveloperPrivateApiTest : public ExtensionApiTest {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kAppsDebugger);
  }

  virtual void LoadExtensions() {
    FilePath base_dir = test_data_dir_.AppendASCII("developer");
    LoadNamedExtension(base_dir, "hosted_app");
  }

 protected:
  void LoadNamedExtension(const FilePath& path,
                          const std::string& name) {
    const Extension* extension = LoadExtension(path.AppendASCII(name));
    ASSERT_TRUE(extension);
    extension_name_to_ids_[name] = extension->id();
  }

  void InstallNamedExtension(const FilePath& path,
                             const std::string& name,
                             Extension::Location install_source) {
    const Extension* extension = InstallExtension(path.AppendASCII(name), 1,
                                                  install_source);
    ASSERT_TRUE(extension);
    extension_name_to_ids_[name] = extension->id();
  }

  std::map<std::string, std::string> extension_name_to_ids_;
};

IN_PROC_BROWSER_TEST_F(DeveloperPrivateApiTest, Basics) {
  LoadExtensions();

  FilePath basedir = test_data_dir_.AppendASCII("developer");
  InstallNamedExtension(basedir, "packaged_app", Extension::INTERNAL);

  InstallNamedExtension(basedir, "simple_extension", Extension::INTERNAL);

  ASSERT_TRUE(RunExtensionSubtest(
      "developer/test", "basics.html", kFlagLoadAsComponent));
}
