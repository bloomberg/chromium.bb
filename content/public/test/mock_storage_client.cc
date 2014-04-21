// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/mock_storage_client.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/stl_util.h"
#include "net/base/net_util.h"
#include "webkit/browser/quota/quota_manager_proxy.h"

using quota::kQuotaErrorInvalidModification;
using quota::kQuotaStatusOk;

namespace content {

using std::make_pair;

MockStorageClient::MockStorageClient(
    QuotaManagerProxy* quota_manager_proxy,
    const MockOriginData* mock_data, QuotaClient::ID id, size_t mock_data_size)
    : quota_manager_proxy_(quota_manager_proxy),
      id_(id),
      mock_time_counter_(0),
      weak_factory_(this) {
  Populate(mock_data, mock_data_size);
}

void MockStorageClient::Populate(
    const MockOriginData* mock_data,
    size_t mock_data_size) {
  for (size_t i = 0; i < mock_data_size; ++i) {
    origin_data_[make_pair(GURL(mock_data[i].origin), mock_data[i].type)] =
        mock_data[i].usage;
  }
}

MockStorageClient::~MockStorageClient() {}

void MockStorageClient::AddOriginAndNotify(
    const GURL& origin_url, StorageType type, int64 size) {
  DCHECK(origin_data_.find(make_pair(origin_url, type)) == origin_data_.end());
  DCHECK_GE(size, 0);
  origin_data_[make_pair(origin_url, type)] = size;
  quota_manager_proxy_->quota_manager()->NotifyStorageModifiedInternal(
      id(), origin_url, type, size, IncrementMockTime());
}

void MockStorageClient::ModifyOriginAndNotify(
    const GURL& origin_url, StorageType type, int64 delta) {
  OriginDataMap::iterator find = origin_data_.find(make_pair(origin_url, type));
  DCHECK(find != origin_data_.end());
  find->second += delta;
  DCHECK_GE(find->second, 0);

  // TODO(tzik): Check quota to prevent usage exceed
  quota_manager_proxy_->quota_manager()->NotifyStorageModifiedInternal(
      id(), origin_url, type, delta, IncrementMockTime());
}

void MockStorageClient::TouchAllOriginsAndNotify() {
  for (OriginDataMap::const_iterator itr = origin_data_.begin();
       itr != origin_data_.end();
       ++itr) {
    quota_manager_proxy_->quota_manager()->NotifyStorageModifiedInternal(
        id(), itr->first.first, itr->first.second, 0, IncrementMockTime());
  }
}

void MockStorageClient::AddOriginToErrorSet(
    const GURL& origin_url, StorageType type) {
  error_origins_.insert(make_pair(origin_url, type));
}

base::Time MockStorageClient::IncrementMockTime() {
  ++mock_time_counter_;
  return base::Time::FromDoubleT(mock_time_counter_ * 10.0);
}

QuotaClient::ID MockStorageClient::id() const {
  return id_;
}

void MockStorageClient::OnQuotaManagerDestroyed() {
  delete this;
}

void MockStorageClient::GetOriginUsage(const GURL& origin_url,
                                       StorageType type,
                                       const GetUsageCallback& callback) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(&MockStorageClient::RunGetOriginUsage,
                 weak_factory_.GetWeakPtr(), origin_url, type, callback));
}

void MockStorageClient::GetOriginsForType(
    StorageType type, const GetOriginsCallback& callback) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(&MockStorageClient::RunGetOriginsForType,
                 weak_factory_.GetWeakPtr(), type, callback));
}

void MockStorageClient::GetOriginsForHost(
    StorageType type, const std::string& host,
    const GetOriginsCallback& callback) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(&MockStorageClient::RunGetOriginsForHost,
                 weak_factory_.GetWeakPtr(), type, host, callback));
}

void MockStorageClient::DeleteOriginData(
    const GURL& origin, StorageType type,
    const DeletionCallback& callback) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(&MockStorageClient::RunDeleteOriginData,
                 weak_factory_.GetWeakPtr(), origin, type, callback));
}

bool MockStorageClient::DoesSupport(quota::StorageType type) const {
  return true;
}

void MockStorageClient::RunGetOriginUsage(
    const GURL& origin_url, StorageType type,
    const GetUsageCallback& callback) {
  OriginDataMap::iterator find = origin_data_.find(make_pair(origin_url, type));
  if (find == origin_data_.end()) {
    callback.Run(0);
  } else {
    callback.Run(find->second);
  }
}

void MockStorageClient::RunGetOriginsForType(
    StorageType type, const GetOriginsCallback& callback) {
  std::set<GURL> origins;
  for (OriginDataMap::iterator iter = origin_data_.begin();
       iter != origin_data_.end(); ++iter) {
    if (type == iter->first.second)
      origins.insert(iter->first.first);
  }
  callback.Run(origins);
}

void MockStorageClient::RunGetOriginsForHost(
    StorageType type, const std::string& host,
    const GetOriginsCallback& callback) {
  std::set<GURL> origins;
  for (OriginDataMap::iterator iter = origin_data_.begin();
       iter != origin_data_.end(); ++iter) {
    std::string host_or_spec = net::GetHostOrSpecFromURL(iter->first.first);
    if (type == iter->first.second && host == host_or_spec)
      origins.insert(iter->first.first);
  }
  callback.Run(origins);
}

void MockStorageClient::RunDeleteOriginData(
    const GURL& origin_url,
    StorageType type,
    const DeletionCallback& callback) {
  ErrorOriginSet::iterator itr_error =
      error_origins_.find(make_pair(origin_url, type));
  if (itr_error != error_origins_.end()) {
    callback.Run(kQuotaErrorInvalidModification);
    return;
  }

  OriginDataMap::iterator itr =
      origin_data_.find(make_pair(origin_url, type));
  if (itr != origin_data_.end()) {
    int64 delta = itr->second;
    quota_manager_proxy_->
        NotifyStorageModified(id(), origin_url, type, -delta);
    origin_data_.erase(itr);
  }

  callback.Run(kQuotaStatusOk);
}

}  // namespace content
