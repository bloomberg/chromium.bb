// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/personal_data_manager.h"

#include <math.h>

#import <AddressBook/AddressBook.h>

#include "base/format_macros.h"
#include "base/guid.h"
#include "base/logging.h"
#import "base/mac/scoped_nsexception_enabler.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/stringprintf.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/autofill/autofill_country.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/autofill/phone_number.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util_mac.h"

namespace {

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
  void GetAddressBookMeCard();

 private:
  void GetAddressBookNames(ABPerson* me,
                           NSString* addressLabelRaw,
                           AutofillProfile* profile);
  void GetAddressBookAddress(NSDictionary* address, AutofillProfile* profile);
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
void AuxiliaryProfilesImpl::GetAddressBookMeCard() {
  profiles_.clear();

  // +[ABAddressBook sharedAddressBook] throws an exception internally in
  // circumstances that aren't clear. The exceptions are only observed in crash
  // reports, so it is unknown whether they would be caught by AppKit and nil
  // returned, or if they would take down the app. In either case, avoid
  // crashing. http://crbug.com/129022
  ABAddressBook* addressBook = base::mac::PerformSelectorIgnoringExceptions(
      NSClassFromString(@"ABAddressBook"), @selector(sharedAddressBook));
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

    scoped_ptr<AutofillProfile> profile(new AutofillProfile(guid));
    DCHECK(base::IsValidGUID(profile->guid()));

    // Fill in name and company information.
    GetAddressBookNames(me, addressLabelRaw, profile.get());

    // Fill in address information.
    GetAddressBookAddress(address, profile.get());

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
void AuxiliaryProfilesImpl::GetAddressBookAddress(NSDictionary* address,
                                                  AutofillProfile* profile) {
  if (NSString* addressField = [address objectForKey:kABAddressStreetKey]) {
    // If there are newlines in the address, split into two lines.
    if ([addressField rangeOfCharacterFromSet:
            [NSCharacterSet newlineCharacterSet]].location != NSNotFound) {
      NSArray* chunks = [addressField componentsSeparatedByCharactersInSet:
          [NSCharacterSet newlineCharacterSet]];
      DCHECK([chunks count] > 1);

      NSString* separator = l10n_util::GetNSString(
            IDS_AUTOFILL_MAC_ADDRESS_LINE_SEPARATOR);

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
    profile->SetInfo(ADDRESS_HOME_COUNTRY,
                     base::SysNSStringToUTF16(country),
                     AutofillCountry::ApplicationLocale());
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
      string16 homePhone = base::SysNSStringToUTF16(
          [phoneNumbers valueAtIndex:reverseK]);
      profile->SetRawInfo(PHONE_HOME_WHOLE_NUMBER, homePhone);
    } else if ([addressLabelRaw isEqualToString:kABAddressWorkLabel] &&
               [phoneLabelRaw isEqualToString:kABPhoneWorkLabel]) {
      string16 workPhone = base::SysNSStringToUTF16(
          [phoneNumbers valueAtIndex:reverseK]);
      profile->SetRawInfo(PHONE_HOME_WHOLE_NUMBER, workPhone);
    } else if ([phoneLabelRaw isEqualToString:kABPhoneMobileLabel] ||
               [phoneLabelRaw isEqualToString:kABPhoneMainLabel]) {
      string16 phone = base::SysNSStringToUTF16(
          [phoneNumbers valueAtIndex:reverseK]);
      profile->SetRawInfo(PHONE_HOME_WHOLE_NUMBER, phone);
    }
  }
}

}  // namespace

// Populate |auxiliary_profiles_| with the Address Book data.
void PersonalDataManager::LoadAuxiliaryProfiles() {
  AuxiliaryProfilesImpl impl(&auxiliary_profiles_);
  impl.GetAddressBookMeCard();
}
