// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/personal_data_manager.h"

#include <math.h>

#import <AddressBook/AddressBook.h>

#include "base/format_macros.h"
#include "base/guid.h"
#include "base/logging.h"
#import "base/mac/scoped_nsexception_enabler.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/browser/autofill_country.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/phone_number.h"
#include "components/autofill/core/common/autofill_pref_names.h"
#include "components/autofill/core/common/form_data.h"
#include "grit/components_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"

namespace autofill {
namespace {

// The maximum number of instances when the access Address Book prompt should
// be shown.
int kMaxTimesToShowMacAddressBook = 5;

// There is an uncommon sequence of events that causes the Address Book
// permissions dialog to appear more than once for a given install of Chrome.
//  1. Chrome has previously presented the Address Book permissions dialog.
//  2. Chrome is launched.
//  3. Chrome performs an auto-update, and changes its binary.
//  4. Chrome attempts to access the Address Book for the first time since (2).
// This sequence of events is rare because Chrome attempts to acess the Address
// Book when the user focuses most form fields, so (4) generally occurs before
// (3). For more details, see http://crbug.com/381763.
//
// When this sequence of events does occur, Chrome should not attempt to access
// the Address Book unless the user explicitly asks Chrome to do so. The
// jarring nature of the permissions dialog is worse than the potential benefit
// of pulling information from the Address Book.
//
// Set to true after the Address Book is accessed for the first time.
static bool g_accessed_address_book = false;

// Set to true after the Chrome binary has been changed.
static bool g_binary_changed = false;

const char kAddressBookOrigin[] = "OS X Address Book";

// Whether Chrome has attempted to access the Mac Address Book.
bool HasQueriedMacAddressBook(PrefService* pref_service) {
  return pref_service->GetBoolean(prefs::kAutofillMacAddressBookQueried);
}

// Whether the user wants Chrome to use the AddressBook to populate Autofill
// entries.
bool ShouldUseAddressBook(PrefService* pref_service) {
  return pref_service->GetBoolean(prefs::kAutofillUseMacAddressBook);
}

// Records a UMA metric indicating whether an attempt to access the Address
// Book was skipped because doing so would cause the Address Book permissions
// prompt to incorrectly appear.
void RecordAccessSkipped(bool skipped) {
  UMA_HISTOGRAM_BOOLEAN("Autofill.AddressBook.AccessSkipped", skipped);
}

ABAddressBook* GetAddressBook(PrefService* pref_service) {
  bool first_access = !HasQueriedMacAddressBook(pref_service);

  // +[ABAddressBook sharedAddressBook] throws an exception internally in
  // circumstances that aren't clear. The exceptions are only observed in crash
  // reports, so it is unknown whether they would be caught by AppKit and nil
  // returned, or if they would take down the app. In either case, avoid
  // crashing. http://crbug.com/129022
  ABAddressBook* addressBook = base::mac::RunBlockIgnoringExceptions(
      ^{ return [ABAddressBook sharedAddressBook]; });
  UMA_HISTOGRAM_BOOLEAN("Autofill.AddressBookAvailable", addressBook != nil);

  if (first_access) {
    UMA_HISTOGRAM_BOOLEAN("Autofill.AddressBookAvailableOnFirstAttempt",
                          addressBook != nil);
  }

  g_accessed_address_book = true;
  pref_service->SetBoolean(prefs::kAutofillMacAddressBookQueried, true);
  return addressBook;
}

// This implementation makes use of the Address Book API.  Profiles are
// generated that correspond to addresses in the "me" card that reside in the
// user's Address Book.  The caller passes a vector of profiles into the
// the constructer and then initiate the fetch from the Mac Address Book "me"
// card using the main |GetAddressBookMeCard()| method.  This clears any
// existing addresses and populates new addresses derived from the data found
// in the "me" card.
class AuxiliaryProfilesImpl {
 public:
  // Constructor takes a reference to the |profiles| that will be filled in
  // by the subsequent call to |GetAddressBookMeCard()|.  |profiles| may not
  // be NULL.
  explicit AuxiliaryProfilesImpl(ScopedVector<AutofillProfile>* profiles)
      : profiles_(*profiles) {
  }
  virtual ~AuxiliaryProfilesImpl() {}

