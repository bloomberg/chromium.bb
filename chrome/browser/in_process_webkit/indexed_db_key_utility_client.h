// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IN_PROCESS_WEBKIT_INDEXED_DB_KEY_UTILITY_CLIENT_H_
#define CHROME_BROWSER_IN_PROCESS_WEBKIT_INDEXED_DB_KEY_UTILITY_CLIENT_H_
#pragma once

#include "base/ref_counted.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/utility_process_host.h"

class IndexedDBKey;
class SerializedScriptValue;

// This class is responsible to obtain IndexedDBKeys from the
// SerializedScriptValues given an IDBKeyPath. It uses UtilityProcess to do this
// inside a sandbox (a V8 lock is required there). At this level, all methods
// are synchronous as required by the caller. The public API is used on
// WEBKIT thread, but internally it moves around to UI and IO as needed.
class IndexedDBKeyUtilityClient
    : public base::RefCountedThreadSafe<IndexedDBKeyUtilityClient> {
 public:
  IndexedDBKeyUtilityClient();

  // Starts the UtilityProcess. Must be called before any other method.
  void StartUtilityProcess();

  // Ends the UtilityProcess. Must be called after StartUtilityProcess() and
  // before destruction.
  // TODO(bulach): figure out an appropriate hook so that we can keep the
  // UtilityProcess running for a longer period of time and avoid spinning it
  // on every IDBObjectStore::Put call.
  void EndUtilityProcess();

  // Synchronously obtain the |keys| from |values| for the given |key_path|.
  void CreateIDBKeysFromSerializedValuesAndKeyPath(
      const std::vector<SerializedScriptValue>& values,
      const string16& key_path,
      std::vector<IndexedDBKey>* keys);

 private:
  class Client : public UtilityProcessHost::Client {
   public:
    explicit Client(IndexedDBKeyUtilityClient* parent);

    // UtilityProcessHost::Client
    virtual void OnProcessCrashed(int exit_code);
    virtual void OnIDBKeysFromValuesAndKeyPathSucceeded(
        int id, const std::vector<IndexedDBKey>& keys);
    virtual void OnIDBKeysFromValuesAndKeyPathFailed(int id);

   private:
    IndexedDBKeyUtilityClient* parent_;

    DISALLOW_COPY_AND_ASSIGN(Client);
  };

  friend class base::RefCountedThreadSafe<IndexedDBKeyUtilityClient>;
  ~IndexedDBKeyUtilityClient();

  void GetRDHAndStartUtilityProcess();
  void StartUtilityProcessInternal(ResourceDispatcherHost* rdh);
  void EndUtilityProcessInternal();
  void CallStartIDBKeyFromValueAndKeyPathFromIOThread(
      const std::vector<SerializedScriptValue>& values,
      const string16& key_path);

  void SetKeys(const std::vector<IndexedDBKey>& keys);
  void FinishCreatingKeys();

  base::WaitableEvent waitable_event_;

  // Used in both IO and WEBKIT threads, but guarded by WaitableEvent, i.e.,
  // these members are only set / read when the other thread is blocked.
  enum State {
    STATE_UNINITIALIZED,
    STATE_INITIALIZED,
    STATE_CREATING_KEYS,
    STATE_SHUTDOWN,
  };
  State state_;
  std::vector<IndexedDBKey> keys_;

  // Used in the IO thread.
  UtilityProcessHost* utility_process_host_;
  scoped_refptr<Client> client_;

  DISALLOW_COPY_AND_ASSIGN(IndexedDBKeyUtilityClient);
};

#endif  // CHROME_BROWSER_IN_PROCESS_WEBKIT_INDEXED_DB_KEY_UTILITY_CLIENT_H_
