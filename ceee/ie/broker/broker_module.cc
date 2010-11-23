// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Declaration of ATL module object for EXE module.

#include <atlbase.h>
#include <atlhost.h>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/logging_win.h"
#include "ceee/ie/broker/broker.h"
#include "ceee/ie/broker/broker_rpc_server.h"
#include "ceee/ie/broker/chrome_postman.h"
#include "ceee/ie/broker/executors_manager.h"
#include "ceee/ie/broker/resource.h"
#include "ceee/ie/broker/window_events_funnel.h"
#include "ceee/ie/common/crash_reporter.h"
#include "ceee/common/com_utils.h"
#include "chrome/common/url_constants.h"
#include "chrome_frame/metrics_service.h"

namespace {

const wchar_t kLogFileName[] = L"CeeeBroker.log";

// {6E3D6168-1DD2-4edb-A183-584C2C66E96D}
const GUID kCeeeBrokerLogProviderName =
    { 0x6e3d6168, 0x1dd2, 0x4edb,
        { 0xa1, 0x83, 0x58, 0x4c, 0x2c, 0x66, 0xe9, 0x6d } };

}  // namespace

// Object entries go here instead of with each object, so that we can keep
// the objects in a lib, and also to decrease the amount of magic.
OBJECT_ENTRY_AUTO(__uuidof(CeeeBroker), CeeeBroker)

class CeeeBrokerModule : public CAtlExeModuleT<CeeeBrokerModule> {
 public:
  CeeeBrokerModule();
  ~CeeeBrokerModule();

  DECLARE_LIBID(LIBID_CeeeBrokerLib)
  static HRESULT WINAPI UpdateRegistryAppId(BOOL register) throw();

  // We have our own version so that we can explicitly specify
  // that we want to be in a multi threaded apartment.
  static HRESULT InitializeCom();

  // Prevent COM objects we don't own to control our lock count.
  // To properly manage our lifespan, yet still be able to control the
  // lifespan of the ChromePostman's thread, we must only rely on the
  // CeeeBroker implementation of the IExternalConnection interface
  // as well as the ExecutorsManager map content to decide when to die.
  virtual LONG Lock() {
    return 1;
  }
  virtual LONG Unlock() {
    return 1;
  }

  // We prevent access to the module lock count from objects we don't
  // own by overriding the Un/Lock methods above. But when we want to
  // access the module lock count, we do it from here, and bypass our
  // override. These are the entry points that only our code accesses.
  LONG LockModule() {
    return CAtlExeModuleT<CeeeBrokerModule>::Lock();
  }
  LONG UnlockModule() {
    return CAtlExeModuleT<CeeeBrokerModule>::Unlock();
  }

  HRESULT PostMessageLoop();
  HRESULT PreMessageLoop(int show);
 private:
  // We maintain a postman COM object on the stack so that we can
  // properly initialize and terminate it at the right time.
  CComObjectStackEx<ChromePostman> chrome_postman_;
  CrashReporter crash_reporter_;
  base::AtExitManager at_exit_;
  BrokerRpcServer rpc_server_;
};

CeeeBrokerModule module;

extern "C" int WINAPI _tWinMain(HINSTANCE, HINSTANCE, LPTSTR, int nShowCmd) {
  return module.WinMain(nShowCmd);
}

CeeeBrokerModule::CeeeBrokerModule()
    : crash_reporter_(L"ceee_broker") {
  TRACE_EVENT_BEGIN("ceee.broker", this, "");

  wchar_t logfile_path[MAX_PATH];
  DWORD len = ::GetTempPath(arraysize(logfile_path), logfile_path);
  ::PathAppend(logfile_path, kLogFileName);

  // It seems we're obliged to initialize the current command line
  // before initializing logging.
  CommandLine::Init(0, NULL);

  logging::InitLogging(
      logfile_path,
      logging::LOG_TO_BOTH_FILE_AND_SYSTEM_DEBUG_LOG,
      logging::LOCK_LOG_FILE,
      logging::APPEND_TO_OLD_LOG_FILE);

  // Initialize ETW logging.
  logging::LogEventProvider::Initialize(kCeeeBrokerLogProviderName);

  // Initialize control hosting.
  BOOL initialized = AtlAxWinInit();
  DCHECK(initialized);

  // Needs to be called before we can use GURL.
  chrome::RegisterChromeSchemes();

  crash_reporter_.InitializeCrashReporting(false);
}

HRESULT CeeeBrokerModule::InitializeCom() {
  HRESULT hr = ::CoInitializeEx(NULL, COINIT_MULTITHREADED);
  if (FAILED(hr))
    return hr;

  // We need to initialize security before setting global options.
  hr = ::CoInitializeSecurity(NULL,
                              -1,
                              NULL,
                              NULL,
                              // Clients must identify.
                              RPC_C_IMP_LEVEL_IDENTIFY,
                              // And we identify.
                              RPC_C_IMP_LEVEL_IDENTIFY,
                              NULL,
                              // We don't want low integrity to be able to
                              // instantiate arbitrary objects in our process.
                              EOAC_NO_CUSTOM_MARSHAL,
                              NULL);
  DCHECK(SUCCEEDED(hr));

  // Ensure the marshaling machinery doesn't eat our crashes.
  CComPtr<IGlobalOptions> options;
  hr = options.CoCreateInstance(CLSID_GlobalOptions);
  if (SUCCEEDED(hr)) {
    hr = options->Set(COMGLB_EXCEPTION_HANDLING, COMGLB_EXCEPTION_DONOT_HANDLE);
  }
  DLOG_IF(WARNING, FAILED(hr)) << "IGlobalOptions::Set failed "
                               << com::LogHr(hr);

  // The above is best-effort, don't bail on error.
  return S_OK;
}

HRESULT CeeeBrokerModule::PreMessageLoop(int show) {
  // It's important to initialize the postman BEFORE we make the Broker
  // and the event funnel available, since we may get requests to execute
  // API invocation or Fire events before the postman is ready to handle them.
  chrome_postman_.Init();
  WindowEventsFunnel::Initialize();

  if (!rpc_server_.Start())
    return RPC_E_FAULT;

  // Initialize metrics. We need the rpc_server_ above to be available.
  MetricsService::Start();

  return CAtlExeModuleT<CeeeBrokerModule>::PreMessageLoop(show);
}

HRESULT CeeeBrokerModule::PostMessageLoop() {
  HRESULT hr = CAtlExeModuleT<CeeeBrokerModule>::PostMessageLoop();
  Singleton<ExecutorsManager,
            ExecutorsManager::SingletonTraits>()->Terminate();
  WindowEventsFunnel::Terminate();

  // Upload data if necessary.
  MetricsService::Stop();

  chrome_postman_.Term();
  return hr;
}

CeeeBrokerModule::~CeeeBrokerModule() {
  crash_reporter_.ShutdownCrashReporting();
  logging::CloseLogFile();

  TRACE_EVENT_END("ceee.broker", this, "");
}

HRESULT WINAPI CeeeBrokerModule::UpdateRegistryAppId(BOOL reg) throw() {
  return com::ModuleRegistrationWithoutAppid(IDR_BROKER_MODULE, reg);
}

namespace ceee_module_util {

LONG LockModule() {
  return module.LockModule();
}

LONG UnlockModule() {
  return module.UnlockModule();
}

}  // namespace
