// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/drive_protocol_handler.h"

#include "base/logging.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/chromeos/drive/drive_url_request_job.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "content/public/browser/browser_thread.h"
#include "net/url_request/url_request.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace drive {

DriveProtocolHandler::DriveProtocolHandler(void* profile_id)
    : profile_id_(profile_id) {
  scoped_refptr<base::SequencedWorkerPool> blocking_pool =
      BrowserThread::GetBlockingPool();
  blocking_task_runner_ =
      blocking_pool->GetSequencedTaskRunner(blocking_pool->GetSequenceToken());
}

DriveProtocolHandler::~DriveProtocolHandler() {
}

net::URLRequestJob* DriveProtocolHandler::MaybeCreateJob(
    net::URLRequest* request, net::NetworkDelegate* network_delegate) const {
  DVLOG(1) << "Handling url: " << request->url().spec();
  return new DriveURLRequestJob(
      base::Bind(&util::GetFileSystemByProfileId, profile_id_),
      blocking_task_runner_.get(),
      request,
      network_delegate);
}

}  // namespace drive
