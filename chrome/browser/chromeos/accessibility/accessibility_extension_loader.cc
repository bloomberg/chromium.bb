// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/accessibility/accessibility_extension_loader.h"

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/path_service.h"
#include "chrome/browser/chromeos/login/lock/screen_locker.h"
#include "chrome/browser/chromeos/login/lock/webui_screen_locker.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/ui/webui_login_view.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/manifest_handlers/content_scripts_handler.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_api_frame_id_map.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/file_reader.h"
#include "extensions/browser/script_executor.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/extension_resource.h"

namespace chromeos {

namespace {

// Uses the ScriptExecutor associated with the given |render_view_host| to
// execute the given |code|.
void ExecuteScriptHelper(content::RenderViewHost* render_view_host,
                         const std::string& code,
                         const std::string& extension_id) {
  content::WebContents* web_contents =
      content::WebContents::FromRenderViewHost(render_view_host);
  if (!web_contents)
    return;
  if (!extensions::TabHelper::FromWebContents(web_contents))
    extensions::TabHelper::CreateForWebContents(web_contents);
  extensions::TabHelper::FromWebContents(web_contents)
      ->script_executor()
      ->ExecuteScript(HostID(HostID::EXTENSIONS, extension_id),
                      extensions::ScriptExecutor::JAVASCRIPT, code,
                      extensions::ScriptExecutor::INCLUDE_SUB_FRAMES,
                      extensions::ExtensionApiFrameIdMap::kTopFrameId,
                      extensions::ScriptExecutor::DONT_MATCH_ABOUT_BLANK,
                      extensions::UserScript::DOCUMENT_IDLE,
                      extensions::ScriptExecutor::ISOLATED_WORLD,
                      extensions::ScriptExecutor::DEFAULT_PROCESS,
                      GURL(),  // No webview src.
                      GURL(),  // No file url.
                      false,   // Not user gesture.
                      extensions::ScriptExecutor::NO_RESULT,
                      extensions::ScriptExecutor::ExecuteScriptCallback());
}

// Helper class that directly loads an extension's content scripts into
// all of the frames corresponding to a given RenderViewHost.
class ContentScriptLoader {
 public:
  // Initialize the ContentScriptLoader with the ID of the extension
  // and the RenderViewHost where the scripts should be loaded.
  ContentScriptLoader(const std::string& extension_id,
                      int render_process_id,
                      int render_view_id)
      : extension_id_(extension_id),
        render_process_id_(render_process_id),
        render_view_id_(render_view_id) {}

  // Call this once with the ExtensionResource corresponding to each
  // content script to be loaded.
  void AppendScript(extensions::ExtensionResource resource) {
    resources_.push(resource);
  }

  // Finally, call this method once to fetch all of the resources and
  // load them. This method will delete this object when done.
  void Run() {
    if (resources_.empty()) {
      delete this;
      return;
    }

    extensions::ExtensionResource resource = resources_.front();
    resources_.pop();
    scoped_refptr<FileReader> reader(new FileReader(
        resource,
        FileReader::OptionalFileThreadTaskCallback(),  // null callback.
        base::Bind(&ContentScriptLoader::OnFileLoaded,
                   base::Unretained(this))));
    reader->Start();
  }

 private:
  void OnFileLoaded(bool success, std::unique_ptr<std::string> data) {
    if (success) {
      content::RenderViewHost* render_view_host =
          content::RenderViewHost::FromID(render_process_id_, render_view_id_);
      if (render_view_host)
        ExecuteScriptHelper(render_view_host, *data, extension_id_);
    }
    Run();
  }

