// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;
import org.chromium.base.ThreadUtils;

import java.util.ArrayList;
import java.util.List;

/**
 * Android wrapper of the PersonalDataManager which provides access from the Java
 * layer.
 *
 * Only usable from the UI thread as it's primary purpose is for supporting the Android
 * preferences UI.
 *
 * See chrome/browser/autofill/personal_data_manager.h for more details.
 */
@JNINamespace("autofill")
public class PersonalDataManager {

    /**
     * Observer of PersonalDataManager events.
     */
    public interface PersonalDataManagerObserver {
        /**
         * Called when the data is changed.
         */
        public abstract void onPersonalDataChanged();
    }

    /**
     * Autofill address information.
     */
    public static class AutofillProfile {
        private String mGUID;
        private String mOrigin;
        private String mFullName;
        private String mCompanyName;
        private String mStreetAddress;
        private String mRegion;
        private String mLocality;
        private String mDependentLocality;
        private String mPostalCode;
        private String mSortingCode;
        private String mCountry;
        private String mPhoneNumber;
        private String mEmailAddress;
        private String mLabel;
        private String mLanguageCode;

        @CalledByNative("AutofillProfile")
        public static AutofillProfile create(String guid, String origin,
                String fullName, String companyName,
                String streetAddress, String region, String locality, String dependentLocality,
                String postalCode, String sortingCode,
                String country, String phoneNumber, String emailAddress, String languageCode) {
            return new AutofillProfile(guid, origin, fullName, companyName,
                    streetAddress,
                    region, locality, dependentLocality,
                    postalCode, sortingCode, country, phoneNumber, emailAddress, languageCode);
        }

        public AutofillProfile(String guid, String origin, String fullName, String companyName,
                String streetAddress,
                String region,
                String locality, String dependentLocality,
                String postalCode, String sortingCode,
                String country, String phoneNumber, String emailAddress, String languageCode) {
            mGUID = guid;
            mOrigin = origin;
            mFullName = fullName;
            mCompanyName = companyName;
            mStreetAddress = streetAddress;
            mRegion = region;
            mLocality = locality;
            mDependentLocality = dependentLocality;
            mPostalCode = postalCode;
            mSortingCode = sortingCode;
            mCountry = country;
            mPhoneNumber = phoneNumber;
            mEmailAddress = emailAddress;
            mLanguageCode = languageCode;
        }

        @CalledByNative("AutofillProfile")
        public String getGUID() {
            return mGUID;
        }

        @CalledByNative("AutofillProfile")
        public String getOrigin() {
            return mOrigin;
        }

        @CalledByNative("AutofillProfile")
        public String getFullName() {
            return mFullName;
        }

        @CalledByNative("AutofillProfile")
        public String getCompanyName() {
            return mCompanyName;
        }

        @CalledByNative("AutofillProfile")
        public String getStreetAddress() {
            return mStreetAddress;
        }

        @CalledByNative("AutofillProfile")
        public String getRegion() {
            return mRegion;
        }

        @CalledByNative("AutofillProfile")
        public String getLocality() {
            return mLocality;
        }

        @CalledByNative("AutofillProfile")
        public String getDependentLocality() {
            return mDependentLocality;
        }

        public String getLabel() {
            return mLabel;
        }

        @CalledByNative("AutofillProfile")
        public String getPostalCode() {
            return mPostalCode;
        }

        @CalledByNative("AutofillProfile")
        public String getSortingCode() {
            return mSortingCode;
        }

        @CalledByNative("AutofillProfile")
        public String getCountry() {
            return mCountry;
        }

        public String getCountryCode() {
            return nativeToCountryCode(mCountry);
        }

        @CalledByNative("AutofillProfile")
        public String getPhoneNumber() {
            return mPhoneNumber;
        }

        @CalledByNative("AutofillProfile")
        public String getEmailAddress() {
            return mEmailAddress;
        }

        @CalledByNative("AutofillProfile")
        public String getLanguageCode() {
            return mLanguageCode;
        }

        public void setGUID(String guid) {
            mGUID = guid;
        }

        public void setLabel(String label) {
            mLabel = label;
        }

        public void setOrigin(String origin) {
            mOrigin = origin;
        }

        public void setFullName(String fullName) {
            mFullName = fullName;
        }

