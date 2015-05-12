// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LAUNCHER_SEARCH_PROVIDER_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_LAUNCHER_SEARCH_PROVIDER_SERVICE_H_

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/launcher_search_provider/error_reporter.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/launcher_search_provider.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/event_router.h"
#include "extensions/common/extension.h"

namespace app_list {
class LauncherSearchProvider;
}  // namespace app_list

namespace chromeos {
namespace launcher_search_provider {

// Relevance score should be provided in a range from 0 to 4. 0 is the lowest
// relevance, 4 is the highest.
const int kMaxSearchResultScore = 4;

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
  void OnQueryStarted(app_list::LauncherSearchProvider* provider,
                      const std::string& query,
                      const int max_result);

  // Dispatches onQueryEnded events to listener extensions.
  void OnQueryEnded();

  // Dispatches onOpenResult event of |item_id| to |extension_id|.
  void OnOpenResult(const extensions::ExtensionId& extension_id,
                    const std::string& item_id);

  // Sets search results of a listener extension.
  void SetSearchResults(
      const extensions::Extension* extension,
      scoped_ptr<ErrorReporter> error_reporter,
      const std::string& query_id,
      const std::vector<linked_ptr<
          extensions::api::launcher_search_provider::SearchResult>>& results);

  // Returns true if there is a running query.
  bool IsQueryRunning() const;

 private:
  // Returns extension ids of listener extensions.
  std::set<extensions::ExtensionId> GetListenerExtensionIds();

  Profile* const profile_;
  extensions::ExtensionRegistry* extension_registry_;
  app_list::LauncherSearchProvider* provider_;
  uint32 query_id_;
  bool is_query_running_;

  DISALLOW_COPY_AND_ASSIGN(Service);
};

}  // namespace launcher_search_provider
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LAUNCHER_SEARCH_PROVIDER_SERVICE_H_