  std::string extension_id_;
  int render_process_id_;
  int render_view_id_;
  std::queue<extensions::ExtensionResource> resources_;
};

}  // namespace

AccessibilityExtensionLoader::AccessibilityExtensionLoader(
    const std::string& extension_id,
    const base::FilePath& extension_path,
    const base::Closure& unload_callback)
    : profile_(nullptr),
      extension_id_(extension_id),
      extension_path_(extension_path),
      loaded_on_lock_screen_(false),
      loaded_on_user_screen_(false),
      unload_callback_(unload_callback),
      weak_ptr_factory_(this) {}

AccessibilityExtensionLoader::~AccessibilityExtensionLoader() {}

void AccessibilityExtensionLoader::SetProfile(
    Profile* profile,
    const base::Closure& done_callback) {
  profile_ = profile;

  if (!loaded_on_user_screen_ && !loaded_on_lock_screen_)
    return;

  // If the extension was already enabled, but not for this profile, add it
  // to this profile.
  auto* extension_service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  auto* component_loader = extension_service->component_loader();
  if (!component_loader->Exists(extension_id_))
    LoadExtension(profile_, nullptr, done_callback);
}

void AccessibilityExtensionLoader::Load(Profile* profile,
                                        const std::string& init_script_str,
                                        const base::Closure& done_cb) {
  profile_ = profile;
  init_script_str_ = init_script_str;
  ScreenLocker* screen_locker = ScreenLocker::default_screen_locker();
  if (screen_locker && screen_locker->locked()) {
    // If on the lock screen, loads only to the lock screen as for
    // now. On unlock, it will be loaded to the user screen.
    // (see. AccessibilityExtensionLoader::Observe())
    LoadToLockScreen(done_cb);
  } else {
    LoadToUserScreen(done_cb);
  }
}

void AccessibilityExtensionLoader::Unload() {
  if (loaded_on_lock_screen_)
    UnloadFromLockScreen();

  if (loaded_on_user_screen_) {
    UnloadExtensionFromProfile(profile_);
    loaded_on_user_screen_ = false;
  }

  profile_ = nullptr;

  if (unload_callback_)
    unload_callback_.Run();
}

void AccessibilityExtensionLoader::LoadToUserScreen(
    const base::Closure& done_cb) {
  if (loaded_on_user_screen_)
    return;

  // Determine whether an OOBE screen is currently being shown. If so,
  // the extension will be injected directly into that screen.
  content::WebUI* login_web_ui = nullptr;

  if (ProfileHelper::IsSigninProfile(profile_)) {
    LoginDisplayHost* login_display_host = LoginDisplayHost::default_host();
    if (login_display_host) {
      WebUILoginView* web_ui_login_view =
          login_display_host->GetWebUILoginView();
      if (web_ui_login_view)
        login_web_ui = web_ui_login_view->GetWebUI();
    }

    // Lock screen uses the signin progile.
    loaded_on_lock_screen_ = true;
  }

  loaded_on_user_screen_ = true;
  LoadExtension(profile_,
                login_web_ui
                    ? login_web_ui->GetWebContents()->GetRenderViewHost()
                    : nullptr,
                done_cb);
}

void AccessibilityExtensionLoader::LoadToLockScreen(
    const base::Closure& done_cb) {
  if (loaded_on_lock_screen_)
    return;

  ScreenLocker* screen_locker = ScreenLocker::default_screen_locker();
  if (screen_locker && screen_locker->locked()) {
    content::WebUI* lock_web_ui = screen_locker->web_ui()->GetWebUI();
    if (lock_web_ui) {
      Profile* profile = Profile::FromWebUI(lock_web_ui);
      loaded_on_lock_screen_ = true;
      LoadExtension(profile, lock_web_ui->GetWebContents()->GetRenderViewHost(),
                    done_cb);
    }
  }
}

//
// private
//

void AccessibilityExtensionLoader::UnloadExtensionFromProfile(
    Profile* profile) {
  ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  extension_service->component_loader()->Remove(extension_path_);
}

void AccessibilityExtensionLoader::UnloadFromLockScreen() {
  // Lock screen uses the signin progile.
  Profile* signin_profile = ProfileHelper::GetSigninProfile();
  UnloadExtensionFromProfile(signin_profile);
  loaded_on_lock_screen_ = false;
}

void AccessibilityExtensionLoader::InjectContentScriptAndCallback(
    ExtensionService* extension_service,
    int render_process_id,
    int render_view_id,
    const base::Closure& done_cb) {
  // Make sure to always run |done_cb|.  The extension was loaded even
  // if we end up not injecting into this particular render view.
  base::ScopedClosureRunner done_runner(done_cb);
  content::RenderViewHost* render_view_host =
      content::RenderViewHost::FromID(render_process_id, render_view_id);
  if (!render_view_host)
    return;
  const content::WebContents* web_contents =
      content::WebContents::FromRenderViewHost(render_view_host);
  GURL content_url;
  if (web_contents)
    content_url = web_contents->GetLastCommittedURL();
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(extension_service->profile())
          ->enabled_extensions()
          .GetByID(extension_id_);

  if (!init_script_str_.empty()) {
    ExecuteScriptHelper(render_view_host, init_script_str_, extension->id());
  }

  // Inject the content scripts.
  ContentScriptLoader* loader = new ContentScriptLoader(
      extension->id(), render_view_host->GetProcess()->GetID(),
      render_view_host->GetRoutingID());

  const extensions::UserScriptList& content_scripts =
      extensions::ContentScriptsInfo::GetContentScripts(extension);
  for (const std::unique_ptr<extensions::UserScript>& script :
       content_scripts) {
    if (web_contents && !script->MatchesURL(content_url))
      continue;
    for (const std::unique_ptr<extensions::UserScript::File>& file :
         script->js_scripts()) {
      extensions::ExtensionResource resource =
          extension->GetResource(file->relative_path());
      loader->AppendScript(resource);
    }
  }
  loader->Run();  // It cleans itself up when done.
}

void AccessibilityExtensionLoader::LoadExtension(
    Profile* profile,
    content::RenderViewHost* render_view_host,
    base::Closure done_cb) {
  ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  if (render_view_host) {
    // Wrap the passed in callback to inject the content script.
    done_cb = base::Bind(
        &AccessibilityExtensionLoader::InjectContentScriptAndCallback,
        weak_ptr_factory_.GetWeakPtr(), extension_service,
        render_view_host->GetProcess()->GetID(),
        render_view_host->GetRoutingID(), done_cb);
  }

  extension_service->component_loader()->AddComponentFromDir(
      extension_path_, extension_id_.c_str(), done_cb);
}

}  // namespace chromeos
