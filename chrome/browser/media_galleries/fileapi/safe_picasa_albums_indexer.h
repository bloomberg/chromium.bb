// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_SAFE_PICASA_ALBUMS_INDEXER_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_SAFE_PICASA_ALBUMS_INDEXER_H_

#include <queue>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "chrome/common/media_galleries/picasa_types.h"
#include "content/public/browser/utility_process_host_client.h"

namespace base {
class FilePath;
}

namespace IPC {
class Message;
}

namespace picasa {

// SafePicasaAlbumsIndexer indexes the contents of Picasa Albums by parsing the
// INI files found in Folders. The SafePicasaAlbumsIndexer object is ref-counted
// and kept alive after Start() is called until the ParserCallback is called.
// The ParserCallback is guaranteed to be called eventually either when the
// utility process replies or when it dies.
class SafePicasaAlbumsIndexer : public content::UtilityProcessHostClient {
 public:
  typedef base::Callback<
      void(bool parse_success, const picasa::AlbumImagesMap&)>
      DoneCallback;

  SafePicasaAlbumsIndexer(const AlbumMap& albums, const AlbumMap& folders);

  void Start(const DoneCallback& callback);

 private:
  enum ParserState {
    INITIAL_STATE,
    STARTED_PARSING_STATE,
    FINISHED_PARSING_STATE,
  };

  // Private because content::UtilityProcessHostClient is ref-counted.
  virtual ~SafePicasaAlbumsIndexer();

  // Processes a batch of folders. Reposts itself until done, then starts IPC.
  void ProcessFoldersBatch();

  // Launches the utility process.  Must run on the IO thread.
  void StartWorkOnIOThread();

  // Notification from the utility process when it finshes indexing all the
  // album contents. On error will return an empty map.
  // Runs on the IO thread.
  void OnIndexPicasaAlbumsContentsFinished(const AlbumImagesMap& albums_images);

  // UtilityProcessHostClient implementation.
  // Runs on the IO thread.
  virtual void OnProcessCrashed(int exit_code) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  AlbumUIDSet album_uids_;

  // List of folders that still need their INI files read.
  std::queue<base::FilePath> folders_queue_;

  std::vector<picasa::FolderINIContents> folders_inis_;

  // Only accessed on the Media Task Runner.
  DoneCallback callback_;

  // Verifies the messages from the utility process came at the right time.
  // Initialized on the Media Task Runner, but only accessed on the IO thread.
  ParserState parser_state_;

  DISALLOW_COPY_AND_ASSIGN(SafePicasaAlbumsIndexer);
};

}  // namespace picasa

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_SAFE_PICASA_ALBUMS_INDEXER_H_
