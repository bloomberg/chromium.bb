// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_URL_VALIDITY_CHECKER_FACTORY_H_
#define CHROME_BROWSER_SEARCH_URL_VALIDITY_CHECKER_FACTORY_H_

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "components/search/url_validity_checker_impl.h"

// Singleton that owns a single UrlValidityCheckerImpl instance. Should only be
// called from the UI thread.
class UrlValidityCheckerFactory {
 public:
  static UrlValidityChecker* GetInstance();

 private:
  friend struct base::LazyInstanceTraitsBase<UrlValidityCheckerFactory>;

  UrlValidityCheckerFactory();
  ~UrlValidityCheckerFactory();

  // The only instance that exists.
  UrlValidityCheckerImpl url_validity_checker_;

  DISALLOW_COPY_AND_ASSIGN(UrlValidityCheckerFactory);
};

#endif  // CHROME_BROWSER_SEARCH_URL_VALIDITY_CHECKER_FACTORY_H_
