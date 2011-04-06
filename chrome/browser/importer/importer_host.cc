// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/importer/importer_host.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/importer/firefox_profile_lock.h"
#include "chrome/browser/importer/importer.h"
#include "chrome/browser/importer/importer_lock_dialog.h"
#include "chrome/browser/importer/importer_progress_observer.h"
#include "chrome/browser/importer/importer_type.h"
#include "chrome/browser/importer/in_process_importer_bridge.h"
#include "chrome/browser/importer/toolbar_importer_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/ui/browser_list.h"
#include "content/browser/browser_thread.h"
#include "content/common/notification_source.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_WIN)
// TODO(port): Port this file.
#include "ui/base/message_box_win.h"
#endif

ImporterHost::ImporterHost()
    : profile_(NULL),
      task_(NULL),
      importer_(NULL),
      waiting_for_bookmarkbar_model_(false),
      installed_bookmark_observer_(false),
      is_source_readable_(true),
      headless_(false),
      parent_window_(NULL),
      observer_(NULL) {
}

void ImporterHost::ShowWarningDialog() {
  if (headless_)
    OnImportLockDialogEnd(false);
  else
    importer::ShowImportLockDialog(parent_window_, this);
}

void ImporterHost::OnImportLockDialogEnd(bool is_continue) {
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
    NotifyImportEnded();
  }
}

void ImporterHost::SetObserver(importer::ImporterProgressObserver* observer) {
  observer_ = observer;
}

void ImporterHost::NotifyImportStarted() {
  if (observer_)
    observer_->ImportStarted();
}

void ImporterHost::NotifyImportItemStarted(importer::ImportItem item) {
  if (observer_)
    observer_->ImportItemStarted(item);
}

void ImporterHost::NotifyImportItemEnded(importer::ImportItem item) {
  if (observer_)
    observer_->ImportItemEnded(item);
}

void ImporterHost::NotifyImportEnded() {
  firefox_lock_.reset();  // Release the Firefox profile lock.
  if (observer_)
    observer_->ImportEnded();
  Release();
}

void ImporterHost::StartImportSettings(
    const importer::SourceProfile& source_profile,
    Profile* target_profile,
    uint16 items,
    ProfileWriter* writer,
    bool first_run) {
  // We really only support importing from one host at a time.
  DCHECK(!profile_);

  profile_ = target_profile;
  // Preserves the observer and creates a task, since we do async import so that
  // it doesn't block the UI. When the import is complete, observer will be
  // notified.
  writer_ = writer;
  importer_ = importer::CreateImporterByType(source_profile.importer_type);
  // If we fail to create the Importer, exit, as we cannot do anything.
  if (!importer_) {
    NotifyImportEnded();
    return;
  }

  importer_->AddRef();
  importer_->set_import_to_bookmark_bar(ShouldImportToBookmarkBar(first_run));
  importer_->set_bookmark_bar_disabled(first_run);

  scoped_refptr<InProcessImporterBridge> bridge(
      new InProcessImporterBridge(writer_.get(), this));
  task_ = NewRunnableMethod(
      importer_, &Importer::StartImport, source_profile, items, bridge);

  CheckForFirefoxLock(source_profile, items, first_run);

#if defined(OS_WIN)
  // For google toolbar import, we need the user to log in and store their GAIA
  // credentials.
  if (source_profile.importer_type == importer::GOOGLE_TOOLBAR5) {
    if (!toolbar_importer_utils::IsGoogleGAIACookieInstalled()) {
      ui::MessageBox(
          NULL,
          UTF16ToWide(l10n_util::GetStringUTF16(
              IDS_IMPORTER_GOOGLE_LOGIN_TEXT)).c_str(),
          L"",
          MB_OK | MB_TOPMOST);

      GURL url("https://www.google.com/accounts/ServiceLogin");
      BrowserList::GetLastActive()->AddSelectedTabWithURL(
          url, PageTransition::TYPED);

      MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
          this, &ImporterHost::OnImportLockDialogEnd, false));

      is_source_readable_ = false;
    }
  }
#endif

  CheckForLoadedModels(items);
  AddRef();
  InvokeTaskIfDone();
}

void ImporterHost::Cancel() {
  if (importer_)
    importer_->Cancel();
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

bool ImporterHost::ShouldImportToBookmarkBar(bool first_run) {
  bool import_to_bookmark_bar = first_run;
  if (profile_ && profile_->GetBookmarkModel()->IsLoaded()) {
    import_to_bookmark_bar = (!profile_->GetBookmarkModel()->HasBookmarks());
  }
  return import_to_bookmark_bar;
}

void ImporterHost::CheckForFirefoxLock(
    const importer::SourceProfile& source_profile,
    uint16 items,
    bool first_run) {
  if (source_profile.importer_type == importer::FIREFOX2 ||
      source_profile.importer_type == importer::FIREFOX3) {
    DCHECK(!firefox_lock_.get());
    firefox_lock_.reset(new FirefoxProfileLock(source_profile.source_path));
    if (!firefox_lock_->HasAcquired()) {
      // If fail to acquire the lock, we set the source unreadable and
      // show a warning dialog, unless running without UI.
      is_source_readable_ = false;
      ShowWarningDialog();
    }
  }
}

void ImporterHost::CheckForLoadedModels(uint16 items) {
  // BookmarkModel should be loaded before adding IE favorites. So we observe
  // the BookmarkModel if needed, and start the task after it has been loaded.
  if ((items & importer::FAVORITES) && !writer_->BookmarkModelIsLoaded()) {
    profile_->GetBookmarkModel()->AddObserver(this);
    waiting_for_bookmarkbar_model_ = true;
    installed_bookmark_observer_ = true;
  }

  // Observes the TemplateURLModel if needed to import search engines from the
  // other browser. We also check to see if we're importing bookmarks because
  // we can import bookmark keywords from Firefox as search engines.
  if ((items & importer::SEARCH_ENGINES) || (items & importer::FAVORITES)) {
    if (!writer_->TemplateURLModelIsLoaded()) {
      TemplateURLModel* model = profile_->GetTemplateURLModel();
      registrar_.Add(this, NotificationType::TEMPLATE_URL_MODEL_LOADED,
                     Source<TemplateURLModel>(model));
      model->Load();
    }
  }
}

void ImporterHost::InvokeTaskIfDone() {
  if (waiting_for_bookmarkbar_model_ || !registrar_.IsEmpty() ||
      !is_source_readable_)
    return;
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE, task_);
}

void ImporterHost::Loaded(BookmarkModel* model) {
  DCHECK(model->IsLoaded());
  model->RemoveObserver(this);
  waiting_for_bookmarkbar_model_ = false;
  installed_bookmark_observer_ = false;

  importer_->set_import_to_bookmark_bar(!model->HasBookmarks());
  InvokeTaskIfDone();
}

void ImporterHost::BookmarkModelBeingDeleted(BookmarkModel* model) {
  installed_bookmark_observer_ = false;
}

void ImporterHost::BookmarkModelChanged() {
}

void ImporterHost::Observe(NotificationType type,
                           const NotificationSource& source,
                           const NotificationDetails& details) {
  DCHECK(type == NotificationType::TEMPLATE_URL_MODEL_LOADED);
  registrar_.RemoveAll();
  InvokeTaskIfDone();
}
