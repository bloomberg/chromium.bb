// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/com_type_info_holder.h"

#include "base/lazy_instance.h"
#include "base/logging.h"

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace com_util {

base::LazyInstance<TypeInfoCache> type_info_cache(base::LINKER_INITIALIZED);

// TypeInfoCache

TypeInfoCache::~TypeInfoCache() {
  CacheMap::iterator it = cache_.begin();
  while (it != cache_.end()) {
    delete it->second;
    it++;
  }
}

TypeInfoNameCache* TypeInfoCache::Lookup(const IID* iid) {
  DCHECK(Singleton() == this);

  TypeInfoNameCache* tih = NULL;

  base::AutoLock lock(lock_);
  CacheMap::iterator it = cache_.find(iid);
  if (it == cache_.end()) {
    tih = new TypeInfoNameCache();
    HRESULT hr = tih ? tih->Initialize(*iid) : E_OUTOFMEMORY;
    if (SUCCEEDED(hr)) {
      cache_[iid] = tih;
    } else {
      NOTREACHED();
      delete tih;
      tih = NULL;
    }
  } else {
    tih = it->second;
  }

  return tih;
}

TypeInfoCache* TypeInfoCache::Singleton() {
  return type_info_cache.Pointer();
}

HRESULT TypeInfoNameCache::Initialize(const IID& iid) {
  DCHECK(type_info_ == NULL);

  wchar_t file_path[MAX_PATH];
  DWORD path_len = ::GetModuleFileNameW(reinterpret_cast<HMODULE>(&__ImageBase),
                                        file_path, arraysize(file_path));
  if (path_len == 0 || path_len == MAX_PATH) {
    NOTREACHED();
    return E_UNEXPECTED;
  }

  ScopedComPtr<ITypeLib> type_lib;
  HRESULT hr = LoadTypeLib(file_path, type_lib.Receive());
  if (SUCCEEDED(hr)) {
    hr = type_lib->GetTypeInfoOfGuid(iid, type_info_.Receive());
  }

  return hr;
}

// TypeInfoNameCache

HRESULT TypeInfoNameCache::GetIDsOfNames(OLECHAR** names, uint32 count,
                                         DISPID* dispids) {
  DCHECK(type_info_ != NULL);

  HRESULT hr = S_OK;
  for (uint32 i = 0; i < count && SUCCEEDED(hr); ++i) {
    NameToDispIdCache::HashType hash = NameToDispIdCache::Hash(names[i]);
    if (!cache_.Lookup(hash, &dispids[i])) {
      hr = type_info_->GetIDsOfNames(&names[i], 1, &dispids[i]);
      if (SUCCEEDED(hr)) {
        cache_.Add(hash, dispids[i]);
      }
    }
  }

  return hr;
}

HRESULT TypeInfoNameCache::Invoke(IDispatch* p, DISPID dispid, WORD flags,
                                  DISPPARAMS* params, VARIANT* result,
                                  EXCEPINFO* excepinfo, UINT* arg_err) {
  DCHECK(type_info_);
  HRESULT hr = type_info_->Invoke(p, dispid, flags, params, result, excepinfo,
                                  arg_err);
  DCHECK(hr != RPC_E_WRONG_THREAD);
  return hr;
}

// NameToDispIdCache

bool NameToDispIdCache::Lookup(HashType hash, DISPID* dispid) const {
  base::AutoLock lock(lock_);
  const DispidMap::const_iterator it = map_.find(hash);
  bool found = (it != map_.end());
  if (found)
    *dispid = it->second;
  return found;
}

void NameToDispIdCache::Add(HashType hash, DISPID dispid) {
  base::AutoLock lock(lock_);
  map_[hash] = dispid;
}

NameToDispIdCache::HashType NameToDispIdCache::Hash(const wchar_t* name) {
  return LHashValOfName(LANG_NEUTRAL, name);
}

}  // namespace com_util
