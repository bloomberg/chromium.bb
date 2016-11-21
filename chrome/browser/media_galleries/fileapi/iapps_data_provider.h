// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_IAPPS_DATA_PROVIDER_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_IAPPS_DATA_PROVIDER_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/files/file_path_watcher.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/media_galleries/fileapi/file_path_watcher_util.h"

namespace iapps {

// This class is the holder for iTunes parsed data. Given a path to the iTunes
// library XML file it will read it in, parse the data, and provide convenient
// methods to access it.  When the file changes, it will update the data.
// It is not thread safe, but can be run on any thread with IO access.
class IAppsDataProvider {
 public:
  typedef base::Callback<void(bool)> ReadyCallback;

  explicit IAppsDataProvider(const base::FilePath& library_path);
  virtual ~IAppsDataProvider();

  // Ask the data provider to refresh the data if necessary. |ready_callback|
  // will be called with the result; false if unable to parse the XML file.
  virtual void RefreshData(const ReadyCallback& ready_callback);

  // Get the platform path for the library XML file.
  const base::FilePath& library_path() const;

 protected:
  bool valid() const;

  // Subclass should set this to true if the last parse succeeded.
  void set_valid(bool valid);

  // Signal to subclass to parse the |library_path|. The |ready_callback|
  // should be called when parsing has finished.
  virtual void DoParseLibrary(const base::FilePath& library_path,
                              const ReadyCallback& ready_callback) = 0;

  // Called when |library_path_| has changed. Virtual for testing.
  virtual void OnLibraryChanged(const base::FilePath& path, bool error);

 private:
  // Called when the FilePathWatcher for |library_path_| has tried to add an
  // watch.
  void OnLibraryWatchStarted(MediaFilePathWatcherUniquePtr library_watcher);

  // Path to the library XML file.
  const base::FilePath library_path_;

  // True if the data needs to be refreshed from disk.
  bool needs_refresh_;

  // True if |library_| contains valid data. False at construction and if
  // reading or parsing the XML file fails.
  bool is_valid_;

  // A watcher on the library xml file.
  MediaFilePathWatcherUniquePtr library_watcher_;

  base::WeakPtrFactory<IAppsDataProvider> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(IAppsDataProvider);
};

}  // namespace iapps

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_IAPPS_DATA_PROVIDER_H_
