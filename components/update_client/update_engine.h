// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_UPDATE_ENGINE_H_
#define COMPONENTS_UPDATE_CLIENT_UPDATE_ENGINE_H_

#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <queue>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "components/update_client/component.h"
#include "components/update_client/crx_downloader.h"
#include "components/update_client/crx_update_item.h"
#include "components/update_client/ping_manager.h"
#include "components/update_client/update_checker.h"
#include "components/update_client/update_client.h"

namespace base {
class TimeTicks;
class SequencedTaskRunner;
}  // namespace base

namespace update_client {

class Configurator;
struct UpdateContext;

// Handles updates for a group of components. Updates for different groups
// are run concurrently but within the same group of components, updates are
// applied one at a time.
class UpdateEngine {
 public:
  using Callback = base::Callback<void(Error error)>;
  using NotifyObserversCallback =
      base::Callback<void(UpdateClient::Observer::Events event,
                          const std::string& id)>;
  using CrxDataCallback = UpdateClient::CrxDataCallback;

  UpdateEngine(const scoped_refptr<Configurator>& config,
               UpdateChecker::Factory update_checker_factory,
               CrxDownloader::Factory crx_downloader_factory,
               PingManager* ping_manager,
               const NotifyObserversCallback& notify_observers_callback);
  ~UpdateEngine();

  bool GetUpdateState(const std::string& id, CrxUpdateItem* update_state);

  void Update(bool is_foreground,
              const std::vector<std::string>& ids,
              const UpdateClient::CrxDataCallback& crx_data_callback,
              const Callback& update_callback);

  void SendUninstallPing(const std::string& id,
                         const base::Version& version,
                         int reason,
                         const Callback& update_callback);

 private:
  using UpdateContexts = std::set<std::unique_ptr<UpdateContext>>;
  using UpdateContextIterator = UpdateContexts::iterator;

  void UpdateComplete(const UpdateContextIterator& it, Error error);

  void ComponentCheckingForUpdatesStart(const UpdateContextIterator& it,
                                        const Component& component);
  void ComponentCheckingForUpdatesComplete(const UpdateContextIterator& it,
                                           const Component& component);
  void UpdateCheckComplete(const UpdateContextIterator& it);

  void DoUpdateCheck(const UpdateContextIterator& it);
  void UpdateCheckDone(const UpdateContextIterator& it,
                       int error,
                       int retry_after_sec);

  void HandleComponent(const UpdateContextIterator& it);
  void HandleComponentComplete(const UpdateContextIterator& it);

  // Returns true if the update engine rejects this update call because it
  // occurs too soon.
  bool IsThrottled(bool is_foreground) const;

  // base::TimeDelta GetNextUpdateDelay(const Component& component) const;

  base::ThreadChecker thread_checker_;

  scoped_refptr<Configurator> config_;

  UpdateChecker::Factory update_checker_factory_;
  CrxDownloader::Factory crx_downloader_factory_;

  // TODO(sorin): refactor as a ref counted class.
  PingManager* ping_manager_;  // Not owned by this class.

  std::unique_ptr<PersistedData> metadata_;

  // Called when CRX state changes occur.
  const NotifyObserversCallback notify_observers_callback_;

  // Contains the contexts associated with each update in progress.
  UpdateContexts update_contexts_;

  // Implements a rate limiting mechanism for background update checks. Has the
  // effect of rejecting the update call if the update call occurs before
  // a certain time, which is negotiated with the server as part of the
  // update protocol. See the comments for X-Retry-After header.
  base::TimeTicks throttle_updates_until_;

  DISALLOW_COPY_AND_ASSIGN(UpdateEngine);
};

// TODO(sorin): consider making this a ref counted type.
struct UpdateContext {
  UpdateContext(
      const scoped_refptr<Configurator>& config,
      bool is_foreground,
      const std::vector<std::string>& ids,
      const UpdateClient::CrxDataCallback& crx_data_callback,
      const UpdateEngine::NotifyObserversCallback& notify_observers_callback,
      const UpdateEngine::Callback& callback,
      CrxDownloader::Factory crx_downloader_factory);

  ~UpdateContext();

  scoped_refptr<Configurator> config;

  // True if this update has been initiated by the user.
  bool is_foreground = false;

  // True if the component updates are enabled in this context.
  const bool enabled_component_updates;

  // Contains the ids of all CRXs in this context.
  const std::vector<std::string> ids;

  // Called before an update check, when update metadata is needed.
  const UpdateEngine::CrxDataCallback& crx_data_callback;

  // Called when there is a state change for any update in this context.
  const UpdateEngine::NotifyObserversCallback notify_observers_callback;

  // Called when the all updates associated with this context have completed.
  const UpdateEngine::Callback callback;

  // Runs tasks in a blocking thread pool.
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner;

  // Creates instances of CrxDownloader;
  CrxDownloader::Factory crx_downloader_factory;

  std::unique_ptr<UpdateChecker> update_checker;

  // The time in seconds to wait until doing further update checks.
  int retry_after_sec = 0;

  int update_check_error = 0;
  size_t num_components_ready_to_check = 0;
  size_t num_components_checked = 0;

  IdToComponentPtrMap components;

  std::queue<std::string> component_queue;

  // The time to wait before handling the update for a component.
  // The wait time is proportional with the cost incurred by updating
  // the component. The more time it takes to download and apply the
  // update for the current component, the longer the wait until the engine
  // is handling the next component in the queue.
  base::TimeDelta next_update_delay;

 private:
  DISALLOW_COPY_AND_ASSIGN(UpdateContext);
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_UPDATE_ENGINE_H_
