// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Declaration of ATL module object and DLL exports.

#include "base/at_exit.h"
#include "base/atomic_ref_count.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/logging_win.h"
#include "ceee/common/com_utils.h"
#include "ceee/common/install_utils.h"
#include "ceee/ie/common/ceee_module_util.h"
#include "ceee/ie/plugin/bho/browser_helper_object.h"
#include "ceee/ie/plugin/bho/executor.h"
#include "ceee/ie/plugin/toolband/toolband_module_reporting.h"
#include "ceee/ie/plugin/toolband/tool_band.h"
#include "ceee/ie/plugin/toolband/toolband_proxy.h"
#include "ceee/ie/plugin/scripting/script_host.h"
#include "ceee/common/windows_constants.h"
#include "chrome/common/url_constants.h"

#include "toolband.h"  // NOLINT

namespace {

const wchar_t kLogFileName[] = L"ceee.log";

// {73213C1A-C369-4740-A75C-FA849E6CE540}
static const GUID kCeeeIeLogProviderName =
    { 0x73213c1a, 0xc369, 0x4740,
        { 0xa7, 0x5c, 0xfa, 0x84, 0x9e, 0x6c, 0xe5, 0x40 } };

// This is the Script Debugging state for all script engines we instantiate.
ScriptHost::DebugApplication debug_application(L"CEEE");

}  // namespace

// Object entries go here instead of with each object, so that we can move
// the objects in a lib, and also to decrease the amount of magic.
OBJECT_ENTRY_AUTO(CLSID_BrowserHelperObject, BrowserHelperObject)
OBJECT_ENTRY_AUTO(CLSID_ToolBand, ToolBand)
OBJECT_ENTRY_AUTO(CLSID_CeeeExecutorCreator, CeeeExecutorCreator)
OBJECT_ENTRY_AUTO(CLSID_CeeeExecutor, CeeeExecutor)

class ToolbandModule : public CAtlDllModuleT<ToolbandModule> {
 public:
  typedef CAtlDllModuleT<ToolbandModule> Super;

  ToolbandModule();
  ~ToolbandModule();

  DECLARE_LIBID(LIBID_ToolbandLib)

  // Needed to make sure we call Init/Term outside the loader lock.
  HRESULT DllCanUnloadNow();
  HRESULT DllGetClassObject(REFCLSID clsid, REFIID iid, void** object);
  void Init();
  void Term();
  bool module_initialized() const {
    return module_initialized_;
  }

  // Un/Register a hook to be unhooked by our termination safety net.
  // This is to protect ourselves against cases where the hook owner never
  // gets torn down (for whatever reason) and then the hook may be called when
  // we don't expect it to be.
  void RegisterHookForSafetyNet(HHOOK hook);
  void UnregisterHookForSafetyNet(HHOOK hook);

  // We override reg/unregserver to register proxy/stubs.
  HRESULT DllRegisterServer();
  HRESULT DllUnregisterServer();

 private:
  base::AtExitManager at_exit_;
  bool module_initialized_;
  bool crash_reporting_initialized_;
  // Accumulate Hooks that may need to be freed before unloading DLL.
  // Thread protected by m_csStaticDataInitAndTypeInfo.
  std::set<HHOOK> hooks_;
};

ToolbandModule::ToolbandModule()
    : crash_reporting_initialized_(false),
      module_initialized_(false) {
  wchar_t logfile_path[MAX_PATH];
  DWORD len = ::GetTempPath(arraysize(logfile_path), logfile_path);
  ::PathAppend(logfile_path, kLogFileName);

  // It seems we're obliged to initialize the current command line
  // before initializing logging. This feels a little strange for
  // a plugin.
  CommandLine::Init(0, NULL);

  logging::InitLogging(
      logfile_path,
      logging::LOG_TO_BOTH_FILE_AND_SYSTEM_DEBUG_LOG,
      logging::LOCK_LOG_FILE,
      logging::APPEND_TO_OLD_LOG_FILE);

  // Initialize ETW logging.
  logging::LogEventProvider::Initialize(kCeeeIeLogProviderName);

  // Initialize control hosting.
  BOOL initialized = AtlAxWinInit();
  DCHECK(initialized);

  // Needs to be called before we can use GURL.
  chrome::RegisterChromeSchemes();

  ScriptHost::set_default_debug_application(&debug_application);
}

ToolbandModule::~ToolbandModule() {
  ScriptHost::set_default_debug_application(NULL);

  // Uninitialize control hosting.
  BOOL uninitialized = AtlAxWinTerm();
  DCHECK(uninitialized);

  logging::CloseLogFile();
}

HRESULT ToolbandModule::DllCanUnloadNow() {
  HRESULT hr = Super::DllCanUnloadNow();
  if (hr == S_OK)
    Term();
  return hr;
}

HRESULT ToolbandModule::DllGetClassObject(REFCLSID clsid, REFIID iid,
                                         void** object)  {
  Init();
  return Super::DllGetClassObject(clsid, iid, object);
}

