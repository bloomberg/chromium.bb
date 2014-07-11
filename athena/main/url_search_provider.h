// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_MAIN_URL_SEARCH_PROVIDER_H_
#define ATHENA_MAIN_URL_SEARCH_PROVIDER_H_

#include "base/memory/scoped_ptr.h"
#include "ui/app_list/search_provider.h"

class TemplateURLService;

namespace content {
class BrowserContext;
}

namespace athena {

// A sample search provider.
class UrlSearchProvider : public app_list::SearchProvider {
 public:
  UrlSearchProvider(content::BrowserContext* browser_context);
  virtual ~UrlSearchProvider();

  // Overridden from app_list::SearchProvider
  virtual void Start(const base::string16& query) OVERRIDE;
  virtual void Stop() OVERRIDE;

 private:
  content::BrowserContext* browser_context_;

  // TODO(mukai): This should be provided through BrowserContextKeyedService.
  scoped_ptr<TemplateURLService> template_url_service_;

  DISALLOW_COPY_AND_ASSIGN(UrlSearchProvider);
};

}  // namespace athena

#endif  // ATHENA_MAIN_URL_SEARCH_PROVIDER_H_
