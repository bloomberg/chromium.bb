// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_FILE_CHANGE_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_FILE_CHANGE_H_

#include <deque>
#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "storage/browser/fileapi/file_system_url.h"

namespace drive {
class ResourceEntry;

class FileChange {
 public:
  enum FileType {
    FILE_TYPE_UNKNOWN,
    FILE_TYPE_FILE,
    FILE_TYPE_DIRECTORY,
  };

  enum ChangeType {
    ADD_OR_UPDATE,
    DELETE,
  };

  class Change {
   public:
    Change(ChangeType change, FileType file_type);

    bool IsAddOrUpdate() const { return change_ == ADD_OR_UPDATE; }
    bool IsDelete() const { return change_ == DELETE; }

    bool IsFile() const { return file_type_ == FILE_TYPE_FILE; }
    bool IsDirectory() const { return file_type_ == FILE_TYPE_DIRECTORY; }
    bool IsTypeUnknown() const { return !IsFile() && !IsDirectory(); }

    ChangeType change() const { return change_; }
    FileType file_type() const { return file_type_; }

    std::string DebugString() const;

    bool operator==(const Change& that) const {
      return change() == that.change() && file_type() == that.file_type();
    }

   private:
    ChangeType change_;
    FileType file_type_;
  };

  class ChangeList {
   public:
    typedef std::deque<Change> List;

    ChangeList();
    ~ChangeList();

    // Updates the list with the |new_change|.
    void Update(const Change& new_change);

    size_t size() const { return list_.size(); }
    bool empty() const { return list_.empty(); }
    void clear() { list_.clear(); }
    const List& list() const { return list_; }
    const Change& front() const { return list_.front(); }
    const Change& back() const { return list_.back(); }

    ChangeList PopAndGetNewList() const;

    std::string DebugString() const;

   private:
    List list_;
  };

 public:
  typedef std::map<base::FilePath, FileChange::ChangeList> Map;

  FileChange();
  ~FileChange();

  void Update(const base::FilePath file_path,
              const FileChange::Change& new_change);
  void Update(const base::FilePath file_path,
              const FileChange::ChangeList& list);
  void Update(const base::FilePath file_path,
              FileType type,
              FileChange::ChangeType change);
  void Update(const base::FilePath file_path,
              const ResourceEntry& entry,
              FileChange::ChangeType change);
  void Apply(const FileChange& new_changed_files);

  const Map& map() const { return map_; }

  size_t size() const { return map_.size(); }
  bool empty() const { return map_.empty(); }
  void ClearForTest() { map_.clear(); }
  size_t CountDirectory(const base::FilePath& directory_path) const;
  size_t count(const base::FilePath& file_path) const {
    return map_.count(file_path);
  }

  std::string DebugString() const;

 private:
  Map map_;
};

}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_FILE_CHANGE_H_
