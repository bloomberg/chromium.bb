// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "blimp/client/app/blimp_startup.h"
#include "blimp/client/app/linux/blimp_client_context_delegate_linux.h"
#include "blimp/client/app/linux/blimp_display_manager.h"
#include "blimp/client/app/linux/blimp_display_manager_delegate_main.h"
#include "blimp/client/public/blimp_client_context.h"
#include "blimp/client/public/contents/blimp_navigation_controller.h"
#include "blimp/client/support/compositor/compositor_dependencies_impl.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/command_line_pref_store.h"
#include "components/prefs/in_memory_pref_store.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"
#include "skia/ext/fontmgr_default_linux.h"
#include "third_party/skia/include/ports/SkFontConfigInterface.h"
#include "third_party/skia/include/ports/SkFontMgr.h"
#include "third_party/skia/include/ports/SkFontMgr_android.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/x/x11_connection.h"

namespace {
// Specifies directory where android fonts are stored.
const char kAndroidFontsPath[] = "android-fonts-path";
const char kDefaultUrl[] = "https://www.google.com";
constexpr int kWindowWidth = 800;
constexpr int kWindowHeight = 600;

class BlimpShellCommandLinePrefStore : public CommandLinePrefStore {
 public:
  explicit BlimpShellCommandLinePrefStore(const base::CommandLine* command_line)
      : CommandLinePrefStore(command_line) {
    blimp::client::BlimpClientContext::ApplyBlimpSwitches(this);
  }

 protected:
  ~BlimpShellCommandLinePrefStore() override = default;
};

bool HasAndroidFontSwitch() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(kAndroidFontsPath);
}

std::string GetAndroidFontsDirectory() {
  std::string android_fonts_dir =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          kAndroidFontsPath);
  if (android_fonts_dir.size() > 0 && android_fonts_dir.back() != '/') {
    android_fonts_dir += '/';
  }
  return android_fonts_dir;
}

sk_sp<SkFontMgr> CreateAndroidFontMgr(std::string android_fonts_dir) {
  SkFontMgr_Android_CustomFonts custom;
  custom.fSystemFontUse =
      SkFontMgr_Android_CustomFonts::SystemFontUse::kOnlyCustom;
  custom.fBasePath = android_fonts_dir.c_str();

  std::string font_config;
  std::string fallback_font_config;
  if (android_fonts_dir.find("kitkat") != std::string::npos) {
    font_config = android_fonts_dir + "system_fonts.xml";
    fallback_font_config = android_fonts_dir + "fallback_fonts.xml";
    custom.fFallbackFontsXml = fallback_font_config.c_str();
  } else {
    font_config = android_fonts_dir + "fonts.xml";
    custom.fFallbackFontsXml = nullptr;
  }
  custom.fFontsXml = font_config.c_str();
  custom.fIsolated = true;

  return SkFontMgr_New_Android(&custom);
}

void SetupAndroidFontManager() {
  if (HasAndroidFontSwitch()) {
    SetDefaultSkiaFactory(CreateAndroidFontMgr(GetAndroidFontsDirectory()));
  }
}

void InitializeResourceBundle() {
  base::FilePath pak_file;
  bool pak_file_valid = base::PathService::Get(base::DIR_MODULE, &pak_file);
  CHECK(pak_file_valid);
  pak_file = pak_file.Append(FILE_PATH_LITERAL("blimp_shell.pak"));
  ui::ResourceBundle::InitSharedInstanceWithPakPath(pak_file);
}

}  // namespace

int main(int argc, const char**argv) {
  base::AtExitManager at_exit;
  base::CommandLine::Init(argc, argv);
  SetupAndroidFontManager();

  CHECK(gfx::InitializeThreadedX11());

  blimp::client::InitializeLogging();
  blimp::client::InitializeMainMessageLoop();
  InitializeResourceBundle();

  base::Thread io_thread("BlimpIOThread");
  base::Thread::Options options;
  options.message_loop_type = base::MessageLoop::TYPE_IO;
  io_thread.StartWithOptions(options);

  // Creating this using "new" and passing to context using "WrapUnique" as
  // opposed to "MakeUnique" because we'll need to pass the compositor
  // dependencies to the display manager as well.
  blimp::client::CompositorDependencies* compositor_dependencies =
      new blimp::client::CompositorDependenciesImpl();
  // Creating the context delegate before the context so that the context is
  // destroyed before the delegate.
  std::unique_ptr<blimp::client::BlimpClientContextDelegate> context_delegate =
      base::MakeUnique<blimp::client::BlimpClientContextDelegateLinux>();

  // Create PrefRegistry and register blimp preferences with it.
  scoped_refptr<user_prefs::PrefRegistrySyncable> pref_registry =
      new ::user_prefs::PrefRegistrySyncable();
  blimp::client::BlimpClientContext::RegisterPrefs(pref_registry.get());

  PrefServiceFactory pref_service_factory;

  // Create command line and user preference stores.
  pref_service_factory.set_command_line_prefs(
      make_scoped_refptr(new BlimpShellCommandLinePrefStore(
          base::CommandLine::ForCurrentProcess())));
  pref_service_factory.set_user_prefs(new InMemoryPrefStore());

  // Create a PrefService binding the PrefRegistry to the pref stores.
  // The PrefService owns the PrefRegistry and pref stores.
  std::unique_ptr<PrefService> pref_service =
      pref_service_factory.Create(pref_registry.get());

  auto context = base::WrapUnique<blimp::client::BlimpClientContext>(
      blimp::client::BlimpClientContext::Create(
          io_thread.task_runner(), io_thread.task_runner(),
          base::WrapUnique(compositor_dependencies), pref_service.get()));
  context->SetDelegate(context_delegate.get());

  context->Connect();

  // If there is a non-switch argument to the command line, load that url.
  base::CommandLine::StringVector args =
      base::CommandLine::ForCurrentProcess()->GetArgs();
  std::string url = args.size() > 0 ? args[0] : kDefaultUrl;
  std::unique_ptr<blimp::client::BlimpContents> contents =
      context->CreateBlimpContents(nullptr);
  contents->GetNavigationController().LoadURL(GURL(url));

  std::unique_ptr<blimp::client::BlimpDisplayManagerDelegate>
      display_manager_delegate =
          base::MakeUnique<blimp::client::BlimpDisplayManagerDelegateMain>();
  blimp::client::BlimpDisplayManager display_manager(
      display_manager_delegate.get(), compositor_dependencies);
  display_manager.SetWindowSize(gfx::Size(kWindowWidth, kWindowHeight));
  display_manager.SetBlimpContents(std::move(contents));

  base::RunLoop().Run();
}