void ToolbandModule::RegisterHookForSafetyNet(HHOOK hook) {
  CComCritSecLock<CComCriticalSection> lock(m_csStaticDataInitAndTypeInfo);
  hooks_.insert(hook);
}

void ToolbandModule::UnregisterHookForSafetyNet(HHOOK hook) {
  CComCritSecLock<CComCriticalSection> lock(m_csStaticDataInitAndTypeInfo);
  hooks_.erase(hook);
}

HRESULT ToolbandModule::DllRegisterServer() {
  // No typelib registration.
  HRESULT hr = Super::DllRegisterServer();
  if (SUCCEEDED(hr) && !RegisterAsyncProxies(true)) {
    hr = SELFREG_E_CLASS;
  }
  return hr;
}

HRESULT ToolbandModule::DllUnregisterServer() {
  // No typelib registration.
  HRESULT hr = Super::DllUnregisterServer(FALSE);
  if (!RegisterAsyncProxies(false) && SUCCEEDED(hr)) {
    // Note the error.
    hr = SELFREG_E_CLASS;
  }
  return hr;
}

// No-op entry point to keep user-level registration happy.
// TODO(robertshield): Remove this as part of registration re-org.
STDAPI DllRegisterUserServer() {
  LOG(WARNING) << "Call to unimplemented DllRegisterUserServer.";
  return S_OK;
}

// No-op entry point to keep user-level unregistration happy.
// TODO(robertshield): Remove this as part of registration re-org.
STDAPI DllUnregisterUserServer() {
  LOG(WARNING) << "Call to unimplemented DllUnregisterUserServer.";
  return S_OK;
}

void ToolbandModule::Init() {
  // We must protect our data member against concurrent calls to check if we
  // can be unloaded. We must also making the call to Term within the lock
  // to make sure we don't try to re-initialize in case a new
  // DllGetClassObject would occur in the mean time, in another thread.
  CComCritSecLock<CComCriticalSection> lock(m_csStaticDataInitAndTypeInfo);
  if (module_initialized_)
    return;
  crash_reporting_initialized_ = InitializeCrashReporting();
  module_initialized_ = true;
}

void ToolbandModule::Term() {
  CComCritSecLock<CComCriticalSection> lock(m_csStaticDataInitAndTypeInfo);
  // We do this before checking if we were initialized because it is not
  // related to initialization or not, but to un/registration from outsiders.
  while (hooks_.size() > 0) {
    std::set<HHOOK>::iterator it = hooks_.begin();
    VLOG(1) << "Hook '" << *it << "' has fallen in our safety net.";
    ::UnhookWindowsHookEx(*it);
    hooks_.erase(it);
  }

  if (!module_initialized_)
    return;
  if (crash_reporting_initialized_) {
    bool crash_reporting_deinitialized = ShutdownCrashReporting();
    DCHECK(crash_reporting_deinitialized);
    crash_reporting_initialized_ = false;
  }
  module_initialized_ = false;
}

ToolbandModule module;

void ceee_module_util::Lock() {
  module.m_csStaticDataInitAndTypeInfo.Lock();
}

void ceee_module_util::Unlock() {
  module.m_csStaticDataInitAndTypeInfo.Unlock();
}

LONG ceee_module_util::LockModule() {
  return module.Lock();
}

LONG ceee_module_util::UnlockModule() {
  return module.Unlock();
}

void ceee_module_util::RegisterHookForSafetyNet(HHOOK hook) {
  module.RegisterHookForSafetyNet(hook);
}

void ceee_module_util::UnregisterHookForSafetyNet(HHOOK hook) {
  module.UnregisterHookForSafetyNet(hook);
}

// DLL Entry Point.
extern "C" BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason,
                               LPVOID reserved) {
  // Prevent us from being loaded by older versions of the shell.
  if (reason == DLL_PROCESS_ATTACH) {
    wchar_t main_exe[MAX_PATH] = { 0 };
    ::GetModuleFileName(NULL, main_exe, arraysize(main_exe));

    // We don't want to be loaded in the explorer process.
    _wcslwr_s(main_exe, arraysize(main_exe));
    if (wcsstr(main_exe, windows::kExplorerModuleName))
      return FALSE;
  }

  return module.DllMain(reason, reserved);
}

// Used to determine whether the DLL can be unloaded by OLE
STDAPI DllCanUnloadNow(void) {
  return module.DllCanUnloadNow();
}

// Returns a class factory to create an object of the requested type
STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv) {
  return module.DllGetClassObject(rclsid, riid, ppv);
}

// DllRegisterServer - Adds entries to the system registry
//
// This is not the actual entrypoint; see the define right below this
// function, which keeps us safe from ever forgetting to check for
// the --enable-ceee flag.
STDAPI DllRegisterServerImpl(void) {
  // Registers objects.
  HRESULT hr = module.DllRegisterServer();
  return hr;
}

CEEE_DEFINE_DLL_REGISTER_SERVER()

// DllUnregisterServer - Removes entries from the system registry
STDAPI DllUnregisterServer(void) {
  // We always allow unregistration, even if no --enable-ceee install flag.
  HRESULT hr = module.DllUnregisterServer();
  return hr;
}
