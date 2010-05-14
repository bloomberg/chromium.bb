// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/importer/importer.h"

#include "app/l10n_util.h"
#include "base/string_util.h"
#include "base/thread.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browsing_instance.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/importer/firefox_profile_lock.h"
#include "chrome/browser/importer/importer_bridge.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/renderer_host/site_instance.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "gfx/codec/png_codec.h"
#include "gfx/favicon_size.h"
#include "grit/generated_resources.h"
#include "skia/ext/image_operations.h"
#include "webkit/glue/image_decoder.h"

// TODO(port): Port these files.
#if defined(OS_WIN)
#include "app/win_util.h"
#include "chrome/browser/views/importer_lock_view.h"
#include "views/window/window.h"
#elif defined(OS_MACOSX)
#include "chrome/browser/cocoa/importer_lock_dialog.h"
#elif defined(TOOLKIT_USES_GTK)
#include "chrome/browser/gtk/import_lock_dialog_gtk.h"
#endif

using webkit_glue::PasswordForm;

// Importer.

Importer::Importer()
    : cancelled_(false),
      import_to_bookmark_bar_(false) {
}

Importer::~Importer() {
}

// static
bool Importer::ReencodeFavicon(const unsigned char* src_data, size_t src_len,
                               std::vector<unsigned char>* png_data) {
  // Decode the favicon using WebKit's image decoder.
  webkit_glue::ImageDecoder decoder(gfx::Size(kFavIconSize, kFavIconSize));
  SkBitmap decoded = decoder.Decode(src_data, src_len);
  if (decoded.empty())
    return false;  // Unable to decode.

  if (decoded.width() != kFavIconSize || decoded.height() != kFavIconSize) {
    // The bitmap is not the correct size, re-sample.
    int new_width = decoded.width();
    int new_height = decoded.height();
    calc_favicon_target_size(&new_width, &new_height);
    decoded = skia::ImageOperations::Resize(
        decoded, skia::ImageOperations::RESIZE_LANCZOS3, new_width, new_height);
  }

  // Encode our bitmap as a PNG.
  gfx::PNGCodec::EncodeBGRASkBitmap(decoded, false, png_data);
  return true;
}

// ImporterHost.

ImporterHost::ImporterHost()
    : profile_(NULL),
      observer_(NULL),
      task_(NULL),
      importer_(NULL),
      waiting_for_bookmarkbar_model_(false),
      installed_bookmark_observer_(false),
      is_source_readable_(true),
      headless_(false),
      parent_window_(NULL) {
  importer_list_.DetectSourceProfiles();
}

ImporterHost::~ImporterHost() {
  if (NULL != importer_)
    importer_->Release();
  if (installed_bookmark_observer_) {
    DCHECK(profile_);  // Only way for waiting_for_bookmarkbar_model_ to be true
                       // is if we have a profile.
    profile_->GetBookmarkModel()->RemoveObserver(this);
  }
}

void ImporterHost::Loaded(BookmarkModel* model) {
  DCHECK(model->IsLoaded());
  model->RemoveObserver(this);
  waiting_for_bookmarkbar_model_ = false;
  installed_bookmark_observer_ = false;

  std::vector<GURL> starred_urls;
  model->GetBookmarks(&starred_urls);
  importer_->set_import_to_bookmark_bar(starred_urls.size() == 0);
  InvokeTaskIfDone();
}

void ImporterHost::BookmarkModelBeingDeleted(BookmarkModel* model) {
  installed_bookmark_observer_ = false;
}

void ImporterHost::Observe(NotificationType type,
                           const NotificationSource& source,
                           const NotificationDetails& details) {
  DCHECK(type == NotificationType::TEMPLATE_URL_MODEL_LOADED);
  registrar_.RemoveAll();
  InvokeTaskIfDone();
}

void ImporterHost::ShowWarningDialog() {
  if (headless_) {
    OnLockViewEnd(false);
  } else {
#if defined(OS_WIN)
    views::Window::CreateChromeWindow(GetActiveWindow(), gfx::Rect(),
                                      new ImporterLockView(this))->Show();
#elif defined(TOOLKIT_USES_GTK)
    ImportLockDialogGtk::Show(parent_window_, this);
#else
    ImportLockDialogCocoa::ShowWarning(this);
#endif
  }
}

void ImporterHost::OnLockViewEnd(bool is_continue) {
  if (is_continue) {
    // User chose to continue, then we check the lock again to make
    // sure that Firefox has been closed. Try to import the settings
    // if successful. Otherwise, show a warning dialog.
    firefox_lock_->Lock();
    if (firefox_lock_->HasAcquired()) {
      is_source_readable_ = true;
      InvokeTaskIfDone();
    } else {
      ShowWarningDialog();
    }
  } else {
    // User chose to skip the import process. We should delete
    // the task and notify the ImporterHost to finish.
    delete task_;
    task_ = NULL;
    importer_ = NULL;
    ImportEnded();
  }
}

