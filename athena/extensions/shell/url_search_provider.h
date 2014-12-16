// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_EXTENSIONS_SHELL_URL_SEARCH_PROVIDER_H_
#define ATHENA_EXTENSIONS_SHELL_URL_SEARCH_PROVIDER_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "components/omnibox/autocomplete_input.h"
#include "components/omnibox/autocomplete_provider_listener.h"
#include "ui/app_list/search_provider.h"

class AutocompleteProvider;
class TemplateURLService;

namespace content {
class BrowserContext;
}

namespace athena {

// A sample search provider.
class UrlSearchProvider : public app_list::SearchProvider,
                          public AutocompleteProviderListener {
 public:
  UrlSearchProvider(content::BrowserContext* browser_context);
  ~UrlSearchProvider() override;

  // Overridden from app_list::SearchProvider
  void Start(bool is_voice_query, const base::string16& query) override;
  void Stop() override;

  // Overridden from AutocompleteProviderListener
  void OnProviderUpdate(bool updated_matches) override;

 private:
  content::BrowserContext* browser_context_;

  // TODO(mukai): This should be provided through BrowserContextKeyedService.
  scoped_ptr<TemplateURLService> template_url_service_;

  AutocompleteInput input_;
  scoped_refptr<AutocompleteProvider> provider_;

  DISALLOW_COPY_AND_ASSIGN(UrlSearchProvider);
};

}  // namespace athena

#endif  // ATHENA_EXTENSIONS_SHELL_URL_SEARCH_PROVIDER_H_
