// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/in_process_webkit/indexed_db_key_utility_client.h"

#include "base/lazy_instance.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/utility_process_host.h"
#include "chrome/common/indexed_db_key.h"
#include "chrome/common/serialized_script_value.h"

// This class is used to obtain IndexedDBKeys from SerializedScriptValues
// given an IDBKeyPath. It uses UtilityProcess to do this inside a sandbox
// (a V8 lock is required there). At this level, all methods are synchronous
// as required by the caller. The public API is used on WEBKIT thread,
// but internally it moves around to UI and IO as needed.
class KeyUtilityClientImpl
    : public base::RefCountedThreadSafe<KeyUtilityClientImpl> {
 public:
  KeyUtilityClientImpl();

  // Starts the UtilityProcess. Must be called before any other method.
  void StartUtilityProcess();

  // Stops the UtilityProcess. No further keys can be created after this.
  void Shutdown();

  // Synchronously obtain the |keys| from |values| for the given |key_path|.
  void CreateIDBKeysFromSerializedValuesAndKeyPath(
      const std::vector<SerializedScriptValue>& values,
      const string16& key_path,
      std::vector<IndexedDBKey>* keys);

  // Synchronously inject |key| into |value| using the given |key_path|,
  // returning the new value.
  SerializedScriptValue InjectIDBKeyIntoSerializedValue(
      const IndexedDBKey& key,
      const SerializedScriptValue& value,
      const string16& key_path);

 private:
  class Client : public UtilityProcessHost::Client {
   public:
    explicit Client(KeyUtilityClientImpl* parent);

    // UtilityProcessHost::Client
    virtual void OnProcessCrashed(int exit_code);
    virtual void OnIDBKeysFromValuesAndKeyPathSucceeded(
        int id, const std::vector<IndexedDBKey>& keys);
    virtual void OnIDBKeysFromValuesAndKeyPathFailed(int id);
    virtual void OnInjectIDBKeyFinished(const SerializedScriptValue& value);

   private:
    KeyUtilityClientImpl* parent_;

    DISALLOW_COPY_AND_ASSIGN(Client);
  };

  friend class base::RefCountedThreadSafe<KeyUtilityClientImpl>;
  ~KeyUtilityClientImpl();

  void GetRDHAndStartUtilityProcess();
  void StartUtilityProcessInternal(ResourceDispatcherHost* rdh);
  void EndUtilityProcessInternal();
  void CallStartIDBKeyFromValueAndKeyPathFromIOThread(
      const std::vector<SerializedScriptValue>& values,
      const string16& key_path);
  void CallStartInjectIDBKeyFromIOThread(
      const IndexedDBKey& key,
      const SerializedScriptValue& value,
      const string16& key_path);

  void SetKeys(const std::vector<IndexedDBKey>& keys);
  void FinishCreatingKeys();
  void SetValueAfterInjection(const SerializedScriptValue& value);
  void FinishInjectingKey();

  base::WaitableEvent waitable_event_;

  // Used in both IO and WEBKIT threads, but guarded by WaitableEvent, i.e.,
  // these members are only set / read when the other thread is blocked.
  enum State {
    STATE_UNINITIALIZED,
    STATE_INITIALIZED,
    STATE_CREATING_KEYS,
    STATE_INJECTING_KEY,
    STATE_SHUTDOWN,
  };
  State state_;
  std::vector<IndexedDBKey> keys_;
  SerializedScriptValue value_after_injection_;

  // Used in the IO thread.
  UtilityProcessHost* utility_process_host_;
  scoped_refptr<Client> client_;

  DISALLOW_COPY_AND_ASSIGN(KeyUtilityClientImpl);
};

// IndexedDBKeyUtilityClient definitions.

static base::LazyInstance<IndexedDBKeyUtilityClient> client_instance(
    base::LINKER_INITIALIZED);

IndexedDBKeyUtilityClient::IndexedDBKeyUtilityClient()
    : is_shutdown_(false) {
  // Note that creating the impl_ object is deferred until it is first needed,
  // as this class can be constructed even though it never gets used.
}

