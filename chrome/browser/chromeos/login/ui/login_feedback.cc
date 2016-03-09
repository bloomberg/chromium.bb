// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/ui/login_feedback.h"

#include "base/bind.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/extensions/api/feedback_private/feedback_private_api.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "grit/browser_resources.h"
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

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// LoginFeedback::ScopedFeedbackExtension

class LoginFeedback::ScopedFeedbackExtension
    : public extensions::ExtensionRegistryObserver {
 public:
  explicit ScopedFeedbackExtension(LoginFeedback* owner);
  ~ScopedFeedbackExtension() override;

  // extensions::ExtensionRegistryObserver
  void OnExtensionReady(content::BrowserContext* browser_context,
                        const extensions::Extension* extension) override;

 private:
  LoginFeedback* const owner_;
  DISALLOW_COPY_AND_ASSIGN(ScopedFeedbackExtension);
};

LoginFeedback::ScopedFeedbackExtension::ScopedFeedbackExtension(
    LoginFeedback* owner)
    : owner_(owner) {
  Profile* profile = owner_->profile_;
  extensions::ExtensionRegistry::Get(profile)->AddObserver(this);

  extensions::ComponentLoader* component_loader = GetComponentLoader(profile);
  DCHECK(!component_loader->Exists(extension_misc::kFeedbackExtensionId))
      << "Feedback extension is already loaded. Is there any other "
         "LoginFeedback instance running?";
  component_loader->Add(IDR_FEEDBACK_MANIFEST,
                        base::FilePath(FILE_PATH_LITERAL("feedback")));
}

LoginFeedback::ScopedFeedbackExtension::~ScopedFeedbackExtension() {
  Profile* profile = owner_->profile_;
  extensions::ExtensionRegistry::Get(profile)->RemoveObserver(this);
  GetComponentLoader(profile)->Remove(extension_misc::kFeedbackExtensionId);
}

void LoginFeedback::ScopedFeedbackExtension::OnExtensionReady(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension) {
  if (extension->id() != extension_misc::kFeedbackExtensionId)
    return;
  owner_->OnFeedbackExtensionReady();
}

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

  feedback_extension_.reset(new ScopedFeedbackExtension(this));
}

void LoginFeedback::OnFeedbackExtensionReady() {
  feedback_window_handler_.reset(new FeedbackWindowHandler(this));
  EnsureFeedbackUI();
}

void LoginFeedback::EnsureFeedbackUI() {
  // Bail if any feedback app window is opened.
  if (feedback_window_handler_->HasFeedbackAppWindow())
    return;

  extensions::FeedbackPrivateAPI* api =
      extensions::FeedbackPrivateAPI::GetFactoryInstance()->Get(profile_);
  api->RequestFeedbackForFlow(
      description_, "Login", GURL(),
      extensions::api::feedback_private::FeedbackFlow::FEEDBACK_FLOW_LOGIN);

  // Check feedback app window shortly after to ensure it is opened. There is
  // some racing between the event dispatching and the starting of the
  // background page. This is a poor man's way to make sure the event is
  // delivered.
  // TODO(xiyuan): Investigate why the occasional event lost and remove this.
  const int kCheckDelaySecond = 1;
  content::BrowserThread::PostDelayedTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(&LoginFeedback::EnsureFeedbackUI, weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromSeconds(kCheckDelaySecond));
}

void LoginFeedback::OnFeedbackFinished() {
  if (!finished_callback_.is_null())
    finished_callback_.Run();
}

}  // namespace chromeos
