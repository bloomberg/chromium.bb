// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/buggy_bho_handling.h"

#include "base/logging.h"
#include "base/scoped_comptr_win.h"

#include "chrome_frame/exception_barrier.h"
#include "chrome_frame/function_stub.h"
#include "chrome_frame/utils.h"
#include "chrome_frame/vtable_patch_manager.h"

namespace buggy_bho {

base::ThreadLocalPointer<BuggyBhoTls> BuggyBhoTls::s_bad_object_tls_;

struct ModuleAndVersion {
  const char* module_name_;
  const uint32 major_version_;
  const uint32 minor_version_;
};

const ModuleAndVersion kBuggyModules[] = {
  { "askbar.dll", 4, 1 },  // troublemaker: 4.1.0.5.
  { "gbieh.dll", 3, 8 },  // troublemaker: 3.8.14.12
  { "gbiehcef.dll", 3, 8 },  // troublemaker: 3.8.11.23
  { "alot.dll", 2, 5 },  // troublemaker: 2.5.12000.509
  { "ctbr.dll", 5, 1 },  // troublemaker: 5.1.0.95
  { "srchbxex.dll", 1, 2 },  // troublemaker: 1.2.123.0
  { "iedvtool32.dll", 8, 0 },  // troublemaker: 8.0.0.4
  { "mst164.dll", 9, 1 },  // troublemaker: 9.1.3700.1
  { "deposit_ie_com.dll", 0, 1  },  // troublemaker: 0.1.0.72
  { "rpshell32.dll", 6, 0 },  // troublemaker: 6.0.6000.1389
  { "msgsres.dll", 6, 0 },  // troublemaker: 6.0.6000.1389
  { "limewireinttb.dll", 4, 1  },  // troublemaker: 4.1.1.1000
  { "pxsecure.dll", 3, 0  },  // troublemaker: 3.0.5.220

  // These BHOs seem to be out of the same buggy BHO factory
  { "tbabso.dll", 4, 5 },  // troublemaker: 4.5.156.0
  { "tbabs0.dll.dll", 4, 5 },  // troublemaker: 4.5.156.0
  { "tbbes0.dll", 4, 5 },  // troublemaker: 4.5.153.0
  { "tbfre0.dll", 4, 5 },  // troublemaker: 4.5.181.1
  { "tbmypl.dll", 4, 5 },  // troublemaker: 4.5.181.3
  { "tbmul1.dll", 4, 5 },  // troublemaker: 4.5.181.1
  { "tbdow1.dll", 4, 5 },  // troublemaker: 4.5.167.0
  { "tbfree.dll", 4, 5 },  // troublemaker: 4.5.178.0

  // Viruses?
  { "msgsc2.dll", 0xffff, 0xffff },  // troublemaker: 1.2.3000.1
  { "essclis.dll", 0xffff, 0xffff },  // troublemaker: 4.3.1800.2
  { "snagwb.dll", 0xffff, 0xffff },  // troublemaker: 2.6.0.28
};

bool IsBuggyBho(HMODULE mod) {
  DCHECK(mod);

  char path[MAX_PATH * 2] = {0};
  ::GetModuleFileNameA(mod, path, arraysize(path));
  const char* file_name = ::PathFindFileNameA(path);
  for (size_t i = 0; i < arraysize(kBuggyModules); ++i) {
    if (lstrcmpiA(file_name, kBuggyModules[i].module_name_) == 0) {
      uint32 version = 0;
      GetModuleVersion(mod, &version, NULL);
      const ModuleAndVersion& buggy = kBuggyModules[i];
      if (HIWORD(version) < buggy.major_version_ ||
          (HIWORD(version) == buggy.major_version_ &&
           LOWORD(version) <= buggy.minor_version_)) {
        return true;
      }
    }
  }

  return false;
}

BuggyBhoTls::BuggyBhoTls()
    : patched_(false) {
  DCHECK(s_bad_object_tls_.Get() == NULL);
  s_bad_object_tls_.Set(this);
}

BuggyBhoTls::~BuggyBhoTls() {
  DCHECK(BuggyBhoTls::GetInstance() == this);
  s_bad_object_tls_.Set(NULL);
}

void BuggyBhoTls::AddBuggyObject(IDispatch* obj) {
  bad_objects_.push_back(obj);
}

bool BuggyBhoTls::ShouldSkipInvoke(IDispatch* obj) const {
  DCHECK(web_browser2_ != NULL);
  if (IsChromeFrameDocument(web_browser2_)) {
    return std::find(bad_objects_.begin(), bad_objects_.end(), obj) !=
           bad_objects_.end();
  }
  return false;
}

// static
BuggyBhoTls* BuggyBhoTls::GetInstance() {
  BuggyBhoTls* tls_instance = s_bad_object_tls_.Get();
  if (!tls_instance) {
    tls_instance = new BuggyBhoTls();
    DCHECK(s_bad_object_tls_.Get() != NULL);
  }
  return tls_instance;
}

// static
void BuggyBhoTls::DestroyInstance() {
  BuggyBhoTls* tls_instance = s_bad_object_tls_.Get();
  if (tls_instance) {
    delete tls_instance;
    DCHECK(s_bad_object_tls_.Get() == NULL);
  }
}

HRESULT BuggyBhoTls::PatchBuggyBHOs(IWebBrowser2* browser) {
  if (patched_)
    return S_FALSE;

  DCHECK(browser);
  DCHECK(web_browser2_ == NULL);

  ScopedComPtr<IConnectionPointContainer> cpc;
  HRESULT hr = cpc.QueryFrom(browser);
  if (SUCCEEDED(hr)) {
    const GUID sinks[] = { DIID_DWebBrowserEvents2, DIID_DWebBrowserEvents };
    for (size_t i = 0; i < arraysize(sinks); ++i) {
      ScopedComPtr<IConnectionPoint> cp;
      cpc->FindConnectionPoint(sinks[i], cp.Receive());
      if (cp) {
        ScopedComPtr<IEnumConnections> connections;
        cp->EnumConnections(connections.Receive());
        if (connections) {
          CONNECTDATA cd = {0};
          DWORD fetched = 0;
          while (connections->Next(1, &cd, &fetched) == S_OK && fetched) {
            PatchIfBuggy(cd.pUnk, sinks[i]);
            cd.pUnk->Release();
            fetched = 0;
          }
        }
      }
    }
  }
  patched_ = true;
  web_browser2_ = browser;
  return hr;
}

bool BuggyBhoTls::PatchIfBuggy(IUnknown* unk, const IID& diid) {
  DCHECK(unk);
  PROC* methods = *reinterpret_cast<PROC**>(unk);
  HMODULE mod = GetModuleFromAddress(methods[0]);
  if (!IsBuggyBho(mod))
    return false;

  ScopedComPtr<IDispatch> disp;
  HRESULT hr = unk->QueryInterface(diid,
      reinterpret_cast<void**>(disp.Receive()));
  if (FAILED(hr))  // Sometimes only IDispatch QI is supported
    hr = disp.QueryFrom(unk);
  DCHECK(SUCCEEDED(hr));

  if (SUCCEEDED(hr)) {
    const int kInvokeIndex = 6;
    DCHECK(static_cast<IUnknown*>(disp) == unk);
    if (SUCCEEDED(PatchInvokeMethod(&methods[kInvokeIndex]))) {
      AddBuggyObject(disp);
    }
  }
  return false;
}

// static
STDMETHODIMP BuggyBhoTls::BuggyBhoInvoke(InvokeFunc original, IDispatch* me,
                                         DISPID dispid, REFIID riid, LCID lcid,
                                         WORD flags, DISPPARAMS* params,
                                         VARIANT* result, EXCEPINFO* ei,
                                         UINT* err) {
  DVLOG(1) << __FUNCTION__;

  DCHECK(BuggyBhoTls::GetInstance())
      << "You must first have an instance of BuggyBhoTls on this thread";
  if (BuggyBhoTls::GetInstance() &&
      BuggyBhoTls::GetInstance()->ShouldSkipInvoke(me)) {
    // Ignore this call and avoid the bug.
    // TODO(tommi): Maybe we should check a specific list of DISPIDs too?
    return S_OK;
  }

  // No need to report crashes in those known-to-be-buggy DLLs.
  ExceptionBarrierReportOnlyModule barrier;
  return original(me, dispid, riid, lcid, flags, params, result, ei, err);
}

// static
HRESULT BuggyBhoTls::PatchInvokeMethod(PROC* invoke) {
  CCritSecLock lock(_pAtlModule->m_csStaticDataInitAndTypeInfo.m_sec, true);

  FunctionStub* stub = FunctionStub::FromCode(*invoke);
  if (stub)
    return S_FALSE;

  DWORD flags = 0;
  if (!::VirtualProtect(invoke, sizeof(PROC), PAGE_EXECUTE_READWRITE, &flags))
    return AtlHresultFromLastError();

  HRESULT hr = S_OK;

  stub = FunctionStub::Create(reinterpret_cast<uintptr_t>(*invoke),
                              BuggyBhoInvoke);
  if (!stub) {
    hr = E_OUTOFMEMORY;
  } else {
    if (!vtable_patch::internal::ReplaceFunctionPointer(
            reinterpret_cast<void**>(invoke), stub->code(),
            reinterpret_cast<void*>(stub->argument()))) {
      hr = E_UNEXPECTED;
      FunctionStub::Destroy(stub);
    } else {
      PinModule();  // No backing out now.
      ::FlushInstructionCache(::GetCurrentProcess(), invoke, sizeof(PROC));
    }
  }
  ::VirtualProtect(invoke, sizeof(PROC), flags, &flags);
  return hr;
}

}  // end namespace buggy_bho
