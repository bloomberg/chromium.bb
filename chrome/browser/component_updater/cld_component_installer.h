// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_CLD_COMPONENT_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_CLD_COMPONENT_INSTALLER_H_

#include <stdint.h>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "components/component_updater/default_component_installer.h"

namespace test {
class ComponentCldDataHarness;
}  // namespace test

namespace component_updater {

class ComponentUpdateService;

// Visible for testing. No need to instantiate this class directly.
class CldComponentInstallerTraits : public ComponentInstallerTraits {
 public:
  CldComponentInstallerTraits();
  ~CldComponentInstallerTraits() override {}

 private:
  friend class CldComponentInstallerTest;  // For access within SetUp()
  friend class test::ComponentCldDataHarness;  // For browser tests only
  FRIEND_TEST_ALL_PREFIXES(CldComponentInstallerTest, ComponentReady);
  FRIEND_TEST_ALL_PREFIXES(CldComponentInstallerTest, GetBaseDirectory);
  FRIEND_TEST_ALL_PREFIXES(CldComponentInstallerTest, GetHash);
  FRIEND_TEST_ALL_PREFIXES(CldComponentInstallerTest, GetInstalledPath);
  FRIEND_TEST_ALL_PREFIXES(CldComponentInstallerTest, GetName);
  FRIEND_TEST_ALL_PREFIXES(CldComponentInstallerTest, OnCustomInstall);
  FRIEND_TEST_ALL_PREFIXES(CldComponentInstallerTest, SetLatestCldDataFile);
  FRIEND_TEST_ALL_PREFIXES(CldComponentInstallerTest, VerifyInstallation);

  // The following methods override ComponentInstallerTraits.
  bool CanAutoUpdate() const override;
  bool RequiresNetworkEncryption() const override;
  bool OnCustomInstall(const base::DictionaryValue& manifest,
                       const base::FilePath& install_dir) override;
  bool VerifyInstallation(const base::DictionaryValue& manifest,
                          const base::FilePath& install_dir) const override;
  void ComponentReady(const base::Version& version,
                      const base::FilePath& path,
                      std::unique_ptr<base::DictionaryValue> manifest) override;
  base::FilePath GetBaseDirectory() const override;
  void GetHash(std::vector<uint8_t>* hash) const override;
  std::string GetName() const override;
  std::string GetAp() const override;

  static base::FilePath GetInstalledPath(const base::FilePath& base);

  // Sets the path to the CLD data file. Called internally once a valid CLD
  // data file has been observed. The implementation of this method is
  // responsible for configuring the CLD data source.
  // This method is threadsafe.
  static void SetLatestCldDataFile(const base::FilePath& path);

  // Returns the file path that was most recently set in SetLatestCldDataFile.
  static base::FilePath GetLatestCldDataFile();

  DISALLOW_COPY_AND_ASSIGN(CldComponentInstallerTraits);
};

// Call once during startup to make the component update service aware of
// the CLD component. This method does nothing if the configured CLD data source
// is not the "Component" data source.
void RegisterCldComponent(ComponentUpdateService* cus);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_CLD_COMPONENT_INSTALLER_H_
