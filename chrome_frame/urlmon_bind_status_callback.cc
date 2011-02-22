// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/urlmon_bind_status_callback.h"

#include <mshtml.h>
#include <shlguid.h>

#include "base/logging.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/threading/platform_thread.h"
#include "base/utf_string_conversions.h"
#include "chrome_frame/bind_context_info.h"
#include "chrome_frame/exception_barrier.h"
#include "chrome_frame/urlmon_moniker.h"
#include "chrome_tab.h"  // NOLINT


// A helper to given feed data to the specified |bscb| using
// CacheStream instance.
HRESULT CacheStream::BSCBFeedData(IBindStatusCallback* bscb, const char* data,
                                  size_t size, CLIPFORMAT clip_format,
                                  size_t flags, bool eof) {
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

  scoped_refptr<CacheStream> cache_ref = cache_stream;
  hr = cache_stream->Initialize(data, size, eof);
  if (FAILED(hr))
    return hr;

  FORMATETC format_etc = { clip_format, NULL, DVASPECT_CONTENT, -1,
                           TYMED_ISTREAM };
  STGMEDIUM medium = {0};
  medium.tymed = TYMED_ISTREAM;
  medium.pstm = cache_stream;

  hr = bscb->OnDataAvailable(flags, size, &format_etc, &medium);
  return hr;
}

HRESULT CacheStream::Initialize(const char* cache, size_t size, bool eof) {
  position_ = 0;
  eof_ = eof;

  HRESULT hr = S_OK;
  cache_.reset(new char[size]);
  if (cache_.get()) {
    memcpy(cache_.get(), cache, size);
    size_ = size;
  } else {
    DLOG(ERROR) << "failed to allocate cache stream.";
    hr = E_OUTOFMEMORY;
  }

  return hr;
}

// Read is the only call that we expect. Return E_PENDING if there
// is no more data to serve. Otherwise this will result in a
// read with 0 bytes indicating that no more data is available.
STDMETHODIMP CacheStream::Read(void* pv, ULONG cb, ULONG* read) {
  if (!pv || !read)
    return E_INVALIDARG;

  if (!cache_.get()) {
    *read = 0;
    return S_FALSE;
  }

  // Default to E_PENDING to signal that this is a partial data.
  HRESULT hr = eof_ ? S_FALSE : E_PENDING;
  if (position_ < size_) {
    *read = std::min(size_ - position_, size_t(cb));
    memcpy(pv, cache_ .get() + position_, *read);
    position_ += *read;
    hr = S_OK;
  }

  return hr;
}


/////////////////////////////////////////////////////////////////////

HRESULT SniffData::InitializeCache(const std::wstring& url) {
  url_ = url;
  renderer_type_ = UNDETERMINED;

  const int kInitialSize = 4 * 1024; // 4K
  HGLOBAL mem = GlobalAlloc(0, kInitialSize);
  DCHECK(mem) << "GlobalAlloc failed: " << GetLastError();

  HRESULT hr = CreateStreamOnHGlobal(mem, TRUE, cache_.Receive());
  if (SUCCEEDED(hr)) {
    ULARGE_INTEGER size = {0};
    cache_->SetSize(size);
  } else {
    DLOG(ERROR) << "CreateStreamOnHGlobal failed: " << hr;
  }

  return hr;
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

  bool last_chance = force_determination || (size() >= kMaxSniffSize);
  eof_ = force_determination;
  DetermineRendererType(last_chance);
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
    hr = CacheStream::BSCBFeedData(bscb, buffer, size_, clip_format, bscf,
                                   eof_);
    GlobalUnlock(memory);
  }

  size_ = 0;
  cache_.Release();
  return hr;
}

