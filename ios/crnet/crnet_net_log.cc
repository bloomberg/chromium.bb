// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/crnet/crnet_net_log.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/values.h"
#include "net/log/net_log.h"
#include "net/log/net_log_util.h"
#include "net/log/write_to_file_net_log_observer.h"

CrNetNetLog::CrNetNetLog() { }
CrNetNetLog::~CrNetNetLog() { }

bool CrNetNetLog::Start(const base::FilePath& log_file,
                        CrNetNetLog::Mode mode) {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::FilePath temp_dir;
  if (!base::GetTempDir(&temp_dir))
    return false;

  base::FilePath full_path = temp_dir.Append(log_file);
  base::ScopedFILE file(base::OpenFile(full_path, "w"));
  if (!file)
    return false;

  scoped_ptr<base::Value> constants(net::GetNetConstants().Pass());

  net::NetLogCaptureMode capture_mode = mode == LOG_ALL_BYTES ?
      net::NetLogCaptureMode::IncludeSocketBytes() :
      net::NetLogCaptureMode::Default();
  write_to_file_observer_.reset(new net::WriteToFileNetLogObserver());
  write_to_file_observer_->set_capture_mode(capture_mode);
  write_to_file_observer_->StartObserving(this, file.Pass(), constants.get(),
                                          nullptr);
  return true;
}

void CrNetNetLog::Stop() {
  DCHECK(thread_checker_.CalledOnValidThread());
  write_to_file_observer_->StopObserving(nullptr);
  write_to_file_observer_.reset();
}
