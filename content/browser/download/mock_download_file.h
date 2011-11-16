// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_MOCK_DOWNLOAD_FILE_H_
#define CONTENT_BROWSER_DOWNLOAD_MOCK_DOWNLOAD_FILE_H_
#pragma once

#include <string>
#include <map>

#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "content/browser/download/download_file.h"
#include "content/browser/download/download_id.h"
#include "content/browser/download/download_manager.h"
#include "content/browser/download/download_request_handle.h"
#include "net/base/net_errors.h"

struct DownloadCreateInfo;

class MockDownloadFile : virtual public DownloadFile {
 public:
  // Class to extract statistics from the usage of |MockDownloadFile|.
  class StatisticsRecorder {
   public:
    enum StatisticsIndex {
      STAT_INITIALIZE,
      STAT_APPEND,
      STAT_RENAME,
      STAT_DETACH,
      STAT_CANCEL,
      STAT_FINISH,
      STAT_BYTES
    };

    StatisticsRecorder();
    ~StatisticsRecorder();

    // Records the statistic.
    // |index| indicates what statistic to use.
    void Record(StatisticsIndex index);

    // Adds to the statistic.
    // |index| indicates what statistic to use.
    void Add(StatisticsIndex index, int count);

    // Returns the statistic value.
    // |index| indicates what statistic to use.
    int Count(StatisticsIndex index);

   private:
    typedef std::map<StatisticsIndex, int> StatisticsMap;

    StatisticsMap map_;
  };

  MockDownloadFile(const DownloadCreateInfo* info,
                   const DownloadRequestHandle& request_handle,
                   DownloadManager* download_manager,
                   StatisticsRecorder* recorder);
  virtual ~MockDownloadFile();

  // DownloadFile functions.
  virtual net::Error Initialize(bool calculate_hash) OVERRIDE;
  virtual net::Error AppendDataToFile(const char* data,
                                      size_t data_len) OVERRIDE;
  virtual net::Error Rename(const FilePath& full_path) OVERRIDE;
  virtual void Detach() OVERRIDE;
  virtual void Cancel() OVERRIDE;
  virtual void Finish() OVERRIDE;
  virtual void AnnotateWithSourceInformation() OVERRIDE;
  virtual FilePath FullPath() const OVERRIDE;
  virtual bool InProgress() const OVERRIDE;
  virtual int64 BytesSoFar() const OVERRIDE;
  virtual bool GetSha256Hash(std::string* hash) OVERRIDE;
  virtual void CancelDownloadRequest() OVERRIDE;
  virtual int Id() const OVERRIDE;
  virtual DownloadManager* GetDownloadManager() OVERRIDE;
  virtual const DownloadId& GlobalId() const OVERRIDE;
  virtual std::string DebugString() const OVERRIDE;

  // Functions relating to setting and checking expectations.
  size_t rename_count() const { return rename_count_; }
  void SetExpectedPath(size_t index, const FilePath& path);

 private:
  // The unique identifier for this download, assigned at creation by
  // the DownloadFileManager for its internal record keeping.
  DownloadId id_;

  // The handle to the request information.  Used for operations outside the
  // download system, specifically canceling a download.
  DownloadRequestHandle request_handle_;

  // DownloadManager this download belongs to.
  scoped_refptr<DownloadManager> download_manager_;

  // Records usage statistics.  Not owned by this class (survives destruction).
  StatisticsRecorder* recorder_;

  // The number of times |Rename()| has been called.
  // Used instead of checking |recorder_| because the latter can be NULL.
  size_t rename_count_;

  // A vector of paths that we expect to call |Rename()| with.
  std::vector<FilePath> expected_rename_path_list_;

  // A buffer to hold the data we write.
  std::string data_;

  // Dummy in-progress flag.
  bool in_progress_;
};

#endif  // CONTENT_BROWSER_DOWNLOAD_MOCK_DOWNLOAD_FILE_H_