// Scan the buffer or OptIn URL list and decide if the renderer is
// to be switched.  Last chance means there's no more data.
void SniffData::DetermineRendererType(bool last_chance) {
  if (is_undetermined()) {
    if (last_chance)
      renderer_type_ = OTHER;
    if (IsChrome(RendererTypeForUrl(url_))) {
      renderer_type_ = CHROME;
    } else {
      if (is_cache_valid() && cache_) {
        HGLOBAL memory = NULL;
        GetHGlobalFromStream(cache_, &memory);
        const char* buffer = reinterpret_cast<const char*>(GlobalLock(memory));

        std::wstring html_contents;
        // TODO(joshia): detect and handle different content encodings
        if (buffer && size_) {
          UTF8ToWide(buffer, std::min(size_, kMaxSniffSize), &html_contents);
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
    DVLOG(1) << __FUNCTION__ << "Url: " << url_ << base::StringPrintf(
          "Renderer type: %s", renderer_type_ == CHROME ? "CHROME" : "OTHER");
  }
}

/////////////////////////////////////////////////////////////////////

BSCBStorageBind::BSCBStorageBind() : clip_format_(CF_NULL) {
}

BSCBStorageBind::~BSCBStorageBind() {
  std::for_each(saved_progress_.begin(), saved_progress_.end(),
                utils::DeleteObject());
}

HRESULT BSCBStorageBind::Initialize(IMoniker* moniker, IBindCtx* bind_ctx) {
  DVLOG(1) << __FUNCTION__ << me()
           << base::StringPrintf(" tid=%i", base::PlatformThread::CurrentId());

  std::wstring url = GetActualUrlFromMoniker(moniker, bind_ctx,
                                             std::wstring());
  HRESULT hr = data_sniffer_.InitializeCache(url);
  if (FAILED(hr))
    return hr;

  hr = AttachToBind(bind_ctx);
  if (FAILED(hr)) {
    NOTREACHED() << __FUNCTION__ << me() << "AttachToBind error: " << hr;
    return hr;
  }

  if (!delegate()) {
    NOTREACHED() << __FUNCTION__ << me() << "No existing callback: " << hr;
    return E_FAIL;
  }

  return hr;
}

STDMETHODIMP BSCBStorageBind::OnProgress(ULONG progress, ULONG progress_max,
                                    ULONG status_code, LPCWSTR status_text) {
  DVLOG(1) << __FUNCTION__ << me()
           << base::StringPrintf(" status=%i tid=%i %ls", status_code,
                                 base::PlatformThread::CurrentId(),
                                 status_text);
  // Report all crashes in the exception handler if we wrap the callback.
  // Note that this avoids having the VEH report a crash if an SEH earlier in
  // the chain handles the exception.
  ExceptionBarrier barrier;

  HRESULT hr = S_OK;

  // TODO(ananta)
  // ChromeFrame will not be informed of any redirects which occur while we
  // switch into Chrome. This will only break the moniker patch which is
  // legacy and needs to be deleted.

  if (ShouldCacheProgress(status_code)) {
    saved_progress_.push_back(new Progress(progress, progress_max, status_code,
                                           status_text));
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
  DVLOG(1) << __FUNCTION__
           << base::StringPrintf(" tid=%i", base::PlatformThread::CurrentId());
  // Report all crashes in the exception handler if we wrap the callback.
  // Note that this avoids having the VEH report a crash if an SEH earlier in
  // the chain handles the exception.
  ExceptionBarrier barrier;
  // Do not touch anything other than text/html.
  bool is_interesting = (format_etc && stgmed && stgmed->pstm &&
      stgmed->tymed == TYMED_ISTREAM &&
      IsTextHtmlClipFormat(format_etc->cfFormat));

  if (!is_interesting) {
    // Play back report progress so far.
    MayPlayBack(flags);
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
  DVLOG(1) << __FUNCTION__
           << base::StringPrintf(" tid=%i", base::PlatformThread::CurrentId());
  // Report all crashes in the exception handler if we wrap the callback.
  // Note that this avoids having the VEH report a crash if an SEH earlier in
  // the chain handles the exception.
  ExceptionBarrier barrier;

  HRESULT hr = MayPlayBack(BSCF_LASTDATANOTIFICATION);
  hr = CallbackImpl::OnStopBinding(hresult, error);
  ReleaseBind();
  return hr;
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
  data_sniffer_.DetermineRendererType(true);
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
      for (ProgressVector::iterator i = saved_progress_.begin();
           i != saved_progress_.end(); i++) {
        Progress* p = (*i);
        // We don't really expect a race condition here but just for sake
        // of completeness we check.
        if (p) {
          (*i) = NULL;
          CallbackImpl::OnProgress(p->progress(), p->progress_max(),
                                   p->status_code(), p->status_text());
          delete p;
        }
      }
      saved_progress_.clear();
    }
  }

  if (data_sniffer_.is_cache_valid()) {
    if (data_sniffer_.is_chrome()) {
      ScopedComPtr<BindContextInfo> info;
      BindContextInfo::FromBindContext(bind_ctx_, info.Receive());
      DCHECK(info);
      if (info) {
        info->SetToSwitch(data_sniffer_.cache_);
      }
    }

    hr = data_sniffer_.DrainCache(delegate(),
        flags | BSCF_FIRSTDATANOTIFICATION, clip_format_);
    DLOG_IF(WARNING, INET_E_TERMINATED_BIND != hr) << __FUNCTION__ <<
      " mshtml OnDataAvailable returned: " << std::hex << hr;
  }

  return hr;
}

// We cache and suppress sending progress notifications till
// we get the first OnDataAvailable. This is to prevent
// mshtml from making up its mind about the mime type.
// However, this is the invasive part of the patch and
// could trip other software that's due to mistimed progress
// notifications. It is probably not a good idea to hide redirects
// and some cookie notifications.
//
// We only need to suppress data notifications like
// BINDSTATUS_MIMETYPEAVAILABLE,
// BINDSTATUS_CACHEFILENAMEAVAILABLE etc.
//
// This is an atempt to reduce the exposure by starting to
// cache only when we receive one of the interesting progress
// notification.
bool BSCBStorageBind::ShouldCacheProgress(unsigned long status_code) const {
  // We need to cache progress notifications only if we haven't yet figured
  // out which way the request is going.
  if (data_sniffer_.is_undetermined()) {
    // If we are already caching then continue.
    if (!saved_progress_.empty())
      return true;
    // Start caching only if we see one of the interesting progress
    // notifications.
    switch (status_code) {
      case BINDSTATUS_BEGINDOWNLOADDATA:
      case BINDSTATUS_DOWNLOADINGDATA:
      case BINDSTATUS_USINGCACHEDCOPY:
      case BINDSTATUS_MIMETYPEAVAILABLE:
      case BINDSTATUS_CACHEFILENAMEAVAILABLE:
      case BINDSTATUS_SERVER_MIMETYPEAVAILABLE:
        return true;
      default:
        break;
    }
  }

  return false;
}
