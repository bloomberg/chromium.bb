// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOOKALIKES_LOOKALIKE_URL_INTERSTITIAL_PAGE_H_
#define CHROME_BROWSER_LOOKALIKES_LOOKALIKE_URL_INTERSTITIAL_PAGE_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/time/time.h"
#include "components/security_interstitials/content/security_interstitial_page.h"

class GURL;

// This class is responsible for showing/hiding the interstitial page that
// occurs when a new domain is visited and it looks suspiciously like another
// more popular domain.
class LookalikeUrlInterstitialPage
    : public security_interstitials::SecurityInterstitialPage {
 public:
  // Interstitial type, used in tests.
  static const InterstitialPageDelegate::TypeID kTypeForTesting;

  LookalikeUrlInterstitialPage(
      content::WebContents* web_contents,
      const GURL& request_url,
      std::unique_ptr<
          security_interstitials::SecurityInterstitialControllerClient>
          controller);

  ~LookalikeUrlInterstitialPage() override;

 protected:
  // InterstitialPageDelegate implementation:
  void CommandReceived(const std::string& command) override;

  // SecurityInterstitialPage implementation:
  bool ShouldCreateNewNavigation() const override;
  void PopulateInterstitialStrings(
      base::DictionaryValue* load_time_data) override;
  void OnInterstitialClosing() override;
  int GetHTMLTemplateId() override;

 private:
  // Values added to get our shared interstitial HTML to play nice.
  void PopulateStringsForSharedHTML(base::DictionaryValue* load_time_data);

  DISALLOW_COPY_AND_ASSIGN(LookalikeUrlInterstitialPage);
};

#endif  // CHROME_BROWSER_LOOKALIKES_LOOKALIKE_URL_INTERSTITIAL_PAGE_H_
