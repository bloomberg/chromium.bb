// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/heap_snapshot_taker.h"

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/devtools_client.h"
#include "chrome/test/chromedriver/chrome/status.h"

HeapSnapshotTaker::HeapSnapshotTaker(DevToolsClient* client)
    : client_(client), snapshot_uid_(-1), finished_(false) {
  client_->AddListener(this);
}

HeapSnapshotTaker::~HeapSnapshotTaker() {}

Status HeapSnapshotTaker::TakeSnapshot(scoped_ptr<base::Value>* snapshot) {
  Status status1 = TakeSnapshotInternal();
  Status status2(kOk);
  if (snapshot_uid_ != -1) {  // Clear the snapshot cached in chrome.
    base::DictionaryValue params;
    status2 = client_->SendCommand("HeapProfiler.clearProfiles", params);
  }

  Status status3(kOk);
  if (status1.IsOk() && status2.IsOk()) {
    scoped_ptr<base::Value> value(base::JSONReader::Read(snapshot_));
    if (!value)
      status3 = Status(kUnknownError, "heap snapshot not in JSON format");
    else
      *snapshot = value.Pass();
  }
  snapshot_uid_ = -1;
  finished_ = false;
  snapshot_.clear();
  if (status1.IsError())
    return status1;
  else if (status2.IsError())
    return status2;
  else
    return status3;
}

Status HeapSnapshotTaker::TakeSnapshotInternal() {
  if (snapshot_uid_ != -1)
    return Status(kUnknownError, "unexpected heap snapshot was triggered");

  base::DictionaryValue params;
  Status status = client_->SendCommand("HeapProfiler.collectGarbage", params);
  if (status.IsError())
    return status;

  status = client_->SendCommand("HeapProfiler.takeHeapSnapshot", params);
  if (status.IsError())
    return status;

  if (snapshot_uid_ == -1)
    return Status(kUnknownError, "failed to receive snapshot uid");

  base::DictionaryValue uid_params;
  uid_params.SetInteger("uid", snapshot_uid_);
  status = client_->SendCommand("HeapProfiler.getHeapSnapshot", uid_params);
  if (status.IsError())
    return status;

  if (!finished_)
    return Status(kUnknownError, "failed to retrieve all heap snapshot data");

  return Status(kOk);
}

Status HeapSnapshotTaker::OnConnected(DevToolsClient* client) {
  base::DictionaryValue params;
  return client_->SendCommand("Debugger.enable", params);
}

Status HeapSnapshotTaker::OnEvent(DevToolsClient* client,
                                  const std::string& method,
                                  const base::DictionaryValue& params) {
  if (method == "HeapProfiler.addProfileHeader") {
    if (snapshot_uid_ != -1) {
      LOG(WARNING) << "multiple heap snapshot triggered";
    } else if (!params.GetInteger("header.uid", &snapshot_uid_)) {
      return Status(kUnknownError,
                    "HeapProfiler.addProfileHeader has invalid 'header.uid'");
    }
  } else if (method == "HeapProfiler.addHeapSnapshotChunk") {
    int uid = -1;
    if (!params.GetInteger("uid", &uid)) {
      return Status(kUnknownError,
                    "HeapProfiler.addHeapSnapshotChunk has no 'uid'");
    } else if (uid == snapshot_uid_) {
      std::string chunk;
      if (!params.GetString("chunk", &chunk)) {
        return Status(kUnknownError,
                      "HeapProfiler.addHeapSnapshotChunk has no 'chunk'");
      }

      snapshot_.append(chunk);
    } else {
      LOG(WARNING) << "expect chunk event uid " << snapshot_uid_
                   << ", but got " << uid;
    }
  } else if (method == "HeapProfiler.finishHeapSnapshot") {
    int uid = -1;
    if (!params.GetInteger("uid", &uid)) {
      return Status(kUnknownError,
                    "HeapProfiler.finishHeapSnapshot has no 'uid'");
    } else if (uid == snapshot_uid_) {
      finished_ = true;
    } else {
      LOG(WARNING) << "expect finish event uid " << snapshot_uid_
                   << ", but got " << uid;
    }
  }
  return Status(kOk);
}
