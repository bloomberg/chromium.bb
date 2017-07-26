// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_UPDATER_CHROME_UPDATE_CLIENT_CONFIG_H_
#define CHROME_BROWSER_EXTENSIONS_UPDATER_CHROME_UPDATE_CLIENT_CONFIG_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/component_updater/configurator_impl.h"
#include "components/update_client/configurator.h"

namespace content {
class BrowserContext;
}

namespace extensions {

class ChromeUpdateClientConfig : public update_client::Configurator {
 public:
  explicit ChromeUpdateClientConfig(content::BrowserContext* context);

  int InitialDelay() const override;
  int NextCheckDelay() const override;
  int OnDemandDelay() const override;
  int UpdateDelay() const override;
  std::vector<GURL> UpdateUrl() const override;
  std::vector<GURL> PingUrl() const override;
  std::string GetProdId() const override;
  base::Version GetBrowserVersion() const override;
  std::string GetChannel() const override;
  std::string GetBrand() const override;
  std::string GetLang() const override;
  std::string GetOSLongName() const override;
  std::string ExtraRequestParams() const override;
  std::string GetDownloadPreference() const override;
  net::URLRequestContextGetter* RequestContext() const override;
  scoped_refptr<update_client::OutOfProcessPatcher> CreateOutOfProcessPatcher()
      const override;
  bool EnabledDeltas() const override;
  bool EnabledComponentUpdates() const override;
  bool EnabledBackgroundDownloader() const override;
  bool EnabledCupSigning() const override;
  PrefService* GetPrefService() const override;
  bool IsPerUserInstall() const override;
  std::vector<uint8_t> GetRunActionKeyHash() const override;

 protected:
  friend class base::RefCountedThreadSafe<ChromeUpdateClientConfig>;
  ~ChromeUpdateClientConfig() override;

 private:
  component_updater::ConfiguratorImpl impl_;

  DISALLOW_COPY_AND_ASSIGN(ChromeUpdateClientConfig);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_UPDATER_CHROME_UPDATE_CLIENT_CONFIG_H_
