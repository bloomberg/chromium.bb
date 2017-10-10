// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TRACING_CHROME_TRACING_DELEGATE_H_
#define CHROME_BROWSER_TRACING_CHROME_TRACING_DELEGATE_H_

#include <memory>

#include "chrome/browser/ui/browser_list_observer.h"
#include "content/public/browser/tracing_delegate.h"

class PrefRegistrySimple;

class ChromeTracingDelegate : public content::TracingDelegate,
                              public chrome::BrowserListObserver {
 public:
  ChromeTracingDelegate();
  ~ChromeTracingDelegate() override;

  static void RegisterPrefs(PrefRegistrySimple* registry);

  std::unique_ptr<content::TraceUploader> GetTraceUploader(
      net::URLRequestContextGetter* request_context) override;

  bool IsAllowedToBeginBackgroundScenario(
      const content::BackgroundTracingConfig& config,
      bool requires_anonymized_data) override;

  bool IsAllowedToEndBackgroundScenario(
      const content::BackgroundTracingConfig& config,
      bool requires_anonymized_data) override;

  bool IsProfileLoaded() override;

  std::unique_ptr<base::DictionaryValue> GenerateMetadataDict() override;

  content::MetadataFilterPredicate GetMetadataFilterPredicate() override;

 private:
  // chrome::BrowserListObserver implementation.
  void OnBrowserAdded(Browser* browser) override;

  bool incognito_launched_;
};

#endif  // CHROME_BROWSER_TRACING_CHROME_TRACING_DELEGATE_H_
