// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/fileapi/safe_picasa_album_table_reader.h"

#include "base/bind.h"
#include "base/logging.h"
#include "chrome/browser/media_galleries/fileapi/media_file_system_backend.h"
#include "chrome/common/chrome_utility_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"

using chrome::MediaFileSystemBackend;
using content::BrowserThread;

namespace picasa {

SafePicasaAlbumTableReader::SafePicasaAlbumTableReader(
    const AlbumTableFiles& album_table_files)
    : album_table_files_(album_table_files), parser_state_(INITIAL_STATE) {
  // TODO(tommycli): Add DCHECK to make sure |album_table_files| are all
  // opened read-only once security adds ability to check PlatformFiles.
  DCHECK(MediaFileSystemBackend::CurrentlyOnMediaTaskRunnerThread());
}

void SafePicasaAlbumTableReader::Start(const ParserCallback& callback) {
  DCHECK(MediaFileSystemBackend::CurrentlyOnMediaTaskRunnerThread());
  DCHECK(!callback.is_null());

  callback_ = callback;

  // Don't bother spawning process if any of the files are invalid.
  if (album_table_files_.indicator_file == base::kInvalidPlatformFileValue ||
      album_table_files_.category_file == base::kInvalidPlatformFileValue ||
      album_table_files_.date_file == base::kInvalidPlatformFileValue ||
      album_table_files_.filename_file == base::kInvalidPlatformFileValue ||
      album_table_files_.name_file == base::kInvalidPlatformFileValue ||
      album_table_files_.token_file == base::kInvalidPlatformFileValue ||
      album_table_files_.uid_file == base::kInvalidPlatformFileValue) {
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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK_EQ(INITIAL_STATE, parser_state_);

  utility_process_host_ = content::UtilityProcessHost::Create(
      this,
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO).get())
      ->AsWeakPtr();
  // Wait for the startup notification before sending the main IPC to the
  // utility process, so that we can dup the file handle.
  utility_process_host_->Send(new ChromeUtilityMsg_StartupPing);
  parser_state_ = PINGED_UTILITY_PROCESS_STATE;
}

void SafePicasaAlbumTableReader::OnProcessStarted() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (parser_state_ != PINGED_UTILITY_PROCESS_STATE)
    return;

  if (utility_process_host_->GetData().handle == base::kNullProcessHandle) {
    DLOG(ERROR) << "Child process handle is null";
  }
  AlbumTableFilesForTransit files_for_transit;
  files_for_transit.indicator_file = IPC::GetFileHandleForProcess(
      album_table_files_.indicator_file,
      utility_process_host_->GetData().handle,
      true /* close_source_handle */);
  files_for_transit.category_file = IPC::GetFileHandleForProcess(
      album_table_files_.category_file,
      utility_process_host_->GetData().handle,
      true /* close_source_handle */);
  files_for_transit.date_file = IPC::GetFileHandleForProcess(
      album_table_files_.date_file,
      utility_process_host_->GetData().handle,
      true /* close_source_handle */);
  files_for_transit.filename_file = IPC::GetFileHandleForProcess(
      album_table_files_.filename_file,
      utility_process_host_->GetData().handle,
      true /* close_source_handle */);
  files_for_transit.name_file = IPC::GetFileHandleForProcess(
      album_table_files_.name_file,
      utility_process_host_->GetData().handle,
      true /* close_source_handle */);
  files_for_transit.token_file = IPC::GetFileHandleForProcess(
      album_table_files_.token_file,
      utility_process_host_->GetData().handle,
      true /* close_source_handle */);
  files_for_transit.uid_file = IPC::GetFileHandleForProcess(
      album_table_files_.uid_file,
      utility_process_host_->GetData().handle,
      true /* close_source_handle */);
  utility_process_host_->Send(new ChromeUtilityMsg_ParsePicasaPMPDatabase(
      files_for_transit));
  parser_state_ = STARTED_PARSING_STATE;
}

void SafePicasaAlbumTableReader::OnParsePicasaPMPDatabaseFinished(
    bool parse_success,
    const std::vector<AlbumInfo>& albums,
    const std::vector<AlbumInfo>& folders) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
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
    IPC_MESSAGE_HANDLER(ChromeUtilityHostMsg_ProcessStarted,
                        OnProcessStarted)
    IPC_MESSAGE_HANDLER(ChromeUtilityHostMsg_ParsePicasaPMPDatabase_Finished,
                        OnParsePicasaPMPDatabaseFinished)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

}  // namespace picasa