IndexedDBKeyUtilityClient::~IndexedDBKeyUtilityClient() {
  DCHECK(!impl_ || is_shutdown_);
}

//  static
void IndexedDBKeyUtilityClient::Shutdown() {
  IndexedDBKeyUtilityClient* instance = client_instance.Pointer();
  if (!instance->impl_)
    return;

  instance->is_shutdown_ = true;
  instance->impl_->Shutdown();
}

//  static
void IndexedDBKeyUtilityClient::CreateIDBKeysFromSerializedValuesAndKeyPath(
      const std::vector<SerializedScriptValue>& values,
      const string16& key_path,
      std::vector<IndexedDBKey>* keys) {
  IndexedDBKeyUtilityClient* instance = client_instance.Pointer();

  if (instance->is_shutdown_) {
    keys->clear();
    return;
  }

  if (!instance->impl_) {
    instance->impl_ = new KeyUtilityClientImpl();
    instance->impl_->StartUtilityProcess();
  }

  instance->impl_->CreateIDBKeysFromSerializedValuesAndKeyPath(values, key_path,
                                                               keys);
}

//  static
SerializedScriptValue
IndexedDBKeyUtilityClient::InjectIDBKeyIntoSerializedValue(
    const IndexedDBKey& key, const SerializedScriptValue& value,
    const string16& key_path) {
  IndexedDBKeyUtilityClient* instance = client_instance.Pointer();

  if (instance->is_shutdown_)
    return SerializedScriptValue();

  if (!instance->impl_) {
    instance->impl_ = new KeyUtilityClientImpl();
    instance->impl_->StartUtilityProcess();
  }

  return instance->impl_->InjectIDBKeyIntoSerializedValue(key, value, key_path);
}



// KeyUtilityClientImpl definitions.

void KeyUtilityClientImpl::Shutdown() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  utility_process_host_->EndBatchMode();
  utility_process_host_ = NULL;
  client_ = NULL;
  state_ = STATE_SHUTDOWN;
}

KeyUtilityClientImpl::KeyUtilityClientImpl()
    : waitable_event_(false, false),
      state_(STATE_UNINITIALIZED),
      utility_process_host_(NULL) {
}

KeyUtilityClientImpl::~KeyUtilityClientImpl() {
  DCHECK(state_ == STATE_UNINITIALIZED || state_ == STATE_SHUTDOWN);
  DCHECK(!utility_process_host_);
  DCHECK(!client_.get());
}

void KeyUtilityClientImpl::StartUtilityProcess() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));
  DCHECK(state_ == STATE_UNINITIALIZED);

  GetRDHAndStartUtilityProcess();
  bool ret = waitable_event_.Wait();

  DCHECK(ret && state_ == STATE_INITIALIZED);
}

void KeyUtilityClientImpl::CreateIDBKeysFromSerializedValuesAndKeyPath(
    const std::vector<SerializedScriptValue>& values,
    const string16& key_path,
    std::vector<IndexedDBKey>* keys) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));
  if (state_ == STATE_SHUTDOWN) {
    keys->clear();
    return;
  }

  DCHECK(state_ == STATE_INITIALIZED);

  state_ = STATE_CREATING_KEYS;
  CallStartIDBKeyFromValueAndKeyPathFromIOThread(values, key_path);
  bool ret = waitable_event_.Wait();
  DCHECK(ret && state_ == STATE_INITIALIZED);

  *keys = keys_;
}

SerializedScriptValue KeyUtilityClientImpl::InjectIDBKeyIntoSerializedValue(
    const IndexedDBKey& key,
    const SerializedScriptValue& value,
    const string16& key_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));
  if (state_ == STATE_SHUTDOWN)
    return SerializedScriptValue();

  DCHECK(state_ == STATE_INITIALIZED);

  state_ = STATE_INJECTING_KEY;
  CallStartInjectIDBKeyFromIOThread(key, value, key_path);

  bool ret = waitable_event_.Wait();
  DCHECK(ret && state_ == STATE_INITIALIZED);

  return value_after_injection_;
}


