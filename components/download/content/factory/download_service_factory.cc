// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "build/build_config.h"
#include "components/download/content/factory/navigation_monitor_factory.h"
#include "components/download/content/internal/download_driver_impl.h"
#include "components/download/internal/client_set.h"
#include "components/download/internal/config.h"
#include "components/download/internal/controller_impl.h"
#include "components/download/internal/download_service_impl.h"
#include "components/download/internal/download_store.h"
#include "components/download/internal/file_monitor_impl.h"
#include "components/download/internal/logger_impl.h"
#include "components/download/internal/model_impl.h"
#include "components/download/internal/proto/entry.pb.h"
#include "components/download/internal/scheduler/scheduler_impl.h"
#include "components/leveldb_proto/proto_database_impl.h"

#if defined(OS_ANDROID)
#include "components/download/internal/android/battery_status_listener_android.h"
#endif

namespace download {
namespace {
const base::FilePath::CharType kEntryDBStorageDir[] =
    FILE_PATH_LITERAL("EntryDB");
const base::FilePath::CharType kFilesStorageDir[] = FILE_PATH_LITERAL("Files");
}  // namespace

DownloadService* CreateDownloadService(
    std::unique_ptr<DownloadClientMap> clients,
    content::DownloadManager* download_manager,
    const base::FilePath& storage_dir,
    const scoped_refptr<base::SequencedTaskRunner>& background_task_runner,
    std::unique_ptr<TaskScheduler> task_scheduler) {
  auto client_set = std::make_unique<ClientSet>(std::move(clients));
  auto config = Configuration::CreateFromFinch();

  auto files_storage_dir = storage_dir.Append(kFilesStorageDir);
  auto driver = std::make_unique<DownloadDriverImpl>(download_manager);

  auto entry_db_storage_dir = storage_dir.Append(kEntryDBStorageDir);
  auto entry_db =
      std::make_unique<leveldb_proto::ProtoDatabaseImpl<protodb::Entry>>(
          background_task_runner);
  auto store = std::make_unique<DownloadStore>(entry_db_storage_dir,
                                               std::move(entry_db));
  auto model = std::make_unique<ModelImpl>(std::move(store));

#if defined(OS_ANDROID)
  auto battery_listener = std::make_unique<BatteryStatusListenerAndroid>(
      config->battery_query_interval);
#else
  auto battery_listener =
      std::make_unique<BatteryStatusListener>(config->battery_query_interval);
#endif

  auto device_status_listener = std::make_unique<DeviceStatusListener>(
      config->network_startup_delay, config->network_change_delay,
      std::move(battery_listener));
  NavigationMonitor* navigation_monitor =
      NavigationMonitorFactory::GetForBrowserContext(
          download_manager->GetBrowserContext());
  auto scheduler = std::make_unique<SchedulerImpl>(
      task_scheduler.get(), config.get(), client_set.get());
  auto file_monitor = std::make_unique<FileMonitorImpl>(
      files_storage_dir, background_task_runner, config->file_keep_alive_time);
  auto logger = std::make_unique<LoggerImpl>();
  auto controller = std::make_unique<ControllerImpl>(
      config.get(), logger.get(), std::move(client_set), std::move(driver),
      std::move(model), std::move(device_status_listener), navigation_monitor,
      std::move(scheduler), std::move(task_scheduler), std::move(file_monitor),
      files_storage_dir);
  logger->SetLogSource(controller.get());

  return new DownloadServiceImpl(std::move(config), std::move(logger),
                                 std::move(controller));
}

}  // namespace download
