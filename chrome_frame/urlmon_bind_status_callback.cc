// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/urlmon_bind_status_callback.h"

#include <mshtml.h>
#include <shlguid.h>

#include "base/logging.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"

#include "chrome_frame/urlmon_moniker.h"
#include "chrome_tab.h"  // NOLINT


// A helper to given feed data to the specified |bscb| using
// CacheStream instance.
HRESULT CacheStream::BSCBFeedData(IBindStatusCallback* bscb, const char* data,
                                  size_t size, CLIPFORMAT clip_format,
                                  size_t flags) {
  if (!bscb) {
    NOTREACHED() << "invalid IBindStatusCallback";
    return E_INVALIDARG;
  }

  // We can't use a CComObjectStackEx here since mshtml will hold
  // onto the stream pointer.
  CComObject<CacheStream>* cache_stream = NULL;
  HRESULT hr = CComObject<CacheStream>::CreateInstance(&cache_stream);
  if (FAILED(hr)) {
    NOTREACHED();
    return hr;
  }

  cache_stream->AddRef();
  cache_stream->Initialize(data, size);

  FORMATETC format_etc = { clip_format, NULL, DVASPECT_CONTENT, -1,
                           TYMED_ISTREAM };
  STGMEDIUM medium = {0};
  medium.tymed = TYMED_ISTREAM;
  medium.pstm = cache_stream;

  hr = bscb->OnDataAvailable(flags, size, &format_etc, &medium);

  cache_stream->Release();
  return hr;
}

void CacheStream::Initialize(const char* cache, size_t size) {
  cache_ = cache;
  size_ = size;
  position_ = 0;
}

// Read is the only call that we expect. Return E_PENDING if there
// is no more data to serve. Otherwise this will result in a
// read with 0 bytes indicating that no more data is available.
STDMETHODIMP CacheStream::Read(void* pv, ULONG cb, ULONG* read) {
  if (!pv || !read)
    return E_INVALIDARG;

  // Default to E_PENDING to signal that this is a partial data.
  HRESULT hr = E_PENDING;
  if (position_ < size_) {
    *read = std::min(size_ - position_, size_t(cb));
    memcpy(pv, cache_ + position_, *read);
    position_ += *read;
    hr = S_OK;
  }

  return hr;
}


/////////////////////////////////////////////////////////////////////

bool SniffData::InitializeCache(const std::wstring& url) {
  url_ = url;
  renderer_type_ = UNDETERMINED;

  const int kAllocationSize = 32 * 1024;
  HGLOBAL memory = GlobalAlloc(0, kAllocationSize);
  HRESULT hr = CreateStreamOnHGlobal(memory, TRUE, cache_.Receive());
  if (FAILED(hr)) {
    GlobalFree(memory);
    NOTREACHED();
    return false;
  }

  return true;
}

HRESULT SniffData::ReadIntoCache(IStream* stream, bool force_determination) {
  if (!stream) {
    NOTREACHED();
    return E_INVALIDARG;
  }

  HRESULT hr = S_OK;
  while (SUCCEEDED(hr)) {
    const size_t kChunkSize = 4 * 1024;
    char buffer[kChunkSize];
    DWORD read = 0;
    hr = stream->Read(buffer, sizeof(buffer), &read);
    if (read) {
      DWORD written = 0;
      cache_->Write(buffer, read, &written);
      size_ += written;
    }

    if ((S_FALSE == hr) || !read)
      break;
  }

  if (force_determination || (size() >= kMaxSniffSize)) {
    DetermineRendererType();
  }

  return hr;
}

HRESULT SniffData::DrainCache(IBindStatusCallback* bscb, DWORD bscf,
                              CLIPFORMAT clip_format) {
  if (!is_cache_valid()) {
    return S_OK;
  }

  // Ideally we could just use the cache_ IStream implementation but
  // can't use it here since we have to return E_PENDING for the
  // last call
  HGLOBAL memory = NULL;
  HRESULT hr = GetHGlobalFromStream(cache_, &memory);
  if (SUCCEEDED(hr) && memory) {
    char* buffer = reinterpret_cast<char*>(GlobalLock(memory));
    hr = CacheStream::BSCBFeedData(bscb, buffer, size_, clip_format, bscf);
    GlobalUnlock(memory);
  }

  size_ = 0;
  cache_.Release();
  return hr;
}

// Scan the buffer or OptIn URL list and decide if the renderer is
// to be switched
void SniffData::DetermineRendererType() {
  if (is_undetermined()) {
    if (IsOptInUrl(url_.c_str())) {
      renderer_type_ = CHROME;
    } else {
      renderer_type_ = OTHER;
      if (is_cache_valid() && cache_) {
        HGLOBAL memory = NULL;
        GetHGlobalFromStream(cache_, &memory);
        char* buffer = reinterpret_cast<char*>(GlobalLock(memory));

        std::wstring html_contents;
        // TODO(joshia): detect and handle different content encodings
        if (buffer && size_) {
          UTF8ToWide(buffer, size_, &html_contents);
          GlobalUnlock(memory);
        }

        // Note that document_contents_ may have NULL characters in it. While
        // browsers may handle this properly, we don't and will stop scanning
        // for the XUACompat content value if we encounter one.
        std::wstring xua_compat_content;
        UtilGetXUACompatContentValue(html_contents, &xua_compat_content);
        if (StrStrI(xua_compat_content.c_str(), kChromeContentPrefix)) {
          renderer_type_ = CHROME;
        }
      }
    }
  }
}

/////////////////////////////////////////////////////////////////////

