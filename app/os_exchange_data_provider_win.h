// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APP_OS_EXCHANGE_DATA_PROVIDER_WIN_H_
#define APP_OS_EXCHANGE_DATA_PROVIDER_WIN_H_

#include <objidl.h>
#include <string>

#include "app/os_exchange_data.h"
#include "base/scoped_comptr_win.h"

class DataObjectImpl : public IDataObject {
 public:
  DataObjectImpl();
  ~DataObjectImpl();

  // IDataObject implementation:
  HRESULT __stdcall GetData(FORMATETC* format_etc, STGMEDIUM* medium);
  HRESULT __stdcall GetDataHere(FORMATETC* format_etc, STGMEDIUM* medium);
  HRESULT __stdcall QueryGetData(FORMATETC* format_etc);
  HRESULT __stdcall GetCanonicalFormatEtc(
      FORMATETC* format_etc, FORMATETC* result);
  HRESULT __stdcall SetData(
      FORMATETC* format_etc, STGMEDIUM* medium, BOOL should_release);
  HRESULT __stdcall EnumFormatEtc(
      DWORD direction, IEnumFORMATETC** enumerator);
  HRESULT __stdcall DAdvise(FORMATETC* format_etc, DWORD advf,
                            IAdviseSink* sink, DWORD* connection);
  HRESULT __stdcall DUnadvise(DWORD connection);
  HRESULT __stdcall EnumDAdvise(IEnumSTATDATA** enumerator);

  // IUnknown implementation:
  HRESULT __stdcall QueryInterface(const IID& iid, void** object);
  ULONG __stdcall AddRef();
  ULONG __stdcall Release();

 private:
  // FormatEtcEnumerator only likes us for our StoredDataMap typedef.
  friend class FormatEtcEnumerator;
  friend class OSExchangeDataProviderWin;

  // Our internal representation of stored data & type info.
  struct StoredDataInfo {
    FORMATETC format_etc;
    STGMEDIUM* medium;
    bool owns_medium;

    StoredDataInfo(CLIPFORMAT cf, STGMEDIUM* a_medium) {
      format_etc.cfFormat = cf;
      format_etc.dwAspect = DVASPECT_CONTENT;
      format_etc.lindex = -1;
      format_etc.ptd = NULL;
      format_etc.tymed = a_medium->tymed;

      owns_medium = true;

      medium = a_medium;
    }

    ~StoredDataInfo() {
      if (owns_medium) {
        ReleaseStgMedium(medium);
        delete medium;
      }
    }
  };

  typedef std::vector<StoredDataInfo*> StoredData;
  StoredData contents_;

  ScopedComPtr<IDataObject> source_object_;

  LONG ref_count_;
};

class OSExchangeDataProviderWin : public OSExchangeData::Provider {
 public:
  // Returns true if source has plain text that is a valid url.
  static bool HasPlainTextURL(IDataObject* source);

  // Returns true if source has plain text that is a valid URL and sets url to
  // that url.
  static bool GetPlainTextURL(IDataObject* source, GURL* url);

  static IDataObject* GetIDataObject(const OSExchangeData& data);

  explicit OSExchangeDataProviderWin(IDataObject* source);
  OSExchangeDataProviderWin();

  virtual ~OSExchangeDataProviderWin();

  IDataObject* data_object() const { return data_.get(); }

  // OSExchangeData::Provider methods.
  virtual void SetString(const std::wstring& data);
  virtual void SetURL(const GURL& url, const std::wstring& title);
  virtual void SetFilename(const std::wstring& full_path);
  virtual void SetPickledData(OSExchangeData::CustomFormat format,
                              const Pickle& data);
  virtual void SetFileContents(const std::wstring& filename,
                               const std::string& file_contents);
  virtual void SetHtml(const std::wstring& html, const GURL& base_url);

  virtual bool GetString(std::wstring* data) const;
  virtual bool GetURLAndTitle(GURL* url, std::wstring* title) const;
  virtual bool GetFilename(std::wstring* full_path) const;
  virtual bool GetPickledData(OSExchangeData::CustomFormat format,
                              Pickle* data) const;
  virtual bool GetFileContents(std::wstring* filename,
                               std::string* file_contents) const;
  virtual bool GetHtml(std::wstring* html, GURL* base_url) const;
  virtual bool HasString() const;
  virtual bool HasURL() const;
  virtual bool HasFile() const;
  virtual bool HasFileContents() const;
  virtual bool HasHtml() const;
  virtual bool HasCustomFormat(OSExchangeData::CustomFormat format) const;

 private:
  scoped_refptr<DataObjectImpl> data_;
  ScopedComPtr<IDataObject> source_object_;

  DISALLOW_COPY_AND_ASSIGN(OSExchangeDataProviderWin);
};

#endif  // APP_OS_EXCHANGE_DATA_PROVIDER_WIN_H_
