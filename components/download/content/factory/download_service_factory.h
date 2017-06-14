// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_CONTENT_FACTORY_DOWNLOAD_SERVICE_FACTORY_H_
#define COMPONENTS_DOWNLOAD_CONTENT_FACTORY_DOWNLOAD_SERVICE_FACTORY_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "components/download/public/clients.h"

namespace content {
class DownloadManager;
}  // namespace content

namespace download {

class DownloadService;
class TaskScheduler;

// |clients| is a map of DownloadClient -> std::unique_ptr<Client>.  This
// represents all of the clients that are allowed to have requests made on
// their behalf.  This cannot be changed after startup.  Any existing requests
// no longer associated with a client will be cancelled.
// |storage_dir| is a path to where all the local storage will be.  This will
// hold the internal database as well as any temporary files on disk.  If this
// is an empty path, the service will not persist any information to disk and
// will act as an in-memory only service (this means no auto-retries after
// restarts, no files written on completion, etc.).
// |background_task_runner| will be used for all disk reads and writes.
DownloadService* CreateDownloadService(
    std::unique_ptr<DownloadClientMap> clients,
    content::DownloadManager* download_manager,
    const base::FilePath& storage_dir,
    const scoped_refptr<base::SequencedTaskRunner>& background_task_runner,
    std::unique_ptr<TaskScheduler> task_scheduler);

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_CONTENT_FACTORY_DOWNLOAD_SERVICE_FACTORY_H_
