// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PERSONAL_DATA_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PERSONAL_DATA_MANAGER_H_

#include <list>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/suggestion.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_member.h"
#include "components/webdata/common/web_data_service_consumer.h"
#include "net/url_request/url_request_context_getter.h"

class AccountTrackerService;
class Browser;
class PrefService;
class RemoveAutofillTester;
class SigninManagerBase;

namespace autofill {
class AutofillInteractiveTest;
class AutofillTest;
class FormStructure;
class PersonalDataManagerObserver;
class PersonalDataManagerFactory;
}  // namespace autofill

namespace autofill_helper {
void SetProfiles(int, std::vector<autofill::AutofillProfile>*);
void SetCreditCards(int, std::vector<autofill::CreditCard>*);
}  // namespace autofill_helper

namespace syncer {
class SyncService;
}  // namespace syncer

namespace autofill {

extern const char kFrecencyFieldTrialName[];
extern const char kFrecencyFieldTrialStateEnabled[];
extern const char kFrecencyFieldTrialLimitParam[];

// Handles loading and saving Autofill profile information to the web database.
// This class also stores the profiles loaded from the database for use during
// Autofill.
class PersonalDataManager : public KeyedService,
                            public WebDataServiceConsumer,
                            public AutofillWebDataServiceObserverOnUISequence {
 public:
  explicit PersonalDataManager(const std::string& app_locale);
  ~PersonalDataManager() override;

  // Kicks off asynchronous loading of profiles and credit cards.
  // |pref_service| must outlive this instance.  |is_off_the_record| informs
  // this instance whether the user is currently operating in an off-the-record
  // context.
  void Init(scoped_refptr<AutofillWebDataService> database,
            PrefService* pref_service,
            AccountTrackerService* account_tracker,
            SigninManagerBase* signin_manager,
            bool is_off_the_record);

  // Called once the sync service is known to be instantiated. Note that it may
  // not be started, but it's preferences can be queried.
  void OnSyncServiceInitialized(syncer::SyncService* sync_service);

  // WebDataServiceConsumer:
  void OnWebDataServiceRequestDone(
      WebDataServiceBase::Handle h,
      std::unique_ptr<WDTypedResult> result) override;

  // AutofillWebDataServiceObserverOnUISequence:
  void AutofillMultipleChanged() override;
  void SyncStarted(syncer::ModelType model_type) override;

  // Adds a listener to be notified of PersonalDataManager events.
  virtual void AddObserver(PersonalDataManagerObserver* observer);

  // Removes |observer| as an observer of this PersonalDataManager.
  virtual void RemoveObserver(PersonalDataManagerObserver* observer);

  // Scans the given |form| for importable Autofill data. If the form includes
  // sufficient address data for a new profile, it is immediately imported. If
  // the form includes sufficient credit card data for a new credit card and
  // |credit_card_enabled| is set to |true|, it is stored into
  // |imported_credit_card| so that we can prompt the user whether to save this
  // data. If the form contains credit card data already present in a local
  // credit card entry *and* |should_return_local_card| is true, the data is
  // stored into |imported_credit_card| so that we can prompt the user whether
  // to upload it. |imported_credit_card_matches_masked_server_credit_card| is
  // set to |true| if the |TypeAndLastFourDigits| in |imported_credit_card|
  // matches the |TypeAndLastFourDigits| in a saved masked server card. Returns
  // |true| if sufficient address or credit card data was found.
  bool ImportFormData(
      const FormStructure& form,
      bool credit_card_enabled,
      bool should_return_local_card,
      std::unique_ptr<CreditCard>* imported_credit_card,
      bool* imported_credit_card_matches_masked_server_credit_card);

  // Called to indicate |data_model| was used (to fill in a form). Updates
  // the database accordingly. Can invalidate |data_model|, particularly if
  // it's a Mac address book entry.
  virtual void RecordUseOf(const AutofillDataModel& data_model);

  // Saves |imported_profile| to the WebDB if it exists. Returns the guid of
  // the new or updated profile, or the empty string if no profile was saved.
  virtual std::string SaveImportedProfile(
      const AutofillProfile& imported_profile);

  // Saves |imported_credit_card| to the WebDB if it exists. Returns the guid of
  // of the new or updated card, or the empty string if no card was saved.
  virtual std::string SaveImportedCreditCard(
      const CreditCard& imported_credit_card);

  // Adds |profile| to the web database.
  virtual void AddProfile(const AutofillProfile& profile);

  // Updates |profile| which already exists in the web database.
  virtual void UpdateProfile(const AutofillProfile& profile);

  // Removes the profile or credit card represented by |guid|.
  virtual void RemoveByGUID(const std::string& guid);

  // Returns the profile with the specified |guid|, or nullptr if there is no
  // profile with the specified |guid|. Both web and auxiliary profiles may
  // be returned.
  AutofillProfile* GetProfileByGUID(const std::string& guid);

  // Returns the profile with the specified |guid| from the given |profiles|, or
  // nullptr if there is no profile with the specified |guid|.
  static AutofillProfile* GetProfileFromProfilesByGUID(
      const std::string& guid,
      const std::vector<AutofillProfile*>& profiles);

  // Adds |credit_card| to the web database as a local card.
  virtual void AddCreditCard(const CreditCard& credit_card);

  // Updates |credit_card| which already exists in the web database. This
  // can only be used on local credit cards.
  virtual void UpdateCreditCard(const CreditCard& credit_card);

  // Adds |credit_card| to the web database as a full server card.
  virtual void AddFullServerCreditCard(const CreditCard& credit_card);

  // Update a server card. Only the full number and masked/unmasked
  // status can be changed. Looks up the card by server ID.
  virtual void UpdateServerCreditCard(const CreditCard& credit_card);

  // Updates the use stats and billing address id for the server |credit_card|.
  // Looks up the card by server_id.
  void UpdateServerCardMetadata(const CreditCard& credit_card);

  // Resets the card for |guid| to the masked state.
  void ResetFullServerCard(const std::string& guid);

  // Resets all unmasked cards to the masked state.
  void ResetFullServerCards();

  // Deletes all server profiles and cards (both masked and unmasked).
  void ClearAllServerData();

  // Sets a server credit card for test.
  void AddServerCreditCardForTest(std::unique_ptr<CreditCard> credit_card);

  // Returns the credit card with the specified |guid|, or nullptr if there is
  // no credit card with the specified |guid|.
  virtual CreditCard* GetCreditCardByGUID(const std::string& guid);

  // Returns the credit card with the specified |number|, or nullptr if there is
  // no credit card with the specified |number|.
  virtual CreditCard* GetCreditCardByNumber(const std::string& number);

  // Gets the field types availabe in the stored address and credit card data.
  void GetNonEmptyTypes(ServerFieldTypeSet* non_empty_types);

  // Returns true if the credit card information is stored with a password.
  bool HasPassword();

  // Returns whether the personal data has been loaded from the web database.
  virtual bool IsDataLoaded() const;

  // This PersonalDataManager owns these profiles and credit cards.  Their
  // lifetime is until the web database is updated with new profile and credit
  // card information, respectively.
  virtual std::vector<AutofillProfile*> GetProfiles() const;
  // Returns just SERVER_PROFILES.
  virtual std::vector<AutofillProfile*> GetServerProfiles() const;
  // Returns just LOCAL_CARD cards.
  virtual std::vector<CreditCard*> GetLocalCreditCards() const;
  // Returns all credit cards, server and local.
  virtual std::vector<CreditCard*> GetCreditCards() const;

  // Returns the profiles to suggest to the user, ordered by frecency.
  std::vector<AutofillProfile*> GetProfilesToSuggest() const;

  // Remove profiles that haven't been used after |min_last_used| from
  // |profiles|. The relative ordering of |profiles| is maintained.
  static void RemoveProfilesNotUsedSinceTimestamp(
      base::Time min_last_used,
      std::vector<AutofillProfile*>* profiles);

  // Loads profiles that can suggest data for |type|. |field_contents| is the
  // part the user has already typed. |field_is_autofilled| is true if the field
  // has already been autofilled. |other_field_types| represents the rest of
  // form.
  std::vector<Suggestion> GetProfileSuggestions(
      const AutofillType& type,
      const base::string16& field_contents,
      bool field_is_autofilled,
      const std::vector<ServerFieldType>& other_field_types);

  // Tries to delete disused addresses once per major version if the
  // feature is enabled.
  bool DeleteDisusedAddresses();

  // Returns the credit cards to suggest to the user. Those have been deduped
  // and ordered by frecency with the expired cards put at the end of the
  // vector.
  const std::vector<CreditCard*> GetCreditCardsToSuggest() const;

  // Remove credit cards that are expired at |comparison_time| and not used
  // since |min_last_used| from |cards|. The relative ordering of |cards| is
  // maintained.
  static void RemoveExpiredCreditCardsNotUsedSinceTimestamp(
      base::Time comparison_time,
      base::Time min_last_used,
      std::vector<CreditCard*>* cards);

  // Gets credit cards that can suggest data for |type|. See
  // GetProfileSuggestions for argument descriptions. The variant in each
  // GUID pair should be ignored.
  std::vector<Suggestion> GetCreditCardSuggestions(
      const AutofillType& type,
      const base::string16& field_contents);

  // Tries to delete disused credit cards once per major version if the
  // feature is enabled.
  bool DeleteDisusedCreditCards();

  // Re-loads profiles and credit cards from the WebDatabase asynchronously.
  // In the general case, this is a no-op and will re-create the same
  // in-memory model as existed prior to the call.  If any change occurred to
  // profiles in the WebDatabase directly, as is the case if the browser sync
  // engine processed a change from the cloud, we will learn of these as a
  // result of this call.
  //
  // Also see SetProfile for more details.
  virtual void Refresh();

  const std::string& app_locale() const { return app_locale_; }

  // Checks suitability of |profile| for adding to the user's set of profiles.
  static bool IsValidLearnableProfile(const AutofillProfile& profile,
                                      const std::string& app_locale);

  // Merges |new_profile| into one of the |existing_profiles| if possible;
  // otherwise appends |new_profile| to the end of that list. Fills
  // |merged_profiles| with the result. Returns the |guid| of the new or updated
  // profile.
  std::string MergeProfile(
      const AutofillProfile& new_profile,
      std::vector<std::unique_ptr<AutofillProfile>>* existing_profiles,
      const std::string& app_locale,
      std::vector<AutofillProfile>* merged_profiles);

  // Returns true if |country_code| is a country that the user is likely to
  // be associated with the user. More concretely, it checks if there are any
  // addresses with this country or if the user's system timezone is in the
  // given country.
  virtual bool IsCountryOfInterest(const std::string& country_code) const;

  // Returns our best guess for the country a user is likely to use when
  // inputting a new address. The value is calculated once and cached, so it
  // will only update when Chrome is restarted.
  virtual const std::string& GetDefaultCountryCodeForNewAddress() const;

  // De-dupe credit card to suggest. Full server cards are preferred over their
  // local duplicates, and local cards are preferred over their masked server
  // card duplicate.
  static void DedupeCreditCardToSuggest(
      std::list<CreditCard*>* cards_to_suggest);

  // Notifies test observers that personal data has changed.
  void NotifyPersonalDataChangedForTest() { NotifyPersonalDataChanged(); }

  // Sets the URL request context getter to be used when normalizing addresses
  // with libaddressinput's address validator.
  void SetURLRequestContextGetter(
      net::URLRequestContextGetter* context_getter) {
    context_getter_ = context_getter;
  }

  // Returns the class used to fetch the address validation rules.
  net::URLRequestContextGetter* GetURLRequestContextGetter() const {
    return context_getter_.get();
  }

  // Extract credit card from the form structure. This function allows for
  // duplicated field types in the form.
  CreditCard ExtractCreditCardFromForm(const FormStructure& form);

  // This function assumes |credit_card| contains the full PAN. Returns |true|
  // if the card number of |credit_card| is equal to any local card or any
  // unmasked server card known by the browser, or |TypeAndLastFourDigits| of
  // |credit_card| is equal to any masked server card known by the browser.
  bool IsKnownCard(const CreditCard& credit_card);

 protected:
  // Only PersonalDataManagerFactory and certain tests can create instances of
  // PersonalDataManager.
  FRIEND_TEST_ALL_PREFIXES(AutofillMetricsTest, FirstMiddleLast);
  FRIEND_TEST_ALL_PREFIXES(AutofillMetricsTest, AutofillIsEnabledAtStartup);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           DedupeProfiles_ProfilesToDelete);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           DedupeProfiles_GuidsMergeMap);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           UpdateCardsBillingAddressReference);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest, ApplyProfileUseDatesFix);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           ApplyProfileUseDatesFix_NotAppliedTwice);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           ApplyDedupingRoutine_CardsBillingAddressIdUpdated);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           ApplyDedupingRoutine_MergedProfileValues);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           ApplyDedupingRoutine_VerifiedProfileFirst);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           ApplyDedupingRoutine_VerifiedProfileLast);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           ApplyDedupingRoutine_MultipleVerifiedProfiles);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           ApplyDedupingRoutine_FeatureDisabled);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           ApplyDedupingRoutine_NopIfZeroProfiles);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           ApplyDedupingRoutine_NopIfOneProfile);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           ApplyDedupingRoutine_OncePerVersion);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           ApplyDedupingRoutine_MultipleDedupes);
  FRIEND_TEST_ALL_PREFIXES(
      PersonalDataManagerTest,
      ConvertWalletAddressesAndUpdateWalletCards_NewProfile);
  FRIEND_TEST_ALL_PREFIXES(
      PersonalDataManagerTest,
      ConvertWalletAddressesAndUpdateWalletCards_MergedProfile);
  FRIEND_TEST_ALL_PREFIXES(
      PersonalDataManagerTest,
      ConvertWalletAddressesAndUpdateWalletCards_NewCrd_AddressAlreadyConverted);  // NOLINT
  FRIEND_TEST_ALL_PREFIXES(
      PersonalDataManagerTest,
      ConvertWalletAddressesAndUpdateWalletCards_AlreadyConverted);
  FRIEND_TEST_ALL_PREFIXES(
      PersonalDataManagerTest,
      ConvertWalletAddressesAndUpdateWalletCards_MultipleSimilarWalletAddresses);  // NOLINT
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           DeleteDisusedCreditCards_OncePerVersion);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           DeleteDisusedCreditCards_DoNothingWhenDisabled);
  FRIEND_TEST_ALL_PREFIXES(
      PersonalDataManagerTest,
      DeleteDisusedCreditCards_OnlyDeleteExpiredDisusedLocalCards);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           GetCreditCardSuggestions_CreditCardAutofillDisabled);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           GetCreditCardSuggestions_NoCardsLoadedIfDisabled);

  friend class autofill::AutofillInteractiveTest;
  friend class autofill::AutofillTest;
  friend class autofill::PersonalDataManagerFactory;
  friend class PersonalDataManagerTest;
  friend class PersonalDataManagerTestBase;
  friend class SaveImportedProfileTest;
  friend class ProfileSyncServiceAutofillTest;
  friend class ::RemoveAutofillTester;
  friend std::default_delete<PersonalDataManager>;
  friend void autofill_helper::SetProfiles(
      int,
      std::vector<autofill::AutofillProfile>*);
  friend void autofill_helper::SetCreditCards(
      int,
      std::vector<autofill::CreditCard>*);
  friend void SetTestProfiles(Browser* browser,
                              std::vector<AutofillProfile>* profiles);

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
  virtual void SetProfiles(std::vector<AutofillProfile>* profiles);

  // Sets |credit_cards_| to the contents of |credit_cards| and updates the web
  // database by adding, updating and removing credit cards.
  void SetCreditCards(std::vector<CreditCard>* credit_cards);

  // Loads the saved profiles from the web database.
  virtual void LoadProfiles();

  // Loads the saved credit cards from the web database.
  virtual void LoadCreditCards();

  // Cancels a pending query to the web database.  |handle| is a pointer to the
  // query handle.
  void CancelPendingQuery(WebDataServiceBase::Handle* handle);

  // Notifies observers that personal data has changed.
  void NotifyPersonalDataChanged();

  // The first time this is called, logs a UMA metrics about the user's autofill
  // addresses. On subsequent calls, does nothing.
  void LogStoredProfileMetrics() const;

  // The first time this is called, logs an UMA metric about the user's autofill
  // credit cardss. On subsequent calls, does nothing.
  void LogStoredCreditCardMetrics() const;

  // Returns the value of the AutofillEnabled pref.
  virtual bool IsAutofillEnabled() const;

  // Overrideable for testing.
  virtual std::string CountryCodeForCurrentTimezone() const;

  // Sets which PrefService to use and observe. |pref_service| is not owned by
  // this class and must outlive |this|.
  void SetPrefService(PrefService* pref_service);

  void set_database(scoped_refptr<AutofillWebDataService> database) {
    database_ = database;
  }

  void set_account_tracker(AccountTrackerService* account_tracker) {
    account_tracker_ = account_tracker;
  }

  void set_signin_manager(SigninManagerBase* signin_manager) {
    signin_manager_ = signin_manager;
  }

  // The backing database that this PersonalDataManager uses.
  scoped_refptr<AutofillWebDataService> database_;

  // True if personal data has been loaded from the web database.
  bool is_data_loaded_;

  // The loaded web profiles. These are constructed from entries on web pages
  // and from manually editing in the settings.
  std::vector<std::unique_ptr<AutofillProfile>> web_profiles_;

  // Profiles read from the user's account stored on the server.
  mutable std::vector<std::unique_ptr<AutofillProfile>> server_profiles_;

  // Storage for web profiles.  Contents are weak references.  Lifetime managed
  // by |web_profiles_|.
  mutable std::vector<AutofillProfile*> profiles_;

  // Cached versions of the local and server credit cards.
  std::vector<std::unique_ptr<CreditCard>> local_credit_cards_;
  std::vector<std::unique_ptr<CreditCard>> server_credit_cards_;

  // When the manager makes a request from WebDataServiceBase, the database
  // is queried on another sequence, we record the query handle until we
  // get called back.  We store handles for both profile and credit card queries
  // so they can be loaded at the same time.
  WebDataServiceBase::Handle pending_profiles_query_;
  WebDataServiceBase::Handle pending_server_profiles_query_;
  WebDataServiceBase::Handle pending_creditcards_query_;
  WebDataServiceBase::Handle pending_server_creditcards_query_;

  // The observers.
  base::ObserverList<PersonalDataManagerObserver> observers_;

 private:
  // Finds the country code that occurs most frequently among all profiles.
  // Prefers verified profiles over unverified ones.
  std::string MostCommonCountryCodeFromProfiles() const;

  // Called when the value of prefs::kAutofillEnabled changes.
  void EnabledPrefChanged();

  // Go through the |form| fields and attempt to extract and import valid
  // address profiles. Returns true on extraction success of at least one
  // profile. There are many reasons that extraction may fail (see
  // implementation).
  bool ImportAddressProfiles(const FormStructure& form);

  // Helper method for ImportAddressProfiles which only considers the fields for
  // a specified |section|.
  bool ImportAddressProfileForSection(const FormStructure& form,
                                      const std::string& section);

  // Go through the |form| fields and attempt to extract a new credit card in
  // |imported_credit_card|, or update an existing card.
  // |should_return_local_card| will indicate whether |imported_credit_card| is
  // filled even if an existing card was updated.
  // |imported_credit_card_matches_masked_server_credit_card| will indicate
  // whether |imported_credit_card| is filled even if an existing masked server
  // card as the same |TypeAndLastFourDigits|. Success is defined as having a
  // new card to import, or having merged with an existing card.
  bool ImportCreditCard(
      const FormStructure& form,
      bool should_return_local_card,
      std::unique_ptr<CreditCard>* imported_credit_card,
      bool* imported_credit_card_matches_masked_server_credit_card);

  // Extracts credit card from the form structure. |hasDuplicateFieldType| will
  // be set as true if there are duplicated field types in the form.
  CreditCard ExtractCreditCardFromForm(const FormStructure& form,
                                       bool* hasDuplicateFieldType);

  // Returns credit card suggestions based on the |cards_to_suggest| and the
  // |type| and |field_contents| of the credit card field.
  std::vector<Suggestion> GetSuggestionsForCards(
      const AutofillType& type,
      const base::string16& field_contents,
      const std::vector<CreditCard*>& cards_to_suggest) const;

  // Returns true if the given credit card can be deleted in a major version
  // upgrade. The card will need to be local and disused, to be deletable.
  bool IsCreditCardDeletable(CreditCard* card);

  // Runs the Autofill use date fix routine if it's never been done. Returns
  // whether the routine was run.
  void ApplyProfileUseDatesFix();

  // Applies the deduping routine once per major version if the feature is
  // enabled. Calls DedupeProfiles with the content of |web_profiles_| as a
  // parameter. Removes the profiles to delete from the database and updates the
  // others. Also updates the credit cards' billing address references. Returns
  // true if the routine was run.
  bool ApplyDedupingRoutine();

  // Goes through all the |existing_profiles| and merges all similar unverified
  // profiles together. Also discards unverified profiles that are similar to a
  // verified profile. All the profiles except the results of the merges will be
  // added to |profile_guids_to_delete|. This routine should be run once per
  // major version. Records all the merges into the |guids_merge_map|.
  //
  // This method should only be called by ApplyDedupingRoutine. It is split for
  // testing purposes.
  void DedupeProfiles(
      std::vector<std::unique_ptr<AutofillProfile>>* existing_profiles,
      std::unordered_set<AutofillProfile*>* profile_guids_to_delete,
      std::unordered_map<std::string, std::string>* guids_merge_map);

  // Updates the credit cards' billing address reference based on the merges
  // that happened during the dedupe, as defined in |guids_merge_map|. Also
  // updates the cards entries in the database.
  void UpdateCardsBillingAddressReference(
      const std::unordered_map<std::string, std::string>& guids_merge_map);

  // Converts the Wallet addresses to local autofill profiles. This should be
  // called after all the syncable data has been processed (local cards and
  // profiles, Wallet data and metadata). Also updates Wallet cards' billing
  // address id to point to the local profiles.
  void ConvertWalletAddressesAndUpdateWalletCards();

  // Converts the Wallet addresses into local profiles either by merging with an
  // existing |local_profiles| of by adding a new one. Populates the
  // |server_id_profiles_map| to be used when updating cards where the address
  // was already converted. Also populates the |guids_merge_map| to keep the
  // link between the Wallet address and the equivalent local profile (from
  // merge or creation).
  bool ConvertWalletAddressesToLocalProfiles(
      std::vector<AutofillProfile>* local_profiles,
      std::unordered_map<std::string, AutofillProfile*>* server_id_profiles_map,
      std::unordered_map<std::string, std::string>* guids_merge_map);

  // Goes through the Wallet cards to find cards where the billing address is a
  // Wallet address which was already converted in a previous pass. Looks for a
  // matching local profile and updates the |guids_merge_map| to make the card
  // refert to it.
  bool UpdateWalletCardsAlreadyConvertedBillingAddresses(
      std::vector<AutofillProfile>* local_profiles,
      std::unordered_map<std::string, AutofillProfile*>* server_id_profiles_map,
      std::unordered_map<std::string, std::string>* guids_merge_map);

  // Tries to merge the |server_address| into the |existing_profiles| if
  // possible. Adds it to the list if no match is found. The existing profiles
  // should be sorted by decreasing frecency outside of this method, since this
  // will be called multiple times in a row. Returns the guid of the new or
  // updated profile.
  std::string MergeServerAddressesIntoProfiles(
      const AutofillProfile& server_address,
      std::vector<AutofillProfile>* existing_profiles);

  // Removes profile from web database according to |guid| and resets credit
  // card's billing address if that address is used by any credit cards.
  // The method does not refresh, this allows multiple removal with one
  // refreshing in the end.
  void RemoveAutofillProfileByGUIDAndBlankCreditCardReferecne(
      const std::string& guid);

  // Returns true if an address can be deleted in a major version upgrade.
  // An address is deletable if it is unverified, and not used by a valid
  // credit card as billing address, and not used for a long time(13 months).
  bool IsAddressDeletable(
      AutofillProfile* profile,
      const std::unordered_set<std::string>& used_billing_address_guids);

  // If the AutofillCreateDataForTest feature is enabled, this helper creates
  // autofill address data that would otherwise be difficult to create
  // manually using the UI.
  void CreateTestAddresses();

  // If the AutofillCreateDataForTest feature is enabled, this helper creates
  // autofill credit card data that would otherwise be difficult to create
  // manually using the UI.
  void CreateTestCreditCards();

  const std::string app_locale_;

  // The default country code for new addresses.
  mutable std::string default_country_code_;

  // The PrefService that this instance uses. Must outlive this instance.
  PrefService* pref_service_;

  // The AccountTrackerService that this instance uses. Must outlive this
  // instance.
  AccountTrackerService* account_tracker_;

  // The signin manager that this instance uses. Must outlive this instance.
  SigninManagerBase* signin_manager_;

  // Whether the user is currently operating in an off-the-record context.
  // Default value is false.
  bool is_off_the_record_;

  // Whether we have already logged the stored profile metrics this session.
  mutable bool has_logged_stored_profile_metrics_;

  // Whether we have already logged the stored credit card metrics this session.
  mutable bool has_logged_stored_credit_card_metrics_;

  // An observer to listen for changes to prefs::kAutofillCreditCardEnabled.
  std::unique_ptr<BooleanPrefMember> credit_card_enabled_pref_;

  // An observer to listen for changes to prefs::kAutofillEnabled.
  std::unique_ptr<BooleanPrefMember> enabled_pref_;

  // An observer to listen for changes to prefs::kAutofillWalletImportEnabled.
  std::unique_ptr<BooleanPrefMember> wallet_enabled_pref_;

  // True if autofill profile cleanup needs to be performed.
  bool is_autofill_profile_cleanup_pending_ = false;

  // Whether new information was received from the sync server.
  bool has_synced_new_data_ = false;

  // True if test data has been created this session.
  bool has_created_test_addresses_ = false;
  bool has_created_test_credit_cards_ = false;

  // The context for the request to be used to fetch libaddressinput's address
  // validation rules.
  scoped_refptr<net::URLRequestContextGetter> context_getter_;

  DISALLOW_COPY_AND_ASSIGN(PersonalDataManager);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PERSONAL_DATA_MANAGER_H_
