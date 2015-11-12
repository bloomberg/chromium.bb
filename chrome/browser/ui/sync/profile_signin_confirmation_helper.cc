// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/sync/profile_signin_confirmation_helper.h"

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/browser_sync/browser/signin_confirmation_helper.h"
#include "components/history/core/browser/history_service.h"
#include "content/public/browser/browser_thread.h"
#include "ui/gfx/color_utils.h"
#include "ui/native_theme/native_theme.h"

#if defined(ENABLE_EXTENSIONS)
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/sync_helper.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#endif

using bookmarks::BookmarkModel;

namespace {

const int kHistoryEntriesBeforeNewProfilePrompt = 10;

bool HasBookmarks(Profile* profile) {
  BookmarkModel* bookmarks = BookmarkModelFactory::GetForProfile(profile);
  bool has_bookmarks = bookmarks && bookmarks->HasBookmarks();
  if (has_bookmarks)
    DVLOG(1) << "SigninConfirmationHelper: profile contains bookmarks";
  return has_bookmarks;
}

}  // namespace

namespace ui {

SkColor GetSigninConfirmationPromptBarColor(SkAlpha alpha) {
  static const SkColor kBackgroundColor =
      ui::NativeTheme::instance()->GetSystemColor(
          ui::NativeTheme::kColorId_DialogBackground);
  return color_utils::BlendTowardOppositeLuminance(kBackgroundColor, alpha);
}

bool HasBeenShutdown(Profile* profile) {
#if defined(OS_IOS)
  // This check is not useful on iOS: the browser can be shut down without
  // explicit user action (for example, in response to memory pressure), and
  // this should be invisible to the user. The desktop assumption that the
  // profile going through a restart indicates something about user intention
  // does not hold. We rely on the other profile dirtiness checks.
  return false;
#else
  bool has_been_shutdown = !profile->IsNewProfile();
  if (has_been_shutdown)
    DVLOG(1) << "ProfileSigninConfirmationHelper: profile is not new";
  return has_been_shutdown;
#endif
}

bool HasSyncedExtensions(Profile* profile) {
#if defined(ENABLE_EXTENSIONS)
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile);
  if (registry) {
    for (const scoped_refptr<const extensions::Extension>& extension :
         registry->enabled_extensions()) {
      // The webstore is synced so that it stays put on the new tab
      // page, but since it's installed by default we don't want to
      // consider it when determining if the profile is dirty.
      if (extensions::sync_helper::IsSyncable(extension.get()) &&
          !extensions::sync_helper::IsSyncableComponentExtension(
              extension.get())) {
        DVLOG(1) << "ProfileSigninConfirmationHelper: "
                 << "profile contains a synced extension: " << extension->id();
        return true;
      }
    }
  }
#endif
  return false;
}

void CheckShouldPromptForNewProfile(
    Profile* profile,
    const base::Callback<void(bool)>& return_result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (HasBeenShutdown(profile) ||
      HasBookmarks(profile) ||
      HasSyncedExtensions(profile)) {
    return_result.Run(true);
    return;
  }
  history::HistoryService* service =
      HistoryServiceFactory::GetForProfileWithoutCreating(profile);
  // Fire asynchronous queries for profile data.
  sync_driver::SigninConfirmationHelper* helper =
      new sync_driver::SigninConfirmationHelper(service, return_result);
  helper->CheckHasHistory(kHistoryEntriesBeforeNewProfilePrompt);
  helper->CheckHasTypedURLs();
}

}  // namespace ui
