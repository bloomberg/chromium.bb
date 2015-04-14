// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_ACTION_UPDATE_H_
#define COMPONENTS_UPDATE_CLIENT_ACTION_UPDATE_H_

#include <map>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/version.h"
#include "components/update_client/action.h"
#include "components/update_client/component_unpacker.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_engine.h"
#include "url/gurl.h"

namespace update_client {

class UpdateChecker;

// Defines a template method design pattern for ActionUpdate. This class
// implements the common code for updating a CRX using either differential or
// full updates algorithm.
class ActionUpdate : public Action, protected ActionImpl {
 public:
  ActionUpdate();
  ~ActionUpdate() override;

  // Action overrides.
  void Run(UpdateContext* update_context, Callback callback) override;

 private:
  virtual bool IsBackgroundDownload(const CrxUpdateItem* item) = 0;
  virtual std::vector<GURL> GetUrls(const CrxUpdateItem* item) = 0;
  virtual void OnDownloadStart(CrxUpdateItem* item) = 0;
  virtual void OnDownloadSuccess(
      CrxUpdateItem* item,
      const CrxDownloader::Result& download_result) = 0;
  virtual void OnDownloadError(
      CrxUpdateItem* item,
      const CrxDownloader::Result& download_result) = 0;
  virtual void OnInstallStart(CrxUpdateItem* item) = 0;
  virtual void OnInstallSuccess(CrxUpdateItem* item) = 0;
  virtual void OnInstallError(CrxUpdateItem* item,
                              ComponentUnpacker::Error error,
                              int extended_error) = 0;

  // Called when progress is being made downloading a CRX. The progress may
  // not monotonically increase due to how the CRX downloader switches between
  // different downloaders and fallback urls.
  void DownloadProgress(const std::string& crx_id,
                        const CrxDownloader::Result& download_result);

  // Called when the CRX package has been downloaded to a temporary location.
  void DownloadComplete(const std::string& crx_id,
                        const CrxDownloader::Result& download_result);

  void Install(const std::string& crx_id, const base::FilePath& crx_path);

  // TODO(sorin): refactor the public interface of ComponentUnpacker so
  // that these calls can run on the main thread.
  void DoInstallOnBlockingTaskRunner(UpdateContext* update_context,
                                     CrxUpdateItem* item,
                                     const base::FilePath& crx_path);

  void EndUnpackingOnBlockingTaskRunner(UpdateContext* update_context,
                                        CrxUpdateItem* item,
                                        const base::FilePath& crx_path,
                                        ComponentUnpacker::Error error,
                                        int extended_error);

  void DoneInstalling(const std::string& crx_id,
                      ComponentUnpacker::Error error,
                      int extended_error);

  DISALLOW_COPY_AND_ASSIGN(ActionUpdate);
};

class ActionUpdateDiff : public ActionUpdate {
 public:
  static scoped_ptr<Action> Create();

 private:
  ActionUpdateDiff();
  ~ActionUpdateDiff() override;

  void TryUpdateFull();

  // ActionUpdate overrides.
  bool IsBackgroundDownload(const CrxUpdateItem* item) override;
  std::vector<GURL> GetUrls(const CrxUpdateItem* item) override;
  void OnDownloadStart(CrxUpdateItem* item) override;
  void OnDownloadSuccess(CrxUpdateItem* item,
                         const CrxDownloader::Result& download_result) override;
  void OnDownloadError(CrxUpdateItem* item,
                       const CrxDownloader::Result& download_result) override;
  void OnInstallStart(CrxUpdateItem* item) override;
  void OnInstallSuccess(CrxUpdateItem* item) override;
  void OnInstallError(CrxUpdateItem* item,
                      ComponentUnpacker::Error error,
                      int extended_error) override;

  DISALLOW_COPY_AND_ASSIGN(ActionUpdateDiff);
};

class ActionUpdateFull : ActionUpdate {
 public:
  static scoped_ptr<Action> Create();

 private:
  ActionUpdateFull();
  ~ActionUpdateFull() override;

  // ActionUpdate overrides.
  bool IsBackgroundDownload(const CrxUpdateItem* item) override;
  std::vector<GURL> GetUrls(const CrxUpdateItem* item) override;
  void OnDownloadStart(CrxUpdateItem* item) override;
  void OnDownloadSuccess(CrxUpdateItem* item,
                         const CrxDownloader::Result& download_result) override;
  void OnDownloadError(CrxUpdateItem* item,
                       const CrxDownloader::Result& download_result) override;
  void OnInstallStart(CrxUpdateItem* item) override;
  void OnInstallSuccess(CrxUpdateItem* item) override;
  void OnInstallError(CrxUpdateItem* item,
                      ComponentUnpacker::Error error,
                      int extended_error) override;

  DISALLOW_COPY_AND_ASSIGN(ActionUpdateFull);
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_ACTION_UPDATE_H_
