// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/startup_helper.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/sandboxed_unpacker.h"
#include "chrome/browser/extensions/webstore_startup_installer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/chrome_extensions_client.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/extension.h"
#include "ipc/ipc_message.h"

#if defined(OS_WIN)
#include "apps/app_window.h"
#include "apps/app_window_registry.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_util.h"
#endif

using content::BrowserThread;

namespace {

void PrintPackExtensionMessage(const std::string& message) {
  VLOG(1) << message;
}

// On Windows, the jumplist action for installing an ephemeral app has to use
// the --install-from-webstore command line arg to initiate an install.
scoped_refptr<extensions::WebstoreStandaloneInstaller>
CreateEphemeralAppInstaller(
    Profile* profile,
    const std::string& app_id,
    extensions::WebstoreStandaloneInstaller::Callback callback) {
  scoped_refptr<extensions::WebstoreStandaloneInstaller> installer;

#if defined(OS_WIN)
  using extensions::ExtensionRegistry;
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile);
  DCHECK(registry);
  if (!registry->GetExtensionById(app_id, ExtensionRegistry::EVERYTHING) ||
      !extensions::util::IsEphemeralApp(app_id, profile)) {
    return installer;
  }

  apps::AppWindowRegistry* app_window_registry =
      apps::AppWindowRegistry::Get(profile);
  DCHECK(app_window_registry);
  apps::AppWindow* app_window =
      app_window_registry->GetCurrentAppWindowForApp(app_id);
  if (!app_window)
    return installer;

  installer = new extensions::WebstoreInstallWithPrompt(
      app_id, profile, app_window->GetNativeWindow(), callback);
#endif

  return installer;
}

}  // namespace

namespace extensions {

StartupHelper::StartupHelper() : pack_job_succeeded_(false) {
  ExtensionsClient::Set(ChromeExtensionsClient::GetInstance());
}

void StartupHelper::OnPackSuccess(
    const base::FilePath& crx_path,
    const base::FilePath& output_private_key_path) {
  pack_job_succeeded_ = true;
  PrintPackExtensionMessage(
      base::UTF16ToUTF8(
          PackExtensionJob::StandardSuccessMessage(crx_path,
                                                   output_private_key_path)));
}

void StartupHelper::OnPackFailure(const std::string& error_message,
                                  ExtensionCreator::ErrorType type) {
  PrintPackExtensionMessage(error_message);
}

bool StartupHelper::PackExtension(const CommandLine& cmd_line) {
  if (!cmd_line.HasSwitch(switches::kPackExtension))
    return false;

  // Input Paths.
  base::FilePath src_dir =
      cmd_line.GetSwitchValuePath(switches::kPackExtension);
  base::FilePath private_key_path;
  if (cmd_line.HasSwitch(switches::kPackExtensionKey)) {
    private_key_path = cmd_line.GetSwitchValuePath(switches::kPackExtensionKey);
  }

  // Launch a job to perform the packing on the file thread.  Ignore warnings
  // from the packing process. (e.g. Overwrite any existing crx file.)
  pack_job_ = new PackExtensionJob(this, src_dir, private_key_path,
                                   ExtensionCreator::kOverwriteCRX);
  pack_job_->set_asynchronous(false);
  pack_job_->Start();

  return pack_job_succeeded_;
}

namespace {

class ValidateCrxHelper : public SandboxedUnpackerClient {
 public:
  ValidateCrxHelper(const base::FilePath& crx_file,
                    const base::FilePath& temp_dir,
                    base::RunLoop* run_loop)
      : crx_file_(crx_file), temp_dir_(temp_dir), run_loop_(run_loop),
        finished_(false), success_(false) {}

  bool finished() { return finished_; }
  bool success() { return success_; }
  const base::string16& error() { return error_; }

  void Start() {
    BrowserThread::PostTask(BrowserThread::FILE,
                            FROM_HERE,
                            base::Bind(&ValidateCrxHelper::StartOnFileThread,
                                       this));
  }

 protected:
  virtual ~ValidateCrxHelper() {}

  virtual void OnUnpackSuccess(const base::FilePath& temp_dir,
                               const base::FilePath& extension_root,
                               const base::DictionaryValue* original_manifest,
                               const Extension* extension,
                               const SkBitmap& install_icon) OVERRIDE {
    finished_ = true;
    success_ = true;
    BrowserThread::PostTask(BrowserThread::UI,
                            FROM_HERE,
                            base::Bind(&ValidateCrxHelper::FinishOnUIThread,
                                       this));
  }

  virtual void OnUnpackFailure(const base::string16& error) OVERRIDE {
    finished_ = true;
    success_ = false;
    error_ = error;
    BrowserThread::PostTask(BrowserThread::UI,
                            FROM_HERE,
                            base::Bind(&ValidateCrxHelper::FinishOnUIThread,
                                       this));
  }

  void FinishOnUIThread() {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    if (run_loop_->running())
      run_loop_->Quit();
  }

  void StartOnFileThread() {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
    scoped_refptr<base::MessageLoopProxy> file_thread_proxy =
        BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE);

    scoped_refptr<SandboxedUnpacker> unpacker(
        new SandboxedUnpacker(crx_file_,
                              Manifest::INTERNAL,
                              0, /* no special creation flags */
                              temp_dir_,
                              file_thread_proxy.get(),
                              this));
    unpacker->Start();
  }

  // The file being validated.
  const base::FilePath& crx_file_;

