// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_FEEDBACK_PRIVATE_LOG_SOURCE_ACCESS_MANAGER_H_
#define EXTENSIONS_BROWSER_API_FEEDBACK_PRIVATE_LOG_SOURCE_ACCESS_MANAGER_H_

#include <map>
#include <string>
#include <utility>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "components/feedback/system_logs/system_logs_source.h"
#include "content/public/browser/browser_context.h"
#include "extensions/common/api/feedback_private.h"

namespace extensions {

// Provides bookkeepping for SingleLogSource usage. It ensures that:
// - Each extension can have only one SingleLogSource for a particular source.
// - A source may not be accessed too frequently by an extension.
class LogSourceAccessManager {
 public:
  using ReadLogSourceCallback =
      base::Callback<void(const api::feedback_private::ReadLogSourceResult&)>;

  explicit LogSourceAccessManager(content::BrowserContext* context);
  ~LogSourceAccessManager();

  // To override the default rate-limiting mechanism of this function, pass in
  // a TimeDelta representing the desired minimum time between consecutive reads
  // of a source from an extension. Does not take ownership of |timeout|. When
  // done testing, call this function again with |timeout|=nullptr to reset to
  // the default behavior.
  static void SetRateLimitingTimeoutForTesting(const base::TimeDelta* timeout);

  // Override the default base::Time clock with a custom clock for testing.
  // Pass in |clock|=nullptr to revert to default behavior.
  void SetTickClockForTesting(std::unique_ptr<base::TickClock> clock) {
    tick_clock_ = std::move(clock);
  }

  // Initiates a fetch from a log source, as specified in |params|. See
  // feedback_private.idl for more info about the actual parameters.
  bool FetchFromSource(const api::feedback_private::ReadLogSourceParams& params,
                       const std::string& extension_id,
                       const ReadLogSourceCallback& callback);

 private:
  FRIEND_TEST_ALL_PREFIXES(LogSourceAccessManagerTest,
                           MaxNumberOfOpenLogSources);

  // Contains a source/extension pair.
  struct SourceAndExtension {
    explicit SourceAndExtension(api::feedback_private::LogSource source,
                                const std::string& extension_id);

    bool operator<(const SourceAndExtension& other) const {
      return std::make_pair(source, extension_id) <
             std::make_pair(other.source, other.extension_id);
    }

    api::feedback_private::LogSource source;
    std::string extension_id;
  };

  // Creates a new LogSourceResource for the source and extension indicated by
  // |key|. Stores the new resource in the API Resource Manager and stores the
  // resource ID in |sources_| as a new entry. Returns the nonzero ID of the
  // newly created resource, or 0 if there was already an existing resource for
  // |key|.
  int CreateResource(const SourceAndExtension& key);

  // Callback that is passed to the log source from FetchFromSource.
  // Arguments:
  // - key: The source that was read, and the extension requesting the read.
  // - delete_source: Set this if the source indicated by |key| should be
  //   removed from both the API Resource Manager and from |sources_|.
  // - response_callback: Callback for sending the response as a
  //   ReadLogSourceResult struct.
  void OnFetchComplete(const SourceAndExtension& key,
                       bool delete_source,
                       const ReadLogSourceCallback& callback,
                       system_logs::SystemLogsResponse* response);

  // Removes an existing log source indicated by |key| from both the API
  // Resource Manager and |sources_|.
  void RemoveSource(const SourceAndExtension& key);

  // Attempts to update the entry for |key| in |last_access_times_| to the
  // current time, to record that the source is being accessed by the extension
  // right now. If less than |min_time_between_reads_| has elapsed since the
  // last successful read, do not update the timestamp in |last_access_times_|,
  // and instead return false. Otherwise returns true.
  //
  // Creates a new entry in |last_access_times_| if it doesn't exist. Will not
  // delete from |last_access_times_|.
  bool UpdateSourceAccessTime(const SourceAndExtension& key);

  // Returns the last time that |key.source| was accessed by |key.extension|.
  // If it was never accessed by the extension, returns an empty base::TimeTicks
  // object.
  base::TimeTicks GetLastExtensionAccessTime(
      const SourceAndExtension& key) const;

  // Returns the number of entries in |sources_| with source=|source|.
  size_t GetNumActiveResourcesForSource(
      api::feedback_private::LogSource source) const;

  // Every SourceAndExtension is linked to a unique SingleLogSource.
  //
  // Keys: SourceAndExtension for which a SingleLogSource has been created
  //       and not yet destroyed. (i.e. currently in use).
  // Values: ID of the API Resource containing the SingleLogSource.
  std::map<SourceAndExtension, int> sources_;

  // Keeps track of the last time each source was accessed by each extension.
  // Each time FetchFromSource() is called, the timestamp gets updated.
  //
  // This intentionally kept separate from |sources_| because entries can be
  // removed from and re-added to |sources_|, but that should not erase the
  // recorded access times.
  std::map<SourceAndExtension, base::TimeTicks> last_access_times_;

  // For fetching browser resources like ApiResourceManager.
  content::BrowserContext* context_;

  // Provides a timer clock implementation for keeping track of access times.
  // Can override the default clock for testing.
  std::unique_ptr<base::TickClock> tick_clock_;

  base::WeakPtrFactory<LogSourceAccessManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(LogSourceAccessManager);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_FEEDBACK_PRIVATE_LOG_SOURCE_ACCESS_MANAGER_H_
