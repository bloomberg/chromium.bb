// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/value_store/caching_value_store.h"

#include "chrome/browser/value_store/leveldb_value_store.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

class CachingValueStore::Backend : public base::RefCountedThreadSafe<Backend> {
 public:
  typedef base::Callback<void(scoped_ptr<base::DictionaryValue>)> ReadyCallback;
  Backend() : storage_(NULL) {}

  void Init(const FilePath& db_path, ReadyCallback callback) {
    BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
        base::Bind(&CachingValueStore::Backend::InitOnFileThread,
                   this, db_path, callback));
  }

  void Set(const std::string& key, scoped_ptr<base::Value> value) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
    // We've already checked that the value has changed, so don't bother
    // checking again. Also, skip changes to avoid wasted copies.
    storage_->Set(ValueStore::IGNORE_QUOTA |
                      ValueStore::NO_GENERATE_CHANGES |
                      ValueStore::NO_CHECK_OLD_VALUE,
                  key, *value.get());
  }

  void Remove(const std::string& key) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
    storage_->Remove(key);
  }

 private:
  friend class base::RefCountedThreadSafe<Backend>;

  virtual ~Backend() {
    if (BrowserThread::CurrentlyOn(BrowserThread::FILE)) {
      delete storage_;
    } else {
      BrowserThread::DeleteSoon(BrowserThread::FILE, FROM_HERE, storage_);
    }
  }

  void InitOnFileThread(const FilePath& db_path, ReadyCallback callback) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
    DCHECK(!storage_);
    storage_ = LeveldbValueStore::Create(db_path);

    ValueStore::ReadResult result = storage_->Get();
    if (result->HasError()) {
      callback.Run(scoped_ptr<base::DictionaryValue>(NULL));
    } else {
      callback.Run(result->settings().Pass());
    }
  }

  // The actual ValueStore that handles persisting the data to disk. Used
  // exclusively on the FILE thread.
  LeveldbValueStore* storage_;

  DISALLOW_COPY_AND_ASSIGN(Backend);
};

CachingValueStore::CachingValueStore(const FilePath& db_path)
    : backend_(new Backend()) {
  backend_->Init(db_path,
                 base::Bind(&CachingValueStore::OnBackendReady, AsWeakPtr()));
}

CachingValueStore::~CachingValueStore() {
}

bool CachingValueStore::Get(const std::string& key,
                            const base::Value** result) {
  DCHECK(CalledOnValidThread());
  DCHECK(IsInitialized());

  base::Value* value = NULL;
  if (cache_->GetWithoutPathExpansion(key, &value)) {
    *result = value;
    return true;
  }
  return false;
}

void CachingValueStore::Set(const std::string& key, base::Value* value) {
  DCHECK(CalledOnValidThread());
  DCHECK(IsInitialized());

  scoped_ptr<base::Value> new_value(value);
  base::Value* old_value = NULL;
  if (!cache_->GetWithoutPathExpansion(key, &old_value) ||
      !value->Equals(old_value)) {
    cache_->SetWithoutPathExpansion(key, new_value.release());

    scoped_ptr<base::Value> passed_value(value->DeepCopy());
    BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
        base::Bind(&CachingValueStore::Backend::Set,
                   backend_, key, base::Passed(passed_value.Pass())));
  }
}

void CachingValueStore::Remove(const std::string& key) {
  DCHECK(CalledOnValidThread());
  DCHECK(IsInitialized());

  if (cache_->RemoveWithoutPathExpansion(key, NULL)) {
    BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
        base::Bind(&CachingValueStore::Backend::Remove,
                   backend_, key));
  }
}

bool CachingValueStore::IsInitialized() {
  DCHECK(CalledOnValidThread());
  return !!cache_.get();
}

void CachingValueStore::AddObserver(Observer* observer) {
  DCHECK(CalledOnValidThread());
  observers_.AddObserver(observer);
}

void CachingValueStore::RemoveObserver(Observer* observer) {
  DCHECK(CalledOnValidThread());
  observers_.RemoveObserver(observer);
}

void CachingValueStore::OnBackendReady(
    scoped_ptr<base::DictionaryValue> values) {
  DCHECK(CalledOnValidThread());
  DCHECK(!cache_.get());
  cache_.swap(values);

  FOR_EACH_OBSERVER(Observer, observers_, OnInitializationComplete());
}
