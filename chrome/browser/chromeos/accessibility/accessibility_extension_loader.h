// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_ACCESSIBILITY_EXTENSION_LOADER_H_
#define CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_ACCESSIBILITY_EXTENSION_LOADER_H_

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"

namespace content {
class RenderViewHost;
}

class ExtensionService;
class Profile;

namespace chromeos {

class AccessibilityExtensionLoader {
 public:
  AccessibilityExtensionLoader(const std::string& extension_id,
                               const base::FilePath& extension_path,
                               const base::Closure& unload_callback);
  ~AccessibilityExtensionLoader();

  void SetProfile(Profile* profile);
  void Load(Profile* profile,
            const std::string& init_script_str,
            const base::Closure& done_cb);
  void Unload();
  void LoadToUserScreen(const base::Closure& done_cb);
  void LoadToLockScreen(const base::Closure& done_cb);
  void LoadExtension(Profile* profile,
                     content::RenderViewHost* render_view_host,
                     base::Closure done_cb);

  bool loaded_on_lock_screen() { return loaded_on_lock_screen_; }

 private:
  void InjectContentScriptAndCallback(ExtensionService* extension_service,
                                      int render_process_id,
                                      int render_view_id,
                                      const base::Closure& done_cb);
  void UnloadFromLockScreen();
  void UnloadExtensionFromProfile(Profile* profile);

  Profile* profile_;
  std::string extension_id_;
  base::FilePath extension_path_;
  std::string init_script_str_;

  // Profile which the extension is currently loaded to.
  // If nullptr, it is not loaded to any profile.
  bool loaded_on_lock_screen_;
  bool loaded_on_user_screen_;

  base::Closure unload_callback_;

  base::WeakPtrFactory<AccessibilityExtensionLoader> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AccessibilityExtensionLoader);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_ACCESSIBILITY_EXTENSION_LOADER_H_
