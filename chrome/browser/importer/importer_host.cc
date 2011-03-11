// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/importer/importer_host.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/importer/firefox_profile_lock.h"
#include "chrome/browser/importer/importer_bridge.h"
#include "chrome/browser/importer/importer_progress_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "content/browser/browser_thread.h"
#include "content/common/notification_source.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_WIN)
// TODO(port): Port this file.
#include "ui/base/message_box_win.h"
#endif

// ImporterHost ----------------------------------------------------------------

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
  if (headless_) {
    OnImportLockDialogEnd(false);
  } else {
    browser::ShowImportLockDialog(parent_window_, this);
  }
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
  importer_ = ImporterList::CreateImporterByType(profile_info.browser_type);
  // If we fail to create Importer, exit as we cannot do anything.
  if (!importer_) {
    NotifyImportEnded();
    return;
  }

  importer_->AddRef();

  importer_->set_import_to_bookmark_bar(ShouldImportToBookmarkBar(first_run));
  importer_->set_bookmark_bar_disabled(first_run);
  scoped_refptr<ImporterBridge> bridge(
      new InProcessImporterBridge(writer_.get(), this));
  task_ = NewRunnableMethod(importer_, &Importer::StartImport,
      profile_info, items, bridge);

  CheckForFirefoxLock(profile_info, items, first_run);

