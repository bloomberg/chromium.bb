// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_blacklist/blacklist_manager.h"

#include "base/message_loop.h"
#include "base/task.h"
#include "base/thread.h"
#include "chrome/browser/privacy_blacklist/blacklist.h"
#include "chrome/browser/privacy_blacklist/blacklist_io.h"
#include "chrome/browser/profile.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"

// Base class for tasks that use BlacklistManager. It ensures that the
// BlacklistManager won't get destroyed while we need it, and that it
// will be destroyed always on the same thread. That's why we use
// a custom Task instead of just NewRunnableMethod.
class BlacklistManagerTask : public Task {
 public:
  explicit BlacklistManagerTask(BlacklistManager* manager)
      : manager_(manager),
        original_loop_(MessageLoop::current()) {
    DCHECK(original_loop_);
    manager->AddRef();
  }

  ~BlacklistManagerTask() {
    original_loop_->ReleaseSoon(FROM_HERE, manager_);
  }

 protected:
  BlacklistManager* blacklist_manager() const { return manager_; }

 private:
  BlacklistManager* manager_;

  MessageLoop* original_loop_;

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
    BlacklistIO io;
    bool success = true;

    for (std::vector<FilePath>::const_iterator i = source_blacklists_.begin();
         i != source_blacklists_.end(); ++i) {
      if (!io.Read(*i)) {
        success = false;
        break;
      }
    }

    // Only overwrite the current compiled blacklist if we read all source
    // files successfully.
    if (success)
      success = io.Write(destination_blacklist_);

    blacklist_manager()->OnBlacklistCompilationFinished(success);
  }

 private:
  FilePath destination_blacklist_;

  std::vector<FilePath> source_blacklists_;

  DISALLOW_COPY_AND_ASSIGN(CompileBlacklistTask);
};

class BlacklistManager::ReadBlacklistTask : public BlacklistManagerTask {
 public:
  ReadBlacklistTask(BlacklistManager* manager, const FilePath& blacklist_path)
      : BlacklistManagerTask(manager),
        blacklist_path_(blacklist_path) {
  }

  virtual void Run() {
    Blacklist* blacklist = new Blacklist(blacklist_path_);
    blacklist_manager()->OnBlacklistReadFinished(blacklist);
  }

 private:
  FilePath blacklist_path_;

  DISALLOW_COPY_AND_ASSIGN(ReadBlacklistTask);
};

BlacklistManager::BlacklistManager(Profile* profile,
                                   base::Thread* backend_thread)
    : compiled_blacklist_path_(
        profile->GetPath().Append(chrome::kPrivacyBlacklistFileName)),
      compiling_blacklist_(false),
      backend_thread_(backend_thread) {
  registrar_.Add(this,
                 NotificationType::PRIVACY_BLACKLIST_PATH_PROVIDER_UPDATED,
                 Source<Profile>(profile));
  ReadBlacklist();
}

void BlacklistManager::RegisterBlacklistPathProvider(
    BlacklistPathProvider* provider) {
  DCHECK(providers_.find(provider) == providers_.end());
  providers_.insert(provider);
}

void BlacklistManager::UnregisterBlacklistPathProvider(
    BlacklistPathProvider* provider) {
  DCHECK(providers_.find(provider) != providers_.end());
  providers_.erase(provider);
}

void BlacklistManager::Observe(NotificationType type,
                               const NotificationSource& source,
                               const NotificationDetails& details) {
  DCHECK(type == NotificationType::PRIVACY_BLACKLIST_PATH_PROVIDER_UPDATED);
  CompileBlacklist();
}

void BlacklistManager::CompileBlacklist() {
  if (compiling_blacklist_) {
    // If we end up here, that means that initial compile succeeded,
    // but then we couldn't read back the resulting Blacklist. Return early
    // to avoid a potential infinite loop.
    // TODO(phajdan.jr): Report the error.
    compiling_blacklist_ = false;
    return;
  }

  compiling_blacklist_ = true;

  std::vector<FilePath> source_blacklists;

  for (ProvidersSet::iterator provider = providers_.begin();
       provider != providers_.end(); ++provider) {
    std::vector<FilePath> provided_paths((*provider)->GetBlacklistPaths());
    source_blacklists.insert(source_blacklists.end(),
                             provided_paths.begin(), provided_paths.end());
  }

  RunTaskOnBackendThread(new CompileBlacklistTask(this, compiled_blacklist_path_,
                                                source_blacklists));
}

void BlacklistManager::ReadBlacklist() {
  RunTaskOnBackendThread(new ReadBlacklistTask(this, compiled_blacklist_path_));
}

void BlacklistManager::OnBlacklistCompilationFinished(bool success) {
  if (success) {
    ReadBlacklist();
  } else {
    // TODO(phajdan.jr): Report the error.
  }
}

void BlacklistManager::OnBlacklistReadFinished(Blacklist* blacklist) {
  if (blacklist->is_good()) {
    compiled_blacklist_.reset(blacklist);
    compiling_blacklist_ = false;
  } else {
    delete blacklist;
    CompileBlacklist();
  }
}

void BlacklistManager::RunTaskOnBackendThread(Task* task) {
  if (backend_thread_) {
    backend_thread_->message_loop()->PostTask(FROM_HERE, task);
  } else {
    task->Run();
    delete task;
  }
}
