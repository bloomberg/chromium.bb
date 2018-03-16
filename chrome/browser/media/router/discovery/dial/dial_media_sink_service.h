// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DIAL_DIAL_MEDIA_SINK_SERVICE_H_
#define CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DIAL_DIAL_MEDIA_SINK_SERVICE_H_

#include <memory>

#include "base/callback_list.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "chrome/common/media_router/discovery/media_sink_internal.h"
#include "chrome/common/media_router/discovery/media_sink_service_util.h"
#include "url/origin.h"

namespace media_router {

class DialMediaSinkServiceImpl;

using OnDialSinkAddedCallback =
    base::RepeatingCallback<void(const MediaSinkInternal&)>;

// Service to discover DIAL media sinks.  All public methods must be invoked on
// the UI thread.  Delegates to DialMediaSinkServiceImpl by posting tasks to its
// SequencedTaskRunner.
class DialMediaSinkService {
 public:
  // Callback is invoked when available sinks for |app_name| changes.
  // |app_name|: app name, e.g. YouTube.
  // |available_sinks|: media sinks on which app with |app_name| is available.
  using SinkQueryByAppFunc =
      void(const std::string& app_name,
           const std::vector<MediaSinkInternal>& available_sinks);
  using SinkQueryByAppCallback = base::RepeatingCallback<SinkQueryByAppFunc>;
  using SinkQueryByAppCallbackList = base::CallbackList<SinkQueryByAppFunc>;
  using SinkQueryByAppSubscription =
      std::unique_ptr<SinkQueryByAppCallbackList::Subscription>;

  DialMediaSinkService();
  virtual ~DialMediaSinkService();

  // Starts discovery of DIAL sinks. Can only be called once.
  // |sink_discovery_cb|: Callback to invoke on UI thread when the list of
  // discovered sinks has been updated.
  // |dial_sink_added_cb|: Callback to invoke when a new DIAL sink has been
  // discovered. The callback may be invoked on any thread, and may be invoked
  // after |this| is destroyed. Can be null.
  // Marked virtual for tests.
  virtual void Start(const OnSinksDiscoveredCallback& sink_discovery_cb,
                     const OnDialSinkAddedCallback& dial_sink_added_cb);

  // Initiates discovery immediately in response to a user gesture
  // (i.e., opening the Media Router dialog). This method can only be called
  // after |Start()|.
  // TODO(imcheng): Rename to ManuallyInitiateDiscovery() or similar.
  // Marked virtual for tests.
  virtual void OnUserGesture();

  // Registers |callback| to callback list entry in |sink_queries_|, with the
  // key |app_name|. Returns a unique_ptr of callback list subscription. Caller
  // owns the returned subscription and is responsible for destroying when it
  // wants to unregister |callback|.
  virtual SinkQueryByAppSubscription StartMonitoringAvailableSinksForApp(
      const std::string& app_name,
      const SinkQueryByAppCallback& callback);

  // Returns cached available sinks for |app_name|.
  virtual std::vector<MediaSinkInternal> GetCachedAvailableSinks(
      const std::string& app_name);

 private:
  friend class DialMediaSinkServiceTest;

  struct SinkListByAppName {
    SinkListByAppName();
    ~SinkListByAppName();

    // Keeps track of callbacks, which could come from different profiles.
    SinkQueryByAppCallbackList callbacks;

    std::vector<MediaSinkInternal> cached_sinks;

    DISALLOW_COPY_AND_ASSIGN(SinkListByAppName);
  };

  // Marked virtual for tests.
  virtual std::unique_ptr<DialMediaSinkServiceImpl, base::OnTaskRunnerDeleter>
  CreateImpl(const OnSinksDiscoveredCallback& sink_discovery_cb,
             const OnDialSinkAddedCallback& dial_sink_added_cb,
             const OnAvailableSinksUpdatedCallback& available_sinks_updated_cb);

  void RunSinksDiscoveredCallback(
      const OnSinksDiscoveredCallback& sinks_discovered_cb,
      std::vector<MediaSinkInternal> sinks);

  void RunAvailableSinksUpdatedCallback(
      const std::string& app_name,
      std::vector<MediaSinkInternal> available_sinks);

  // Invoked when callback subscription returned by
  // |StartMonitoringAvailableSinksForApp| is destroyed by the caller.
  void OnAvailableSinksUpdatedCallbackRemoved(const std::string& app_name);

  // Created on the UI thread, used and destroyed on its SequencedTaskRunner.
  std::unique_ptr<DialMediaSinkServiceImpl, base::OnTaskRunnerDeleter> impl_;

  // Map of available sinks and sink query callbacks, keyed by app name.
  base::flat_map<std::string, std::unique_ptr<SinkListByAppName>>
      sinks_by_app_name_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<DialMediaSinkService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DialMediaSinkService);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DIAL_DIAL_MEDIA_SINK_SERVICE_H_