#if defined(OS_WIN)
  // For google toolbar import, we need the user to log in and store their GAIA
  // credentials.
  if (profile_info.browser_type == importer::GOOGLE_TOOLBAR5) {
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

ImporterHost::~ImporterHost() {
  if (NULL != importer_)
    importer_->Release();

  if (installed_bookmark_observer_) {
    DCHECK(profile_);  // Only way for waiting_for_bookmarkbar_model_ to be true
                       // is if we have a profile.
    profile_->GetBookmarkModel()->RemoveObserver(this);
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

bool ImporterHost::ShouldImportToBookmarkBar(bool first_run) {
  bool import_to_bookmark_bar = first_run;
  if (profile_ && profile_->GetBookmarkModel()->IsLoaded()) {
    import_to_bookmark_bar = (!profile_->GetBookmarkModel()->HasBookmarks());
  }
  return import_to_bookmark_bar;
}

void ImporterHost::CheckForFirefoxLock(
    const importer::ProfileInfo& profile_info, uint16 items, bool first_run) {
  if (profile_info.browser_type == importer::FIREFOX2 ||
      profile_info.browser_type == importer::FIREFOX3) {
    DCHECK(!firefox_lock_.get());
    firefox_lock_.reset(new FirefoxProfileLock(profile_info.source_path));
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

// ExternalProcessImporterHost -------------------------------------------------

ExternalProcessImporterHost::ExternalProcessImporterHost()
    : items_(0),
      import_to_bookmark_bar_(false),
      cancelled_(false),
      import_process_launched_(false) {
}

void ExternalProcessImporterHost::Cancel() {
  cancelled_ = true;
  if (import_process_launched_)
    client_->Cancel();
  NotifyImportEnded();  // Tells the observer that we're done, and releases us.
}

void ExternalProcessImporterHost::StartImportSettings(
    const importer::ProfileInfo& profile_info,
    Profile* target_profile,
    uint16 items,
    ProfileWriter* writer,
    bool first_run) {
  DCHECK(!profile_);
  profile_ = target_profile;
  writer_ = writer;
  profile_info_ = &profile_info;
  items_ = items;

  ImporterHost::AddRef();  // Balanced in ImporterHost::NotifyImportEnded.

  import_to_bookmark_bar_ = ShouldImportToBookmarkBar(first_run);
  CheckForFirefoxLock(profile_info, items, first_run);
  CheckForLoadedModels(items);

  InvokeTaskIfDone();
}

void ExternalProcessImporterHost::InvokeTaskIfDone() {
  if (waiting_for_bookmarkbar_model_ || !registrar_.IsEmpty() ||
      !is_source_readable_ || cancelled_)
    return;

  // The in-process half of the bridge which catches data from the IPC pipe
  // and feeds it to the ProfileWriter.  The external process half of the
  // bridge lives in the external process -- see ProfileImportThread.
  // The ExternalProcessImporterClient created in the next line owns this
  // bridge, and will delete it.
  InProcessImporterBridge* bridge =
      new InProcessImporterBridge(writer_.get(), this);
  client_ = new ExternalProcessImporterClient(this, *profile_info_, items_,
                                              bridge, import_to_bookmark_bar_);
  import_process_launched_ = true;
  client_->Start();
}

void ExternalProcessImporterHost::Loaded(BookmarkModel* model) {
  DCHECK(model->IsLoaded());
  model->RemoveObserver(this);
  waiting_for_bookmarkbar_model_ = false;
  installed_bookmark_observer_ = false;

  // Because the import process is running externally, the decision whether
  // to import to the bookmark bar must be stored here so that it can be
  // passed to the importer when the import task is invoked.
  import_to_bookmark_bar_ = (!model->HasBookmarks());
  InvokeTaskIfDone();
}

// ExternalProcessImporterClient -----------------------------------------------

ExternalProcessImporterClient::ExternalProcessImporterClient(
    ExternalProcessImporterHost* importer_host,
    const importer::ProfileInfo& profile_info,
    int items,
    InProcessImporterBridge* bridge,
    bool import_to_bookmark_bar)
    : bookmarks_options_(0),
      total_bookmarks_count_(0),
      total_history_rows_count_(0),
      total_fav_icons_count_(0),
      process_importer_host_(importer_host),
      profile_import_process_host_(NULL),
      profile_info_(profile_info),
      items_(items),
      import_to_bookmark_bar_(import_to_bookmark_bar),
      bridge_(bridge),
      cancelled_(false) {
  bridge_->AddRef();
  process_importer_host_->NotifyImportStarted();
}

ExternalProcessImporterClient::~ExternalProcessImporterClient() {
  bridge_->Release();
}

void ExternalProcessImporterClient::Start() {
  AddRef();  // balanced in Cleanup.
  BrowserThread::ID thread_id;
  CHECK(BrowserThread::GetCurrentThreadIdentifier(&thread_id));
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(this,
          &ExternalProcessImporterClient::StartProcessOnIOThread,
          g_browser_process->resource_dispatcher_host(), thread_id));
}

void ExternalProcessImporterClient::StartProcessOnIOThread(
    ResourceDispatcherHost* rdh,
    BrowserThread::ID thread_id) {
  profile_import_process_host_ =
      new ProfileImportProcessHost(rdh, this, thread_id);
  profile_import_process_host_->StartProfileImportProcess(profile_info_,
      items_, import_to_bookmark_bar_);
}

void ExternalProcessImporterClient::Cancel() {
  if (cancelled_)
    return;

  cancelled_ = true;
  if (profile_import_process_host_) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        NewRunnableMethod(this,
            &ExternalProcessImporterClient::CancelImportProcessOnIOThread));
  }
  Release();
}

void ExternalProcessImporterClient::CancelImportProcessOnIOThread() {
  profile_import_process_host_->CancelProfileImportProcess();
}

void ExternalProcessImporterClient::NotifyItemFinishedOnIOThread(
    importer::ImportItem import_item) {
  profile_import_process_host_->ReportImportItemFinished(import_item);
}

void ExternalProcessImporterClient::OnProcessCrashed(int exit_code) {
  if (cancelled_)
    return;

  process_importer_host_->Cancel();
}

void ExternalProcessImporterClient::Cleanup() {
  if (cancelled_)
    return;

  if (process_importer_host_)
    process_importer_host_->NotifyImportEnded();
  Release();
}

void ExternalProcessImporterClient::OnImportStart() {
  if (cancelled_)
    return;

  bridge_->NotifyStarted();
}

void ExternalProcessImporterClient::OnImportFinished(bool succeeded,
                                                     std::string error_msg) {
  if (cancelled_)
    return;

  if (!succeeded)
    LOG(WARNING) << "Import failed.  Error: " << error_msg;
  Cleanup();
}

void ExternalProcessImporterClient::OnImportItemStart(int item_data) {
  if (cancelled_)
    return;

  bridge_->NotifyItemStarted(static_cast<importer::ImportItem>(item_data));
}

void ExternalProcessImporterClient::OnImportItemFinished(int item_data) {
  if (cancelled_)
    return;

  importer::ImportItem import_item =
      static_cast<importer::ImportItem>(item_data);
  bridge_->NotifyItemEnded(import_item);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(this,
          &ExternalProcessImporterClient::NotifyItemFinishedOnIOThread,
          import_item));
}

void ExternalProcessImporterClient::OnHistoryImportStart(
    size_t total_history_rows_count) {
  if (cancelled_)
    return;

  total_history_rows_count_ = total_history_rows_count;
  history_rows_.reserve(total_history_rows_count);
}

void ExternalProcessImporterClient::OnHistoryImportGroup(
    const std::vector<history::URLRow>& history_rows_group,
    int visit_source) {
  if (cancelled_)
    return;

  history_rows_.insert(history_rows_.end(), history_rows_group.begin(),
                       history_rows_group.end());
  if (history_rows_.size() == total_history_rows_count_)
    bridge_->SetHistoryItems(history_rows_,
                             static_cast<history::VisitSource>(visit_source));
}

void ExternalProcessImporterClient::OnHomePageImportReady(
    const GURL& home_page) {
  if (cancelled_)
    return;

  bridge_->AddHomePage(home_page);
}

void ExternalProcessImporterClient::OnBookmarksImportStart(
    const std::wstring first_folder_name,
    int options, size_t total_bookmarks_count) {
  if (cancelled_)
    return;

  bookmarks_first_folder_name_ = first_folder_name;
  bookmarks_options_ = options;
  total_bookmarks_count_ = total_bookmarks_count;
  bookmarks_.reserve(total_bookmarks_count);
}

void ExternalProcessImporterClient::OnBookmarksImportGroup(
    const std::vector<ProfileWriter::BookmarkEntry>& bookmarks_group) {
  if (cancelled_)
    return;

  // Collect sets of bookmarks from importer process until we have reached
  // total_bookmarks_count_:
  bookmarks_.insert(bookmarks_.end(), bookmarks_group.begin(),
                    bookmarks_group.end());
  if (bookmarks_.size() == total_bookmarks_count_) {
    bridge_->AddBookmarkEntries(bookmarks_, bookmarks_first_folder_name_,
                                bookmarks_options_);
  }
}

void ExternalProcessImporterClient::OnFavIconsImportStart(
    size_t total_fav_icons_count) {
  if (cancelled_)
    return;

  total_fav_icons_count_ = total_fav_icons_count;
  fav_icons_.reserve(total_fav_icons_count);
}

void ExternalProcessImporterClient::OnFavIconsImportGroup(
    const std::vector<history::ImportedFavIconUsage>& fav_icons_group) {
  if (cancelled_)
    return;

  fav_icons_.insert(fav_icons_.end(), fav_icons_group.begin(),
                    fav_icons_group.end());
  if (fav_icons_.size() == total_fav_icons_count_)
    bridge_->SetFavIcons(fav_icons_);
}

void ExternalProcessImporterClient::OnPasswordFormImportReady(
    const webkit_glue::PasswordForm& form) {
  if (cancelled_)
    return;

  bridge_->SetPasswordForm(form);
}

void ExternalProcessImporterClient::OnKeywordsImportReady(
    const std::vector<TemplateURL>& template_urls,
        int default_keyword_index, bool unique_on_host_and_path) {
  if (cancelled_)
    return;

  std::vector<TemplateURL*> template_url_vec;
  template_url_vec.reserve(template_urls.size());
  std::vector<TemplateURL>::const_iterator iter;
  for (iter = template_urls.begin();
       iter != template_urls.end();
       ++iter) {
    template_url_vec.push_back(new TemplateURL(*iter));
  }
  bridge_->SetKeywords(template_url_vec, default_keyword_index,
                       unique_on_host_and_path);
}
