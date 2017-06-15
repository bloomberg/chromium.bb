// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/ui/login_feedback.h"

#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/extensions/api/feedback_private/feedback_private_api.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/grit/browser_resources.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_manager_observer.h"
#include "extensions/common/extension.h"
#include "ui/aura/window.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_util.h"

namespace chromeos {

namespace {

extensions::ComponentLoader* GetComponentLoader(
    content::BrowserContext* context) {
  extensions::ExtensionSystem* extension_system =
      extensions::ExtensionSystem::Get(context);
  ExtensionService* extension_service = extension_system->extension_service();
  return extension_service->component_loader();
}

extensions::ProcessManager* GetProcessManager(
    content::BrowserContext* context) {
  return extensions::ProcessManager::Get(context);
}

// Ensures that the feedback extension is loaded on the signin profile and
// invokes the callback when the extension is ready to use. Unload the
// extension and delete itself when the extension's background page shuts down.
class FeedbackExtensionLoader : public extensions::ProcessManagerObserver,
                                public content::WebContentsObserver {
 public:
  // Loads the feedback extension on the given profile and invokes
  // |on_ready_callback| when it is ready.
  static void Load(Profile* profile, const base::Closure& on_ready_callback);

 private:
  explicit FeedbackExtensionLoader(Profile* profile);
  ~FeedbackExtensionLoader() override;

  void Initialize();

  void AddOnReadyCallback(const base::Closure& on_ready_callback);
  void RunOnReadyCallbacks();

  // extensions::ProcessManagerObserver
  void OnBackgroundHostCreated(extensions::ExtensionHost* host) override;
  void OnBackgroundHostClose(const std::string& extension_id) override;

  // content::WebContentsObserver
  void DocumentOnLoadCompletedInMainFrame() override;

  Profile* const profile_;
  std::vector<base::Closure> on_ready_callbacks_;
  bool ready_ = false;

  DISALLOW_COPY_AND_ASSIGN(FeedbackExtensionLoader);
};

// Current live instance of FeedbackExtensionLoader.
FeedbackExtensionLoader* instance = nullptr;

// static
void FeedbackExtensionLoader::Load(Profile* profile,
                                   const base::Closure& on_ready_callback) {
  if (instance == nullptr) {
    instance = new FeedbackExtensionLoader(profile);
    instance->Initialize();
  }

  DCHECK_EQ(instance->profile_, profile);
  DCHECK(!on_ready_callback.is_null());
  instance->AddOnReadyCallback(on_ready_callback);
}

FeedbackExtensionLoader::FeedbackExtensionLoader(Profile* profile)
    : profile_(profile) {}

FeedbackExtensionLoader::~FeedbackExtensionLoader() {
  DCHECK_EQ(instance, this);
  instance = nullptr;

  GetProcessManager(profile_)->RemoveObserver(this);
  GetComponentLoader(profile_)->Remove(extension_misc::kFeedbackExtensionId);
}

void FeedbackExtensionLoader::Initialize() {
  extensions::ProcessManager* pm = GetProcessManager(profile_);
  pm->AddObserver(this);
  extensions::ExtensionHost* const host =
      pm->GetBackgroundHostForExtension(extension_misc::kFeedbackExtensionId);
  if (host) {
    OnBackgroundHostCreated(host);
    if (!host->host_contents()->IsLoading())
      DocumentOnLoadCompletedInMainFrame();
    return;
  }

  extensions::ComponentLoader* component_loader = GetComponentLoader(profile_);
  if (!component_loader->Exists(extension_misc::kFeedbackExtensionId)) {
    component_loader->Add(IDR_FEEDBACK_MANIFEST,
                          base::FilePath(FILE_PATH_LITERAL("feedback")));
  }
}

void FeedbackExtensionLoader::AddOnReadyCallback(
    const base::Closure& on_ready_callback) {
  on_ready_callbacks_.push_back(on_ready_callback);
  if (ready_)
    RunOnReadyCallbacks();
}

void FeedbackExtensionLoader::RunOnReadyCallbacks() {
  std::vector<base::Closure> callbacks;
  callbacks.swap(on_ready_callbacks_);

  for (const auto& callback : callbacks)
    callback.Run();
}

void FeedbackExtensionLoader::OnBackgroundHostCreated(
    extensions::ExtensionHost* host) {
  if (host->extension_id() == extension_misc::kFeedbackExtensionId)
    Observe(host->host_contents());
}

void FeedbackExtensionLoader::OnBackgroundHostClose(
    const std::string& extension_id) {
  if (extension_id == extension_misc::kFeedbackExtensionId)
    delete this;
}

void FeedbackExtensionLoader::DocumentOnLoadCompletedInMainFrame() {
  ready_ = true;
  RunOnReadyCallbacks();
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// LoginFeedback::FeedbackWindowHandler

class LoginFeedback::FeedbackWindowHandler
    : public extensions::AppWindowRegistry::Observer {
 public:
  explicit FeedbackWindowHandler(LoginFeedback* owner);
  ~FeedbackWindowHandler() override;

  bool HasFeedbackAppWindow() const;

  // extensions::AppWindowRegistry::Observer
  void OnAppWindowAdded(extensions::AppWindow* app_window) override;
  void OnAppWindowRemoved(extensions::AppWindow* app_window) override;

 private:
  LoginFeedback* const owner_;
  extensions::AppWindowRegistry* const window_registry_;

  DISALLOW_COPY_AND_ASSIGN(FeedbackWindowHandler);
};

LoginFeedback::FeedbackWindowHandler::FeedbackWindowHandler(
    LoginFeedback* owner)
    : owner_(owner),
      window_registry_(extensions::AppWindowRegistry::Get(owner_->profile_)) {
  window_registry_->AddObserver(this);
}

LoginFeedback::FeedbackWindowHandler::~FeedbackWindowHandler() {
  window_registry_->RemoveObserver(this);
}

bool LoginFeedback::FeedbackWindowHandler::HasFeedbackAppWindow() const {
  return !window_registry_
              ->GetAppWindowsForApp(extension_misc::kFeedbackExtensionId)
              .empty();
}

void LoginFeedback::FeedbackWindowHandler::OnAppWindowAdded(
    extensions::AppWindow* app_window) {
  if (app_window->extension_id() != extension_misc::kFeedbackExtensionId)
    return;

  // Move the feedback window to the same container as the login screen and make
  // it a transient child of the login screen.
  views::Widget::ReparentNativeView(
      app_window->GetNativeWindow(),
      LoginDisplayHost::default_host()->GetNativeWindow()->parent());
  wm::AddTransientChild(LoginDisplayHost::default_host()->GetNativeWindow(),
                        app_window->GetNativeWindow());
}

void LoginFeedback::FeedbackWindowHandler::OnAppWindowRemoved(
    extensions::AppWindow* app_window) {
  if (app_window->extension_id() != extension_misc::kFeedbackExtensionId)
    return;

  if (!HasFeedbackAppWindow())
    owner_->OnFeedbackFinished();
}

////////////////////////////////////////////////////////////////////////////////
// LoginFeedback

LoginFeedback::LoginFeedback(Profile* signin_profile)
    : profile_(signin_profile), weak_factory_(this) {}

LoginFeedback::~LoginFeedback() {}

void LoginFeedback::Request(const std::string& description,
                            const base::Closure& finished_callback) {
  description_ = description;
  finished_callback_ = finished_callback;
  feedback_window_handler_.reset(new FeedbackWindowHandler(this));

  FeedbackExtensionLoader::Load(
      profile_,
      base::Bind(&LoginFeedback::EnsureFeedbackUI, weak_factory_.GetWeakPtr()));

  // Triggers the extension background to be loaded.
  EnsureFeedbackUI();
}

void LoginFeedback::EnsureFeedbackUI() {
  // Bail if any feedback app window is opened.
  if (feedback_window_handler_->HasFeedbackAppWindow())
    return;

  extensions::FeedbackPrivateAPI* api =
      extensions::FeedbackPrivateAPI::GetFactoryInstance()->Get(profile_);
  api->RequestFeedbackForFlow(
      description_, "Login", std::string(), GURL(),
      extensions::api::feedback_private::FeedbackFlow::FEEDBACK_FLOW_LOGIN);

  // Make sure there is a feedback app window opened.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&LoginFeedback::EnsureFeedbackUI, weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromSeconds(1));
}

void LoginFeedback::OnFeedbackFinished() {
  if (!finished_callback_.is_null())
    finished_callback_.Run();
}

}  // namespace chromeos
