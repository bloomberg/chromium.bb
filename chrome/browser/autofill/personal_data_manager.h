// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_PERSONAL_DATA_MANAGER_H_
#define CHROME_BROWSER_AUTOFILL_PERSONAL_DATA_MANAGER_H_

#include <set>
#include <vector>

#include "base/scoped_ptr.h"
#include "base/scoped_vector.h"
#include "base/string16.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/autofill/credit_card.h"
#include "chrome/browser/autofill/field_types.h"
#include "chrome/browser/webdata/web_data_service.h"

class AutoFillManager;
class FormStructure;
class Profile;

// Handles loading and saving AutoFill profile information to the web database.
// This class also stores the profiles loaded from the database for use during
// AutoFill.
class PersonalDataManager : public WebDataServiceConsumer {
 public:
  // An interface the PersonalDataManager uses to notify its clients (observers)
  // when it has finished loading personal data from the web database.  Register
  // the observer via PersonalDataManager::SetObserver.
  class Observer {
   public:
    // Notifies the observer that the PersonalDataManager has finished loading.
    virtual void OnPersonalDataLoaded() = 0;

   protected:
    virtual ~Observer() {}
  };

  virtual ~PersonalDataManager();

  // WebDataServiceConsumer implementation:
  virtual void OnWebDataServiceRequestDone(WebDataService::Handle h,
                                           const WDTypedResult* result);

  // Sets the listener to be notified of PersonalDataManager events.
  void SetObserver(PersonalDataManager::Observer* observer);

  // Removes |observer| as the observer of this PersonalDataManager.
  void RemoveObserver(PersonalDataManager::Observer* observer);

  // If AutoFill is able to determine the field types of a significant number
  // of field types that contain information in the FormStructures and the user
  // has not previously been prompted, the user will be asked if he would like
  // to import the data. If the user selects yes, a profile will be created
  // with all of the information from recognized fields.
  bool ImportFormData(const std::vector<FormStructure*>& form_structures,
                      AutoFillManager* autofill_manager);

  // Sets |profiles_| to the contents of |profiles| and updates the web database
  // by adding, updating and removing profiles.
  void SetProfiles(std::vector<AutoFillProfile>* profiles);

  // Gets the possible field types for the given text, determined by matching
  // the text with all known personal information and returning matching types.
  void GetPossibleFieldTypes(const string16& text,
                             FieldTypeSet* possible_types);

  // Returns true if the credit card information is stored with a password.
  bool HasPassword();

  // Returns whether the personal data has been loaded from the web database.
  bool IsDataLoaded() const { return is_data_loaded_; }

  // This PersonalDataManager owns these profiles.  Their lifetime is until the
  // web database is updated with new profile information.
  const std::vector<AutoFillProfile*>& profiles() { return profiles_.get(); }

 private:
  // Make sure that only Profile and the PersonalDataManager tests can create an
  // instance of PersonalDataManager.
  friend class ProfileImpl;
  friend class PersonalDataManagerTest;

  explicit PersonalDataManager(Profile* profile);

  // Returns the profile of the tab contents.
  Profile* profile();

  // Initializes the object if needed.  This should be called at the beginning
  // of all the public functions to make sure that the object has been properly
  // initialized before use.
  void InitializeIfNeeded();

  // This will create and reserve a new unique ID for a profile.
  int CreateNextUniqueID();

  // Parses value to extract the components of a phone number and adds them to
  // profile.
  //
  // TODO(jhawkins): Investigate if this can be moved to PhoneField.
  void ParsePhoneNumber(AutoFillProfile* profile,
                        string16* value,
                        AutoFillFieldType number,
                        AutoFillFieldType city_code,
                        AutoFillFieldType country_code) const;

  // Loads the saved profiles from the web database.
  void LoadProfiles();

  // Cancels a pending query to the web database.
  void CancelPendingQuery();

  // The profile hosting this PersonalDataManager.
  Profile* profile_;

  // True if PersonalDataManager is initialized.
  bool is_initialized_;

  // True if personal data has been loaded from the web database.
  bool is_data_loaded_;

  // The set of already created unique IDs, used to create a new unique ID.
  std::set<int> unique_ids_;

  // The loaded profiles.
  ScopedVector<AutoFillProfile> profiles_;

  // The loaded credit cards.
  ScopedVector<FormGroup> credit_cards_;

  // The profile that is imported from a web form by ImportFormData.
  scoped_ptr<AutoFillProfile> imported_profile_;

  // The credit card that is imported from a web form by ImportFormData.
  scoped_ptr<CreditCard> imported_credit_card_;

  // The hash of the password used to store the credit card.  This is empty if
  // no password exists.
  string16 password_hash_;

  // When the manager makes a request from WebDataService, the database
  // is queried on another thread, we record the query handle until we
  // get called back.
  WebDataService::Handle pending_query_handle_;

  // The observer.  This can be NULL.
  Observer* observer_;

  DISALLOW_COPY_AND_ASSIGN(PersonalDataManager);
};

#endif  // CHROME_BROWSER_AUTOFILL_PERSONAL_DATA_MANAGER_H_
