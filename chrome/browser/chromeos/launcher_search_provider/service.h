// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LAUNCHER_SEARCH_PROVIDER_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_LAUNCHER_SEARCH_PROVIDER_SERVICE_H_

#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/event_router.h"
#include "extensions/common/extension.h"

namespace chromeos {
namespace launcher_search_provider {

// Manages listener extensions and routes events. Listener extensions are
// extensions which are allowed to use this API. When this API becomes public,
// this API is white listed for file manager, and opt-in for other extensions.
// This service provides access control for it.
//
// TODO(yawano): Implement opt-in control (crbug.com/440649).
class Service : public KeyedService {
 public:
  Service(Profile* profile, extensions::ExtensionRegistry* extension_registry);
  ~Service() override;
  static Service* Get(content::BrowserContext* context);

  // Dispatches onQueryStarted events to listener extensions.
  void OnQueryStarted(const std::string& query, const int max_result);

  // Dispatches onQueryEnded events to listener extensions.
  void OnQueryEnded();

  // Returns true if there is a running query.
  bool IsQueryRunning() const;

 private:
  // Returns extension ids of listener extensions.
  std::set<extensions::ExtensionId> GetListenerExtensionIds();

  Profile* const profile_;
  extensions::ExtensionRegistry* extension_registry_;
  uint32 query_id_;
  bool is_query_running_;

  DISALLOW_COPY_AND_ASSIGN(Service);
};

}  // namespace launcher_search_provider
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LAUNCHER_SEARCH_PROVIDER_SERVICE_H_
