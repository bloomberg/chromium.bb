// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_NEW_TAB_PAGE_INTERCEPTOR_SERVICE_H_
#define CHROME_BROWSER_UI_SEARCH_NEW_TAB_PAGE_INTERCEPTOR_SERVICE_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/search_engines/template_url_service_observer.h"

class NewTabPageInterceptor;
class Profile;
class TemplateURLService;

namespace net {
class URLRequestInterceptor;
}

// Owns a NewTabPageInterceptor.
class NewTabPageInterceptorService : public KeyedService,
                                     public TemplateURLServiceObserver {
 public:
  explicit NewTabPageInterceptorService(Profile* profile);
  ~NewTabPageInterceptorService() override;

  // TemplateURLServiceObserver override.
  void OnTemplateURLServiceChanged() override;

  scoped_ptr<net::URLRequestInterceptor> CreateInterceptor();

 private:
  Profile* profile_;
  base::WeakPtr<NewTabPageInterceptor> interceptor_;
  // The TemplateURLService that we are observing. It will outlive this
  // NewTabPageInterceptorService due to the dependency declared in
  // NewTabPageInterceptorServiceFactory.
  TemplateURLService* template_url_service_;

  DISALLOW_COPY_AND_ASSIGN(NewTabPageInterceptorService);
};

#endif  // CHROME_BROWSER_UI_SEARCH_NEW_TAB_PAGE_INTERCEPTOR_SERVICE_H_
