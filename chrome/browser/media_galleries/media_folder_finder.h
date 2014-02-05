// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_MEDIA_FOLDER_FINDER_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_MEDIA_FOLDER_FINDER_H_

#include <map>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/media_galleries/media_scan_types.h"

// MediaFolderFinder scans local hard drives and look for folders that contain
// media files.
class MediaFolderFinder {
 public:
  typedef std::map<base::FilePath, MediaGalleryScanResult>
      MediaFolderFinderResults;
  typedef base::Callback<void(bool /*success*/,
                              const MediaFolderFinderResults& /*results*/)>
      MediaFolderFinderResultsCallback;
  typedef base::Callback<MediaGalleryScanFileType(const base::FilePath&)>
      FilterCallback;

  // |callback| will get called when the scan finishes. If the object is deleted
  // before it finishes, the scan will stop and |callback| will get called with
  // success = false.
  // MediaFolderFinder has a default set of per-platform paths to scan.
  // Override in tests with SetRootsForTesting().
  explicit MediaFolderFinder(const MediaFolderFinderResultsCallback& callback);
  virtual ~MediaFolderFinder();

  // Start the scan.
  virtual void StartScan();

 private:
  friend class MediaFolderFinderTest;

  enum ScanState {
    SCAN_STATE_NOT_STARTED,
    SCAN_STATE_STARTED,
    SCAN_STATE_FINISHED,
  };

  void SetRootsForTesting(const std::vector<base::FilePath>& roots);

  void OnInitialized(const std::vector<base::FilePath>& roots);

  // Scan a folder from |folders_to_scan_|.
  void ScanFolder();

  // Callback that returns the |scan_result| for |path| and the |new_folders|
  // to scan in future calls to ScanFolder().
  void GotScanResults(const base::FilePath& path,
                      const MediaGalleryScanResult* scan_result,
                      const std::vector<base::FilePath>* new_folders);

  const MediaFolderFinderResultsCallback results_callback_;
  MediaFolderFinderResults results_;

  std::vector<base::FilePath> folders_to_scan_;
  ScanState scan_state_;

  // Token to make sure all calls with |filter_callback_| are on the same
  // sequence.
  base::SequencedWorkerPool::SequenceToken token_;

  // Callback used to filter through files and make sure they are media files.
  FilterCallback filter_callback_;

  // Set of roots to scan for testing.
  bool has_roots_for_testing_;
  std::vector<base::FilePath> roots_for_testing_;

  base::WeakPtrFactory<MediaFolderFinder> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaFolderFinder);
};

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_MEDIA_FOLDER_FINDER_H_
