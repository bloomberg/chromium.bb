// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_BLACKLIST_BLACKLIST_MANAGER_H_
#define CHROME_BROWSER_PRIVACY_BLACKLIST_BLACKLIST_MANAGER_H_

#include <vector>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/non_thread_safe.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "chrome/common/notification_registrar.h"

class Blacklist;
class MessageLoop;
class Profile;
class Task;

namespace base {
class Thread;
}

class BlacklistPathProvider {
 public:
  virtual ~BlacklistPathProvider();

  virtual std::vector<FilePath> GetPersistentBlacklistPaths() = 0;

  virtual std::vector<FilePath> GetTransientBlacklistPaths() = 0;
};

// Updates one compiled binary blacklist based on a list of plaintext
// blacklists.
class BlacklistManager : public base::RefCountedThreadSafe<BlacklistManager>,
                         public NotificationObserver,
                         public NonThreadSafe {
 public:
  // You must create and destroy BlacklistManager on the same thread.
  BlacklistManager(Profile* profile,
                   BlacklistPathProvider* path_provider,
                   base::Thread* backend_thread);

  const Blacklist* GetCompiledBlacklist() const {
    return compiled_blacklist_.get();
  }

  // NotificationObserver
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

#ifdef UNIT_TEST
  const FilePath& compiled_blacklist_path() { return compiled_blacklist_path_; }
#endif  // UNIT_TEST

 private:
  class CompileBlacklistTask;
  class ReadBlacklistTask;

  friend class base::RefCountedThreadSafe<BlacklistManager>;

  ~BlacklistManager() {}

  void CompileBlacklist();
  void ReadBlacklist();

  void OnBlacklistCompilationFinished(bool success);
  void OnBlacklistReadFinished(Blacklist* blacklist);

  void RunTaskOnBackendThread(Task* task);

  // True after the first blacklist read has finished (regardless of success).
  // Used to avoid an infinite loop.
  bool first_read_finished_;

  Profile* profile_;

  // Path where we store the compiled blacklist.
  FilePath compiled_blacklist_path_;

  // Keep the compiled blacklist object in memory.
  scoped_ptr<Blacklist> compiled_blacklist_;

  BlacklistPathProvider* path_provider_;

  // Backend thread we will execute I/O operations on (NULL means no separate
  // thread).
  base::Thread* backend_thread_;

  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(BlacklistManager);
};

#endif  // CHROME_BROWSER_PRIVACY_BLACKLIST_BLACKLIST_MANAGER_H_
