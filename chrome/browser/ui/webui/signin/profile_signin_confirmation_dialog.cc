// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/profile_signin_confirmation_dialog.h"

#include "base/basictypes.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/prefs/pref_service.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/history/history_backend.h"
#include "chrome/browser/history/history_db_task.h"
#include "chrome/browser/history/history_service.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/constrained_web_dialog_ui.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_set.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace content {
class WebContents;
}

namespace {

class ProfileSigninConfirmationHandler : public content::WebUIMessageHandler {
 public:
  ProfileSigninConfirmationHandler(
      const ProfileSigninConfirmationDialog* dialog,
      const base::Closure& cancel_signin,
      const base::Closure& signin_with_new_profile,
      const base::Closure& continue_signin);
  virtual ~ProfileSigninConfirmationHandler();
  virtual void RegisterMessages() OVERRIDE;

 private:
  // content::WebUIMessageHandler implementation.
  void OnCancelButtonClicked(const base::ListValue* args);
  void OnCreateProfileClicked(const base::ListValue* args);
  void OnContinueButtonClicked(const base::ListValue* args);

  // Weak ptr to parent dialog.
  const ProfileSigninConfirmationDialog* dialog_;

  // Dialog button callbacks.
  base::Closure cancel_signin_;
  base::Closure signin_with_new_profile_;
  base::Closure continue_signin_;
};

const int kHistoryEntriesBeforeNewProfilePrompt = 10;

// Determines whether a profile has any typed URLs in its history.
class HasTypedURLsTask : public history::HistoryDBTask {
 public:
  HasTypedURLsTask(base::Callback<void(bool)> cb)
      : has_typed_urls_(0), cb_(cb) {
  }
  virtual bool RunOnDBThread(history::HistoryBackend* backend,
                             history::HistoryDatabase* db) OVERRIDE {
    history::URLRows rows;
    backend->GetAllTypedURLs(&rows);
    if (!rows.empty())
      has_typed_urls_ = true;
    return true;
  }
  virtual void DoneRunOnMainThread() OVERRIDE {
    cb_.Run(has_typed_urls_);
  }
 private:
  virtual ~HasTypedURLsTask() {}
  bool has_typed_urls_;
  base::Callback<void(bool)> cb_;
};

// Invokes one of two callbacks depending on a boolean flag.
void CallbackFork(base::Closure first, base::Closure second,
                  bool invoke_first) {
  if (invoke_first)
    first.Run();
  else
    second.Run();
}

// Invokes a chain of callbacks until one of them signals completion.
//
// Each callback accepts a callback parameter that should be invoked
// with |true| to signal completion (the chain should be broken) or
// |false| otherwise.  When the chain is stopped |return_result| will be
// invoked with the result that stopped the chain: |true| if one of the
// callbacks stopped the chain, or |false| if none of them did.
//
// This is essentially a special case of "series" from the async.js
// library: https://github.com/caolan/async
//
// TODO(dconnelly): This should really be in a library.
void ChainCallbacksUntilTrue(
    base::Callback<void(base::Callback<void(bool)>)> first,
    base::Callback<void(base::Callback<void(bool)>)> second,
    base::Callback<void(bool)> return_result) {
  // We implement the completion signalling callback using CallbackFork:
  // if the first callback passes it |true|, |return_result| will be
  // invoked immediately, and otherwise the second callback will be
  // invoked and passed |return_result| directly since it's last in
  // the chain.
  first.Run(base::Bind(&CallbackFork,
                       base::Bind(return_result, true),
                       base::Bind(second, return_result)));
}

}  // namespace

ProfileSigninConfirmationHandler::ProfileSigninConfirmationHandler(
      const ProfileSigninConfirmationDialog* dialog,
      const base::Closure& cancel_signin,
      const base::Closure& signin_with_new_profile,
      const base::Closure& continue_signin)
  : dialog_(dialog),
    cancel_signin_(cancel_signin),
    signin_with_new_profile_(signin_with_new_profile),
    continue_signin_(continue_signin) {
}

ProfileSigninConfirmationHandler::~ProfileSigninConfirmationHandler() {
}

void ProfileSigninConfirmationHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("cancel",
      base::Bind(&ProfileSigninConfirmationHandler::OnCancelButtonClicked,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("createNewProfile",
      base::Bind(&ProfileSigninConfirmationHandler::OnCreateProfileClicked,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("continue",
      base::Bind(&ProfileSigninConfirmationHandler::OnContinueButtonClicked,
                 base::Unretained(this)));
}

void ProfileSigninConfirmationHandler::OnCancelButtonClicked(
    const base::ListValue* args) {
  // TODO(dconnelly): redirect back to NTP?
  cancel_signin_.Run();
  dialog_->Close();
}

void ProfileSigninConfirmationHandler::OnCreateProfileClicked(
    const base::ListValue* args) {
  signin_with_new_profile_.Run();
  dialog_->Close();
}

void ProfileSigninConfirmationHandler::OnContinueButtonClicked(
    const base::ListValue* args) {
  continue_signin_.Run();
  dialog_->Close();
}

void ProfileSigninConfirmationDialog::ShowDialog(
    Profile* profile,
    const std::string& username,
    const base::Closure& cancel_signin,
    const base::Closure& signin_with_new_profile,
    const base::Closure& continue_signin) {
  ProfileSigninConfirmationDialog *dialog =
    new ProfileSigninConfirmationDialog(profile,
                                        username,
                                        cancel_signin,
                                        signin_with_new_profile,
                                        continue_signin);
  dialog->CheckShouldPromptForNewProfile(
      base::Bind(&ProfileSigninConfirmationDialog::Show,
                 dialog->weak_pointer_factory_.GetWeakPtr()));
}

ProfileSigninConfirmationDialog::ProfileSigninConfirmationDialog(
    Profile* profile,
    const std::string& username,
    const base::Closure& cancel_signin,
    const base::Closure& signin_with_new_profile,
    const base::Closure& continue_signin)
  : username_(username),
    prompt_for_new_profile_(true),
    cancel_signin_(cancel_signin),
    signin_with_new_profile_(signin_with_new_profile),
    continue_signin_(continue_signin),
    profile_(profile),
    weak_pointer_factory_(this) {
}

ProfileSigninConfirmationDialog::~ProfileSigninConfirmationDialog() {
}

void ProfileSigninConfirmationDialog::Close() const {
  closed_by_handler_ = true;
  delegate_->OnDialogCloseFromWebUI();
}

void ProfileSigninConfirmationDialog::Show(bool prompt) {
  prompt_for_new_profile_ = prompt;

  Browser* browser = FindBrowserWithProfile(profile_,
                                            chrome::GetActiveDesktop());
  if (!browser) {
    DLOG(WARNING) << "No browser found to display the confirmation dialog";
    cancel_signin_.Run();
    return;
  }

  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (!web_contents) {
    DLOG(WARNING) << "No web contents found to display the confirmation dialog";
    cancel_signin_.Run();
    return;
  }

  delegate_ = CreateConstrainedWebDialog(profile_, this, NULL, web_contents);
}

ui::ModalType ProfileSigninConfirmationDialog::GetDialogModalType() const {
  return ui::MODAL_TYPE_WINDOW;
}

string16 ProfileSigninConfirmationDialog::GetDialogTitle() const {
  return l10n_util::GetStringUTF16(
      IDS_ENTERPRISE_SIGNIN_PROFILE_LINK_DIALOG_TITLE);
}

GURL ProfileSigninConfirmationDialog::GetDialogContentURL() const {
  return GURL(chrome::kChromeUIProfileSigninConfirmationURL);
}

void ProfileSigninConfirmationDialog::GetWebUIMessageHandlers(
    std::vector<content::WebUIMessageHandler*>* handlers) const {
  handlers->push_back(
      new ProfileSigninConfirmationHandler(this,
                                           cancel_signin_,
                                           signin_with_new_profile_,
                                           continue_signin_));
}

void ProfileSigninConfirmationDialog::GetDialogSize(gfx::Size* size) const {
  const int kMinimumDialogWidth = 480;
#if defined(OS_WIN)
  const int kMinimumDialogHeight = 180;
#else
  const int kMinimumDialogHeight = 210;
#endif
  const int kProfileCreationMessageHeight = prompt_for_new_profile_ ? 50 : 0;
  size->SetSize(kMinimumDialogWidth,
                kMinimumDialogHeight + kProfileCreationMessageHeight);
}

std::string ProfileSigninConfirmationDialog::GetDialogArgs() const {
  std::string data;
  DictionaryValue dict;
  dict.SetString("username", username_);
  dict.SetBoolean("promptForNewProfile", prompt_for_new_profile_);
#if defined(OS_WIN)
  dict.SetBoolean("hideTitle", true);
#endif
  base::JSONWriter::Write(&dict, &data);
  return data;
}

void ProfileSigninConfirmationDialog::OnDialogClosed(
    const std::string& json_retval) {
  if (!closed_by_handler_)
    cancel_signin_.Run();
}

void ProfileSigninConfirmationDialog::OnCloseContents(
    content::WebContents* source,
    bool* out_close_dialog) {
  if (out_close_dialog)
    *out_close_dialog = true;
}

bool ProfileSigninConfirmationDialog::ShouldShowDialogTitle() const {
  return true;
}

bool ProfileSigninConfirmationDialog::HasBookmarks() {
  BookmarkModel* bookmarks = BookmarkModelFactory::GetForProfile(profile_);
  return bookmarks && bookmarks->HasBookmarks();
}

bool ProfileSigninConfirmationDialog::HasBeenShutdown() {
  return profile_->GetPrefs()->GetInitializationStatus() !=
    PrefService::INITIALIZATION_STATUS_CREATED_NEW_PROFILE;
}

void ProfileSigninConfirmationDialog::OnHistoryQueryResults(
    size_t max_entries,
    base::Callback<void(bool)> cb,
    CancelableRequestProvider::Handle handle,
    history::QueryResults* results) {
  history::QueryResults owned_results;
  results->Swap(&owned_results);
  cb.Run(owned_results.size() >= max_entries);
}

void ProfileSigninConfirmationDialog::CheckHasHistory(
    int max_entries,
    base::Callback<void(bool)> cb) {
  HistoryService* service =
    HistoryServiceFactory::GetForProfileWithoutCreating(profile_);
  if (!service) {
    cb.Run(false);
    return;
  }
  history::QueryOptions opts;
  opts.max_count = max_entries;
  service->QueryHistory(
      UTF8ToUTF16(""), opts, &history_count_request_consumer,
      base::Bind(&ProfileSigninConfirmationDialog::OnHistoryQueryResults,
                 weak_pointer_factory_.GetWeakPtr(),
                 max_entries, cb));
}

bool ProfileSigninConfirmationDialog::HasSyncedExtensions() {
  extensions::ExtensionSystem* system =
      extensions::ExtensionSystem::Get(profile_);
  if (system && system->extension_service()) {
    const ExtensionSet* extensions = system->extension_service()->extensions();
    for (ExtensionSet::const_iterator iter = extensions->begin();
         iter != extensions->end(); ++iter) {
      // The webstore is synced so that it stays put on the new tab
      // page, but since it's installed by default we don't want to
      // consider it when determining if the profile is dirty.
      if ((*iter)->IsSyncable() &&
          (*iter)->id() != extension_misc::kWebStoreAppId) {
        return true;
      }
    }
  }
  return false;
}

void ProfileSigninConfirmationDialog::CheckHasTypedURLs(
    base::Callback<void(bool)> cb) {
  HistoryService* service =
    HistoryServiceFactory::GetForProfileWithoutCreating(profile_);
  if (!service) {
    cb.Run(false);
    return;
  }
  service->ScheduleDBTask(new HasTypedURLsTask(cb),
                          &typed_urls_request_consumer);
}

void ProfileSigninConfirmationDialog::CheckShouldPromptForNewProfile(
    base::Callback<void(bool)> return_result) {
  if (HasBeenShutdown() ||
      HasBookmarks() ||
      HasSyncedExtensions()) {
    return_result.Run(true);
    return;
  }
  ChainCallbacksUntilTrue(
      base::Bind(&ProfileSigninConfirmationDialog::CheckHasHistory,
                 weak_pointer_factory_.GetWeakPtr(),
                 kHistoryEntriesBeforeNewProfilePrompt),
      base::Bind(&ProfileSigninConfirmationDialog::CheckHasTypedURLs,
                 weak_pointer_factory_.GetWeakPtr()),
      return_result);
}