  // Import the "me" card from the Mac Address Book and fill in |profiles_|.
  void GetAddressBookMeCard(const std::string& app_locale,
                            PrefService* pref_service,
                            bool record_metrics);

 private:
  void GetAddressBookNames(ABPerson* me,
                           NSString* addressLabelRaw,
                           AutofillProfile* profile);
  void GetAddressBookAddress(const std::string& app_locale,
                             NSDictionary* address,
                             AutofillProfile* profile);
  void GetAddressBookEmail(ABPerson* me,
                           NSString* addressLabelRaw,
                           AutofillProfile* profile);
  void GetAddressBookPhoneNumbers(ABPerson* me,
                                  NSString* addressLabelRaw,
                                  AutofillProfile* profile);

 private:
  // A reference to the profiles this class populates.
  ScopedVector<AutofillProfile>& profiles_;

  DISALLOW_COPY_AND_ASSIGN(AuxiliaryProfilesImpl);
};

// This method uses the |ABAddressBook| system service to fetch the "me" card
// from the active user's address book.  It looks for the user address
// information and translates it to the internal list of |AutofillProfile| data
// structures.
void AuxiliaryProfilesImpl::GetAddressBookMeCard(const std::string& app_locale,
                                                 PrefService* pref_service,
                                                 bool record_metrics) {
  profiles_.clear();

  // The user does not want Chrome to use the AddressBook to populate Autofill
  // entries.
  if (!ShouldUseAddressBook(pref_service))
    return;

  // See the comment at the definition of g_accessed_address_book for an
  // explanation of this logic.
  if (g_binary_changed && !g_accessed_address_book) {
    if (record_metrics)
      RecordAccessSkipped(true);
    return;
  }

  if (record_metrics)
    RecordAccessSkipped(false);

  ABAddressBook* addressBook = GetAddressBook(pref_service);

  ABPerson* me = [addressBook me];
  if (!me)
    return;

  ABMultiValue* addresses = [me valueForProperty:kABAddressProperty];

  // The number of characters at the end of the GUID to reserve for
  // distinguishing addresses within the "me" card.  Cap the number of addresses
  // we will fetch to the number that can be distinguished by this fragment of
  // the GUID.
  const size_t kNumAddressGUIDChars = 2;
  const size_t kNumHexDigits = 16;
  const size_t kMaxAddressCount = pow(kNumHexDigits, kNumAddressGUIDChars);
  NSUInteger count = MIN([addresses count], kMaxAddressCount);
  for (NSUInteger i = 0; i < count; i++) {
    NSDictionary* address = [addresses valueAtIndex:i];
    NSString* addressLabelRaw = [addresses labelAtIndex:i];

    // Create a new profile where the guid is set to the guid portion of the
    // |kABUIDProperty| taken from from the "me" address.  The format of
    // the |kABUIDProperty| is "<guid>:ABPerson", so we're stripping off the
    // raw guid here and using it directly, with one modification: we update the
    // last |kNumAddressGUIDChars| characters in the GUID to reflect the address
    // variant.  Note that we capped the number of addresses above, so this is
    // safe.
    const size_t kGUIDLength = 36U;
    const size_t kTrimmedGUIDLength = kGUIDLength - kNumAddressGUIDChars;
    std::string guid = base::SysNSStringToUTF8(
        [me valueForProperty:kABUIDProperty]).substr(0, kTrimmedGUIDLength);

    // The format string to print |kNumAddressGUIDChars| hexadecimal characters,
    // left-padded with 0's.
    const std::string kAddressGUIDFormat =
        base::StringPrintf("%%0%" PRIuS "X", kNumAddressGUIDChars);
    guid += base::StringPrintf(kAddressGUIDFormat.c_str(), i);
    DCHECK_EQ(kGUIDLength, guid.size());

    scoped_ptr<AutofillProfile> profile(
        new AutofillProfile(guid, kAddressBookOrigin));
    DCHECK(base::IsValidGUID(profile->guid()));

    // Fill in name and company information.
    GetAddressBookNames(me, addressLabelRaw, profile.get());

    // Fill in address information.
    GetAddressBookAddress(app_locale, address, profile.get());

    // Fill in email information.
    GetAddressBookEmail(me, addressLabelRaw, profile.get());

    // Fill in phone number information.
    GetAddressBookPhoneNumbers(me, addressLabelRaw, profile.get());

    profiles_.push_back(profile.release());
  }
}

// Name and company information is stored once in the Address Book against
// multiple addresses.  We replicate that information for each profile.
// We only propagate the company name to work profiles.
void AuxiliaryProfilesImpl::GetAddressBookNames(
    ABPerson* me,
    NSString* addressLabelRaw,
    AutofillProfile* profile) {
  NSString* firstName = [me valueForProperty:kABFirstNameProperty];
  NSString* middleName = [me valueForProperty:kABMiddleNameProperty];
  NSString* lastName = [me valueForProperty:kABLastNameProperty];
  NSString* companyName = [me valueForProperty:kABOrganizationProperty];

  profile->SetRawInfo(NAME_FIRST, base::SysNSStringToUTF16(firstName));
  profile->SetRawInfo(NAME_MIDDLE, base::SysNSStringToUTF16(middleName));
  profile->SetRawInfo(NAME_LAST, base::SysNSStringToUTF16(lastName));
  if ([addressLabelRaw isEqualToString:kABAddressWorkLabel])
    profile->SetRawInfo(COMPANY_NAME, base::SysNSStringToUTF16(companyName));
}

// Addresss information from the Address Book may span multiple lines.
// If it does then we represent the address with two lines in the profile.  The
// second line we join with commas.
// For example:  "c/o John Doe\n1122 Other Avenue\nApt #7" translates to
// line 1: "c/o John Doe", line 2: "1122 Other Avenue, Apt #7".
void AuxiliaryProfilesImpl::GetAddressBookAddress(const std::string& app_locale,
                                                  NSDictionary* address,
                                                  AutofillProfile* profile) {
  if (NSString* addressField = [address objectForKey:kABAddressStreetKey]) {
    // If there are newlines in the address, split into two lines.
    if ([addressField rangeOfCharacterFromSet:
            [NSCharacterSet newlineCharacterSet]].location != NSNotFound) {
      NSArray* chunks = [addressField componentsSeparatedByCharactersInSet:
          [NSCharacterSet newlineCharacterSet]];
      DCHECK([chunks count] > 1);

      NSString* separator =
          l10n_util::GetNSString(IDS_AUTOFILL_ADDRESS_LINE_SEPARATOR);

      NSString* addressField1 = [chunks objectAtIndex:0];
      NSString* addressField2 =
          [[chunks subarrayWithRange:NSMakeRange(1, [chunks count] - 1)]
              componentsJoinedByString:separator];
      profile->SetRawInfo(ADDRESS_HOME_LINE1,
                          base::SysNSStringToUTF16(addressField1));
      profile->SetRawInfo(ADDRESS_HOME_LINE2,
                          base::SysNSStringToUTF16(addressField2));
    } else {
      profile->SetRawInfo(ADDRESS_HOME_LINE1,
                          base::SysNSStringToUTF16(addressField));
    }
  }

  if (NSString* city = [address objectForKey:kABAddressCityKey])
    profile->SetRawInfo(ADDRESS_HOME_CITY, base::SysNSStringToUTF16(city));

  if (NSString* state = [address objectForKey:kABAddressStateKey])
    profile->SetRawInfo(ADDRESS_HOME_STATE, base::SysNSStringToUTF16(state));

  if (NSString* zip = [address objectForKey:kABAddressZIPKey])
    profile->SetRawInfo(ADDRESS_HOME_ZIP, base::SysNSStringToUTF16(zip));

  if (NSString* country = [address objectForKey:kABAddressCountryKey]) {
    profile->SetInfo(AutofillType(ADDRESS_HOME_COUNTRY),
                     base::SysNSStringToUTF16(country),
                     app_locale);
  }
}

// Fills in email address matching current address label.  Note that there may
// be multiple matching email addresses for a given label.  We take the
// first we find (topmost) as preferred.
void AuxiliaryProfilesImpl::GetAddressBookEmail(
    ABPerson* me,
    NSString* addressLabelRaw,
    AutofillProfile* profile) {
  ABMultiValue* emailAddresses = [me valueForProperty:kABEmailProperty];
  NSString* emailAddress = nil;
  for (NSUInteger j = 0, emailCount = [emailAddresses count];
       j < emailCount; j++) {
    NSString* emailAddressLabelRaw = [emailAddresses labelAtIndex:j];
    if ([emailAddressLabelRaw isEqualToString:addressLabelRaw]) {
      emailAddress = [emailAddresses valueAtIndex:j];
      break;
    }
  }
  profile->SetRawInfo(EMAIL_ADDRESS, base::SysNSStringToUTF16(emailAddress));
}

// Fills in telephone numbers.  Each of these are special cases.
// We match two cases: home/tel, work/tel.
// Note, we traverse in reverse order so that top values in address book
// take priority.
void AuxiliaryProfilesImpl::GetAddressBookPhoneNumbers(
    ABPerson* me,
    NSString* addressLabelRaw,
    AutofillProfile* profile) {
  ABMultiValue* phoneNumbers = [me valueForProperty:kABPhoneProperty];
  for (NSUInteger k = 0, phoneCount = [phoneNumbers count];
       k < phoneCount; k++) {
    NSUInteger reverseK = phoneCount - k - 1;
    NSString* phoneLabelRaw = [phoneNumbers labelAtIndex:reverseK];
    if ([addressLabelRaw isEqualToString:kABAddressHomeLabel] &&
        [phoneLabelRaw isEqualToString:kABPhoneHomeLabel]) {
      base::string16 homePhone = base::SysNSStringToUTF16(
          [phoneNumbers valueAtIndex:reverseK]);
      profile->SetRawInfo(PHONE_HOME_WHOLE_NUMBER, homePhone);
    } else if ([addressLabelRaw isEqualToString:kABAddressWorkLabel] &&
               [phoneLabelRaw isEqualToString:kABPhoneWorkLabel]) {
      base::string16 workPhone = base::SysNSStringToUTF16(
          [phoneNumbers valueAtIndex:reverseK]);
      profile->SetRawInfo(PHONE_HOME_WHOLE_NUMBER, workPhone);
    } else if ([phoneLabelRaw isEqualToString:kABPhoneMobileLabel] ||
               [phoneLabelRaw isEqualToString:kABPhoneMainLabel]) {
      base::string16 phone = base::SysNSStringToUTF16(
          [phoneNumbers valueAtIndex:reverseK]);
      profile->SetRawInfo(PHONE_HOME_WHOLE_NUMBER, phone);
    }
  }
}

}  // namespace

// Populate |auxiliary_profiles_| with the Address Book data.
void PersonalDataManager::LoadAuxiliaryProfiles(bool record_metrics) const {
  AuxiliaryProfilesImpl impl(&auxiliary_profiles_);
  impl.GetAddressBookMeCard(app_locale_, pref_service_, record_metrics);
}

bool PersonalDataManager::AccessAddressBook() {
  // The user is attempting to give Chrome access to the user's Address Book.
  // This implicitly acknowledges that the user wants to use auxiliary
  // profiles.
  pref_service_->SetBoolean(prefs::kAutofillUseMacAddressBook, true);

  // Request permissions.
  GetAddressBook(pref_service_);
  return true;
}

bool PersonalDataManager::ShouldShowAccessAddressBookSuggestion(
    AutofillType type) {
  // Don't show the access Address Book prompt if the user has built up any
  // Autofill state.
  if (!web_profiles_.empty())
    return false;

  if (!enabled_pref_->GetValue())
    return false;

  if (HasQueriedMacAddressBook(pref_service_))
    return false;

  if (AccessAddressBookPromptCount() >= kMaxTimesToShowMacAddressBook)
    return false;

  switch (type.group()) {
    case ADDRESS_BILLING:
    case ADDRESS_HOME:
    case EMAIL:
    case NAME:
    case NAME_BILLING:
    case PHONE_BILLING:
    case PHONE_HOME:
      return true;
    case NO_GROUP:
    case COMPANY:
    case CREDIT_CARD:
    case PASSWORD_FIELD:
    case TRANSACTION:
      return false;
  }

  return false;
}

void PersonalDataManager::ShowedAccessAddressBookPrompt() {
  pref_service_->SetInteger(prefs::kAutofillMacAddressBookShowedCount,
                            AccessAddressBookPromptCount() + 1);
}

int PersonalDataManager::AccessAddressBookPromptCount() {
  return pref_service_->GetInteger(prefs::kAutofillMacAddressBookShowedCount);
}

void PersonalDataManager::BinaryChanging() {
  g_binary_changed = true;
}

}  // namespace autofill
