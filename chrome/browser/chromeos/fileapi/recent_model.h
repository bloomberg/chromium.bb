// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILEAPI_RECENT_MODEL_H_
#define CHROME_BROWSER_CHROMEOS_FILEAPI_RECENT_MODEL_H_

#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "storage/browser/fileapi/file_system_url.h"

class Profile;

namespace chromeos {

class RecentContext;
class RecentModelFactory;
class RecentSource;

// The maximum number of files from a single source.
extern const size_t kMaxFilesFromSingleSource;

// Provides a list of recently modified files.
//
// All member functions must be called on the UI thread.
class RecentModel : public KeyedService {
 public:
  using RecentFileList = std::vector<storage::FileSystemURL>;
  using GetRecentFilesCallback =
      base::OnceCallback<void(const RecentFileList& files)>;

  ~RecentModel() override;

  // Returns an instance for the given profile.
  static RecentModel* GetForProfile(Profile* profile);

  // Creates an instance with given sources. Only for testing.
  static std::unique_ptr<RecentModel> CreateForTest(
      std::vector<std::unique_ptr<RecentSource>> sources);

  // Returns a list of recent files by querying sources. Results are internally
  // cached for better performance.
  void GetRecentFiles(RecentContext context, GetRecentFilesCallback callback);

 private:
  friend class RecentModelFactory;
  FRIEND_TEST_ALL_PREFIXES(RecentModelTest, GetRecentFiles_UmaStats);

  static const char kLoadHistogramName[];

  explicit RecentModel(Profile* profile);
  explicit RecentModel(std::vector<std::unique_ptr<RecentSource>> sources);

  void OnGetRecentFiles(RecentFileList files);
  void OnGetRecentFilesCompleted();
  void ClearCache();

  const std::vector<std::unique_ptr<RecentSource>> sources_;

  // Cached RecentFileList.
  base::Optional<RecentFileList> cached_files_ = base::nullopt;

  // Timer to clear the cache.
  base::OneShotTimer cache_clear_timer_;

  // Time when the build started.
  base::TimeTicks build_start_time_;

  // While a recent file list is built, this vector contains callbacks to be
  // invoked with the new list.
  std::vector<GetRecentFilesCallback> pending_callbacks_;

  // Number of in-flight sources building recent file lists.
  int num_inflight_sources_ = 0;

  // Intermediate container of recent files while building a list.
  RecentFileList intermediate_files_;

  base::WeakPtrFactory<RecentModel> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(RecentModel);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILEAPI_RECENT_MODEL_H_
