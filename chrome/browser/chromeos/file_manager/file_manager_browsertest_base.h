// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_MANAGER_FILE_MANAGER_BROWSERTEST_BASE_H_
#define CHROME_BROWSER_CHROMEOS_FILE_MANAGER_FILE_MANAGER_BROWSERTEST_BASE_H_

#include <map>
#include <string>

#include "base/values.h"
#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"

// Slow tests are disabled on debug build. http://crbug.com/327719
// Disabled under MSAN, ASAN, and LSAN as well. http://crbug.com/468980.
#if !defined(NDEBUG) || defined(MEMORY_SANITIZER) || \
    defined(ADDRESS_SANITIZER) || defined(LEAK_SANITIZER)
#define DISABLE_SLOW_FILESAPP_TESTS
#endif

namespace file_manager {

enum GuestMode { NOT_IN_GUEST_MODE, IN_GUEST_MODE, IN_INCOGNITO };

class LocalTestVolume;
class DriveTestVolume;
class FakeTestVolume;

// The base test class.
class FileManagerBrowserTestBase : public ExtensionApiTest {
 protected:
  FileManagerBrowserTestBase();
  ~FileManagerBrowserTestBase() override;

  void SetUp() override;

  void SetUpInProcessBrowserTestFixture() override;

  void SetUpOnMainThread() override;

  // Adds an incognito and guest-mode flags for tests in the guest mode.
  void SetUpCommandLine(base::CommandLine* command_line) override;

  // Installs an extension at the specified |path| using the |manifest_name|
  // manifest.
  void InstallExtension(const base::FilePath& path, const char* manifest_name);
  // Loads our testing extension and sends it a string identifying the current
  // test.
  virtual void StartTest();
  void RunTestMessageLoop();

  // Overriding point for test configurations.
  virtual const char* GetTestManifestName() const = 0;
  virtual GuestMode GetGuestModeParam() const = 0;
  virtual const char* GetTestCaseNameParam() const = 0;
  virtual void OnMessage(const std::string& name,
                         const base::DictionaryValue& value,
                         std::string* output);

  scoped_ptr<LocalTestVolume> local_volume_;
  linked_ptr<DriveTestVolume> drive_volume_;
  std::map<Profile*, linked_ptr<DriveTestVolume>> drive_volumes_;
  scoped_ptr<FakeTestVolume> usb_volume_;
  scoped_ptr<FakeTestVolume> mtp_volume_;

 private:
  drive::DriveIntegrationService* CreateDriveIntegrationService(
      Profile* profile);
  drive::DriveIntegrationServiceFactory::FactoryCallback
      create_drive_integration_service_;
  scoped_ptr<drive::DriveIntegrationServiceFactory::ScopedFactoryForTest>
      service_factory_for_test_;
};

}  // namespace file_manager

#endif  // CHROME_BROWSER_CHROMEOS_FILE_MANAGER_FILE_MANAGER_BROWSERTEST_BASE_H_
