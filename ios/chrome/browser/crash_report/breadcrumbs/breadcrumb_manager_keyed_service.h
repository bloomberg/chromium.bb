// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CRASH_REPORT_BREADCRUMBS_BREADCRUMB_MANAGER_KEYED_SERVICE_H_
#define IOS_CHROME_BROWSER_CRASH_REPORT_BREADCRUMBS_BREADCRUMB_MANAGER_KEYED_SERVICE_H_

#include <list>
#include <memory>
#include <string>

#include "components/keyed_service/core/keyed_service.h"

class BreadcrumbManager;
class BreadcrumbManagerObserver;

namespace web {
class BrowserState;
}

// Associates a BreadcrumbManager instance with a BrowserState.
class BreadcrumbManagerKeyedService : public KeyedService {
 public:
  explicit BreadcrumbManagerKeyedService(web::BrowserState* browser_state);
  ~BreadcrumbManagerKeyedService() override;

  // Logs a breadcrumb |event| associated with the BrowserState passed in at
  // initialization of this instance. Prepends the |browsing_mode_| identifier
  // to the event before passing it to the |breadcrumb_manager_|.
  void AddEvent(const std::string& event);

  // Adds and removes observers to the underlying |breadcrumb_manager_|.
  void AddObserver(BreadcrumbManagerObserver* observer);
  void RemoveObserver(BreadcrumbManagerObserver* observer);

  // Returns up to |event_count_limit| events from the underlying
  // |breadcrumb_manager|. See |BreadcrumbManager::GetEvents| for returned event
  // details.
  const std::list<std::string> GetEvents(size_t event_count_limit) const;

 private:
  // A short string identifying the browser state used to initialize the
  // receiver. For example, "I" for "I"ncognito browsing mode. This value is
  // prepended to events sent to |AddEvent| in order to differentiate the
  // BrowserState associated with each event.
  // Note: Normal browsing mode uses an empty string in order to prevent
  // prepending most events with the same static value.
  std::string browsing_mode_;

  // The associated BreadcrumbManager to store events added with |AddEvent|.
  std::unique_ptr<BreadcrumbManager> breadcrumb_manager_;

  DISALLOW_COPY_AND_ASSIGN(BreadcrumbManagerKeyedService);
};

#endif  // IOS_CHROME_BROWSER_CRASH_REPORT_BREADCRUMBS_BREADCRUMB_MANAGER_KEYED_SERVICE_H_
