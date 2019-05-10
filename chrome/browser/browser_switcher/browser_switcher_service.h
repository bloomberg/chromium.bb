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

// A definition of a source for an XML sitelist: a URL + what to do once it's
// downloaded.
struct RulesetSource {
  RulesetSource(GURL url_,
                bool contains_inverted_rules,
                base::OnceCallback<void(ParsedXml xml)> parsed_callback_);
  RulesetSource(RulesetSource&&);
  ~RulesetSource();

  // URL to download the ruleset from.
  GURL url;
  // If true, all the rules are inverted before being passed to the
  // callback. This is used for greylists.
  bool contains_inverted_rules;
  // What to do once the URL download + parsing is complete (or failed).
  base::OnceCallback<void(ParsedXml xml)> parsed_callback;

  std::unique_ptr<network::SimpleURLLoader> url_loader;
};

class XmlDownloader {
 public:
  // Posts a task to start downloading+parsing the rulesets after |delay|. Calls
  // each source's callback once they're done (or failed). In addition, calls
  // |all_done_callback| once all the rulesets have been processed.
  XmlDownloader(Profile* profile,
                std::vector<RulesetSource> sources,
                base::OnceCallback<void()> all_done_callback);
  virtual ~XmlDownloader();

 private:
  void FetchXml();
  void ParseXml(RulesetSource* source, std::unique_ptr<std::string> bytes);
  void DoneParsing(RulesetSource* source, ParsedXml xml);

  network::mojom::URLLoaderFactory* GetURLLoaderFactoryForURL(const GURL& url);

  std::unique_ptr<network::mojom::URLLoaderFactory> file_url_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> other_url_factory_;

  std::vector<RulesetSource> sources_;

  base::OnceCallback<void()> all_done_callback_;

  // Number of |RulesetSource|s that have finished processing. Used to
  // trigger the callback once they've all been parsed.
  unsigned int counter_ = 0;

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
  BrowserSwitcherPrefs& prefs();

  void SetDriverForTesting(std::unique_ptr<AlternativeBrowserDriver> driver);
  void SetSitelistForTesting(std::unique_ptr<BrowserSwitcherSitelist> sitelist);

  static void SetFetchDelayForTesting(base::TimeDelta delay);
  static void SetRefreshDelayForTesting(base::TimeDelta delay);

 protected:
  // Return a platform-specific list of URLs to download 1 minute after startup,
  // and what to do with each of them once their XML has been parsed.
  virtual std::vector<RulesetSource> GetRulesetSources();

  virtual void OnAllRulesetsParsed();

  // Delay for the IEEM/external XML fetch tasks, launched from the constructor.
  static base::TimeDelta fetch_delay_;
  static base::TimeDelta refresh_delay_;

 private:
  void StartDownload(Profile* profile);
  void OnExternalSitelistParsed(ParsedXml xml);
  void OnExternalGreylistParsed(ParsedXml xml);

  std::unique_ptr<XmlDownloader> sitelist_downloader_;

  BrowserSwitcherPrefs prefs_;

  // Per-profile helpers.
  std::unique_ptr<AlternativeBrowserDriver> driver_;
  std::unique_ptr<BrowserSwitcherSitelist> sitelist_;

  base::WeakPtrFactory<BrowserSwitcherService> weak_ptr_factory_;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(BrowserSwitcherService);
};

}  // namespace browser_switcher

#endif  // CHROME_BROWSER_BROWSER_SWITCHER_BROWSER_SWITCHER_SERVICE_H_