        public void setCompanyName(String companyName) {
            mCompanyName = companyName;
        }

        public void setStreetAddress(String streetAddress) {
            mStreetAddress = streetAddress;
        }

        public void setRegion(String region) {
            mRegion = region;
        }

        public void setLocality(String locality) {
            mLocality = locality;
        }

        public void setDependentLocality(String dependentLocality) {
            mDependentLocality = dependentLocality;
        }

        public void setPostalCode(String postalCode) {
            mPostalCode = postalCode;
        }

        public void setSortingCode(String sortingCode) {
            mSortingCode = sortingCode;
        }

        public void setCountry(String country) {
            mCountry = country;
        }

        public void setPhoneNumber(String phoneNumber) {
            mPhoneNumber = phoneNumber;
        }

        public void setEmailAddress(String emailAddress) {
            mEmailAddress = emailAddress;
        }

        public void setLanguageCode(String languageCode) {
            mLanguageCode = languageCode;
        }
    }

    /**
     * Autofill credit card information.
     */
    public static class CreditCard {
        // Note that while some of these fields are numbers, they're predominantly read,
        // marshaled and compared as strings. To save conversions, we use strings everywhere.
        private String mGUID;
        private String mOrigin;
        private String mName;
        private String mNumber;
        private String mObfuscatedNumber;
        private String mMonth;
        private String mYear;

        @CalledByNative("CreditCard")
        public static CreditCard create(String guid, String origin, String name, String number,
                String obfuscatedNumber, String month, String year) {
            return new CreditCard(guid, origin, name, number, obfuscatedNumber, month, year);
        }

        public CreditCard(String guid, String origin, String name, String number,
                String obfuscatedNumber, String month, String year) {
            mGUID = guid;
            mOrigin = origin;
            mName = name;
            mNumber = number;
            mObfuscatedNumber = obfuscatedNumber;
            mMonth = month;
            mYear = year;
        }

        @CalledByNative("CreditCard")
        public String getGUID() {
            return mGUID;
        }

        @CalledByNative("CreditCard")
        public String getOrigin() {
            return mOrigin;
        }

        @CalledByNative("CreditCard")
        public String getName() {
            return mName;
        }

        @CalledByNative("CreditCard")
        public String getNumber() {
            return mNumber;
        }

        public String getObfuscatedNumber() {
            return mObfuscatedNumber;
        }

        @CalledByNative("CreditCard")
        public String getMonth() {
            return mMonth;
        }

        @CalledByNative("CreditCard")
        public String getYear() {
            return mYear;
        }

        public void setGUID(String guid) {
            mGUID = guid;
        }

        public void setOrigin(String origin) {
            mOrigin = origin;
        }

        public void setName(String name) {
            mName = name;
        }

        public void setNumber(String number) {
            mNumber = number;
        }

        public void setObfuscatedNumber(String obfuscatedNumber) {
            mObfuscatedNumber = obfuscatedNumber;
        }

        public void setMonth(String month) {
            mMonth = month;
        }

        public void setYear(String year) {
            mYear = year;
        }
    }

    private static PersonalDataManager sManager;

    public static PersonalDataManager getInstance() {
        ThreadUtils.assertOnUiThread();
        if (sManager == null) {
            sManager = new PersonalDataManager();
        }
        return sManager;
    }

    private final long mPersonalDataManagerAndroid;
    private final List<PersonalDataManagerObserver> mDataObservers =
            new ArrayList<PersonalDataManagerObserver>();

    private PersonalDataManager() {
        // Note that this technically leaks the native object, however, PersonalDataManager
        // is a singleton that lives forever and there's no clean shutdown of Chrome on Android
        mPersonalDataManagerAndroid = nativeInit();
    }

    /**
     * Called from native when template URL service is done loading.
     */
    @CalledByNative
    private void personalDataChanged() {
        ThreadUtils.assertOnUiThread();
        for (PersonalDataManagerObserver observer : mDataObservers) {
            observer.onPersonalDataChanged();
        }
    }

    /**
     * Registers a PersonalDataManagerObserver on the native side.
     */
    public void registerDataObserver(PersonalDataManagerObserver observer) {
        ThreadUtils.assertOnUiThread();
        assert !mDataObservers.contains(observer);
        mDataObservers.add(observer);
    }

