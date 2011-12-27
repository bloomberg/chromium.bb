// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_TOP_SITES_EXTENSION_API_H_
#define CHROME_BROWSER_HISTORY_TOP_SITES_EXTENSION_API_H_
#pragma once

#include "chrome/browser/cancelable_request.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/history/history_types.h"

class GetTopSitesFunction : public AsyncExtensionFunction {
 public:
  GetTopSitesFunction();
  virtual ~GetTopSitesFunction();
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.topSites.get")

 private:
  void OnMostVisitedURLsAvailable(
      const history::MostVisitedURLList& data);

  CancelableRequestConsumer topsites_consumer_;
};

#endif  // CHROME_BROWSER_HISTORY_TOP_SITES_EXTENSION_API_H_
