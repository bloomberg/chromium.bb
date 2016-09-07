// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/fileapi/safe_picasa_album_table_reader.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "chrome/browser/media_galleries/fileapi/media_file_system_backend.h"
#include "chrome/common/chrome_utility_messages.h"
#include "chrome/common/extensions/chrome_utility_extensions_messages.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "ui/base/l10n/l10n_util.h"

using content::BrowserThread;

namespace picasa {

SafePicasaAlbumTableReader::SafePicasaAlbumTableReader(
    AlbumTableFiles album_table_files)
    : album_table_files_(std::move(album_table_files)),
      parser_state_(INITIAL_STATE) {
  // TODO(tommycli): Add DCHECK to make sure |album_table_files| are all
  // opened read-only once security adds ability to check PlatformFiles.
  MediaFileSystemBackend::AssertCurrentlyOnMediaSequence();
}

void SafePicasaAlbumTableReader::Start(const ParserCallback& callback) {
  MediaFileSystemBackend::AssertCurrentlyOnMediaSequence();
  DCHECK(!callback.is_null());

  callback_ = callback;

  // Don't bother spawning process if any of the files are invalid.
  if (!album_table_files_.indicator_file.IsValid() ||
      !album_table_files_.category_file.IsValid() ||
      !album_table_files_.date_file.IsValid() ||
      !album_table_files_.filename_file.IsValid() ||
      !album_table_files_.name_file.IsValid() ||
      !album_table_files_.token_file.IsValid() ||
      !album_table_files_.uid_file.IsValid()) {
    MediaFileSystemBackend::MediaTaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(callback_,
                   false /* parse_success */,
                   std::vector<AlbumInfo>(),
                   std::vector<AlbumInfo>()));
    return;
  }

  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&SafePicasaAlbumTableReader::StartWorkOnIOThread, this));
}

SafePicasaAlbumTableReader::~SafePicasaAlbumTableReader() {
}

void SafePicasaAlbumTableReader::StartWorkOnIOThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_EQ(INITIAL_STATE, parser_state_);

  utility_process_host_ =
      content::UtilityProcessHost::Create(
          this, BrowserThread::GetTaskRunnerForThread(BrowserThread::IO).get())
          ->AsWeakPtr();
  utility_process_host_->SetName(l10n_util::GetStringUTF16(
      IDS_UTILITY_PROCESS_MEDIA_LIBRARY_FILE_CHECKER_NAME));

  AlbumTableFilesForTransit files_for_transit;
  files_for_transit.indicator_file = IPC::TakePlatformFileForTransit(
      std::move(album_table_files_.indicator_file));
  files_for_transit.category_file = IPC::TakePlatformFileForTransit(
      std::move(album_table_files_.category_file));
  files_for_transit.date_file =
      IPC::TakePlatformFileForTransit(std::move(album_table_files_.date_file));
  files_for_transit.filename_file = IPC::TakePlatformFileForTransit(
      std::move(album_table_files_.filename_file));
  files_for_transit.name_file =
      IPC::TakePlatformFileForTransit(std::move(album_table_files_.name_file));
  files_for_transit.token_file =
      IPC::TakePlatformFileForTransit(std::move(album_table_files_.token_file));
  files_for_transit.uid_file =
      IPC::TakePlatformFileForTransit(std::move(album_table_files_.uid_file));
  utility_process_host_->Send(new ChromeUtilityMsg_ParsePicasaPMPDatabase(
      files_for_transit));
  parser_state_ = STARTED_PARSING_STATE;
}

void SafePicasaAlbumTableReader::OnParsePicasaPMPDatabaseFinished(
    bool parse_success,
    const std::vector<AlbumInfo>& albums,
    const std::vector<AlbumInfo>& folders) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!callback_.is_null());
  if (parser_state_ != STARTED_PARSING_STATE)
    return;

  MediaFileSystemBackend::MediaTaskRunner()->PostTask(
      FROM_HERE, base::Bind(callback_, parse_success, albums, folders));
  parser_state_ = FINISHED_PARSING_STATE;
}

void SafePicasaAlbumTableReader::OnProcessCrashed(int exit_code) {
  DLOG(ERROR) << "SafePicasaAlbumTableReader::OnProcessCrashed()";
  OnParsePicasaPMPDatabaseFinished(
      false, std::vector<AlbumInfo>(), std::vector<AlbumInfo>());
}

bool SafePicasaAlbumTableReader::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(SafePicasaAlbumTableReader, message)
    IPC_MESSAGE_HANDLER(ChromeUtilityHostMsg_ParsePicasaPMPDatabase_Finished,
                        OnParsePicasaPMPDatabaseFinished)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

}  // namespace picasa
