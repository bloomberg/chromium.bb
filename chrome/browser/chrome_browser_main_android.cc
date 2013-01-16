// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_main_android.h"

#include "base/path_service.h"
#include "chrome/app/breakpad_linux.h"
#include "chrome/browser/android/crash_dump_manager.h"
#include "chrome/common/env_vars.h"
#include "content/public/browser/android/compositor.h"
#include "content/public/common/main_function_params.h"
#include "net/android/network_change_notifier_factory_android.h"
#include "net/base/network_change_notifier.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"

ChromeBrowserMainPartsAndroid::ChromeBrowserMainPartsAndroid(
    const content::MainFunctionParams& parameters)
    : ChromeBrowserMainParts(parameters) {
}

ChromeBrowserMainPartsAndroid::~ChromeBrowserMainPartsAndroid() {
}

void ChromeBrowserMainPartsAndroid::PreProfileInit() {
#if defined(USE_LINUX_BREAKPAD)
#if defined(GOOGLE_CHROME_BUILD)
  // TODO(jcivelli): we should not initialize the crash-reporter when it was not
  // enabled. Right now if it is disabled we still generate the minidumps but we
  // do not upload them.
  bool breakpad_enabled = true;
#else
  bool breakpad_enabled = false;
#endif
  // Allow Breakpad to be enabled in Chromium builds for testing purposes.
  if (!breakpad_enabled)
    breakpad_enabled = getenv(env_vars::kEnableBreakpad) != NULL;

  if (breakpad_enabled) {
    InitCrashReporter();
    crash_dump_manager_.reset(new CrashDumpManager());
  }
#endif

  ChromeBrowserMainParts::PreProfileInit();
}

void ChromeBrowserMainPartsAndroid::PreEarlyInitialization() {
  net::NetworkChangeNotifier::SetFactory(
      new net::NetworkChangeNotifierFactoryAndroid());

  content::Compositor::Initialize();

  // Chrome on Android does not use default MessageLoop. It has its own
  // Android specific MessageLoop.
  DCHECK(!main_message_loop_.get());
  main_message_loop_.reset(new MessageLoop(MessageLoop::TYPE_UI));
  MessageLoopForUI::current()->Start();

  ChromeBrowserMainParts::PreEarlyInitialization();
}

int ChromeBrowserMainPartsAndroid::PreCreateThreads() {
  // PreCreateThreads initializes ResourceBundle instance.
  const int result = ChromeBrowserMainParts::PreCreateThreads();

  // Add devtools_resources.pak which is used in Chromium TestShell.
  FilePath paks_path;
  PathService::Get(ui::DIR_RESOURCE_PAKS_ANDROID, &paks_path);
  ResourceBundle::GetSharedInstance().AddOptionalDataPackFromPath(
      paks_path.Append(FILE_PATH_LITERAL("devtools_resources.pak")),
      ui::SCALE_FACTOR_NONE);

  return result;
}

void ChromeBrowserMainPartsAndroid::ShowMissingLocaleMessageBox() {
  NOTREACHED();
}

void RecordBreakpadStatusUMA(MetricsService* metrics) {
  // TODO: crbug.com/139023
  NOTIMPLEMENTED();
}

void WarnAboutMinimumSystemRequirements() {
}
