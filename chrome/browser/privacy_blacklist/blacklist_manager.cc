// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_blacklist/blacklist_manager.h"

#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/task.h"
#include "base/thread.h"
#include "chrome/browser/privacy_blacklist/blacklist.h"
#include "chrome/browser/privacy_blacklist/blacklist_io.h"
#include "chrome/browser/profile.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"

BlacklistPathProvider::~BlacklistPathProvider() {
}

BlacklistManager::BlacklistManager()
    : first_read_finished_(false),
      profile_(NULL),
      path_provider_(NULL) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
}

void BlacklistManager::Initialize(Profile* profile,
                                  BlacklistPathProvider* path_provider) {
  profile_ = profile;
  compiled_blacklist_path_ =
      profile->GetPath().Append(chrome::kPrivacyBlacklistFileName);
  path_provider_ = path_provider;

  registrar_.Add(this,
                 NotificationType::EXTENSION_LOADED,
                 Source<Profile>(profile));
  registrar_.Add(this,
                 NotificationType::EXTENSION_UNLOADED,
                 Source<Profile>(profile));
  ReadBlacklist();
}

void BlacklistManager::Observe(NotificationType type,
                               const NotificationSource& source,
                               const NotificationDetails& details) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

  switch (type.value) {
    case NotificationType::EXTENSION_LOADED:
    case NotificationType::EXTENSION_UNLOADED:
      CompileBlacklist();
      break;
    default:
      NOTREACHED();
      break;
  }
}

void BlacklistManager::CompileBlacklist() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

  ChromeThread::PostTask(ChromeThread::FILE, FROM_HERE,
      NewRunnableMethod(this, &BlacklistManager::DoCompileBlacklist,
                        path_provider_->GetPersistentBlacklistPaths()));
}

void BlacklistManager::DoCompileBlacklist(
    const std::vector<FilePath>& source_blacklists) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));

  bool success = true;

  Blacklist blacklist;
  std::string error_string;

  for (std::vector<FilePath>::const_iterator i = source_blacklists.begin();
       i != source_blacklists.end(); ++i) {
    if (!BlacklistIO::ReadText(&blacklist, *i, &error_string)) {
      success = false;
      break;
    }
  }

  // Only overwrite the current compiled blacklist if we read all source
  // files successfully.
  if (success)
    success = BlacklistIO::WriteBinary(&blacklist, compiled_blacklist_path_);

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

void BlacklistManager::ReadBlacklist() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

  ChromeThread::PostTask(ChromeThread::FILE, FROM_HERE,
      NewRunnableMethod(this, &BlacklistManager::DoReadBlacklist,
                        path_provider_->GetTransientBlacklistPaths()));
}

void BlacklistManager::DoReadBlacklist(
    const std::vector<FilePath>& transient_blacklists) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));

  scoped_ptr<Blacklist> blacklist(new Blacklist);
  if (!BlacklistIO::ReadBinary(blacklist.get(), compiled_blacklist_path_)) {
    ReportBlacklistReadResult(NULL);
    return;
  }

  std::string error_string;
  std::vector<FilePath>::const_iterator i;
  for (i = transient_blacklists.begin();
       i != transient_blacklists.end(); ++i) {
    if (!BlacklistIO::ReadText(blacklist.get(), *i, &error_string)) {
      ReportBlacklistReadResult(NULL);
      return;
    }
  }

  ReportBlacklistReadResult(blacklist.release());
}

void BlacklistManager::ReportBlacklistReadResult(Blacklist* blacklist) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));
  ChromeThread::PostTask(ChromeThread::UI, FROM_HERE,
      NewRunnableMethod(this, &BlacklistManager::OnBlacklistReadFinished,
                        blacklist));
}

void BlacklistManager::OnBlacklistReadFinished(Blacklist* blacklist) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

  if (!blacklist) {
    if (!first_read_finished_) {
      // If we're loading for the first time, the compiled blacklist could
      // just not exist. Try compiling it once.
      first_read_finished_ = true;
      CompileBlacklist();
    } else {
      string16 error_message(ASCIIToUTF16("Blacklist read failed."));
      NotificationService::current()->Notify(
          NotificationType::BLACKLIST_MANAGER_ERROR,
          Source<Profile>(profile_),
          Details<string16>(&error_message));
    }
    return;
  }
  first_read_finished_ = true;
  compiled_blacklist_.reset(blacklist);

  NotificationService::current()->Notify(
      NotificationType::BLACKLIST_MANAGER_BLACKLIST_READ_FINISHED,
      Source<Profile>(profile_),
      Details<Blacklist>(blacklist));
}
