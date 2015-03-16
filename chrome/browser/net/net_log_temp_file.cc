// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/net_log_temp_file.h"

#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/values.h"
#include "chrome/browser/net/chrome_net_log.h"
#include "chrome/browser/ui/webui/net_internals/net_internals_ui.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/net_log_logger.h"

using content::BrowserThread;

NetLogTempFile::NetLogTempFile(ChromeNetLog* chrome_net_log)
    : state_(STATE_UNINITIALIZED),
      log_type_(LOG_TYPE_NONE),
      log_filename_(FILE_PATH_LITERAL("chrome-net-export-log.json")),
      chrome_net_log_(chrome_net_log) {
}

NetLogTempFile::~NetLogTempFile() {
  if (net_log_logger_)
    net_log_logger_->StopObserving(nullptr);
}

void NetLogTempFile::ProcessCommand(Command command) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE_USER_BLOCKING));
  if (!EnsureInit())
    return;

  switch (command) {
    case DO_START_LOG_BYTES:
      StartNetLog(LOG_TYPE_LOG_BYTES);
      break;
    case DO_START:
      StartNetLog(LOG_TYPE_NORMAL);
      break;
    case DO_START_STRIP_PRIVATE_DATA:
      StartNetLog(LOG_TYPE_STRIP_PRIVATE_DATA);
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
    case STATE_NOT_LOGGING:
      dict->SetString("state", "NOT_LOGGING");
      break;
    case STATE_LOGGING:
      dict->SetString("state", "LOGGING");
      break;
    case STATE_UNINITIALIZED:
      dict->SetString("state", "UNINITIALIZED");
      break;
  }

  switch (log_type_) {
    case LOG_TYPE_NONE:
      dict->SetString("logType", "NONE");
      break;
    case LOG_TYPE_UNKNOWN:
      dict->SetString("logType", "UNKNOWN");
      break;
    case LOG_TYPE_LOG_BYTES:
      dict->SetString("logType", "LOG_BYTES");
      break;
    case LOG_TYPE_NORMAL:
      dict->SetString("logType", "NORMAL");
      break;
    case LOG_TYPE_STRIP_PRIVATE_DATA:
      dict->SetString("logType", "STRIP_PRIVATE_DATA");
      break;
  }

  return dict;
}

net::NetLog::LogLevel NetLogTempFile::GetLogLevelForLogType(LogType log_type) {
  switch (log_type) {
  case LOG_TYPE_LOG_BYTES:
    return net::NetLog::LOG_ALL;
  case LOG_TYPE_NORMAL:
    return net::NetLog::LOG_ALL_BUT_BYTES;
  case LOG_TYPE_STRIP_PRIVATE_DATA:
    return net::NetLog::LOG_STRIP_PRIVATE_DATA;
  case LOG_TYPE_NONE:
  case LOG_TYPE_UNKNOWN:
    NOTREACHED();
  }
  return net::NetLog::LOG_STRIP_PRIVATE_DATA;
}

bool NetLogTempFile::EnsureInit() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE_USER_BLOCKING));
  if (state_ != STATE_UNINITIALIZED)
    return true;

  if (!GetNetExportLog())
    return false;

  state_ = STATE_NOT_LOGGING;
  if (NetExportLogExists())
    log_type_ = LOG_TYPE_UNKNOWN;
  else
    log_type_ = LOG_TYPE_NONE;

  return true;
}

void NetLogTempFile::StartNetLog(LogType log_type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE_USER_BLOCKING));
  if (state_ == STATE_LOGGING)
    return;

  DCHECK_NE(STATE_UNINITIALIZED, state_);
  DCHECK(!log_path_.empty());

  // Try to make sure we can create the file.
  // TODO(rtenneti): Find a better for doing the following. Surface some error
  // to the user if we couldn't create the file.
  base::ScopedFILE file(base::OpenFile(log_path_, "w"));
  if (!file)
    return;

  log_type_ = log_type;
  state_ = STATE_LOGGING;

  scoped_ptr<base::Value> constants(NetInternalsUI::GetConstants());
  net_log_logger_.reset(new net::NetLogLogger());
  net_log_logger_->set_log_level(GetLogLevelForLogType(log_type));
  net_log_logger_->StartObserving(chrome_net_log_, file.Pass(), constants.get(),
                                  nullptr);
}

void NetLogTempFile::StopNetLog() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE_USER_BLOCKING));
  if (state_ != STATE_LOGGING)
    return;

  net_log_logger_->StopObserving(nullptr);
  net_log_logger_.reset();
  state_ = STATE_NOT_LOGGING;
}

bool NetLogTempFile::GetFilePath(base::FilePath* path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE_USER_BLOCKING));
  if (log_type_ == LOG_TYPE_NONE || state_ == STATE_LOGGING)
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
