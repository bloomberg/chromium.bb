// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CONTACTS_CONTACT_DATABASE_H_
#define CHROME_BROWSER_CHROMEOS_CONTACTS_CONTACT_DATABASE_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"

namespace base {
class SequencedTaskRunner;
}

namespace leveldb {
class DB;
}

namespace contacts {

class Contact;
class UpdateMetadata;
typedef std::vector<const Contact*> ContactPointers;

// Interface for classes providing persistent storage of Contact objects.
class ContactDatabaseInterface {
 public:
  typedef std::vector<std::string> ContactIds;
  typedef base::Callback<void(bool success)> InitCallback;
  typedef base::Callback<void(bool success)> SaveCallback;
  typedef base::Callback<void(bool success,
                              scoped_ptr<ScopedVector<Contact> >,
                              scoped_ptr<UpdateMetadata>)>
      LoadCallback;

  // Asynchronously destroys the object after all in-progress file operations
  // have completed.
  virtual void DestroyOnUIThread() {}

  // Asynchronously initializes the object.  |callback| will be invoked on the
  // UI thread when complete.
  virtual void Init(const base::FilePath& database_dir,
                    InitCallback callback) = 0;

  // Asynchronously saves |contacts_to_save| and |metadata| to the database and
  // removes contacts with IDs contained in |contact_ids_to_delete|.  If
  // |is_full_update| is true, all existing contacts in the database not present
  // in |contacts_to_save| will be removed.  |callback| will be invoked on the
  // UI thread when complete.  The caller must not make changes to the
  // underlying passed-in Contact objects until the callback has been invoked.
  virtual void SaveContacts(scoped_ptr<ContactPointers> contacts_to_save,
                            scoped_ptr<ContactIds> contact_ids_to_delete,
                            scoped_ptr<UpdateMetadata> metadata,
                            bool is_full_update,
                            SaveCallback callback) = 0;

  // Asynchronously loads all contacts from the database and invokes |callback|
  // when complete.
  virtual void LoadContacts(LoadCallback callback) = 0;

 protected:
  virtual ~ContactDatabaseInterface() {}
};

class ContactDatabase : public ContactDatabaseInterface {
 public:
  ContactDatabase();

  // ContactDatabaseInterface implementation.
  virtual void DestroyOnUIThread() OVERRIDE;
  virtual void Init(const base::FilePath& database_dir,
                    InitCallback callback) OVERRIDE;
  virtual void SaveContacts(scoped_ptr<ContactPointers> contacts_to_save,
                            scoped_ptr<ContactIds> contact_ids_to_delete,
                            scoped_ptr<UpdateMetadata> metadata,
                            bool is_full_update,
                            SaveCallback callback) OVERRIDE;
  virtual void LoadContacts(LoadCallback callback) OVERRIDE;

 protected:
  virtual ~ContactDatabase();

 private:
  // Are we currently being run by |task_runner_|?
  bool IsRunByTaskRunner() const;

  // Deletes |this|.
  void DestroyFromTaskRunner();

  // Passes the supplied parameters to |callback| after being called on the UI
  // thread.  Used for replies sent in response to |task_runner_|'s tasks, so
  // that weak_ptrs can be used to avoid running the replies after the
  // ContactDatabase has been deleted.
  void RunInitCallback(InitCallback callback, const bool* success);
  void RunSaveCallback(SaveCallback callback, const bool* success);
  void RunLoadCallback(LoadCallback callback,
                       const bool* success,
                       scoped_ptr<ScopedVector<Contact> > contacts,
                       scoped_ptr<UpdateMetadata> metadata);

  // Initializes the database in |database_dir| and updates |success|.
  void InitFromTaskRunner(const base::FilePath& database_dir, bool* success);

  // Saves data to disk and updates |success|.
  void SaveContactsFromTaskRunner(scoped_ptr<ContactPointers> contacts_to_save,
                                  scoped_ptr<ContactIds> contact_ids_to_delete,
                                  scoped_ptr<UpdateMetadata> metadata,
                                  bool is_full_update,
                                  bool* success);

  // Loads contacts from disk and updates |success|.
  void LoadContactsFromTaskRunner(bool* success,
                                  ScopedVector<Contact>* contacts,
                                  UpdateMetadata* metadata);

  // Used to run blocking tasks in-order.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  scoped_ptr<leveldb::DB> db_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<ContactDatabase> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ContactDatabase);
};

}  // namespace contacts

#endif  // CHROME_BROWSER_CHROMEOS_CONTACTS_CONTACT_DATABASE_H_
