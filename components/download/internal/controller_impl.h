// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_CONTROLLER_IMPL_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_CONTROLLER_IMPL_H_

#include <map>
#include <memory>

#include "base/macros.h"
#include "base/optional.h"
#include "components/download/internal/controller.h"
#include "components/download/internal/download_driver.h"
#include "components/download/internal/model.h"
#include "components/download/internal/startup_status.h"
#include "components/download/public/download_params.h"

namespace download {

class ClientSet;
class DownloadDriver;
class Model;

struct Configuration;
struct SchedulingParams;

// The internal Controller implementation.  This class does all of the heavy
// lifting for the DownloadService.
class ControllerImpl : public Controller,
                       public DownloadDriver::Client,
                       public Model::Client {
 public:
  // |clients| is externally owned and must be guaranteed to outlive this class.
  ControllerImpl(std::unique_ptr<ClientSet> clients,
                 std::unique_ptr<Configuration> config,
                 std::unique_ptr<DownloadDriver> driver,
                 std::unique_ptr<Model> model);
  ~ControllerImpl() override;

  // Controller implementation.
  void Initialize() override;
  const StartupStatus* GetStartupStatus() override;
  void StartDownload(const DownloadParams& params) override;
  void PauseDownload(const std::string& guid) override;
  void ResumeDownload(const std::string& guid) override;
  void CancelDownload(const std::string& guid) override;
  void ChangeDownloadCriteria(const std::string& guid,
                              const SchedulingParams& params) override;
  DownloadClient GetOwnerOfDownload(const std::string& guid) override;

 private:
  // DownloadDriver::Client implementation.
  void OnDriverReady(bool success) override;
  void OnDownloadCreated(const DriverEntry& download) override;
  void OnDownloadFailed(const DriverEntry& download, int reason) override;
  void OnDownloadSucceeded(const DriverEntry& download,
                           const base::FilePath& path) override;
  void OnDownloadUpdated(const DriverEntry& download) override;

  // Model::Client implementation.
  void OnModelReady(bool success) override;
  void OnItemAdded(bool success,
                   DownloadClient client,
                   const std::string& guid) override;
  void OnItemUpdated(bool success,
                     DownloadClient client,
                     const std::string& guid) override;
  void OnItemRemoved(bool success,
                     DownloadClient client,
                     const std::string& guid) override;

  // Checks if initialization is complete and successful.  If so, completes the
  // internal state initialization.
  void AttemptToFinalizeSetup();

  // Cancels and cleans upany requests that are no longer associated with a
  // Client in |clients_|.
  void CancelOrphanedRequests();

  // Fixes any discrepencies in state between |model_| and |driver_|.  Meant to
  // resolve state issues during startup.
  void ResolveInitialRequestStates();

  // Notifies all Client in |clients_| that this controller is initialized and
  // lets them know which download requests we are aware of for their
  // DownloadClient.
  void NotifyClientsOfStartup();

  // Pulls the current state of requests from |model_| and |driver_| and sets
  // the other internal states appropriately.
  void PullCurrentRequestStatus();

  void HandleStartDownloadResponse(DownloadClient client,
                                   const std::string& guid,
                                   DownloadParams::StartResult result);
  void HandleStartDownloadResponse(
      DownloadClient client,
      const std::string& guid,
      DownloadParams::StartResult result,
      const DownloadParams::StartCallback& callback);

  std::unique_ptr<ClientSet> clients_;
  std::unique_ptr<Configuration> config_;

  // Owned Dependencies.
  std::unique_ptr<DownloadDriver> driver_;
  std::unique_ptr<Model> model_;

  // Internal state.
  StartupStatus startup_status_;
  std::map<std::string, DownloadParams::StartCallback> start_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(ControllerImpl);
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_CONTROLLER_IMPL_H_