HRESULT BSCBStorageBind::Initialize(IMoniker* moniker, IBindCtx* bind_ctx) {
  DLOG(INFO) << __FUNCTION__ << me() << StringPrintf(" tid=%i",
      PlatformThread::CurrentId());

  HRESULT hr = AttachToBind(bind_ctx);
  if (FAILED(hr)) {
    NOTREACHED() << __FUNCTION__ << me() << "AttachToBind error: " << hr;
    return hr;
  }

  if (!delegate()) {
    NOTREACHED() << __FUNCTION__ << me() << "No existing callback: " << hr;
    return E_FAIL;
  }

  std::wstring url = GetActualUrlFromMoniker(moniker, bind_ctx,
                                             std::wstring());
  data_sniffer_.InitializeCache(url);
  return hr;
}

STDMETHODIMP BSCBStorageBind::OnProgress(ULONG progress, ULONG progress_max,
                                    ULONG status_code, LPCWSTR status_text) {
  DLOG(INFO) << __FUNCTION__ << me() << StringPrintf(" status=%i tid=%i %ls",
      status_code, PlatformThread::CurrentId(), status_text);

  HRESULT hr = S_OK;
  if (data_sniffer_.is_undetermined()) {
    Progress new_progress = { progress, progress_max, status_code,
                              status_text ? status_text : std::wstring() };
    saved_progress_.push_back(new_progress);
  } else {
    hr = CallbackImpl::OnProgress(progress, progress_max, status_code,
                               status_text);
  }

  return hr;
}

// Refer to urlmon_moniker.h for explanation of how things work.
STDMETHODIMP BSCBStorageBind::OnDataAvailable(DWORD flags, DWORD size,
                                              FORMATETC* format_etc,
                                              STGMEDIUM* stgmed) {
  DLOG(INFO) << __FUNCTION__ << StringPrintf(" tid=%i",
      PlatformThread::CurrentId());
  if (!stgmed || !format_etc) {
    DLOG(INFO) << __FUNCTION__ << me() << "Invalid stgmed or format_etc";
    return CallbackImpl::OnDataAvailable(flags, size, format_etc, stgmed);
  }

  if ((stgmed->tymed != TYMED_ISTREAM) || !stgmed->pstm) {
    DLOG(INFO) << __FUNCTION__ << me() << "stgmedium is not a valid stream";
    return CallbackImpl::OnDataAvailable(flags, size, format_etc, stgmed);
  }

  HRESULT hr = S_OK;
  if (!clip_format_)
    clip_format_ = format_etc->cfFormat;

  if (data_sniffer_.is_undetermined()) {
    bool force_determination = !!(flags &
        (BSCF_LASTDATANOTIFICATION | BSCF_DATAFULLYAVAILABLE));
    hr = data_sniffer_.ReadIntoCache(stgmed->pstm, force_determination);
    // If we don't have sufficient data to determine renderer type
    // wait for the next data notification.
    if (data_sniffer_.is_undetermined())
      return S_OK;
  }

  DCHECK(!data_sniffer_.is_undetermined());

  if (data_sniffer_.is_cache_valid()) {
    hr = MayPlayBack(flags);
    DCHECK(!data_sniffer_.is_cache_valid());
  } else {
    hr = CallbackImpl::OnDataAvailable(flags, size, format_etc, stgmed);
  }

  return hr;
}

STDMETHODIMP BSCBStorageBind::OnStopBinding(HRESULT hresult, LPCWSTR error) {
  DLOG(INFO) << __FUNCTION__ << StringPrintf(" tid=%i",
      PlatformThread::CurrentId());
  HRESULT hr = MayPlayBack(BSCF_LASTDATANOTIFICATION);
  return CallbackImpl::OnStopBinding(hresult, error);
}

// Play back the cached data to the delegate. Normally this would happen
// when we have read enough data to determine the renderer. In this case
// we first play back the data from the cache and then go into a 'pass
// through' mode.  In some cases we may end up getting OnStopBinding
// before we get a chance to determine. Also it's possible that the
// BindToStorage call will return before OnStopBinding is sent. Hence
// This is called from 3 places and it's important to maintain the
// exact sequence of calls.
// Once the data is played back, calling this again is a no op.
HRESULT BSCBStorageBind::MayPlayBack(DWORD flags) {
  // Force renderer type determination if not already done since
  // we want to play back data now.
  data_sniffer_.DetermineRendererType();
  DCHECK(!data_sniffer_.is_undetermined());

  HRESULT hr = S_OK;
  if (data_sniffer_.is_chrome()) {
    // Remember clip format.  If we are switching to chrome, then in order
    // to make mshtml return INET_E_TERMINATED_BIND and reissue navigation
    // with the same bind context, we have to return a mime type that is
    // special cased by mshtml.
    static const CLIPFORMAT kMagicClipFormat =
        RegisterClipboardFormat(CFSTR_MIME_MPEG);
    clip_format_ = kMagicClipFormat;
  } else {
    if (!saved_progress_.empty()) {
      for (std::vector<Progress>::iterator i = saved_progress_.begin();
          i != saved_progress_.end(); i++) {
        const wchar_t* status_text = i->status_text_.empty() ?
            NULL : i->status_text_.c_str();
        CallbackImpl::OnProgress(i->progress_, i->progress_max_,
                                 i->status_code_, status_text);
      }
      saved_progress_.clear();
    }
  }

  if (data_sniffer_.is_cache_valid()) {
    hr = data_sniffer_.DrainCache(delegate(),
        flags | BSCF_FIRSTDATANOTIFICATION, clip_format_);
    if (data_sniffer_.is_chrome())
      NavigationManager::AttachCFObject(bind_ctx_);
  }

  return hr;
}