void KeyUtilityClientImpl::GetRDHAndStartUtilityProcess() {
  // In order to start the UtilityProcess, we need to grab
  // a pointer to the ResourceDispatcherHost. This can only
  // be done on the UI thread. See the comment at the top of
  // browser_process.h
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(
            this,
            &KeyUtilityClientImpl::GetRDHAndStartUtilityProcess));
    return;
  }
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  StartUtilityProcessInternal(g_browser_process->resource_dispatcher_host());
}

void KeyUtilityClientImpl::StartUtilityProcessInternal(
    ResourceDispatcherHost* rdh) {
  DCHECK(rdh);
  // The ResourceDispatcherHost can only be used on the IO thread.
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        NewRunnableMethod(
            this,
            &KeyUtilityClientImpl::StartUtilityProcessInternal,
            rdh));
    return;
  }
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(state_ == STATE_UNINITIALIZED);

  client_ = new KeyUtilityClientImpl::Client(this);
  utility_process_host_ = new UtilityProcessHost(
      rdh, client_.get(), BrowserThread::IO);
  utility_process_host_->StartBatchMode();
  state_ = STATE_INITIALIZED;
  waitable_event_.Signal();
}

void KeyUtilityClientImpl::EndUtilityProcessInternal() {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        NewRunnableMethod(
            this,
            &KeyUtilityClientImpl::EndUtilityProcessInternal));
    return;
  }

  utility_process_host_->EndBatchMode();
  utility_process_host_ = NULL;
  client_ = NULL;
  state_ = STATE_SHUTDOWN;
  waitable_event_.Signal();
}

void KeyUtilityClientImpl::CallStartIDBKeyFromValueAndKeyPathFromIOThread(
    const std::vector<SerializedScriptValue>& values,
    const string16& key_path) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        NewRunnableMethod(this,
            &KeyUtilityClientImpl::
                CallStartIDBKeyFromValueAndKeyPathFromIOThread,
            values, key_path));
    return;
  }

  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  utility_process_host_->StartIDBKeysFromValuesAndKeyPath(
      0, values, key_path);
}

void KeyUtilityClientImpl::CallStartInjectIDBKeyFromIOThread(
    const IndexedDBKey& key,
    const SerializedScriptValue& value,
    const string16& key_path) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        NewRunnableMethod(this,
            &KeyUtilityClientImpl::
                CallStartInjectIDBKeyFromIOThread,
            key, value, key_path));
    return;
  }

  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  utility_process_host_->StartInjectIDBKey(key, value, key_path);
}

void KeyUtilityClientImpl::SetKeys(const std::vector<IndexedDBKey>& keys) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  keys_ = keys;
}

void KeyUtilityClientImpl::FinishCreatingKeys() {
  DCHECK(state_ == STATE_CREATING_KEYS);
  state_ = STATE_INITIALIZED;
  waitable_event_.Signal();
}

void KeyUtilityClientImpl::SetValueAfterInjection(
    const SerializedScriptValue& value) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  value_after_injection_ = value;
}

void KeyUtilityClientImpl::FinishInjectingKey() {
  DCHECK(state_ == STATE_INJECTING_KEY);
  state_ = STATE_INITIALIZED;
  waitable_event_.Signal();
}

KeyUtilityClientImpl::Client::Client(KeyUtilityClientImpl* parent)
    : parent_(parent) {
}

void KeyUtilityClientImpl::Client::OnProcessCrashed(int exit_code) {
  if (parent_->state_ == STATE_CREATING_KEYS)
    parent_->FinishCreatingKeys();
}

void KeyUtilityClientImpl::Client::OnIDBKeysFromValuesAndKeyPathSucceeded(
    int id, const std::vector<IndexedDBKey>& keys) {
  parent_->SetKeys(keys);
  parent_->FinishCreatingKeys();
}

void KeyUtilityClientImpl::Client::OnInjectIDBKeyFinished(
    const SerializedScriptValue& value) {
  parent_->SetValueAfterInjection(value);
  parent_->FinishInjectingKey();
}

void KeyUtilityClientImpl::Client::OnIDBKeysFromValuesAndKeyPathFailed(
    int id) {
  parent_->FinishCreatingKeys();
}
