// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_STH_SET_COMPONENT_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_STH_SET_COMPONENT_INSTALLER_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "components/component_updater/default_component_installer.h"

namespace base {
class FilePath;
class Value;
}  // namespace base

namespace net {
namespace ct {
class STHObserver;
}  // namespace ct
}  // namespace net

namespace component_updater {

class ComponentUpdateService;

// Component for receiving Signed Tree Heads updates for Certificate
// Transparency logs recognized in Chrome.
// The STHs are in JSON format.
// To identify the log each STH belongs to, the name of the file is
// hex-encoded Log ID of the log that produced this STH.
//
// Notifications of each of the new STHs are sent to the net::ct::STHObserver,
// so that it can take appropriate steps, including possible persistence.
class STHSetComponentInstallerTraits : public ComponentInstallerTraits {
 public:
  // The |sth_distributor| will be notified each time a new STH is observed.
  explicit STHSetComponentInstallerTraits(
      std::unique_ptr<net::ct::STHObserver> sth_observer);
  ~STHSetComponentInstallerTraits() override;

 private:
  friend class STHSetComponentInstallerTest;

  // ComponentInstallerTraits implementation.
  bool CanAutoUpdate() const override;
  bool RequiresNetworkEncryption() const override;
  bool OnCustomInstall(const base::DictionaryValue& manifest,
                       const base::FilePath& install_dir) override;
  bool VerifyInstallation(const base::DictionaryValue& manifest,
                          const base::FilePath& install_dir) const override;
  void ComponentReady(const base::Version& version,
                      const base::FilePath& install_dir,
                      std::unique_ptr<base::DictionaryValue> manifest) override;
  base::FilePath GetBaseDirectory() const override;
  void GetHash(std::vector<uint8_t>* hash) const override;
  std::string GetName() const override;
  std::string GetAp() const override;

  // Reads and parses the on-disk json.
  void LoadSTHsFromDisk(const base::FilePath& sths_file_path,
                        const base::Version& version);

  // Handle successful parsing of JSON by distributing the new STH.
  void OnJsonParseSuccess(const std::string& log_id,
                          std::unique_ptr<base::Value> parsed_json);

  // STH parsing failed - do nothing.
  void OnJsonParseError(const std::string& log_id, const std::string& error);

  std::unique_ptr<net::ct::STHObserver> sth_observer_;

  DISALLOW_COPY_AND_ASSIGN(STHSetComponentInstallerTraits);
};

void RegisterSTHSetComponent(ComponentUpdateService* cus,
                             const base::FilePath& user_data_dir);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_STH_SET_COMPONENT_INSTALLER_H_
