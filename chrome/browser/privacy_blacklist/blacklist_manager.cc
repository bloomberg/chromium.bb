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

// Base class for tasks that use BlacklistManager. It ensures that the
// BlacklistManager won't get destroyed while we need it, and that it
// will be destroyed always on the same thread. That's why we use
// a custom Task instead of just NewRunnableMethod.
class BlacklistManagerTask : public Task {
 public:
  explicit BlacklistManagerTask(BlacklistManager* manager)
      : original_loop_(MessageLoop::current()),
        manager_(manager) {
    DCHECK(original_loop_);
    manager->AddRef();
  }

  ~BlacklistManagerTask() {
    original_loop_->ReleaseSoon(FROM_HERE, manager_);
  }

 protected:
  BlacklistManager* blacklist_manager() const { return manager_; }
  
  MessageLoop* original_loop_;

 private:
  BlacklistManager* manager_;

  DISALLOW_COPY_AND_ASSIGN(BlacklistManagerTask);
};

BlacklistPathProvider::~BlacklistPathProvider() {
}

class BlacklistManager::CompileBlacklistTask : public BlacklistManagerTask {
 public:
  CompileBlacklistTask(BlacklistManager* manager,
                       const FilePath& destination_blacklist,
                       const std::vector<FilePath>& source_blacklists)
      : BlacklistManagerTask(manager),
        destination_blacklist_(destination_blacklist),
        source_blacklists_(source_blacklists) {
  }

  virtual void Run() {
    bool success = true;
    
    Blacklist blacklist;
    std::string error_string;

    for (std::vector<FilePath>::const_iterator i = source_blacklists_.begin();
         i != source_blacklists_.end(); ++i) {
      if (!BlacklistIO::ReadText(&blacklist, *i, &error_string)) {
        success = false;
        break;
      }
    }

    // Only overwrite the current compiled blacklist if we read all source
    // files successfully.
    if (success)
      success = BlacklistIO::WriteBinary(&blacklist, destination_blacklist_);

    original_loop_->PostTask(
        FROM_HERE,
        NewRunnableMethod(blacklist_manager(),
                          &BlacklistManager::OnBlacklistCompilationFinished,
                          success));
  }

 private:
  FilePath destination_blacklist_;

  std::vector<FilePath> source_blacklists_;

  DISALLOW_COPY_AND_ASSIGN(CompileBlacklistTask);
};

class BlacklistManager::ReadBlacklistTask : public BlacklistManagerTask {
 public:
  ReadBlacklistTask(BlacklistManager* manager,
                    const FilePath& compiled_blacklist,
                    const std::vector<FilePath>& transient_blacklists)
      : BlacklistManagerTask(manager),
        compiled_blacklist_(compiled_blacklist),
        transient_blacklists_(transient_blacklists) {
  }

  virtual void Run() {
    scoped_ptr<Blacklist> blacklist(new Blacklist);
    if (!BlacklistIO::ReadBinary(blacklist.get(), compiled_blacklist_)) {
      ReportReadResult(NULL);
      return;
    }
    
    std::string error_string;
    std::vector<FilePath>::const_iterator i;
    for (i = transient_blacklists_.begin();
         i != transient_blacklists_.end(); ++i) {
      if (!BlacklistIO::ReadText(blacklist.get(), *i, &error_string)) {
        ReportReadResult(NULL);
        return;
      }
    }
    
    ReportReadResult(blacklist.release());
  }

 private:
  void ReportReadResult(Blacklist* blacklist) {
    original_loop_->PostTask(
        FROM_HERE, NewRunnableMethod(blacklist_manager(),
                                     &BlacklistManager::OnBlacklistReadFinished,
                                     blacklist));
  }
  
  FilePath compiled_blacklist_;
  std::vector<FilePath> transient_blacklists_;

  DISALLOW_COPY_AND_ASSIGN(ReadBlacklistTask);
};

BlacklistManager::BlacklistManager(Profile* profile,
                                   BlacklistPathProvider* path_provider,
                                   base::Thread* backend_thread)
    : first_read_finished_(false),
      profile_(profile),
      compiled_blacklist_path_(
        profile->GetPath().Append(chrome::kPrivacyBlacklistFileName)),
      path_provider_(path_provider),
      backend_thread_(backend_thread) {
  registrar_.Add(this,
                 NotificationType::BLACKLIST_PATH_PROVIDER_UPDATED,
                 Source<Profile>(profile));
  ReadBlacklist();
}

void BlacklistManager::Observe(NotificationType type,
                               const NotificationSource& source,
                               const NotificationDetails& details) {
  DCHECK(type == NotificationType::BLACKLIST_PATH_PROVIDER_UPDATED);
  CompileBlacklist();
}

void BlacklistManager::CompileBlacklist() {
  DCHECK(CalledOnValidThread());

  RunTaskOnBackendThread(new CompileBlacklistTask(
      this, compiled_blacklist_path_,
      path_provider_->GetPersistentBlacklistPaths()));
}

void BlacklistManager::ReadBlacklist() {
  DCHECK(CalledOnValidThread());
  
  RunTaskOnBackendThread(new ReadBlacklistTask(
      this, compiled_blacklist_path_,
      path_provider_->GetTransientBlacklistPaths()));
}

void BlacklistManager::OnBlacklistCompilationFinished(bool success) {
  DCHECK(CalledOnValidThread());
  
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

void BlacklistManager::OnBlacklistReadFinished(Blacklist* blacklist) {
  DCHECK(CalledOnValidThread());
  
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

void BlacklistManager::RunTaskOnBackendThread(Task* task) {
  if (backend_thread_) {
    backend_thread_->message_loop()->PostTask(FROM_HERE, task);
  } else {
    task->Run();
    delete task;
  }
}