  // The temporary directory where the sandboxed unpacker will do work.
  const base::FilePath& temp_dir_;

  // Unowned pointer to a runloop, so our consumer can wait for us to finish.
  base::RunLoop* run_loop_;

  // Whether we're finished unpacking;
  bool finished_;

  // Whether the unpacking was successful.
  bool success_;

  // If the unpacking wasn't successful, this contains an error message.
  base::string16 error_;
};

}  // namespace

bool StartupHelper::ValidateCrx(const CommandLine& cmd_line,
                                std::string* error) {
  CHECK(error);
  base::FilePath path = cmd_line.GetSwitchValuePath(switches::kValidateCrx);
  if (path.empty()) {
    *error = base::StringPrintf("Empty path passed for %s",
                                switches::kValidateCrx);
    return false;
  }
  base::ScopedTempDir temp_dir;

  if (!temp_dir.CreateUniqueTempDir()) {
    *error = std::string("Failed to create temp dir");
    return false;
  }

  base::RunLoop run_loop;
  scoped_refptr<ValidateCrxHelper> helper(
      new ValidateCrxHelper(path, temp_dir.path(), &run_loop));
  helper->Start();
  if (!helper->finished())
    run_loop.Run();

  bool success = helper->success();
  if (!success)
    *error = base::UTF16ToUTF8(helper->error());
  return success;
}

namespace {

class AppInstallHelper {
 public:
  // A callback for when the install process is done.
  typedef base::Callback<void()> DoneCallback;

  AppInstallHelper();
  virtual ~AppInstallHelper();
  bool success() { return success_; }
  const std::string& error() { return error_; }
  void BeginInstall(Profile* profile,
                    const std::string& id,
                    bool show_prompt,
                    DoneCallback callback);

 private:
  WebstoreStandaloneInstaller::Callback Callback();
  void OnAppInstallComplete(bool success,
                            const std::string& error,
                            webstore_install::Result result);

  DoneCallback done_callback_;

  // These hold on to the result of the app install when it is complete.
  bool success_;
  std::string error_;

  scoped_refptr<WebstoreStandaloneInstaller> installer_;
};

AppInstallHelper::AppInstallHelper() : success_(false) {}

AppInstallHelper::~AppInstallHelper() {}

WebstoreStandaloneInstaller::Callback AppInstallHelper::Callback() {
  return base::Bind(&AppInstallHelper::OnAppInstallComplete,
                    base::Unretained(this));
}

void AppInstallHelper::BeginInstall(
    Profile* profile,
    const std::string& id,
    bool show_prompt,
    DoneCallback done_callback) {
  done_callback_ = done_callback;

  WebstoreStandaloneInstaller::Callback callback =
      base::Bind(&AppInstallHelper::OnAppInstallComplete,
                 base::Unretained(this));

  installer_ = CreateEphemeralAppInstaller(profile, id, callback);
  if (!installer_.get()) {
    installer_ =
        new WebstoreStartupInstaller(id, profile, show_prompt, callback);
  }
  installer_->BeginInstall();
}

void AppInstallHelper::OnAppInstallComplete(bool success,
                                            const std::string& error,
                                            webstore_install::Result result) {
  success_ = success;
  error_= error;
  done_callback_.Run();
}

void DeleteHelperAndRunCallback(AppInstallHelper* helper,
                                base::Callback<void()> callback) {
  delete helper;
  callback.Run();
}

}  // namespace

bool StartupHelper::InstallFromWebstore(const CommandLine& cmd_line,
                                        Profile* profile) {
  std::string id = cmd_line.GetSwitchValueASCII(switches::kInstallFromWebstore);
  if (!Extension::IdIsValid(id)) {
    LOG(ERROR) << "Invalid id for " << switches::kInstallFromWebstore
               << " : '" << id << "'";
    return false;
  }

  AppInstallHelper helper;
  base::RunLoop run_loop;
  helper.BeginInstall(profile, id, true, run_loop.QuitClosure());
  run_loop.Run();

  if (!helper.success())
    LOG(ERROR) << "InstallFromWebstore failed with error: " << helper.error();
  return helper.success();
}

void StartupHelper::LimitedInstallFromWebstore(
    const CommandLine& cmd_line,
    Profile* profile,
    base::Callback<void()> done_callback) {
  std::string id = WebStoreIdFromLimitedInstallCmdLine(cmd_line);
  if (!Extension::IdIsValid(id)) {
    LOG(ERROR) << "Invalid index for " << switches::kLimitedInstallFromWebstore;
    done_callback.Run();
    return;
  }

  AppInstallHelper* helper = new AppInstallHelper();
  helper->BeginInstall(profile, id, false /*show_prompt*/,
                       base::Bind(&DeleteHelperAndRunCallback,
                                  helper, done_callback));
}

std::string StartupHelper::WebStoreIdFromLimitedInstallCmdLine(
    const CommandLine& cmd_line) {
  std::string index = cmd_line.GetSwitchValueASCII(
      switches::kLimitedInstallFromWebstore);
  std::string id;
  if (index == "1") {
    id = "nckgahadagoaajjgafhacjanaoiihapd";
  } else if (index == "2") {
    id = "ecglahbcnmdpdciemllbhojghbkagdje";
  }
  return id;
}

StartupHelper::~StartupHelper() {
  if (pack_job_.get())
    pack_job_->ClearClient();
}

}  // namespace extensions
