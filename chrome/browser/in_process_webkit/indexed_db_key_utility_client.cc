// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/browser/in_process_webkit/indexed_db_key_utility_client.h"

#include "chrome/browser/browser_process.h"
#include "chrome/common/indexed_db_key.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/serialized_script_value.h"

IndexedDBKeyUtilityClient::IndexedDBKeyUtilityClient()
    : waitable_event_(false, false),
      state_(STATE_UNINITIALIZED),
      utility_process_host_(NULL) {
}

IndexedDBKeyUtilityClient::~IndexedDBKeyUtilityClient() {
  DCHECK(state_ == STATE_UNINITIALIZED || state_ == STATE_SHUTDOWN);
  DCHECK(!utility_process_host_);
  DCHECK(!client_.get());
}

void IndexedDBKeyUtilityClient::StartUtilityProcess() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::WEBKIT));
  DCHECK(state_ == STATE_UNINITIALIZED);

  GetRDHAndStartUtilityProcess();
  bool ret = waitable_event_.Wait();

  DCHECK(ret && state_ == STATE_INITIALIZED);
}

void IndexedDBKeyUtilityClient::EndUtilityProcess() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::WEBKIT));
  DCHECK(state_ == STATE_INITIALIZED);

  EndUtilityProcessInternal();
  bool ret = waitable_event_.Wait();

  DCHECK(ret && state_ == STATE_SHUTDOWN);
}

void IndexedDBKeyUtilityClient::CreateIDBKeysFromSerializedValuesAndKeyPath(
    const std::vector<SerializedScriptValue>& values,
    const string16& key_path,
    std::vector<IndexedDBKey>* keys) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::WEBKIT));
  DCHECK(state_ == STATE_INITIALIZED);

  state_ = STATE_CREATING_KEYS;
  CallStartIDBKeyFromValueAndKeyPathFromIOThread(values, key_path);
  bool ret = waitable_event_.Wait();
  DCHECK(ret && state_ == STATE_INITIALIZED);

  *keys = keys_;
}

void IndexedDBKeyUtilityClient::GetRDHAndStartUtilityProcess() {
  // In order to start the UtilityProcess, we need to grab
  // a pointer to the ResourceDispatcherHost. This can only
  // be done on the UI thread. See the comment at the top of
  // browser_process.h
  if (!ChromeThread::CurrentlyOn(ChromeThread::UI)) {
    ChromeThread::PostTask(
        ChromeThread::UI, FROM_HERE,
        NewRunnableMethod(
            this,
            &IndexedDBKeyUtilityClient::GetRDHAndStartUtilityProcess));
    return;
  }
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  StartUtilityProcessInternal(g_browser_process->resource_dispatcher_host());
}

void IndexedDBKeyUtilityClient::StartUtilityProcessInternal(
    ResourceDispatcherHost* rdh) {
  DCHECK(rdh);
  // The ResourceDispatcherHost can only be used on the IO thread.
  if (!ChromeThread::CurrentlyOn(ChromeThread::IO)) {
    DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
    ChromeThread::PostTask(
        ChromeThread::IO, FROM_HERE,
        NewRunnableMethod(
            this,
            &IndexedDBKeyUtilityClient::StartUtilityProcessInternal,
            rdh));
    return;
  }
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  DCHECK(state_ == STATE_UNINITIALIZED);

  client_ = new IndexedDBKeyUtilityClient::Client(this);
  utility_process_host_ = new UtilityProcessHost(
      rdh, client_.get(), ChromeThread::IO);
  utility_process_host_->StartBatchMode();
  state_ = STATE_INITIALIZED;
  waitable_event_.Signal();
}

void IndexedDBKeyUtilityClient::EndUtilityProcessInternal() {
  if (!ChromeThread::CurrentlyOn(ChromeThread::IO)) {
    ChromeThread::PostTask(
        ChromeThread::IO, FROM_HERE,
        NewRunnableMethod(
            this,
            &IndexedDBKeyUtilityClient::EndUtilityProcessInternal));
    return;
  }

  utility_process_host_->EndBatchMode();
  utility_process_host_ = NULL;
  client_ = NULL;
  state_ = STATE_SHUTDOWN;
  waitable_event_.Signal();
}

void IndexedDBKeyUtilityClient::CallStartIDBKeyFromValueAndKeyPathFromIOThread(
    const std::vector<SerializedScriptValue>& values,
    const string16& key_path) {
  if (!ChromeThread::CurrentlyOn(ChromeThread::IO)) {
    ChromeThread::PostTask(
        ChromeThread::IO, FROM_HERE,
        NewRunnableMethod(this,
            &IndexedDBKeyUtilityClient::
                CallStartIDBKeyFromValueAndKeyPathFromIOThread,
            values, key_path));
    return;
  }

  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  utility_process_host_->StartIDBKeysFromValuesAndKeyPath(
      0, values, key_path);
}

void IndexedDBKeyUtilityClient::SetKeys(const std::vector<IndexedDBKey>& keys) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  keys_ = keys;
}

void IndexedDBKeyUtilityClient::FinishCreatingKeys() {
  DCHECK(state_ == STATE_CREATING_KEYS);
  state_ = STATE_INITIALIZED;
  waitable_event_.Signal();
}

IndexedDBKeyUtilityClient::Client::Client(IndexedDBKeyUtilityClient* parent)
    : parent_(parent) {
}

void IndexedDBKeyUtilityClient::Client::OnProcessCrashed() {
  if (parent_->state_ == STATE_CREATING_KEYS)
    parent_->FinishCreatingKeys();
}

void IndexedDBKeyUtilityClient::Client::OnIDBKeysFromValuesAndKeyPathSucceeded(
    int id, const std::vector<IndexedDBKey>& keys) {
  parent_->SetKeys(keys);
  parent_->FinishCreatingKeys();
}

void IndexedDBKeyUtilityClient::Client::OnIDBKeysFromValuesAndKeyPathFailed(
    int id) {
  parent_->FinishCreatingKeys();
}
