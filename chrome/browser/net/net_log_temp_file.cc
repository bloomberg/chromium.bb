// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/net_log_temp_file.h"

#include "base/file_util.h"
#include "base/values.h"
#include "chrome/browser/net/chrome_net_log.h"
#include "chrome/browser/ui/webui/net_internals/net_internals_ui.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/net_log_logger.h"

using content::BrowserThread;

NetLogTempFile::NetLogTempFile(ChromeNetLog* chrome_net_log)
    : state_(STATE_UNINITIALIZED),
      log_filename_(FILE_PATH_LITERAL("chrome-net-export-log.json")),
      chrome_net_log_(chrome_net_log) {
}

NetLogTempFile::~NetLogTempFile() {
  if (net_log_logger_)
    net_log_logger_->StopObserving();
}

void NetLogTempFile::ProcessCommand(Command command) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE_USER_BLOCKING));
  if (!EnsureInit())
    return;

  switch (command) {
    case DO_START:
      StartNetLog();
      break;
    case DO_STOP:
      StopNetLog();
      break;
    default:
      NOTREACHED();
      break;
  }
}

base::DictionaryValue* NetLogTempFile::GetState() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE_USER_BLOCKING));
  base::DictionaryValue* dict = new base::DictionaryValue;

  EnsureInit();

#ifndef NDEBUG
  dict->SetString("file", log_path_.LossyDisplayName());
#endif  // NDEBUG

  switch (state_) {
    case STATE_ALLOW_START:
      dict->SetString("state", "ALLOW_START");
      break;
    case STATE_ALLOW_STOP:
      dict->SetString("state", "ALLOW_STOP");
      break;
    case STATE_ALLOW_START_SEND:
      dict->SetString("state", "ALLOW_START_SEND");
      break;
    default:
      dict->SetString("state", "UNINITIALIZED");
      break;
  }
  return dict;
}

bool NetLogTempFile::EnsureInit() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE_USER_BLOCKING));
  if (state_ != STATE_UNINITIALIZED)
    return true;

  if (!GetNetExportLog())
    return false;

  if (NetExportLogExists())
    state_ = STATE_ALLOW_START_SEND;
  else
    state_ = STATE_ALLOW_START;

  return true;
}

void NetLogTempFile::StartNetLog() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE_USER_BLOCKING));
  if (state_ == STATE_ALLOW_STOP)
    return;

  DCHECK_NE(STATE_UNINITIALIZED, state_);
  DCHECK(!log_path_.empty());

  // Try to make sure we can create the file.
  // TODO(rtenneti): Find a better for doing the following. Surface some error
  // to the user if we couldn't create the file.
  FILE* file = base::OpenFile(log_path_, "w");
  if (file == NULL)
    return;

  scoped_ptr<base::Value> constants(NetInternalsUI::GetConstants());
  net_log_logger_.reset(new net::NetLogLogger(file, *constants));
  net_log_logger_->StartObserving(chrome_net_log_);
  state_ = STATE_ALLOW_STOP;
}

void NetLogTempFile::StopNetLog() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE_USER_BLOCKING));
  if (state_ != STATE_ALLOW_STOP)
    return;

  net_log_logger_->StopObserving();
  net_log_logger_.reset();
  state_ = STATE_ALLOW_START_SEND;
}

bool NetLogTempFile::GetFilePath(base::FilePath* path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE_USER_BLOCKING));
  if (state_ != STATE_ALLOW_START_SEND)
    return false;

  if (!NetExportLogExists())
    return false;

  DCHECK(!log_path_.empty());
#if defined(OS_POSIX)
  // Users, group and others can read, write and traverse.
  int mode = base::FILE_PERMISSION_MASK;
  base::SetPosixFilePermissions(log_path_, mode);
#endif  // defined(OS_POSIX)

  *path = log_path_;
  return true;
}

bool NetLogTempFile::GetNetExportLog() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE_USER_BLOCKING));
  base::FilePath temp_dir;
  if (!GetNetExportLogDirectory(&temp_dir))
    return false;

  log_path_ = temp_dir.Append(log_filename_);
  return true;
}

bool NetLogTempFile::GetNetExportLogDirectory(base::FilePath* path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE_USER_BLOCKING));
  return base::GetTempDir(path);
}

bool NetLogTempFile::NetExportLogExists() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE_USER_BLOCKING));
  DCHECK(!log_path_.empty());
  return base::PathExists(log_path_);
}
