// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_blacklist/blacklist_manager.h"

#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/task.h"
#include "base/thread.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/privacy_blacklist/blacklist.h"
#include "chrome/browser/privacy_blacklist/blacklist_io.h"
#include "chrome/browser/profile.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"

BlacklistPathProvider::~BlacklistPathProvider() {
}

BlacklistManager::BlacklistManager(Profile* profile,
                                   BlacklistPathProvider* path_provider)
    : first_read_finished_(false),
      profile_(profile),
      compiled_blacklist_path_(
        profile->GetPath().Append(chrome::kPrivacyBlacklistFileName)),
      path_provider_(path_provider) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  DCHECK(path_provider_);

  registrar_.Add(this,
                 NotificationType::EXTENSIONS_READY,
                 Source<Profile>(profile));
  registrar_.Add(this,
                 NotificationType::EXTENSION_LOADED,
                 Source<Profile>(profile));
  registrar_.Add(this,
                 NotificationType::EXTENSION_UNLOADED,
                 Source<Profile>(profile));
}

void BlacklistManager::Initialize() {
  if (path_provider_->AreBlacklistPathsReady())
    ReadBlacklist();
}

const Blacklist* BlacklistManager::GetCompiledBlacklist() const {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  return compiled_blacklist_.get();
}

void BlacklistManager::Observe(NotificationType type,
                               const NotificationSource& source,
                               const NotificationDetails& details) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

  switch (type.value) {
    case NotificationType::EXTENSIONS_READY:
      ReadBlacklist();
      break;
    case NotificationType::EXTENSION_LOADED:
    case NotificationType::EXTENSION_UNLOADED:
      // Don't do anything if the path provider isn't ready. We're going to get
      // the paths when it becomes ready.
      // This makes an assumption that it isn't possible to install an extension
      // before ExtensionsService becomes ready.
      if (!path_provider_->AreBlacklistPathsReady())
        break;

      // We don't need to recompile the on-disk blacklist when the extension
      // doesn't have any blacklist.
      if (Details<Extension>(details).ptr()->privacy_blacklists().empty())
        break;

      CompileBlacklist();
      break;
    default:
      NOTREACHED();
      break;
  }
}

BlacklistManager::~BlacklistManager() {
}

void BlacklistManager::CompileBlacklist() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  DCHECK(path_provider_->AreBlacklistPathsReady());

  // The compiled blacklist is going to change. Make sure our clients don't use
  // the old one and wait for the update instead by indicating that the compiled
  // blacklist is not ready.
  ChromeThread::PostTask(ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(this,
                        &BlacklistManager::ResetPublishedCompiledBlacklist));

  // Schedule actual compilation on the background thread.
  ChromeThread::PostTask(ChromeThread::FILE, FROM_HERE,
      NewRunnableMethod(this, &BlacklistManager::DoCompileBlacklist,
                        path_provider_->GetPersistentBlacklistPaths()));
}

void BlacklistManager::DoCompileBlacklist(
    const std::vector<FilePath>& source_blacklists) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));

  Blacklist blacklist;
  std::string error_string;
  std::vector<string16> errors;
  for (std::vector<FilePath>::const_iterator i = source_blacklists.begin();
       i != source_blacklists.end(); ++i) {
    std::string error_string;
    if (!BlacklistIO::ReadText(&blacklist, *i, &error_string)) {
      string16 path = WideToUTF16(i->ToWStringHack());
      errors.push_back(path + ASCIIToUTF16(": " + error_string));
    }
  }

  if (!errors.empty()) {
    ChromeThread::PostTask(ChromeThread::UI, FROM_HERE,
        NewRunnableMethod(this,
                          &BlacklistManager::OnBlacklistCompilationReadErrors,
                          errors));
  }

  // Write the new compiled blacklist based on the data we could read
  // successfully.
  bool success = BlacklistIO::WriteBinary(&blacklist, compiled_blacklist_path_);
  ChromeThread::PostTask(ChromeThread::UI, FROM_HERE,
      NewRunnableMethod(this, &BlacklistManager::OnBlacklistCompilationFinished,
                        success));
}

void BlacklistManager::OnBlacklistCompilationFinished(bool success) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

  if (success) {
    ReadBlacklist();
  } else {
    string16 error_message(ASCIIToUTF16("Blacklist compilation failed."));
    NotificationService::current()->Notify(
        NotificationType::BLACKLIST_MANAGER_ERROR,
        Source<Profile>(profile_),
        Details<string16>(&error_message));
  }
}

void BlacklistManager::OnBlacklistCompilationReadErrors(
    const std::vector<string16>& errors) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

  string16 error_message(ASCIIToUTF16("Blacklist compilation failed.\n"));
  for (std::vector<string16>::const_iterator i = errors.begin();
       i != errors.end(); ++i) {
    error_message += *i + ASCIIToUTF16("\n");
  }
  NotificationService::current()->Notify(
      NotificationType::BLACKLIST_MANAGER_ERROR,
      Source<Profile>(profile_),
      Details<string16>(&error_message));
}

void BlacklistManager::ReadBlacklist() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  DCHECK(path_provider_->AreBlacklistPathsReady());

  ChromeThread::PostTask(ChromeThread::FILE, FROM_HERE,
      NewRunnableMethod(this, &BlacklistManager::DoReadBlacklist,
                        path_provider_->GetTransientBlacklistPaths()));
}

void BlacklistManager::DoReadBlacklist(
    const std::vector<FilePath>& transient_blacklists) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));

  scoped_ptr<Blacklist> blacklist(new Blacklist);
  if (BlacklistIO::ReadBinary(blacklist.get(), compiled_blacklist_path_)) {
    std::string error_string;
    std::vector<FilePath>::const_iterator i;
    for (i = transient_blacklists.begin();
         i != transient_blacklists.end(); ++i) {
      if (!BlacklistIO::ReadText(blacklist.get(), *i, &error_string)) {
        blacklist.reset();
        break;
      }
    }
  } else {
    blacklist.reset();
  }
  ChromeThread::PostTask(ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(this,
                        &BlacklistManager::UpdatePublishedCompiledBlacklist,
                        blacklist.release()));
}

void BlacklistManager::UpdatePublishedCompiledBlacklist(Blacklist* blacklist) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  if (blacklist)
    compiled_blacklist_.reset(blacklist);
  ChromeThread::PostTask(ChromeThread::UI, FROM_HERE,
      NewRunnableMethod(this, &BlacklistManager::OnBlacklistReadFinished,
                        blacklist != NULL));
}

void BlacklistManager::OnBlacklistReadFinished(bool success) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

  if (success) {
    NotificationService::current()->Notify(
        NotificationType::BLACKLIST_MANAGER_BLACKLIST_READ_FINISHED,
        Source<Profile>(profile_),
        NotificationService::NoDetails());
  } else {
    string16 error_message(ASCIIToUTF16("Blacklist read failed."));
    NotificationService::current()->Notify(
        NotificationType::BLACKLIST_MANAGER_ERROR,
        Source<Profile>(profile_),
        Details<string16>(&error_message));
    if (!first_read_finished_) {
      // If we're loading for the first time, the compiled blacklist could
      // just not exist. Try compiling it once.
      CompileBlacklist();
    }
  }
  first_read_finished_ = true;
}

void BlacklistManager::ResetPublishedCompiledBlacklist() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  compiled_blacklist_.reset();
}