void ImporterHost::StartImportSettings(
    const importer::ProfileInfo& profile_info,
    Profile* target_profile,
    uint16 items,
    ProfileWriter* writer,
    bool first_run) {
  DCHECK(!profile_);  // We really only support importing from one host at a
                      // time.
  profile_ = target_profile;
  // Preserves the observer and creates a task, since we do async import
  // so that it doesn't block the UI. When the import is complete, observer
  // will be notified.
  writer_ = writer;
  importer_ = importer_list_.CreateImporterByType(profile_info.browser_type);
  // If we fail to create Importer, exit as we can not do anything.
  if (!importer_) {
    ImportEnded();
    return;
  }

  importer_->AddRef();

  bool import_to_bookmark_bar = first_run;
  if (target_profile && target_profile->GetBookmarkModel()->IsLoaded()) {
    std::vector<GURL> starred_urls;
    target_profile->GetBookmarkModel()->GetBookmarks(&starred_urls);
    import_to_bookmark_bar = (starred_urls.size() == 0);
  }
  importer_->set_import_to_bookmark_bar(import_to_bookmark_bar);
  scoped_refptr<ImporterBridge> bridge(
      new InProcessImporterBridge(writer_.get(), this));
  task_ = NewRunnableMethod(importer_, &Importer::StartImport,
      profile_info, items, bridge);

  // We should lock the Firefox profile directory to prevent corruption.
  if (profile_info.browser_type == importer::FIREFOX2 ||
      profile_info.browser_type == importer::FIREFOX3) {
    firefox_lock_.reset(new FirefoxProfileLock(profile_info.source_path));
    if (!firefox_lock_->HasAcquired()) {
      // If fail to acquire the lock, we set the source unreadable and
      // show a warning dialog.
      // However, if we're running without a UI (silently) and trying to
      // import just the home page, then import anyway. The home page setting
      // is stored in an unlocked text file, so it is the only preference safe
      // to import even if Firefox is running.
      if (items == importer::HOME_PAGE && first_run && this->headless_) {
        AddRef();
        InvokeTaskIfDone();
        return;
      }
      is_source_readable_ = false;
      ShowWarningDialog();
    }
  }

#if defined(OS_WIN)
  // For google toolbar import, we need the user to log in and store their GAIA
  // credentials.
  if (profile_info.browser_type == importer::GOOGLE_TOOLBAR5) {
    if (!toolbar_importer_utils::IsGoogleGAIACookieInstalled()) {
      win_util::MessageBox(
          NULL,
          l10n_util::GetString(IDS_IMPORTER_GOOGLE_LOGIN_TEXT).c_str(),
          L"",
          MB_OK | MB_TOPMOST);

      GURL url("https://www.google.com/accounts/ServiceLogin");
      BrowsingInstance* instance = new BrowsingInstance(writer_->profile());
      SiteInstance* site = instance->GetSiteInstanceForURL(url);
      Browser* browser = BrowserList::GetLastActive();
      browser->AddTabWithURL(url, GURL(), PageTransition::TYPED,
                             -1, Browser::ADD_SELECTED, site, std::string());

      MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
        this, &ImporterHost::OnLockViewEnd, false));

      is_source_readable_ = false;
    }
  }
#endif

  // BookmarkModel should be loaded before adding IE favorites. So we observe
  // the BookmarkModel if needed, and start the task after it has been loaded.
  if ((items & importer::FAVORITES) && !writer_->BookmarkModelIsLoaded()) {
    target_profile->GetBookmarkModel()->AddObserver(this);
    waiting_for_bookmarkbar_model_ = true;
    installed_bookmark_observer_ = true;
  }

  // Observes the TemplateURLModel if needed to import search engines from the
  // other browser. We also check to see if we're importing bookmarks because
  // we can import bookmark keywords from Firefox as search engines.
  if ((items & importer::SEARCH_ENGINES) || (items & importer::FAVORITES)) {
    if (!writer_->TemplateURLModelIsLoaded()) {
      TemplateURLModel* model = target_profile->GetTemplateURLModel();
      registrar_.Add(this, NotificationType::TEMPLATE_URL_MODEL_LOADED,
                     Source<TemplateURLModel>(model));
      model->Load();
    }
  }

  AddRef();
  InvokeTaskIfDone();
}

void ImporterHost::Cancel() {
  if (importer_)
    importer_->Cancel();
}

void ImporterHost::SetObserver(Observer* observer) {
  observer_ = observer;
}

void ImporterHost::InvokeTaskIfDone() {
  if (waiting_for_bookmarkbar_model_ || !registrar_.IsEmpty() ||
      !is_source_readable_)
    return;
  ChromeThread::PostTask(ChromeThread::FILE, FROM_HERE, task_);
}

void ImporterHost::ImportItemStarted(importer::ImportItem item) {
  if (observer_)
    observer_->ImportItemStarted(item);
}

void ImporterHost::ImportItemEnded(importer::ImportItem item) {
  if (observer_)
    observer_->ImportItemEnded(item);
}

void ImporterHost::ImportStarted() {
  if (observer_)
    observer_->ImportStarted();
}

void ImporterHost::ImportEnded() {
  firefox_lock_.reset();  // Release the Firefox profile lock.
  if (observer_)
    observer_->ImportEnded();
  Release();
}
