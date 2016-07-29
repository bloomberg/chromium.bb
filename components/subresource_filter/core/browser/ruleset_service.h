// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CORE_BROWSER_RULESET_SERVICE_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CORE_BROWSER_RULESET_SERVICE_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/version.h"

class PrefService;
class PrefRegistrySimple;

namespace base {
class FileProxy;
class SequencedTaskRunner;
}  // namespace base

namespace subresource_filter {

class RulesetDistributor;

// Encapsulates the combination of the binary format version of the indexed
// ruleset, and the version of the ruleset contents.
//
// The wire format of unindexed rules coming through the component updater
// is expected to be relatively stable, so a ruleset is uniquely identified by
// its content version.
//
// In contrast, the binary format of the index data structures is expected to
// evolve over time, so the indexed ruleset is identified by a pair of versions:
// the content version of the rules that have been indexed; and the binary
// format version of the indexed data structures.
struct IndexedRulesetVersion {
  IndexedRulesetVersion();
  IndexedRulesetVersion(const std::string& content_version, int format_version);
  ~IndexedRulesetVersion();
  IndexedRulesetVersion& operator=(const IndexedRulesetVersion&);

  static void RegisterPrefs(PrefRegistrySimple* registry);

  static int CurrentFormatVersion();

  bool IsValid() const;
  bool IsCurrentFormatVersion() const;

  void SaveToPrefs(PrefService* local_state) const;
  void ReadFromPrefs(PrefService* local_state);
  base::FilePath GetSubdirectoryPathForVersion(
      const base::FilePath& base_dir) const;

  std::string content_version;
  int format_version = 0;
};

// Responsible for indexing subresource filtering rules that are downloaded
// through the component updater; for versioned storage of the indexed ruleset;
// and for supplying the most up-to-date version of the indexed ruleset to the
// RulesetDistributors for distribution.
//
// Files corresponding to each version of the indexed ruleset are stored in a
// separate subdirectory inside |indexed_ruleset_base_dir| named after the
// version. The version information of the most recent successfully stored
// ruleset is written into |local_state|. The invariant is maintained that the
// version pointed to by preferences, if valid, will exist on disk at any point
// in time. All file operations are posted to |blocking_task_runner|.
class RulesetService : public base::SupportsWeakPtr<RulesetService> {
 public:
  // Creates a new instance that will immediately publish the most recently
  // indexed version of the ruleset if one is available according to prefs.
  // See class comments for details of arguments.
  RulesetService(PrefService* local_state,
                 scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
                 const base::FilePath& indexed_ruleset_base_dir);
  virtual ~RulesetService();

  // Indexes, stores, and publishes the specified |content_version| of the
  // subresource filtering rules available at |unindexed_ruleset_path|, unless
  // the specified version matches that of the most recently indexed version, in
  // which case it does nothing. The |unindexed_ruleset_path| needs to remain
  // accessible even after the method returns.
  //
  // Computation-heavy steps and I/O are performed on a background thread.
  // Still, to prevent start-up congestion, this method should be called at the
  // earliest shortly after start-up.
  //
  // Virtual so that it can be mocked out in tests.
  virtual void IndexAndStoreAndPublishRulesetVersionIfNeeded(
      const base::FilePath& unindexed_ruleset_path,
      const std::string& content_version);

  // Registers a |distributor| that will be notified each time a new version of
  // the ruleset becomes avalable. The |distributor| will be destroyed along
  // with |this|.
  void RegisterDistributor(std::unique_ptr<RulesetDistributor> distributor);

 private:
  friend class SubresourceFilteringRulesetServiceTest;

  using WriteRulesetCallback =
      base::Callback<void(const IndexedRulesetVersion&)>;

  // Posts a task to the |blocking_task_runner_| to index and persist a new
  // |content_version| of the unindexed |ruleset|. Then, on success, updates the
  // most recently indexed version in preferences and invokes |success_callback|
  // on the calling thread. There is no callback on failure.
  void IndexAndStoreRuleset(const base::FilePath& unindexed_ruleset_path,
                            const std::string& content_version,
                            const WriteRulesetCallback& success_callback);

  // Reads the unindexed rules from |unindexed_ruleset_path|, indexes them, and
  // calls WriteRuleset() to write the indexed ruleset data. To be called on the
  // |blocking_task_runner_|.
  static bool IndexAndWriteRuleset(
      const base::FilePath& indexed_ruleset_base_dir,
      const IndexedRulesetVersion& indexed_version,
      const base::FilePath& unindexed_ruleset_path);

  // Writes the indexed ruleset |data| of the given |length| to the subdirectory
  // corresponding to |indexed_version| under |indexed_ruleset_base_dir|.
  // Returns true on success. To be called on the |blocking_task_runner_|.
  //
  // Writing is factored out into this separate function so it can be
  // independently exercised in tests.
  static bool WriteRuleset(const base::FilePath& indexed_ruleset_base_dir,
                           const IndexedRulesetVersion& indexed_version,
                           const uint8_t* data,
                           size_t length);

  void OnWrittenRuleset(const IndexedRulesetVersion& version,
                        const WriteRulesetCallback& result_callback,
                        bool success);

  void OpenAndPublishRuleset(const IndexedRulesetVersion& version);
  void OnOpenedRuleset(base::File::Error error);

  PrefService* const local_state_;
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;

  const base::FilePath indexed_ruleset_base_dir_;
  std::unique_ptr<base::FileProxy> ruleset_data_;
  std::vector<std::unique_ptr<RulesetDistributor>> distributors_;

  DISALLOW_COPY_AND_ASSIGN(RulesetService);
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CORE_BROWSER_RULESET_SERVICE_H_
