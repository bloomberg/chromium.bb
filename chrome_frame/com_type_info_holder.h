// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_COM_TYPE_INFO_HOLDER_H_
#define CHROME_FRAME_COM_TYPE_INFO_HOLDER_H_

#include <map>
#include <ocidl.h>  // IProvideClassInfo2

#include "base/scoped_comptr_win.h"
#include "base/synchronization/lock.h"

#define NO_VTABLE __declspec(novtable)

namespace com_util {

// A map from a name hash (32 bit value) to a DISPID.
// Used as a caching layer before looking the name up in a type lib.
class NameToDispIdCache {
 public:
  typedef uint32 HashType;

  bool Lookup(HashType hash, DISPID* dispid) const;
  void Add(HashType hash, DISPID dispid);

  // Hashes the name by calling LHashValOfName.
  // The returned hash value is independent of the case of the characters
  // in |name|.
  static HashType Hash(const wchar_t* name);

 protected:
  typedef std::map<HashType, DISPID> DispidMap;
  DispidMap map_;
  mutable base::Lock lock_;
};

// Wraps an instance of ITypeInfo and builds+maintains a cache of names
// to dispids.  Also offers an Invoke method that simply forwards the call
// to ITypeInfo::Invoke.
class TypeInfoNameCache {
 public:
  // Loads the module's type library and fetches the ITypeInfo object for
  // the specified interface ID.
  HRESULT Initialize(const IID& iid);

  // Fetches the id's of the given names.  If there's a cache miss, the results
  // are fetched from the underlying ITypeInfo and then cached.
  HRESULT GetIDsOfNames(OLECHAR** names, uint32 count, DISPID* dispids);

  // Calls ITypeInfo::Invoke.
  HRESULT Invoke(IDispatch* p, DISPID dispid, WORD flags, DISPPARAMS* params,
                 VARIANT* result, EXCEPINFO* excepinfo, UINT* arg_err);

  inline ITypeInfo* CopyTypeInfo() {
    ITypeInfo* ti = type_info_.get();
    if (ti)
      ti->AddRef();
    return ti;
  }

 protected:
  ScopedComPtr<ITypeInfo> type_info_;
  NameToDispIdCache cache_;
};

// The root class for type lib access.
// This class has only one instance that should be accessed via the
// Singleton method.
class TypeInfoCache {
 public:
  TypeInfoCache() {
  }

  ~TypeInfoCache();

  // Looks up a previously cached TypeInfoNameCache instance or creates and
  // caches a new one.
  TypeInfoNameCache* Lookup(const IID* iid);

  // Call to get access to the singleton instance of TypeInfoCache.
  static TypeInfoCache* Singleton();

 protected:
  typedef std::map<const IID*, TypeInfoNameCache*> CacheMap;
  base::Lock lock_;
  CacheMap cache_;
};

// Holds a pointer to the type info of a given COM interface.
// The type info is loaded once on demand and after that cached.
// NOTE: This class only supports loading the first typelib from the
// current module.
template <const IID& iid>
class TypeInfoHolder {
 public:
  TypeInfoHolder() : type_info_(NULL) {
  }

  bool EnsureTI() {
    if (!type_info_)
      type_info_ = TypeInfoCache::Singleton()->Lookup(&iid);
    return type_info_ != NULL;
  }

  HRESULT GetTypeInfo(UINT itinfo, LCID lcid, ITypeInfo** info) {
    if (EnsureTI()) {
      *info = type_info_->CopyTypeInfo();
      return S_OK;
    }

    return E_UNEXPECTED;
  }

  HRESULT GetIDsOfNames(REFIID riid, OLECHAR** names, UINT count, LCID lcid,
                        DISPID* dispids) {
    if (!EnsureTI())
      return E_UNEXPECTED;
    return type_info_->GetIDsOfNames(names, count, dispids);
  }

  HRESULT Invoke(IDispatch* p, DISPID dispid, REFIID riid, LCID lcid,
                 WORD flags, DISPPARAMS* params, VARIANT* result,
                 EXCEPINFO* excepinfo, UINT* arg_err) {
    if (!EnsureTI())
      return E_UNEXPECTED;

    return type_info_->Invoke(p, dispid, flags, params, result, excepinfo,
                              arg_err);
  }

 protected:
  TypeInfoNameCache* type_info_;
};

// Implements IDispatch part of T (where T is an IDispatch derived interface).
// The class assumes that the type info of T is available in a typelib of the
// current module.
template <class T, const IID& iid = __uuidof(T)>
class NO_VTABLE IDispatchImpl : public T {
 public:
  STDMETHOD(GetTypeInfoCount)(UINT* count) {
    if (count == NULL) 
      return E_POINTER; 
    *count = 1;
    return S_OK;
  }

  STDMETHOD(GetTypeInfo)(UINT itinfo, LCID lcid, ITypeInfo** pptinfo) {
    return type_info_.GetTypeInfo(itinfo, lcid, pptinfo);
  }

  STDMETHOD(GetIDsOfNames)(REFIID riid, LPOLESTR* names, UINT count,
                           LCID lcid, DISPID* dispids) {
    return type_info_.GetIDsOfNames(riid, names, count, lcid, dispids);
  }
  STDMETHOD(Invoke)(DISPID dispid, REFIID riid, LCID lcid, WORD flags,
                    DISPPARAMS* params, VARIANT* result, EXCEPINFO* excepinfo,
                    UINT* arg_err) {
    return type_info_.Invoke(static_cast<IDispatch*>(this), dispid, riid, lcid,
                             flags, params, result, excepinfo, arg_err);
  }

 protected:
  TypeInfoHolder<iid> type_info_;
};

// Simple implementation of IProvideClassInfo[2].
template <const CLSID& class_id, const IID& source_iid>
class NO_VTABLE IProvideClassInfo2Impl : public IProvideClassInfo2 {
 public:
  STDMETHOD(GetClassInfo)(ITypeInfo** pptinfo) {
    return type_info_.GetTypeInfo(0, LANG_NEUTRAL, pptinfo);
  }

  STDMETHOD(GetGUID)(DWORD guid_kind, GUID* guid) {
    if(guid == NULL || guid_kind != GUIDKIND_DEFAULT_SOURCE_DISP_IID)
      return E_INVALIDARG;

    *guid = source_iid;

    return S_OK;
  }

 protected:
  TypeInfoHolder<class_id> type_info_;
};

}  // namespace com_util

#endif  // CHROME_FRAME_COM_TYPE_INFO_HOLDER_H_
