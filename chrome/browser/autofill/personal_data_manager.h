// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_PERSONAL_DATA_MANAGER_H_
#define CHROME_BROWSER_AUTOFILL_PERSONAL_DATA_MANAGER_H_
#pragma once

#include <set>
#include <vector>

#include "base/observer_list.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/scoped_vector.h"
#include "base/string16.h"
#include "chrome/browser/autofill/autofill_dialog.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/autofill/credit_card.h"
#include "chrome/browser/autofill/field_types.h"
#include "chrome/browser/webdata/web_data_service.h"

class AutofillManager;
class AutofillMetrics;
class FormStructure;
class Profile;

// Handles loading and saving AutoFill profile information to the web database.
// This class also stores the profiles loaded from the database for use during
// AutoFill.
class PersonalDataManager
    : public WebDataServiceConsumer,
      public AutoFillDialogObserver,
      public base::RefCountedThreadSafe<PersonalDataManager> {
 public:
  // An interface the PersonalDataManager uses to notify its clients (observers)
  // when it has finished loading personal data from the web database.  Register
  // the observer via PersonalDataManager::SetObserver.
  class Observer {
   public:
    // Notifies the observer that the PersonalDataManager has finished loading.
    // TODO: OnPersonalDataLoaded should be nuked in favor of only
    // OnPersonalDataChanged.
    virtual void OnPersonalDataLoaded() = 0;

    // Notifies the observer that the PersonalDataManager changed in some way.
    virtual void OnPersonalDataChanged() {}

   protected:
    virtual ~Observer() {}
  };

  // WebDataServiceConsumer implementation:
  virtual void OnWebDataServiceRequestDone(WebDataService::Handle h,
                                           const WDTypedResult* result);

  // AutoFillDialogObserver implementation:
  virtual void OnAutoFillDialogApply(std::vector<AutofillProfile>* profiles,
                                     std::vector<CreditCard>* credit_cards);

  // Sets the listener to be notified of PersonalDataManager events.
  virtual void SetObserver(PersonalDataManager::Observer* observer);

  // Removes |observer| as the observer of this PersonalDataManager.
  virtual void RemoveObserver(PersonalDataManager::Observer* observer);

        // TODO(isherman): Update this comment
  // If AutoFill is able to determine the field types of a significant number of
  // field types that contain information in the FormStructures a profile will
  // be created with all of the information from recognized fields. Returns
  // whether a profile was created.
  bool ImportFormData(const std::vector<const FormStructure*>& form_structures,
                      const CreditCard** credit_card);

  // Saves a credit card value detected in |ImportedFormData|.
  void SaveImportedCreditCard(const CreditCard& imported_credit_card);

  // Sets |web_profiles_| to the contents of |profiles| and updates the web
  // database by adding, updating and removing profiles.
  //
  // The relationship between this and Refresh is subtle.
  // A call to |SetProfiles| could include out-of-date data that may conflict
  // if we didn't refresh-to-latest before an AutoFill window was opened for
  // editing. |SetProfiles| is implemented to make a "best effort" to apply the
  // changes, but in extremely rare edge cases it is possible not all of the
  // updates in |profiles| make it to the DB.  This is why SetProfiles will
  // invoke Refresh after finishing, to ensure we get into a
  // consistent state.  See Refresh for details.
  void SetProfiles(std::vector<AutofillProfile>* profiles);

  // Sets |credit_cards_| to the contents of |credit_cards| and updates the web
  // database by adding, updating and removing credit cards.
  void SetCreditCards(std::vector<CreditCard>* credit_cards);

  // Adds |profile| to the web database.
  void AddProfile(const AutofillProfile& profile);

  // Updates |profile| which already exists in the web database.
  void UpdateProfile(const AutofillProfile& profile);

  // Removes the profile represented by |guid|.
  void RemoveProfile(const std::string& guid);

  // Returns the profile with the specified |guid|, or NULL if there is no
  // profile with the specified |guid|.
  AutofillProfile* GetProfileByGUID(const std::string& guid);

  // Adds |credit_card| to the web database.
  void AddCreditCard(const CreditCard& credit_card);

  // Updates |credit_card| which already exists in the web database.
  void UpdateCreditCard(const CreditCard& credit_card);

  // Removes the credit card represented by |guid|.
  void RemoveCreditCard(const std::string& guid);

  // Returns the credit card with the specified |guid|, or NULL if there is
  // no credit card with the specified |guid|.
  CreditCard* GetCreditCardByGUID(const std::string& guid);

  // Gets the possible field types for the given text, determined by matching
  // the text with all known personal information and returning matching types.
  void GetPossibleFieldTypes(const string16& text,
                             FieldTypeSet* possible_types);

  // Returns true if the credit card information is stored with a password.
  bool HasPassword();

  // Returns whether the personal data has been loaded from the web database.
  virtual bool IsDataLoaded() const;

  // This PersonalDataManager owns these profiles and credit cards.  Their
  // lifetime is until the web database is updated with new profile and credit
  // card information, respectively.  |profiles()| returns both web and
  // auxiliary profiles.  |web_profiles()| returns only web profiles.
  const std::vector<AutofillProfile*>& profiles();
  virtual const std::vector<AutofillProfile*>& web_profiles();
  virtual const std::vector<CreditCard*>& credit_cards();

  // Re-loads profiles and credit cards from the WebDatabase asynchronously.
  // In the general case, this is a no-op and will re-create the same
  // in-memory model as existed prior to the call.  If any change occurred to
  // profiles in the WebDatabase directly, as is the case if the browser sync
  // engine processed a change from the cloud, we will learn of these as a
  // result of this call.
  //
  // Also see SetProfile for more details.
  virtual void Refresh();

  // Kicks off asynchronous loading of profiles and credit cards.
  void Init(Profile* profile);

 protected:
  // Make sure that only Profile and certain tests can create an instance of
  // PersonalDataManager.
  friend class base::RefCountedThreadSafe<PersonalDataManager>;
  friend class AutoFillMergeTest;
  friend class PersonalDataManagerTest;
  friend class ProfileImpl;
  friend class ProfileSyncServiceAutofillTest;

  // For tests.
  static void set_has_logged_profile_count(bool has_logged_profile_count);

  PersonalDataManager();
  virtual ~PersonalDataManager();

  // Returns the profile of the tab contents.
  Profile* profile();

  // Loads the saved profiles from the web database.
  virtual void LoadProfiles();

  // Loads the auxiliary profiles.  Currently Mac only.
  void LoadAuxiliaryProfiles();

  // Loads the saved credit cards from the web database.
  virtual void LoadCreditCards();

  // Receives the loaded profiles from the web data service and stores them in
  // |credit_cards_|.
  void ReceiveLoadedProfiles(WebDataService::Handle h,
                             const WDTypedResult* result);

  // Receives the loaded credit cards from the web data service and stores them
  // in |credit_cards_|.
  void ReceiveLoadedCreditCards(WebDataService::Handle h,
                                const WDTypedResult* result);

  // Cancels a pending query to the web database.  |handle| is a pointer to the
  // query handle.
  void CancelPendingQuery(WebDataService::Handle* handle);

  // Saves |imported_profile| to the WebDB if it exists.
  virtual void SaveImportedProfile(const AutofillProfile& imported_profile);

  // Merges |profile| into one of the |existing_profiles| if possible; otherwise
  // appends |profile| to the end of that list. Fills |merged_profiles| with the
  // result.
  bool MergeProfile(const AutofillProfile& profile,
                    const std::vector<AutofillProfile*>& existing_profiles,
                    std::vector<AutofillProfile>* merged_profiles);

  // The first time this is called, logs an UMA metrics for the number of
  // profiles the user has. On subsequent calls, does nothing.
  void LogProfileCount() const;

  // For tests.
  const AutofillMetrics* metric_logger() const;
  void set_metric_logger(const AutofillMetrics* metric_logger);

  // The profile hosting this PersonalDataManager.
  Profile* profile_;

  // True if personal data has been loaded from the web database.
  bool is_data_loaded_;

  // The loaded web profiles.
  ScopedVector<AutofillProfile> web_profiles_;

  // Auxiliary profiles.
  ScopedVector<AutofillProfile> auxiliary_profiles_;

  // Storage for combined web and auxiliary profiles.  Contents are weak
  // references.  Lifetime managed by |web_profiles_| and |auxiliary_profiles_|.
  std::vector<AutofillProfile*> profiles_;

  // The loaded credit cards.
  ScopedVector<CreditCard> credit_cards_;

  // The hash of the password used to store the credit card.  This is empty if
  // no password exists.
  string16 password_hash_;

  // When the manager makes a request from WebDataService, the database
  // is queried on another thread, we record the query handle until we
  // get called back.  We store handles for both profile and credit card queries
  // so they can be loaded at the same time.
  WebDataService::Handle pending_profiles_query_;
  WebDataService::Handle pending_creditcards_query_;

  // The observers.
  ObserverList<Observer> observers_;

 private:
  // For logging UMA metrics. Overridden by metrics tests.
  scoped_ptr<const AutofillMetrics> metric_logger_;

  DISALLOW_COPY_AND_ASSIGN(PersonalDataManager);
};

#endif  // CHROME_BROWSER_AUTOFILL_PERSONAL_DATA_MANAGER_H_
