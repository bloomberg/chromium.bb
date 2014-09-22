// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_BROWSER_FILEAPI_MOCK_FILE_UPDATE_OBSERVER_H_
#define WEBKIT_BROWSER_FILEAPI_MOCK_FILE_UPDATE_OBSERVER_H_

#include <map>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "storage/browser/fileapi/file_observers.h"
#include "storage/browser/fileapi/file_system_url.h"
#include "storage/browser/fileapi/task_runner_bound_observer_list.h"

namespace storage {

// Mock file change observer.
class MockFileUpdateObserver : public FileUpdateObserver {
 public:
  MockFileUpdateObserver();
  virtual ~MockFileUpdateObserver();

  // Creates a ChangeObserverList which only contains given |observer|.
  static UpdateObserverList CreateList(MockFileUpdateObserver* observer);

  // FileUpdateObserver overrides.
  virtual void OnStartUpdate(const FileSystemURL& url) OVERRIDE;
  virtual void OnUpdate(const FileSystemURL& url, int64 delta) OVERRIDE;
  virtual void OnEndUpdate(const FileSystemURL& url) OVERRIDE;

  void Enable() { is_ready_ = true; }

  void Disable() {
    start_update_count_.clear();
    end_update_count_.clear();
    is_ready_ = false;
  }

 private:
  std::map<FileSystemURL, int, FileSystemURL::Comparator> start_update_count_;
  std::map<FileSystemURL, int, FileSystemURL::Comparator> end_update_count_;
  bool is_ready_;

  DISALLOW_COPY_AND_ASSIGN(MockFileUpdateObserver);
};

}  // namespace storage

#endif  // WEBKIT_BROWSER_FILEAPI_MOCK_FILE_UPDATE_OBSERVER_H_
