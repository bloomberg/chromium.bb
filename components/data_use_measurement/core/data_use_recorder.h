// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_USE_MEASUREMENT_CORE_DATA_USE_RECORDER_H_
#define COMPONENTS_DATA_USE_MEASUREMENT_CORE_DATA_USE_RECORDER_H_

#include <stdint.h>

#include "base/containers/hash_tables.h"
#include "base/macros.h"
#include "base/supports_user_data.h"
#include "components/data_use_measurement/core/data_use.h"
#include "net/base/net_export.h"

namespace net {
class URLRequest;
}

namespace data_use_measurement {

// Tracks all network data used by a single high level entity. An entity
// can make multiple URLRequests, so there is a one:many relationship between
// DataUseRecorders and URLRequests. Data used by each URLRequest will be
// tracked by exactly one DataUseRecorder.
class DataUseRecorder {
 public:
  DataUseRecorder();
  virtual ~DataUseRecorder();

  // Returns the actual data used by the entity being tracked.
  DataUse& data_use() { return data_use_; }
  const base::hash_set<net::URLRequest*>& pending_url_requests() const {
    return pending_url_requests_;
  }
  const net::URLRequest* main_url_request() const { return main_url_request_; }

  void set_main_url_request(const net::URLRequest* request) {
    main_url_request_ = request;
  }

  bool is_visible() const { return is_visible_; }

  void set_is_visible(bool is_visible) { is_visible_ = is_visible; }

  // Returns whether data use is complete and no additional data can be used
  // by the entity tracked by this recorder. For example,
  bool IsDataUseComplete();

  // Adds |request| to the list of pending URLRequests that ascribe data use to
  // this recorder.
  void AddPendingURLRequest(net::URLRequest* request);

  // Clears the list of pending URLRequests that ascribe data use to this
  // recorder.
  void RemoveAllPendingURLRequests();

  // Returns whether there are any pending URLRequests whose data use is tracked
  // by this DataUseRecorder.
  bool HasPendingURLRequest(net::URLRequest* request);

  // Merge another DataUseRecorder to this instance.
  void MergeFrom(DataUseRecorder* other);

 private:
  friend class DataUseAscriber;

  // Methods for tracking data use sources. These sources can initiate
  // URLRequests directly or indirectly. The entity whose data use is being
  // tracked by this recorder may comprise of sub-entities each of which use
  // network data. These helper methods help track these sub-entities.
  // A recorder will not be marked as having completed data use as long as it
  // has pending data sources.
  void AddPendingDataSource(void* source);
  bool HasPendingDataSource(void* source);
  void RemovePendingDataSource(void* source);

  // Network Delegate methods:
  void OnBeforeUrlRequest(net::URLRequest* request);
  void OnUrlRequestDestroyed(net::URLRequest* request);
  void OnNetworkBytesSent(net::URLRequest* request, int64_t bytes_sent);
  void OnNetworkBytesReceived(net::URLRequest* request, int64_t bytes_received);

  // Pending URLRequests whose data is being tracked by this DataUseRecorder.
  base::hash_set<net::URLRequest*> pending_url_requests_;

  // Data sources other than URLRequests, whose data is being tracked by this
  // DataUseRecorder.
  base::hash_set<const void*> pending_data_sources_;

  // The main frame URLRequest for page loads. Null if this is not tracking a
  // page load.
  const net::URLRequest* main_url_request_;

  // The network data use measured by this DataUseRecorder.
  DataUse data_use_;

  // Whether the entity that owns this data use is currently visible.
  bool is_visible_;

  DISALLOW_COPY_AND_ASSIGN(DataUseRecorder);
};

}  // namespace data_use_measurement

#endif  // COMPONENTS_DATA_USE_MEASUREMENT_CORE_DATA_USE_RECORDER_H_
