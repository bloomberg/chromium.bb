// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_SAVED_FILES_SERVICE_H_
#define APPS_SAVED_FILES_SERVICE_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/stl_util.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class Profile;
class SavedFilesServiceUnitTest;
FORWARD_DECLARE_TEST(SavedFilesServiceUnitTest, RetainTwoFilesTest);
FORWARD_DECLARE_TEST(SavedFilesServiceUnitTest, EvictionTest);
FORWARD_DECLARE_TEST(SavedFilesServiceUnitTest, SequenceNumberCompactionTest);

namespace extensions {
class Extension;
}

namespace apps {

// Represents a file entry that a user has given an app permission to
// access. Will be persisted to disk (in the Preferences file), so should remain
// serializable.
struct SavedFileEntry {
  SavedFileEntry();

  SavedFileEntry(const std::string& id,
                 const base::FilePath& path,
                 bool is_directory,
                 int sequence_number);

  // The opaque id of this file entry.
  std::string id;

  // The path to a file entry that the app had permission to access.
  base::FilePath path;

  // Whether or not the entry refers to a directory.
  bool is_directory;

  // The sequence number in the LRU of the file entry. The value 0 indicates
  // that the entry is not in the LRU.
  int sequence_number;
};

// Tracks the files that apps have retained access to both while running and
// when suspended.
class SavedFilesService : public KeyedService,
                          public content::NotificationObserver {
 public:
  explicit SavedFilesService(Profile* profile);
  virtual ~SavedFilesService();

  static SavedFilesService* Get(Profile* profile);

  // Registers a file entry with the saved files service, making it eligible to
  // be put into the queue. File entries that are in the retained files queue at
  // object construction are automatically registered.
  void RegisterFileEntry(const std::string& extension_id,
                         const std::string& id,
                         const base::FilePath& file_path,
                         bool is_directory);

  // If the file with |id| is not in the queue of files to be retained
  // permanently, adds the file to the back of the queue, evicting the least
  // recently used entry at the front of the queue if it is full. If it is
  // already present, moves it to the back of the queue. The |id| must have been
  // registered.
  void EnqueueFileEntry(const std::string& extension_id, const std::string& id);

  // Returns whether the file entry with the given |id| has been registered.
  bool IsRegistered(const std::string& extension_id, const std::string& id);

  // Gets a borrowed pointer to the file entry with the specified |id|. Returns
  // NULL if the file entry has not been registered.
  const SavedFileEntry* GetFileEntry(const std::string& extension_id,
                                     const std::string& id);

  // Returns all registered file entries.
  std::vector<SavedFileEntry> GetAllFileEntries(
      const std::string& extension_id);

  // Clears all retained files if the app does not have the
  // fileSystem.retainEntries permission.
  void ClearQueueIfNoRetainPermission(const extensions::Extension* extension);

  // Clears all retained files.
  void ClearQueue(const extensions::Extension* extension);

 private:
  FRIEND_TEST_ALL_PREFIXES(::SavedFilesServiceUnitTest, RetainTwoFilesTest);
  FRIEND_TEST_ALL_PREFIXES(::SavedFilesServiceUnitTest, EvictionTest);
  FRIEND_TEST_ALL_PREFIXES(::SavedFilesServiceUnitTest,
                           SequenceNumberCompactionTest);
  friend class ::SavedFilesServiceUnitTest;

  // A container for the registered files for an app.
  class SavedFiles;

  // content::NotificationObserver.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Returns the SavedFiles for |extension_id| or NULL if one does not exist.
  SavedFiles* Get(const std::string& extension_id) const;

  // Returns the SavedFiles for |extension_id|, creating it if necessary.
  SavedFiles* GetOrInsert(const std::string& extension_id);

  // Clears the SavedFiles for |extension_id|.
  void Clear(const std::string& extension_id);

  static void SetMaxSequenceNumberForTest(int max_value);
  static void ClearMaxSequenceNumberForTest();
  static void SetLruSizeForTest(int size);
  static void ClearLruSizeForTest();

  std::map<std::string, SavedFiles*> extension_id_to_saved_files_;
  STLValueDeleter<std::map<std::string, SavedFiles*> >
      extension_id_to_saved_files_deleter_;
  content::NotificationRegistrar registrar_;
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(SavedFilesService);
};

}  // namespace apps

#endif  // APPS_SAVED_FILES_SERVICE_H_
