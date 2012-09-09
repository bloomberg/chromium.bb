// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CONTACTS_GOOGLE_CONTACT_STORE_H_
#define CHROME_BROWSER_CHROMEOS_CONTACTS_GOOGLE_CONTACT_STORE_H_

#include "chrome/browser/chromeos/contacts/contact_store.h"

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time.h"
#include "base/timer.h"
#include "chrome/browser/chromeos/contacts/contact_map.h"
#include "net/base/network_change_notifier.h"

class Profile;

namespace gdata {
class GDataContactsServiceInterface;
}

namespace contacts {

class Contact;
class ContactDatabaseInterface;
class UpdateMetadata;

// A collection of contacts from a Google account.
class GoogleContactStore
    : public ContactStore,
      public net::NetworkChangeNotifier::ConnectionTypeObserver {
 public:
  class TestAPI {
   public:
    explicit TestAPI(GoogleContactStore* store);
    ~TestAPI();

    bool update_scheduled() { return store_->update_timer_.IsRunning(); }

    void set_current_time(const base::Time& time) {
      store_->current_time_for_testing_ = time;
    }

    // Takes ownership of |db|. Must be called before Init().
    void SetDatabase(ContactDatabaseInterface* db);

    // Takes ownership of |service|. Must be called before Init(). The caller is
    // responsible for calling |service|'s Initialize() method.
    void SetGDataService(gdata::GDataContactsServiceInterface* service);

    // Triggers an update, similar to what happens when the update timer fires.
    void DoUpdate();

    // Notifies the store that the system has gone online or offline.
    void NotifyAboutNetworkStateChange(bool online);

   private:
    GoogleContactStore* store_;  // not owned

    DISALLOW_COPY_AND_ASSIGN(TestAPI);
  };

  explicit GoogleContactStore(Profile* profile);
  virtual ~GoogleContactStore();

  // ContactStore implementation:
  virtual void Init() OVERRIDE;
  virtual void AppendContacts(ContactPointers* contacts_out) OVERRIDE;
  virtual const Contact* GetContactById(const std::string& contact_id) OVERRIDE;
  virtual void AddObserver(ContactStoreObserver* observer) OVERRIDE;
  virtual void RemoveObserver(ContactStoreObserver* observer) OVERRIDE;

  // net::NetworkChangeNotifier::ConnectionTypeObserver implementation:
  virtual void OnConnectionTypeChanged(
      net::NetworkChangeNotifier::ConnectionType type) OVERRIDE;

 private:
  // Returns the current time. Uses |current_time_for_testing_| instead if it's
  // set.
  base::Time GetCurrentTime() const;

  // Destroys |db_| if non-NULL and resets the pointer.
  // The underlying data is preserved on-disk.
  void DestroyDatabase();

  // Asynchronously downloads updated contacts and merges them into |contacts_|.
  void UpdateContacts();

  // Starts |update_timer_| so UpdateContacts() will be run.
  void ScheduleUpdate(bool last_update_was_successful);

  // Moves |updated_contacts| into |contacts_| and updates
  // |last_contact_update_time_|.
  void MergeContacts(bool is_full_update,
                     scoped_ptr<ScopedVector<Contact> > updated_contacts);

  // Handles a successful download, merging |updated_contacts| and saving the
  // updated contacts to |db_|.
  void OnDownloadSuccess(bool is_full_update,
                         const base::Time& update_start_time,
                         scoped_ptr<ScopedVector<Contact> > updated_contacts);

  // Handles a failed update. A new update is scheduled.
  void OnDownloadFailure();

  // Handles |db_|'s initialization. On success, we start loading the contacts
  // from the database; otherwise, we throw out the database and schedule an
  // update.
  void OnDatabaseInitialized(bool success);

  // Handles contacts being loaded from |db_|. On success, we merge the loaded
  // contacts. No matter what, we call UpdateContacts().
  void OnDatabaseContactsLoaded(bool success,
                                scoped_ptr<ScopedVector<Contact> > contacts,
                                scoped_ptr<UpdateMetadata> metadata);

  // Handles contacts being saved to |db_|. Now that the contacts are no longer
  // being accessed by the database, we schedule an update.
  void OnDatabaseContactsSaved(bool success);

  Profile* profile_;  // not owned

  ObserverList<ContactStoreObserver> observers_;

  // Owns the pointed-to Contact values.
  ContactMap contacts_;

  // Most-recent time that an entry in |contacts_| has been updated (as reported
  // by Google).
  base::Time last_contact_update_time_;

  // Used to download contacts.
  scoped_ptr<gdata::GDataContactsServiceInterface> gdata_service_;

  // Used to save contacts to disk and load them at startup. Owns the object.
  ContactDatabaseInterface* db_;

  // Used to schedule calls to UpdateContacts().
  base::OneShotTimer<GoogleContactStore> update_timer_;

  // Time at which the last successful update was started.
  base::Time last_successful_update_start_time_;

  // Amount of time that we'll wait before retrying the next time that an update
  // fails.
  base::TimeDelta update_delay_on_next_failure_;

  // Do we believe that it's likely that we'll be able to make network
  // connections?
  bool is_online_;

  // Should we call UpdateContacts() when |is_online_| becomes true?  Set when
  // UpdateContacts() is called while we're offline.
  bool should_update_when_online_;

  // If non-null, used in place of base::Time::Now() when the current time is
  // needed.
  base::Time current_time_for_testing_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<GoogleContactStore> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(GoogleContactStore);
};

// ContactStoreFactory implementation that returns GoogleContactStores.
class GoogleContactStoreFactory : public ContactStoreFactory {
 public:
  GoogleContactStoreFactory();
  virtual ~GoogleContactStoreFactory();

  // ContactStoreFactory implementation:
  virtual bool CanCreateContactStoreForProfile(Profile* profile) OVERRIDE;
  virtual ContactStore* CreateContactStore(Profile* profile) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(GoogleContactStoreFactory);
};

}  // namespace contacts

#endif  // CHROME_BROWSER_CHROMEOS_CONTACTS_GOOGLE_CONTACT_STORE_H_
