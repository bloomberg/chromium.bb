// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extensions/web_app_extension_shortcut_mac.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/threading/scoped_blocking_call.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_ui_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_shortcut_mac.h"
#include "chrome/browser/web_applications/extensions/web_app_extension_shortcut.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#import "chrome/common/mac/app_mode_common.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"

using content::BrowserThread;

namespace {

// Class that invokes a provided |callback| when destroyed, and supplies a means
// to keep the instance alive via posted tasks. The provided |callback| will
// always be invoked on the UI thread.
class Latch : public base::RefCountedThreadSafe<
                  Latch,
                  content::BrowserThread::DeleteOnUIThread> {
 public:
  explicit Latch(base::OnceClosure callback) : callback_(std::move(callback)) {}

  // Wraps a reference to |this| in a Closure and returns it. Running the
  // Closure does nothing. The Closure just serves to keep a reference alive
  // until |this| is ready to be destroyed; invoking the |callback|.
  base::RepeatingClosure NoOpClosure() {
    return base::BindRepeating(base::DoNothing::Repeatedly<Latch*>(),
                               base::RetainedRef(this));
  }

 private:
  friend class base::RefCountedThreadSafe<Latch>;
  friend class base::DeleteHelper<Latch>;
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::UI>;

  ~Latch() { std::move(callback_).Run(); }

  base::OnceClosure callback_;

  DISALLOW_COPY_AND_ASSIGN(Latch);
};

}  // namespace

namespace web_app {

void RevealAppShimInFinderForAppOnFileThread(
    const base::FilePath& app_path,
    const web_app::ShortcutInfo& shortcut_info) {
  web_app::WebAppShortcutCreator shortcut_creator(app_path, &shortcut_info);
  shortcut_creator.RevealAppShimInFinder();
}

void RevealAppShimInFinderForApp(Profile* profile,
                                 const extensions::Extension* app) {
  web_app::internals::PostShortcutIOTask(
      base::BindOnce(&RevealAppShimInFinderForAppOnFileThread, app->path()),
      ShortcutInfoForExtensionAndProfile(app, profile));
}

void RebuildAppAndLaunch(std::unique_ptr<web_app::ShortcutInfo> shortcut_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  Profile* profile =
      profile_manager->GetProfileByPath(shortcut_info->profile_path);
  if (!profile || !profile_manager->IsValidProfile(profile))
    return;

  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile);
  const extensions::Extension* extension = registry->GetExtensionById(
      shortcut_info->extension_id, extensions::ExtensionRegistry::ENABLED);
  if (!extension || !extension->is_platform_app())
    return;
  base::OnceCallback<void(base::Process)> launched_callback = base::DoNothing();
  base::OnceClosure terminated_callback = base::DoNothing();
  web_app::GetShortcutInfoForApp(
      extension, profile,
      base::BindOnce(
          &LaunchShim, LaunchShimUpdateBehavior::RECREATE_IF_INSTALLED,
          std::move(launched_callback), std::move(terminated_callback)));
}

bool MaybeRebuildShortcut(const base::CommandLine& command_line) {
  if (!command_line.HasSwitch(app_mode::kAppShimError))
    return false;

  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&RecordAppShimErrorAndBuildShortcutInfo,
                     command_line.GetSwitchValuePath(app_mode::kAppShimError)),
      base::BindOnce(&RebuildAppAndLaunch));
  return true;
}

// Mac-specific version of web_app::ShouldCreateShortcutFor() used during batch
// upgrades to ensure all shortcuts a user may still have are repaired when
// required by a Chrome upgrade.
bool ShouldUpgradeShortcutFor(Profile* profile,
                              const extensions::Extension* extension) {
  if (extension->location() == extensions::Manifest::COMPONENT ||
      !extensions::ui_util::CanDisplayInAppLauncher(extension, profile)) {
    return false;
  }

  return extension->is_app();
}

void UpdateShortcutsForAllApps(Profile* profile, base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile);
  if (!registry)
    return;

  scoped_refptr<Latch> latch = new Latch(std::move(callback));

  // Update all apps.
  std::unique_ptr<extensions::ExtensionSet> candidates =
      registry->GenerateInstalledExtensionsSet();
  for (auto& extension_refptr : *candidates) {
    const extensions::Extension* extension = extension_refptr.get();
    if (ShouldUpgradeShortcutFor(profile, extension)) {
      web_app::UpdateAllShortcuts(base::string16(), profile, extension,
                                  latch->NoOpClosure());
    }
  }
}

void GetProfilesForAppShim(
    const std::string& app_id,
    const std::vector<base::FilePath>& profile_paths_to_check,
    GetProfilesForAppShimCallback callback) {
  auto io_thread_lambda =
      [](const std::string& app_id,
         const std::vector<base::FilePath>& profile_paths_to_check,
         GetProfilesForAppShimCallback callback) {
        base::ScopedBlockingCall scoped_blocking_call(
            FROM_HERE, base::BlockingType::MAY_BLOCK);

        // Determine if an extension is installed for a profile by whether or
        // not the subpath Extensions/|app_id| exists. We do this instead of
        // checking the Profile's properties because the Profile may not be
        // loaded yet.
        std::vector<base::FilePath> profile_paths_with_app;
        for (const auto& profile_path : profile_paths_to_check) {
          base::FilePath extension_path =
              profile_path.AppendASCII(extensions::kInstallDirectoryName)
                  .AppendASCII(app_id);
          if (base::PathExists(extension_path))
            profile_paths_with_app.push_back(profile_path);
        }
        base::PostTask(
            FROM_HERE, {content::BrowserThread::UI},
            base::BindOnce(std::move(callback), profile_paths_with_app));
      };
  internals::GetShortcutIOTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(io_thread_lambda, app_id,
                                profile_paths_to_check, std::move(callback)));
}

}  // namespace web_app

namespace chrome {

void ShowCreateChromeAppShortcutsDialog(
    gfx::NativeWindow /*parent_window*/,
    Profile* profile,
    const extensions::Extension* app,
    const base::Callback<void(bool)>& close_callback) {
  // On Mac, the Applications folder is the only option, so don't bother asking
  // the user anything. Just create shortcuts.
  CreateShortcuts(web_app::SHORTCUT_CREATION_BY_USER,
                  web_app::ShortcutLocations(), profile, app,
                  base::DoNothing());
  if (!close_callback.is_null())
    close_callback.Run(true);
}

}  // namespace chrome
