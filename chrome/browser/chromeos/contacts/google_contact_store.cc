// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/contacts/google_contact_store.h"

#include <algorithm>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/contacts/contact.pb.h"
#include "chrome/browser/chromeos/contacts/contact_database.h"
#include "chrome/browser/chromeos/contacts/contact_store_observer.h"
#include "chrome/browser/chromeos/contacts/gdata_contacts_service.h"
#include "chrome/browser/google_apis/auth_service.h"
#include "chrome/browser/google_apis/time_util.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace contacts {

namespace {

// Name of the directory within the profile directory where the contact database
// is stored.
const base::FilePath::CharType kDatabaseDirectoryName[] =
    FILE_PATH_LITERAL("Google Contacts");

// We wait this long after the last update has completed successfully before
// updating again.
// TODO(derat): Decide what this should be.
const int kUpdateIntervalSec = 600;

// https://developers.google.com/google-apps/contacts/v3/index says that deleted
// contact (groups?) will only be returned for 30 days after deletion when the
// "showdeleted" parameter is set. If it's been longer than that since the last
// successful update, we do a full refresh to make sure that we haven't missed
// any deletions. Use 29 instead to make sure that we don't run afoul of
// daylight saving time shenanigans or minor skew in the system clock.
const int kForceFullUpdateDays = 29;

// When an update fails, we initially wait this many seconds before retrying.
// The delay increases exponentially in response to repeated failures.
const int kUpdateFailureInitialRetrySec = 5;

// Amount by which |update_delay_on_next_failure_| is multiplied on failure.
const int kUpdateFailureBackoffFactor = 2;

}  // namespace

GoogleContactStore::TestAPI::TestAPI(GoogleContactStore* store)
    : store_(store) {
  DCHECK(store);
}

GoogleContactStore::TestAPI::~TestAPI() {
  store_ = NULL;
}

void GoogleContactStore::TestAPI::SetDatabase(ContactDatabaseInterface* db) {
  store_->DestroyDatabase();
  store_->db_ = db;
}

void GoogleContactStore::TestAPI::SetGDataService(
    GDataContactsServiceInterface* service) {
  store_->gdata_service_.reset(service);
}

void GoogleContactStore::TestAPI::DoUpdate() {
  store_->UpdateContacts();
}

void GoogleContactStore::TestAPI::NotifyAboutNetworkStateChange(bool online) {
  net::NetworkChangeNotifier::ConnectionType type =
      online ?
      net::NetworkChangeNotifier::CONNECTION_UNKNOWN :
      net::NetworkChangeNotifier::CONNECTION_NONE;
  store_->OnConnectionTypeChanged(type);
}

scoped_ptr<ContactPointers> GoogleContactStore::TestAPI::GetLoadedContacts() {
  scoped_ptr<ContactPointers> contacts(new ContactPointers);
  for (ContactMap::const_iterator it = store_->contacts_.begin();
       it != store_->contacts_.end(); ++it) {
    contacts->push_back(it->second);
  }
  return contacts.Pass();
}

GoogleContactStore::GoogleContactStore(
    net::URLRequestContextGetter* url_request_context_getter,
    Profile* profile)
    : url_request_context_getter_(url_request_context_getter),
      profile_(profile),
      db_(new ContactDatabase),
      update_delay_on_next_failure_(
          base::TimeDelta::FromSeconds(kUpdateFailureInitialRetrySec)),
      is_online_(true),
      should_update_when_online_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  net::NetworkChangeNotifier::AddConnectionTypeObserver(this);
  is_online_ = !net::NetworkChangeNotifier::IsOffline();
}

GoogleContactStore::~GoogleContactStore() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  weak_ptr_factory_.InvalidateWeakPtrs();
  net::NetworkChangeNotifier::RemoveConnectionTypeObserver(this);
  DestroyDatabase();
}

void GoogleContactStore::Init() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Create a GData service if one hasn't already been assigned for testing.
  if (!gdata_service_.get()) {
    gdata_service_.reset(new GDataContactsService(
        url_request_context_getter_, profile_));
    gdata_service_->Initialize();
  }

  base::FilePath db_path = profile_->GetPath().Append(kDatabaseDirectoryName);
  VLOG(1) << "Initializing contact database \"" << db_path.value() << "\" for "
          << profile_->GetProfileName();
  db_->Init(db_path,
            base::Bind(&GoogleContactStore::OnDatabaseInitialized,
                       weak_ptr_factory_.GetWeakPtr()));
}

void GoogleContactStore::AppendContacts(ContactPointers* contacts_out) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(contacts_out);
  for (ContactMap::const_iterator it = contacts_.begin();
       it != contacts_.end(); ++it) {
    if (!it->second->deleted())
      contacts_out->push_back(it->second);
  }
}