    /**
     * Unregisters the provided observer.
     */
    public void unregisterDataObserver(PersonalDataManagerObserver observer) {
        ThreadUtils.assertOnUiThread();
        assert (mDataObservers.size() > 0);
        assert (mDataObservers.contains(observer));
        mDataObservers.remove(observer);
    }

    public List<AutofillProfile> getProfiles() {
        ThreadUtils.assertOnUiThread();

        String[] profileLabels = nativeGetProfileLabels(mPersonalDataManagerAndroid);

        int profileCount = nativeGetProfileCount(mPersonalDataManagerAndroid);
        List<AutofillProfile> profiles = new ArrayList<AutofillProfile>(profileCount);
        for (int i = 0; i < profileCount; i++) {
            AutofillProfile profile = nativeGetProfileByIndex(mPersonalDataManagerAndroid, i);
            profile.setLabel(profileLabels[i]);
            profiles.add(profile);
        }

        return profiles;
    }

    public AutofillProfile getProfile(String guid) {
        ThreadUtils.assertOnUiThread();
        return nativeGetProfileByGUID(mPersonalDataManagerAndroid, guid);
    }

    public void deleteProfile(String guid) {
        ThreadUtils.assertOnUiThread();
        nativeRemoveByGUID(mPersonalDataManagerAndroid, guid);
    }

    public String setProfile(AutofillProfile profile) {
        ThreadUtils.assertOnUiThread();
        return nativeSetProfile(mPersonalDataManagerAndroid, profile);
    }

    public List<CreditCard> getCreditCards() {
        ThreadUtils.assertOnUiThread();
        int count = nativeGetCreditCardCount(mPersonalDataManagerAndroid);
        List<CreditCard> cards = new ArrayList<CreditCard>(count);
        for (int i = 0; i < count; i++) {
            cards.add(nativeGetCreditCardByIndex(mPersonalDataManagerAndroid, i));
        }
        return cards;
    }

    public CreditCard getCreditCard(String guid) {
        ThreadUtils.assertOnUiThread();
        return nativeGetCreditCardByGUID(mPersonalDataManagerAndroid, guid);
    }

    public String setCreditCard(CreditCard card) {
        ThreadUtils.assertOnUiThread();
        return nativeSetCreditCard(mPersonalDataManagerAndroid, card);
    }

    public void deleteCreditCard(String guid) {
        ThreadUtils.assertOnUiThread();
        nativeRemoveByGUID(mPersonalDataManagerAndroid, guid);
    }

    /**
     * @return Whether the Autofill feature is enabled.
     */
    public static boolean isAutofillEnabled() {
        return nativeIsAutofillEnabled();
    }

    /**
     * Enables or disables the Autofill feature.
     * @param enable True to disable Autofill, false otherwise.
     */
    public static void setAutofillEnabled(boolean enable) {
        nativeSetAutofillEnabled(enable);
    }

    /**
     * @return Whether the Autofill feature is managed.
     */
    public static boolean isAutofillManaged() {
        return nativeIsAutofillManaged();
    }

    private native long nativeInit();
    private native int nativeGetProfileCount(long nativePersonalDataManagerAndroid);
    private native String[] nativeGetProfileLabels(long nativePersonalDataManagerAndroid);
    private native AutofillProfile nativeGetProfileByIndex(long nativePersonalDataManagerAndroid,
            int index);
    private native AutofillProfile nativeGetProfileByGUID(long nativePersonalDataManagerAndroid,
            String guid);
    private native String nativeSetProfile(long nativePersonalDataManagerAndroid,
            AutofillProfile profile);
    private native int nativeGetCreditCardCount(long nativePersonalDataManagerAndroid);
    private native CreditCard nativeGetCreditCardByIndex(long nativePersonalDataManagerAndroid,
            int index);
    private native CreditCard nativeGetCreditCardByGUID(long nativePersonalDataManagerAndroid,
            String guid);
    private native String nativeSetCreditCard(long nativePersonalDataManagerAndroid,
            CreditCard card);
    private native void nativeRemoveByGUID(long nativePersonalDataManagerAndroid, String guid);
    private static native boolean nativeIsAutofillEnabled();
    private static native void nativeSetAutofillEnabled(boolean enable);
    private static native boolean nativeIsAutofillManaged();
    private static native String nativeToCountryCode(String countryName);
}
