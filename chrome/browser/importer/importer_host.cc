// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/importer/importer_host.h"

#include "base/bind.h"
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
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/simple_message_box.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_source.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

using content::BrowserThread;

ImporterHost::ImporterHost()
    : profile_(NULL),
      waiting_for_bookmarkbar_model_(false),
      installed_bookmark_observer_(false),
      is_source_readable_(true),
      importer_(NULL),
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
    // User chose to skip the import process. We should reset the |task_| and
    // notify the ImporterHost to finish.
    task_ = base::Closure();
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
  DCHECK(target_profile);

  profile_ = target_profile;
  PrefService* user_prefs = profile_->GetPrefs();

  // Make sure only items that were not disabled by policy are imported.
  if (!user_prefs->GetBoolean(prefs::kImportHistory))
    items &= ~importer::HISTORY;
  if (!user_prefs->GetBoolean(prefs::kImportSearchEngine))
    items &= ~importer::SEARCH_ENGINES;
  if (!user_prefs->GetBoolean(prefs::kImportBookmarks))
    items &= ~importer::FAVORITES;
  if (!user_prefs->GetBoolean(prefs::kImportSavedPasswords))
    items &= ~importer::PASSWORDS;

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

  scoped_refptr<InProcessImporterBridge> bridge(
      new InProcessImporterBridge(writer_.get(), this));
  task_ = base::Bind(
      &Importer::StartImport, importer_, source_profile, items, bridge);

  CheckForFirefoxLock(source_profile, items, first_run);

#if defined(OS_WIN)
  // For google toolbar import, we need the user to log in and store their GAIA
  // credentials.
  if (source_profile.importer_type == importer::TYPE_GOOGLE_TOOLBAR5) {
    toolbar_importer_utils::IsGoogleGAIACookieInstalled(
        base::Bind(&ImporterHost::OnGoogleGAIACookieChecked, this), profile_);
    is_source_readable_ = false;
  }
#endif

  CheckForLoadedModels(items);
  AddRef();
  InvokeTaskIfDone();
}

void ImporterHost::OnGoogleGAIACookieChecked(bool result) {
#if defined(OS_WIN)
  if (!result) {
    browser::ShowMessageBox(NULL,
        l10n_util::GetStringUTF16(IDS_IMPORTER_GOOGLE_LOGIN_TEXT), string16(),
        browser::MESSAGE_BOX_TYPE_INFORMATION);

    GURL url("https://accounts.google.com/ServiceLogin");
    DCHECK(profile_);
    Browser* browser = BrowserList::GetLastActiveWithProfile(profile_);
    if (browser)
      browser->AddSelectedTabWithURL(url, content::PAGE_TRANSITION_TYPED);

    MessageLoop::current()->PostTask(FROM_HERE, base::Bind(
        &ImporterHost::OnImportLockDialogEnd, this, false));
  } else {
    is_source_readable_ = true;
    InvokeTaskIfDone();
  }
#endif
}

void ImporterHost::Cancel() {
  if (importer_)
    importer_->Cancel();
}

ImporterHost::~ImporterHost() {
  if (NULL != importer_)
    importer_->Release();

  if (installed_bookmark_observer_) {
    DCHECK(profile_);
    profile_->GetBookmarkModel()->RemoveObserver(this);
  }
}

void ImporterHost::CheckForFirefoxLock(
    const importer::SourceProfile& source_profile,
    uint16 items,
    bool first_run) {
  if (source_profile.importer_type == importer::TYPE_FIREFOX2 ||
      source_profile.importer_type == importer::TYPE_FIREFOX3) {
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
  // A target profile must be loaded by StartImportSettings().
  DCHECK(profile_);

  // BookmarkModel should be loaded before adding IE favorites. So we observe
  // the BookmarkModel if needed, and start the task after it has been loaded.
  if ((items & importer::FAVORITES) && !writer_->BookmarkModelIsLoaded()) {
    profile_->GetBookmarkModel()->AddObserver(this);
    waiting_for_bookmarkbar_model_ = true;
    installed_bookmark_observer_ = true;
  }

  // Observes the TemplateURLService if needed to import search engines from the
  // other browser. We also check to see if we're importing bookmarks because
  // we can import bookmark keywords from Firefox as search engines.
  if ((items & importer::SEARCH_ENGINES) || (items & importer::FAVORITES)) {
    if (!writer_->TemplateURLServiceIsLoaded()) {
      TemplateURLService* model =
          TemplateURLServiceFactory::GetForProfile(profile_);
      registrar_.Add(this, chrome::NOTIFICATION_TEMPLATE_URL_SERVICE_LOADED,
                     content::Source<TemplateURLService>(model));
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

void ImporterHost::Loaded(BookmarkModel* model, bool ids_reassigned) {
  DCHECK(model->IsLoaded());
  model->RemoveObserver(this);
  waiting_for_bookmarkbar_model_ = false;
  installed_bookmark_observer_ = false;

  InvokeTaskIfDone();
}

void ImporterHost::BookmarkModelBeingDeleted(BookmarkModel* model) {
  installed_bookmark_observer_ = false;
}

void ImporterHost::BookmarkModelChanged() {
}

void ImporterHost::Observe(int type,
                           const content::NotificationSource& source,
                           const content::NotificationDetails& details) {
  DCHECK(type == chrome::NOTIFICATION_TEMPLATE_URL_SERVICE_LOADED);
  registrar_.RemoveAll();
  InvokeTaskIfDone();
}