const Contact* GoogleContactStore::GetContactById(
    const std::string& contact_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return contacts_.Find(contact_id);
}

void GoogleContactStore::AddObserver(ContactStoreObserver* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void GoogleContactStore::RemoveObserver(ContactStoreObserver* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

void GoogleContactStore::OnConnectionTypeChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  bool was_online = is_online_;
  is_online_ = (type != net::NetworkChangeNotifier::CONNECTION_NONE);
  if (!was_online && is_online_ && should_update_when_online_) {
    should_update_when_online_ = false;
    UpdateContacts();
  }
}

base::Time GoogleContactStore::GetCurrentTime() const {
  return !current_time_for_testing_.is_null() ?
         current_time_for_testing_ :
         base::Time::Now();
}

void GoogleContactStore::DestroyDatabase() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (db_) {
    db_->DestroyOnUIThread();
    db_ = NULL;
  }
}

void GoogleContactStore::UpdateContacts() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // If we're offline, defer the update.
  if (!is_online_) {
    VLOG(1) << "Deferring contact update due to offline state";
    should_update_when_online_ = true;
    return;
  }

  base::Time min_update_time;
  base::TimeDelta time_since_last_update =
      last_successful_update_start_time_.is_null() ?
      base::TimeDelta() :
      GetCurrentTime() - last_successful_update_start_time_;

  if (!last_contact_update_time_.is_null() &&
      time_since_last_update <
      base::TimeDelta::FromDays(kForceFullUpdateDays)) {
    // TODO(derat): I'm adding one millisecond to the last update time here as I
    // don't want to re-download the same most-recently-updated contact each
    // time, but what happens if within the same millisecond, contact A is
    // updated, we do a sync, and then contact B is updated? I'm probably being
    // overly paranoid about this.
    min_update_time =
        last_contact_update_time_ + base::TimeDelta::FromMilliseconds(1);
  }
  if (min_update_time.is_null()) {
    VLOG(1) << "Downloading all contacts for " << profile_->GetProfileName();
  } else {
    VLOG(1) << "Downloading contacts updated since "
            << google_apis::util::FormatTimeAsString(min_update_time) << " for "
            << profile_->GetProfileName();
  }

  gdata_service_->DownloadContacts(
      base::Bind(&GoogleContactStore::OnDownloadSuccess,
                 weak_ptr_factory_.GetWeakPtr(),
                 min_update_time.is_null(),
                 GetCurrentTime()),
      base::Bind(&GoogleContactStore::OnDownloadFailure,
                 weak_ptr_factory_.GetWeakPtr()),
      min_update_time);
}

void GoogleContactStore::ScheduleUpdate(bool last_update_was_successful) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  base::TimeDelta delay;
  if (last_update_was_successful) {
    delay = base::TimeDelta::FromSeconds(kUpdateIntervalSec);
    update_delay_on_next_failure_ =
        base::TimeDelta::FromSeconds(kUpdateFailureInitialRetrySec);
  } else {
    delay = update_delay_on_next_failure_;
    update_delay_on_next_failure_ = std::min(
        update_delay_on_next_failure_ * kUpdateFailureBackoffFactor,
        base::TimeDelta::FromSeconds(kUpdateIntervalSec));
  }
  VLOG(1) << "Scheduling update of " << profile_->GetProfileName()
          << " in " << delay.InSeconds() << " second(s)";
  update_timer_.Start(
      FROM_HERE, delay, this, &GoogleContactStore::UpdateContacts);
}

void GoogleContactStore::MergeContacts(
    bool is_full_update,
    scoped_ptr<ScopedVector<Contact> > updated_contacts) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (is_full_update) {
    contacts_.Clear();
    last_contact_update_time_ = base::Time();
  }

  // Find the maximum update time from |updated_contacts| since contacts whose
  // |deleted| flags are set won't be saved to |contacts_|.
  for (ScopedVector<Contact>::iterator it = updated_contacts->begin();
       it != updated_contacts->end(); ++it) {
    last_contact_update_time_ =
        std::max(last_contact_update_time_,
                 base::Time::FromInternalValue((*it)->update_time()));
  }
  VLOG(1) << "Last contact update time is "
          << (last_contact_update_time_.is_null() ?
              std::string("null") :
              google_apis::util::FormatTimeAsString(last_contact_update_time_));

  contacts_.Merge(updated_contacts.Pass(), ContactMap::DROP_DELETED_CONTACTS);
}

