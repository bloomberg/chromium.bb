// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_PERSONAL_DATA_MANAGER_H_
#define CHROME_BROWSER_AUTOFILL_PERSONAL_DATA_MANAGER_H_

#include <set>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/observer_list.h"
#include "base/string16.h"
#include "chrome/browser/api/sync/profile_sync_service_observer.h"
#include "chrome/browser/api/webdata/web_data_service_consumer.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/autofill/credit_card.h"
#include "chrome/browser/autofill/field_types.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class AutofillMetrics;
class FormStructure;
class PersonalDataManagerObserver;

namespace autofill_helper {
void SetProfiles(int, std::vector<AutofillProfile>*);
void SetCreditCards(int, std::vector<CreditCard>*);
}

namespace content {
class BrowserContext;
}

// Handles loading and saving Autofill profile information to the web database.
// This class also stores the profiles loaded from the database for use during
// Autofill.
class PersonalDataManager
    : public WebDataServiceConsumer,
      public ProfileSyncServiceObserver,
      public ProfileKeyedService,
      public content::NotificationObserver {
 public:
  // A pair of GUID and variant index. Represents a single FormGroup and a
  // specific data variant.
  typedef std::pair<std::string, size_t> GUIDPair;

  // WebDataServiceConsumer:
  virtual void OnWebDataServiceRequestDone(
      WebDataServiceBase::Handle h,
      const WDTypedResult* result) OVERRIDE;

  // Sets the listener to be notified of PersonalDataManager events.
  virtual void SetObserver(PersonalDataManagerObserver* observer);

  // Removes |observer| as the observer of this PersonalDataManager.
  virtual void RemoveObserver(PersonalDataManagerObserver* observer);

  // ProfileSyncServiceObserver:
  virtual void OnStateChanged() OVERRIDE;

  // ProfileKeyedService:
  // Cancels any pending requests to WebDataServiceBase and stops
  // listening for Sync notifications.
  virtual void Shutdown() OVERRIDE;

  // content::NotificationObserver:
  // Observes "batch" changes made by Sync and refreshes data from the
  // WebDataServiceBase in response.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Scans the given |form| for importable Autofill data. If the form includes
  // sufficient address data, it is immediately imported. If the form includes
  // sufficient credit card data, it is stored into |credit_card|, so that we
  // can prompt the user whether to save this data.
  // Returns |true| if sufficient address or credit card data was found.
  bool ImportFormData(const FormStructure& form,
                      const CreditCard** credit_card);

  // Saves |imported_profile| to the WebDB if it exists.
  virtual void SaveImportedProfile(const AutofillProfile& imported_profile);

  // Saves a credit card value detected in |ImportedFormData|.
  virtual void SaveImportedCreditCard(const CreditCard& imported_credit_card);

  // Adds |profile| to the web database.
  void AddProfile(const AutofillProfile& profile);

  // Updates |profile| which already exists in the web database.
  void UpdateProfile(const AutofillProfile& profile);

  // Removes the profile or credit card represented by |guid|.
  virtual void RemoveByGUID(const std::string& guid);

  // Returns the profile with the specified |guid|, or NULL if there is no
  // profile with the specified |guid|. Both web and auxiliary profiles may
  // be returned.
  AutofillProfile* GetProfileByGUID(const std::string& guid);

  // Adds |credit_card| to the web database.
  void AddCreditCard(const CreditCard& credit_card);

  // Updates |credit_card| which already exists in the web database.
  void UpdateCreditCard(const CreditCard& credit_card);

  // Returns the credit card with the specified |guid|, or NULL if there is
  // no credit card with the specified |guid|.
  CreditCard* GetCreditCardByGUID(const std::string& guid);

  // Gets the field types availabe in the stored address and credit card data.
  void GetNonEmptyTypes(FieldTypeSet* non_empty_types);

  // Returns true if the credit card information is stored with a password.
  bool HasPassword();

  // Returns whether the personal data has been loaded from the web database.
  virtual bool IsDataLoaded() const;

  // This PersonalDataManager owns these profiles and credit cards.  Their
  // lifetime is until the web database is updated with new profile and credit
  // card information, respectively.  |profiles()| returns both web and
  // auxiliary profiles.  |web_profiles()| returns only web profiles.
  const std::vector<AutofillProfile*>& GetProfiles();
  virtual const std::vector<AutofillProfile*>& web_profiles() const;
  virtual const std::vector<CreditCard*>& credit_cards() const;

  // Loads profiles that can suggest data for |type|. |field_contents| is the
  // part the user has already typed. |field_is_autofilled| is true if the field
  // has already been autofilled. |other_field_types| represents the rest of
  // form. Identifying info is loaded into the last four outparams.
  void GetProfileSuggestions(
      AutofillFieldType type,
      const string16& field_contents,
      bool field_is_autofilled,
      std::vector<AutofillFieldType> other_field_types,
      std::vector<string16>* values,
      std::vector<string16>* labels,
      std::vector<string16>* icons,
      std::vector<GUIDPair>* guid_pairs);

  // Gets credit cards that can suggest data for |type|. See
  // GetProfileSuggestions for argument descriptions. The variant in each
  // GUID pair should be ignored.
  void GetCreditCardSuggestions(
      AutofillFieldType type,
      const string16& field_contents,
      std::vector<string16>* values,
      std::vector<string16>* labels,
      std::vector<string16>* icons,
      std::vector<GUIDPair>* guid_pairs);

  // Re-loads profiles and credit cards from the WebDatabase asynchronously.
  // In the general case, this is a no-op and will re-create the same
  // in-memory model as existed prior to the call.  If any change occurred to
  // profiles in the WebDatabase directly, as is the case if the browser sync
  // engine processed a change from the cloud, we will learn of these as a
  // result of this call.
  //
  // Also see SetProfile for more details.
  virtual void Refresh();

  // Checks suitability of |profile| for adding to the user's set of profiles.
  static bool IsValidLearnableProfile(const AutofillProfile& profile);

  // Merges |profile| into one of the |existing_profiles| if possible; otherwise
  // appends |profile| to the end of that list. Fills |merged_profiles| with the
  // result.
  static bool MergeProfile(
      const AutofillProfile& profile,
      const std::vector<AutofillProfile*>& existing_profiles,
      std::vector<AutofillProfile>* merged_profiles);

 protected:
  // Only PersonalDataManagerFactory and certain tests can create instances of
  // PersonalDataManager.
  FRIEND_TEST_ALL_PREFIXES(AutofillMetricsTest, FirstMiddleLast);
  FRIEND_TEST_ALL_PREFIXES(AutofillMetricsTest, AutofillIsEnabledAtStartup);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           AggregateExistingAuxiliaryProfile);
  friend class AutofillTest;
  friend class PersonalDataManagerFactory;
  friend class PersonalDataManagerTest;
  friend class ProfileSyncServiceAutofillTest;
  friend class RemoveAutofillTester;
  friend class TestingAutomationProvider;
  friend struct base::DefaultDeleter<PersonalDataManager>;
  friend void autofill_helper::SetProfiles(int, std::vector<AutofillProfile>*);
  friend void autofill_helper::SetCreditCards(int, std::vector<CreditCard>*);

  PersonalDataManager();
  virtual ~PersonalDataManager();

  // Sets |web_profiles_| to the contents of |profiles| and updates the web
  // database by adding, updating and removing profiles.
  // The relationship between this and Refresh is subtle.
  // A call to |SetProfiles| could include out-of-date data that may conflict
  // if we didn't refresh-to-latest before an Autofill window was opened for
  // editing. |SetProfiles| is implemented to make a "best effort" to apply the
  // changes, but in extremely rare edge cases it is possible not all of the
  // updates in |profiles| make it to the DB.  This is why SetProfiles will
  // invoke Refresh after finishing, to ensure we get into a
  // consistent state.  See Refresh for details.
  void SetProfiles(std::vector<AutofillProfile>* profiles);

  // Sets |credit_cards_| to the contents of |credit_cards| and updates the web
  // database by adding, updating and removing credit cards.
  void SetCreditCards(std::vector<CreditCard>* credit_cards);

  // Loads the saved profiles from the web database.
  virtual void LoadProfiles();

  // Loads the auxiliary profiles.  Currently Mac only.
  virtual void LoadAuxiliaryProfiles();

  // Loads the saved credit cards from the web database.
  virtual void LoadCreditCards();

  // Receives the loaded profiles from the web data service and stores them in
  // |credit_cards_|.
  void ReceiveLoadedProfiles(WebDataServiceBase::Handle h,
                             const WDTypedResult* result);

  // Receives the loaded credit cards from the web data service and stores them
  // in |credit_cards_|.
  void ReceiveLoadedCreditCards(WebDataServiceBase::Handle h,
                                const WDTypedResult* result);

  // Cancels a pending query to the web database.  |handle| is a pointer to the
  // query handle.
  void CancelPendingQuery(WebDataServiceBase::Handle* handle);

  // Notifies Sync about data migration if necessary.
  void EmptyMigrationTrash();

  // The first time this is called, logs an UMA metrics for the number of
  // profiles the user has. On subsequent calls, does nothing.
  void LogProfileCount() const;

  // Returns the value of the AutofillEnabled pref.
  virtual bool IsAutofillEnabled() const;

  // For tests.
  const AutofillMetrics* metric_logger() const;
  void set_metric_logger(const AutofillMetrics* metric_logger);
  void set_browser_context(content::BrowserContext* context);

  // The browser context this PersonalDataManager is in.
  content::BrowserContext* browser_context_;

  // True if personal data has been loaded from the web database.
  bool is_data_loaded_;

  // The loaded web profiles.
  ScopedVector<AutofillProfile> web_profiles_;

  // Auxiliary profiles.
  mutable ScopedVector<AutofillProfile> auxiliary_profiles_;

  // Storage for combined web and auxiliary profiles.  Contents are weak
  // references.  Lifetime managed by |web_profiles_| and |auxiliary_profiles_|.
  mutable std::vector<AutofillProfile*> profiles_;

  // The loaded credit cards.
  ScopedVector<CreditCard> credit_cards_;

  // When the manager makes a request from WebDataServiceBase, the database
  // is queried on another thread, we record the query handle until we
  // get called back.  We store handles for both profile and credit card queries
  // so they can be loaded at the same time.
  WebDataServiceBase::Handle pending_profiles_query_;
  WebDataServiceBase::Handle pending_creditcards_query_;

  // The observers.
  ObserverList<PersonalDataManagerObserver> observers_;

 private:
  // Kicks off asynchronous loading of profiles and credit cards.
  void Init(content::BrowserContext* context);

  // For logging UMA metrics. Overridden by metrics tests.
  scoped_ptr<const AutofillMetrics> metric_logger_;

  // Whether we have already logged the number of profiles this session.
  mutable bool has_logged_profile_count_;

  // Manages registration lifetime for NotificationObserver implementation.
  content::NotificationRegistrar notification_registrar_;

  DISALLOW_COPY_AND_ASSIGN(PersonalDataManager);
};

#endif  // CHROME_BROWSER_AUTOFILL_PERSONAL_DATA_MANAGER_H_
