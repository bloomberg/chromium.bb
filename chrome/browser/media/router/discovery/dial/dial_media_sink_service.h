// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DIAL_DIAL_MEDIA_SINK_SERVICE_H_
#define CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DIAL_DIAL_MEDIA_SINK_SERVICE_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner_helpers.h"
#include "chrome/common/media_router/discovery/media_sink_internal.h"
#include "chrome/common/media_router/discovery/media_sink_service_util.h"
#include "content/public/browser/browser_thread.h"
#include "net/url_request/url_request_context_getter.h"

namespace net {
class URLRequestContextGetter;
}

namespace media_router {

class DialMediaSinkServiceImpl;

using OnDialSinkAddedCallback =
    base::RepeatingCallback<void(const MediaSinkInternal&)>;

// Service to discover DIAL media sinks.  All public methods must be invoked on
// the UI thread.  Delegates to DialMediaSinkServiceImpl by posting tasks to its
// SequencedTaskRunner.
class DialMediaSinkService {
 public:
  explicit DialMediaSinkService(
      const scoped_refptr<net::URLRequestContextGetter>& request_context);
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

 private:
  friend class DialMediaSinkServiceTest;

  // Marked virtual for tests.
  virtual std::unique_ptr<DialMediaSinkServiceImpl, base::OnTaskRunnerDeleter>
  CreateImpl(
      const OnSinksDiscoveredCallback& sink_discovery_cb,
      const OnDialSinkAddedCallback& dial_sink_added_cb,
      const scoped_refptr<net::URLRequestContextGetter>& request_context);

  void RunSinksDiscoveredCallback(
      const OnSinksDiscoveredCallback& sinks_discovered_cb,
      std::vector<MediaSinkInternal> sinks);

  // Created on the UI thread, used and destroyed on its SequencedTaskRunner.
  std::unique_ptr<DialMediaSinkServiceImpl, base::OnTaskRunnerDeleter> impl_;

  // Passed to |impl_| when |Start| is called.
  scoped_refptr<net::URLRequestContextGetter> request_context_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<DialMediaSinkService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DialMediaSinkService);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DIAL_DIAL_MEDIA_SINK_SERVICE_H_