void GoogleContactStore::OnDownloadSuccess(
    bool is_full_update,
    const base::Time& update_start_time,
    scoped_ptr<ScopedVector<Contact> > updated_contacts) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  VLOG(1) << "Got " << updated_contacts->size() << " contact(s) for "
          << profile_->GetProfileName();

  // Copy the pointers so we can update just these contacts in the database.
  scoped_ptr<ContactPointers> contacts_to_save_to_db(new ContactPointers);
  scoped_ptr<ContactDatabaseInterface::ContactIds>
      contact_ids_to_delete_from_db(new ContactDatabaseInterface::ContactIds);
  if (db_) {
    for (size_t i = 0; i < updated_contacts->size(); ++i) {
      Contact* contact = (*updated_contacts)[i];
      if (contact->deleted())
        contact_ids_to_delete_from_db->push_back(contact->contact_id());
      else
        contacts_to_save_to_db->push_back(contact);
    }
  }
  bool got_updates = !updated_contacts->empty();

  MergeContacts(is_full_update, updated_contacts.Pass());
  last_successful_update_start_time_ = update_start_time;

  if (is_full_update || got_updates) {
    FOR_EACH_OBSERVER(ContactStoreObserver,
                      observers_,
                      OnContactsUpdated(this));
  }

  if (db_) {
    // Even if this was an incremental update and we didn't get any updated
    // contacts, we still want to write updated metadata containing
    // |update_start_time|.
    VLOG(1) << "Saving " << contacts_to_save_to_db->size() << " contact(s) to "
            << "database and deleting " << contact_ids_to_delete_from_db->size()
            << " as " << (is_full_update ? "full" : "incremental") << " update";

    scoped_ptr<UpdateMetadata> metadata(new UpdateMetadata);
    metadata->set_last_update_start_time(update_start_time.ToInternalValue());
    metadata->set_last_contact_update_time(
        last_contact_update_time_.ToInternalValue());

    db_->SaveContacts(
        contacts_to_save_to_db.Pass(),
        contact_ids_to_delete_from_db.Pass(),
        metadata.Pass(),
        is_full_update,
        base::Bind(&GoogleContactStore::OnDatabaseContactsSaved,
                   weak_ptr_factory_.GetWeakPtr()));

    // We'll schedule an update from OnDatabaseContactsSaved() after we're done
    // writing to the database -- we don't want to modify the contacts while
    // they're being used by the database.
  } else {
    ScheduleUpdate(true);
  }
}

void GoogleContactStore::OnDownloadFailure() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  LOG(WARNING) << "Contacts download failed for " << profile_->GetProfileName();
  ScheduleUpdate(false);
}

void GoogleContactStore::OnDatabaseInitialized(bool success) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (success) {
    VLOG(1) << "Contact database initialized for "
            << profile_->GetProfileName();
    db_->LoadContacts(base::Bind(&GoogleContactStore::OnDatabaseContactsLoaded,
                                 weak_ptr_factory_.GetWeakPtr()));
  } else {
    LOG(WARNING) << "Failed to initialize contact database for "
                 << profile_->GetProfileName();
    // Limp along as best as we can: throw away the database and do an update,
    // which will schedule further updates.
    DestroyDatabase();
    UpdateContacts();
  }
}

void GoogleContactStore::OnDatabaseContactsLoaded(
    bool success,
    scoped_ptr<ScopedVector<Contact> > contacts,
    scoped_ptr<UpdateMetadata> metadata) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (success) {
    VLOG(1) << "Loaded " << contacts->size() << " contact(s) from database";
    MergeContacts(true, contacts.Pass());
    last_successful_update_start_time_ =
        base::Time::FromInternalValue(metadata->last_update_start_time());
    last_contact_update_time_ = std::max(
        last_contact_update_time_,
        base::Time::FromInternalValue(metadata->last_contact_update_time()));

    if (!contacts_.empty()) {
      FOR_EACH_OBSERVER(ContactStoreObserver,
                        observers_,
                        OnContactsUpdated(this));
    }
  } else {
    LOG(WARNING) << "Failed to load contacts from database";
  }
  UpdateContacts();
}

void GoogleContactStore::OnDatabaseContactsSaved(bool success) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!success)
    LOG(WARNING) << "Failed to save contacts to database";

  // We only update the database when we've successfully downloaded contacts, so
  // report success to ScheduleUpdate() even if the database update failed.
  ScheduleUpdate(true);
}

GoogleContactStoreFactory::GoogleContactStoreFactory() {
}

GoogleContactStoreFactory::~GoogleContactStoreFactory() {
}

bool GoogleContactStoreFactory::CanCreateContactStoreForProfile(
    Profile* profile) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(profile);
  return google_apis::AuthService::CanAuthenticate(profile);
}

ContactStore* GoogleContactStoreFactory::CreateContactStore(Profile* profile) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(CanCreateContactStoreForProfile(profile));
  return new GoogleContactStore(
      g_browser_process->system_request_context(), profile);
}

}  // namespace contacts
