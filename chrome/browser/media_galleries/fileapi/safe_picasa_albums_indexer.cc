// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/fileapi/safe_picasa_albums_indexer.h"

#include "base/file_util.h"
#include "chrome/browser/media_galleries/fileapi/media_file_system_backend.h"
#include "chrome/common/extensions/chrome_utility_extensions_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/utility_process_host.h"

using content::BrowserThread;
using content::UtilityProcessHost;

namespace picasa {

namespace {

// Arbitrarily chosen to be a decent size but not block thread too much.
const int kPicasaINIReadBatchSize = 10;

}  // namespace

// Picasa INI files are named "picasa.ini" on Picasa for Windows before version
// 71.18. Later versions and Picasa for Mac uses ".picasa.ini".
// See: https://support.google.com/picasa/answer/11257?hl=en
const char kPicasaINIFilenameLegacy[] = "picasa.ini";

SafePicasaAlbumsIndexer::SafePicasaAlbumsIndexer(const AlbumMap& albums,
                                                 const AlbumMap& folders)
    : parser_state_(INITIAL_STATE) {
  DCHECK(MediaFileSystemBackend::CurrentlyOnMediaTaskRunnerThread());

  folders_inis_.reserve(folders.size());

  for (AlbumMap::const_iterator it = albums.begin(); it != albums.end(); ++it)
    album_uids_.insert(it->second.uid);

  for (AlbumMap::const_iterator it = folders.begin(); it != folders.end(); ++it)
    folders_queue_.push(it->second.path);
}

void SafePicasaAlbumsIndexer::Start(const DoneCallback& callback) {
  DCHECK(MediaFileSystemBackend::CurrentlyOnMediaTaskRunnerThread());
  DCHECK(!callback.is_null());

  callback_ = callback;
  ProcessFoldersBatch();
}

SafePicasaAlbumsIndexer::~SafePicasaAlbumsIndexer() {
}

void SafePicasaAlbumsIndexer::ProcessFoldersBatch() {
  DCHECK(MediaFileSystemBackend::CurrentlyOnMediaTaskRunnerThread());

  for (int i = 0; i < kPicasaINIReadBatchSize && !folders_queue_.empty(); ++i) {
    base::FilePath folder_path = folders_queue_.front();
    folders_queue_.pop();

    folders_inis_.push_back(FolderINIContents());

    bool ini_read =
        base::ReadFileToString(
            folder_path.AppendASCII(kPicasaINIFilename),
            &folders_inis_.back().ini_contents) ||
        base::ReadFileToString(
            folder_path.AppendASCII(kPicasaINIFilenameLegacy),
            &folders_inis_.back().ini_contents);

    // See kPicasaINIFilename declaration for details.
    if (ini_read)
      folders_inis_.back().folder_path = folder_path;
    else
      folders_inis_.pop_back();
  }

  // If queue of folders to process not empty, post self onto task runner again.
  if (!folders_queue_.empty()) {
    MediaFileSystemBackend::MediaTaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(&SafePicasaAlbumsIndexer::ProcessFoldersBatch, this));
  } else {
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&SafePicasaAlbumsIndexer::StartWorkOnIOThread, this));
  }
}

void SafePicasaAlbumsIndexer::StartWorkOnIOThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_EQ(INITIAL_STATE, parser_state_);

  UtilityProcessHost* host =
      UtilityProcessHost::Create(this, base::MessageLoopProxy::current());
  host->Send(new ChromeUtilityMsg_IndexPicasaAlbumsContents(album_uids_,
                                                            folders_inis_));
  parser_state_ = STARTED_PARSING_STATE;
}

void SafePicasaAlbumsIndexer::OnIndexPicasaAlbumsContentsFinished(
    const AlbumImagesMap& albums_images) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!callback_.is_null());
  if (parser_state_ != STARTED_PARSING_STATE)
    return;

  MediaFileSystemBackend::MediaTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(callback_, true, albums_images));
  parser_state_ = FINISHED_PARSING_STATE;
}

void SafePicasaAlbumsIndexer::OnProcessCrashed(int exit_code) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!callback_.is_null());

  MediaFileSystemBackend::MediaTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(callback_, false, AlbumImagesMap()));
}

bool SafePicasaAlbumsIndexer::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(SafePicasaAlbumsIndexer, message)
    IPC_MESSAGE_HANDLER(
        ChromeUtilityHostMsg_IndexPicasaAlbumsContents_Finished,
        OnIndexPicasaAlbumsContentsFinished)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

}  // namespace picasa
