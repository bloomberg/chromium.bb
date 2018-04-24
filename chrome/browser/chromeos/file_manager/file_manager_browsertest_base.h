// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_MANAGER_FILE_MANAGER_BROWSERTEST_BASE_H_
#define CHROME_BROWSER_CHROMEOS_FILE_MANAGER_FILE_MANAGER_BROWSERTEST_BASE_H_

#include <map>
#include <string>

#include "base/memory/linked_ptr.h"
#include "base/values.h"
#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"

// Slow tests are disabled on debug build. http://crbug.com/327719
// Disabled under MSAN, ASAN, and LSAN as well. http://crbug.com/468980.
// TODO(noel): maybe move this into the browser test files that need it
// now that the FileManagerBrowserTest flake issues have been removed.
#if !defined(NDEBUG) || defined(MEMORY_SANITIZER) || \
    defined(ADDRESS_SANITIZER) || defined(LEAK_SANITIZER)
#define DISABLE_SLOW_FILESAPP_TESTS
#endif

class NotificationDisplayServiceTester;

namespace file_manager {

enum GuestMode { NOT_IN_GUEST_MODE, IN_GUEST_MODE, IN_INCOGNITO };

class DriveTestVolume;
class FakeTestVolume;
class LocalTestVolume;

class FileManagerBrowserTestBase : public ExtensionApiTest {
 protected:
  FileManagerBrowserTestBase();
  ~FileManagerBrowserTestBase() override;

  // ExtensionApiTest overrides.
  void SetUp() override;
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpInProcessBrowserTestFixture() override;
  void SetUpOnMainThread() override;

  // Loads our testing extension and sends it a string identifying the current
  // test.
  virtual void StartTest();

  // Overriding point for test configurations.
  virtual const char* GetTestManifestName() const = 0;
  virtual GuestMode GetGuestModeParam() const = 0;
  virtual const char* GetTestCaseNameParam() const = 0;

 private:
  // Installs an extension at the specified |path| by |manifest_name|.
  void InstallExtension(const base::FilePath& path, const char* manifest_name);
  // TODO(noel): describe this function.
  void RunTestMessageLoop();
  // TODO(noel): describe this function.
  virtual void OnMessage(const std::string& name,
                         const base::DictionaryValue& value,
                         std::string* output);

  std::unique_ptr<LocalTestVolume> local_volume_;
  linked_ptr<DriveTestVolume> drive_volume_;
  std::map<Profile*, linked_ptr<DriveTestVolume>> drive_volumes_;
  std::unique_ptr<FakeTestVolume> usb_volume_;
  std::unique_ptr<FakeTestVolume> mtp_volume_;

  drive::DriveIntegrationService* CreateDriveIntegrationService(
      Profile* profile);
  drive::DriveIntegrationServiceFactory::FactoryCallback
      create_drive_integration_service_;
  std::unique_ptr<drive::DriveIntegrationServiceFactory::ScopedFactoryForTest>
      service_factory_for_test_;

  std::unique_ptr<NotificationDisplayServiceTester> display_service_;
};

}  // namespace file_manager

#endif  // CHROME_BROWSER_CHROMEOS_FILE_MANAGER_FILE_MANAGER_BROWSERTEST_BASE_H_
