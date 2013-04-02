// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/startup_helper.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/message_loop.h"
#include "base/run_loop.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/sandboxed_unpacker.h"
#include "chrome/browser/extensions/webstore_startup_installer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/api/i18n/default_locale_handler.h"
#include "chrome/common/extensions/background_info.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/incognito_handler.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "ipc/ipc_message.h"

using content::BrowserThread;

namespace {

void PrintPackExtensionMessage(const std::string& message) {
  printf("%s\n", message.c_str());
}

}  // namespace

namespace extensions {

StartupHelper::StartupHelper() : pack_job_succeeded_(false) {
  (new DefaultLocaleHandler)->Register();
  (new BackgroundManifestHandler)->Register();
  (new IncognitoHandler)->Register();
}

void StartupHelper::OnPackSuccess(
    const base::FilePath& crx_path,
    const base::FilePath& output_private_key_path) {
  pack_job_succeeded_ = true;
  PrintPackExtensionMessage(
      UTF16ToUTF8(
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
  const string16& error() { return error_; }

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
                               const Extension* extension) OVERRIDE {
    finished_ = true;
    success_ = true;
    BrowserThread::PostTask(BrowserThread::UI,
                            FROM_HERE,
                            base::Bind(&ValidateCrxHelper::FinishOnUIThread,
                                       this));
  }

  virtual void OnUnpackFailure(const string16& error) OVERRIDE {
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
                              true /* out of process */,
                              Manifest::INTERNAL,
                              0, /* no special creation flags */
                              temp_dir_,
                              file_thread_proxy,
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
  string16 error_;
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
    *error = UTF16ToUTF8(helper->error());
  return success;
}

bool StartupHelper::UninstallExtension(const CommandLine& cmd_line,
                                       Profile* profile) {
  DCHECK(profile);

  if (!cmd_line.HasSwitch(switches::kUninstallExtension))
    return false;

  ExtensionService* extension_service = profile->GetExtensionService();
  if (!extension_service)
    return false;

  std::string extension_id = cmd_line.GetSwitchValueASCII(
      switches::kUninstallExtension);
  return ExtensionService::UninstallExtensionHelper(extension_service,
                                                    extension_id);
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
  void OnAppInstallComplete(bool success, const std::string& error);

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
  installer_ = new WebstoreStartupInstaller(
      id,
      profile,
      show_prompt,
      callback);
  installer_->BeginInstall();
}

void AppInstallHelper::OnAppInstallComplete(bool success,
                                            const std::string& error) {
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
  helper.BeginInstall(profile, id,
                      !cmd_line.HasSwitch(switches::kForceAppMode),
                      MessageLoop::QuitWhenIdleClosure());

  MessageLoop::current()->Run();
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
  helper->BeginInstall(profile, id, true /*show_prompt*/,
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
