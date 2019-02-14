// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_SWITCHER_BROWSER_SWITCHER_SERVICE_H_
#define CHROME_BROWSER_BROWSER_SWITCHER_BROWSER_SWITCHER_SERVICE_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/browser_switcher/browser_switcher_prefs.h"
#include "components/keyed_service/core/keyed_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

class Profile;

namespace browser_switcher {

class AlternativeBrowserDriver;
class BrowserSwitcherSitelist;
class ParsedXml;

class XmlDownloader {
 public:
  // Posts a task to start downloading+parsing the rules after |delay|. Calls
  // |done_callback| when done, so the caller can apply the parsed rules and
  // clean up this object.
  XmlDownloader(Profile* profile,
                GURL url,
                base::TimeDelta delay,
                base::OnceCallback<void(ParsedXml)> done_callback);
  virtual ~XmlDownloader();

 private:
  void FetchXml();
  void ParseXml(std::unique_ptr<std::string> bytes);
  void DoneParsing(ParsedXml xml);

  GURL url_;
  scoped_refptr<network::SharedURLLoaderFactory> factory_;

  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  base::OnceCallback<void(ParsedXml)> done_callback_;

  base::WeakPtrFactory<XmlDownloader> weak_ptr_factory_;
};

// Manages per-profile resources for BrowserSwitcher.
class BrowserSwitcherService : public KeyedService {
 public:
  explicit BrowserSwitcherService(Profile* profile);
  ~BrowserSwitcherService() override;

  // KeyedService:
  void Shutdown() override;

  AlternativeBrowserDriver* driver();
  BrowserSwitcherSitelist* sitelist();
  const BrowserSwitcherPrefs& prefs() const;

  void SetDriverForTesting(std::unique_ptr<AlternativeBrowserDriver> driver);
  void SetSitelistForTesting(std::unique_ptr<BrowserSwitcherSitelist> sitelist);

  static void SetFetchDelayForTesting(base::TimeDelta delay);

 protected:
  BrowserSwitcherPrefs prefs_;

  // Delay for the IEEM/external XML fetch tasks, launched from the constructor.
  static base::TimeDelta fetch_delay_;

 private:
  void OnExternalSitelistParsed(ParsedXml xml);

  std::unique_ptr<XmlDownloader> external_sitelist_downloader_;

  // Per-profile helpers.
  std::unique_ptr<AlternativeBrowserDriver> driver_;
  std::unique_ptr<BrowserSwitcherSitelist> sitelist_;

  base::WeakPtrFactory<BrowserSwitcherService> weak_ptr_factory_;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(BrowserSwitcherService);
};

}  // namespace browser_switcher

#endif  // CHROME_BROWSER_BROWSER_SWITCHER_BROWSER_SWITCHER_SERVICE_H_
