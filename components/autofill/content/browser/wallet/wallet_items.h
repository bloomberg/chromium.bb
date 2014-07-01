// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_BROWSER_WALLET_WALLET_ITEMS_H_
#define COMPONENTS_AUTOFILL_CONTENT_BROWSER_WALLET_WALLET_ITEMS_H_

#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/strings/string16.h"
#include "components/autofill/content/browser/wallet/required_action.h"
#include "components/autofill/content/browser/wallet/wallet_address.h"
#include "url/gurl.h"

namespace base {
class DictionaryValue;
}

namespace gfx {
class Image;
}

namespace autofill {

class AutofillType;

FORWARD_DECLARE_TEST(WalletInstrumentWrapperTest, GetInfoCreditCardExpMonth);
FORWARD_DECLARE_TEST(WalletInstrumentWrapperTest,
                     GetDisplayTextEmptyWhenExpired);

namespace wallet {

class GaiaAccount;
class WalletItemsTest;

enum AmexPermission {
  AMEX_ALLOWED,
  AMEX_DISALLOWED,
};

// WalletItems is a collection of cards and addresses that a user picks from to
// construct a full wallet. However, it also provides a transaction ID which
// must be used throughout all API calls being made using this data.
// Additionally, user actions may be required before a purchase can be completed
// using Online Wallet and those actions are present in the object as well.
class WalletItems {
 public:
  // Container for all information about a credit card except for it's card
  // verfication number (CVN) and it's complete primary account number (PAN).
  class MaskedInstrument {
   public:
    enum Type {
      AMEX,
      DISCOVER,
      MAESTRO,
      MASTER_CARD,
      SOLO,
      SWITCH,
      UNKNOWN,  // Catch all type.
      VISA,
    };
    enum Status {
      AMEX_NOT_SUPPORTED,
      BILLING_INCOMPLETE,
      DECLINED,
      DISABLED_FOR_THIS_MERCHANT,  // Deprecated.
      EXPIRED,
      INAPPLICABLE,  // Catch all status.
      PENDING,
      UNSUPPORTED_COUNTRY,
      VALID,
    };

    ~MaskedInstrument();

    // Returns an empty scoped_ptr if input is invalid or a valid masked
    // instrument.
    static scoped_ptr<MaskedInstrument>
        CreateMaskedInstrument(const base::DictionaryValue& dictionary);

    bool operator==(const MaskedInstrument& other) const;
    bool operator!=(const MaskedInstrument& other) const;

    // Gets an image to display for this instrument.
    const gfx::Image& CardIcon() const;

    // Returns a pair of strings that summarizes this CC,
    // suitable for display to the user.
    base::string16 DisplayName() const;
    base::string16 DisplayNameDetail() const;

    // Gets info that corresponds with |type|.
    base::string16 GetInfo(const AutofillType& type,
                           const std::string& app_locale) const;

    // Returns the display type of the and last four digits (e.g. Visa - 4444).
    base::string16 TypeAndLastFourDigits() const;

    const base::string16& descriptive_name() const { return descriptive_name_; }
    const Type& type() const { return type_; }
    const base::string16& last_four_digits() const { return last_four_digits_; }
    int expiration_month() const { return expiration_month_; }
    int expiration_year() const { return expiration_year_; }
    const Address& address() const { return *address_; }
    const Status& status() const { return status_; }
    const std::string& object_id() const { return object_id_; }

   private:
    friend class WalletItemsTest;
    friend scoped_ptr<MaskedInstrument> GetTestMaskedInstrumentWithDetails(
        const std::string& id,
        scoped_ptr<Address> address,
        Type type,
        Status status);
    FRIEND_TEST_ALL_PREFIXES(::autofill::WalletInstrumentWrapperTest,
                             GetInfoCreditCardExpMonth);
    FRIEND_TEST_ALL_PREFIXES(::autofill::WalletInstrumentWrapperTest,
                             GetDisplayTextEmptyWhenExpired);
    FRIEND_TEST_ALL_PREFIXES(WalletItemsTest, CreateMaskedInstrument);
    FRIEND_TEST_ALL_PREFIXES(WalletItemsTest, CreateWalletItems);

    MaskedInstrument(const base::string16& descriptive_name,
                     const Type& type,
                     const base::string16& last_four_digits,
                     int expiration_month,
                     int expiration_year,
                     scoped_ptr<Address> address,
                     const Status& status,
                     const std::string& object_id);

    // A user-provided description of the instrument. For example, "Google Visa
    // Card".
    base::string16 descriptive_name_;

    // The payment network of the instrument. For example, Visa.
    Type type_;

    // The last four digits of the primary account number of the instrument.
    base::string16 last_four_digits_;

    // |expiration month_| should be 1-12.
    int expiration_month_;

    // |expiration_year_| should be a 4-digit year.
    int expiration_year_;

    // The billing address for the instrument.
    scoped_ptr<Address> address_;

    // The current status of the instrument. For example, expired or declined.
    Status status_;

    // Externalized Online Wallet id for this instrument.
    std::string object_id_;

    DISALLOW_COPY_AND_ASSIGN(MaskedInstrument);
  };

  // Class representing a legal document that the user must accept before they
  // can use Online Wallet.
  class LegalDocument {
   public:
    ~LegalDocument();

    // Returns null if input is invalid or a valid legal document.
    static scoped_ptr<LegalDocument>
        CreateLegalDocument(const base::DictionaryValue& dictionary);

    // Returns a document for the privacy policy (acceptance of which is not
    // tracked by the server).
    static scoped_ptr<LegalDocument> CreatePrivacyPolicyDocument();

    bool operator==(const LegalDocument& other) const;
    bool operator!=(const LegalDocument& other) const;

    const std::string& id() { return id_; }
    const GURL& url() const { return url_; }
    const base::string16& display_name() const { return display_name_; }

   private:
    friend class WalletItemsTest;
    FRIEND_TEST_ALL_PREFIXES(WalletItemsTest, CreateLegalDocument);
    FRIEND_TEST_ALL_PREFIXES(WalletItemsTest, CreateWalletItems);
    FRIEND_TEST_ALL_PREFIXES(WalletItemsTest, LegalDocumentUrl);
    FRIEND_TEST_ALL_PREFIXES(WalletItemsTest, LegalDocumentEmptyId);
    LegalDocument(const std::string& id,
                  const base::string16& display_name);
    LegalDocument(const GURL& url,
                  const base::string16& display_name);

    // Externalized Online Wallet id for the document, or an empty string for
    // documents not tracked by the server (such as the privacy policy).
    std::string id_;
    // The human-visitable URL that displays the document.
    GURL url_;
    // User displayable name for the document.
    base::string16 display_name_;
    DISALLOW_COPY_AND_ASSIGN(LegalDocument);
  };

  ~WalletItems();

  // Returns null on invalid input, an empty wallet items with required
  // actions if any are present, and a populated wallet items otherwise. Caller
  // owns returned pointer.
  static scoped_ptr<WalletItems>
      CreateWalletItems(const base::DictionaryValue& dictionary);

  bool operator==(const WalletItems& other) const;
  bool operator!=(const WalletItems& other) const;

  void AddAccount(scoped_ptr<GaiaAccount> account);
  void AddInstrument(scoped_ptr<MaskedInstrument> instrument) {
    DCHECK(instrument);
    instruments_.push_back(instrument.release());
  }
  void AddAddress(scoped_ptr<Address> address) {
    DCHECK(address);
    addresses_.push_back(address.release());
  }
  void AddLegalDocument(scoped_ptr<LegalDocument> legal_document) {
    DCHECK(legal_document);
    legal_documents_.push_back(legal_document.release());
  }
  void AddAllowedShippingCountry(const std::string& country_code) {
    allowed_shipping_countries_.insert(country_code);
  }

  // Return the corresponding instrument for |id| or NULL if it doesn't exist.
  const WalletItems::MaskedInstrument* GetInstrumentById(
      const std::string& object_id) const;

  // Whether or not |action| is in |required_actions_|.
  bool HasRequiredAction(RequiredAction action) const;

  // Checks whether |card_number| is supported by Wallet for this merchant and
  // if not, fills in |message| with a user-visible explanation.
  bool SupportsCard(const base::string16& card_number,
                    base::string16* message) const;

  const std::vector<GaiaAccount*>& gaia_accounts() const {
    return gaia_accounts_.get();
  }
  const std::vector<RequiredAction>& required_actions() const {
    return required_actions_;
  }
  const std::string& google_transaction_id() const {
    return google_transaction_id_;
  }
  const std::vector<MaskedInstrument*>& instruments() const {
    return instruments_.get();
  }
  const std::string& default_instrument_id() const {
    return default_instrument_id_;
  }
  const std::vector<Address*>& addresses() const { return addresses_.get(); }
  const std::string& default_address_id() const { return default_address_id_; }
  // Returns the GAIA id of the active account, or an empty string if no account
  // is active.
  std::string ObfuscatedGaiaId() const;
  size_t active_account_index() const { return active_account_index_; }
  const std::vector<LegalDocument*>& legal_documents() const {
    return legal_documents_.get();
  }
  const std::set<std::string>& allowed_shipping_countries() const {
    return allowed_shipping_countries_;
  }

 private:
  friend class WalletItemsTest;
  friend scoped_ptr<WalletItems> GetTestWalletItemsWithDetails(
      const std::vector<RequiredAction>& required_actions,
      const std::string& default_instrument_id,
      const std::string& default_address_id,
      AmexPermission amex_permission);
  friend scoped_ptr<WalletItems> GetTestWalletItemsWithDefaultIds(
      RequiredAction action);
  FRIEND_TEST_ALL_PREFIXES(WalletItemsTest, CreateWalletItems);
  FRIEND_TEST_ALL_PREFIXES(WalletItemsTest,
                           CreateWalletItemsWithRequiredActions);

  WalletItems(const std::vector<RequiredAction>& required_actions,
              const std::string& google_transaction_id,
              const std::string& default_instrument_id,
              const std::string& default_address_id,
              AmexPermission amex_permission);

  // Actions that must be completed by the user before a FullWallet can be
  // issued to them by the Online Wallet service.
  std::vector<RequiredAction> required_actions_;

  // The id for this transaction issued by Google.
  std::string google_transaction_id_;

  // The id of the user's default instrument.
  std::string default_instrument_id_;

  // The id of the user's default address.
  std::string default_address_id_;

  // The index into |gaia_accounts_| of the account for which this object
  // holds data.
  size_t active_account_index_;

  // The complete set of logged in GAIA accounts.
  ScopedVector<GaiaAccount> gaia_accounts_;

  // The user's backing instruments.
  ScopedVector<MaskedInstrument> instruments_;

  // The user's shipping addresses.
  ScopedVector<Address> addresses_;

  // Legal documents the user must accept before using Online Wallet.
  ScopedVector<LegalDocument> legal_documents_;

  // Country codes for allowed Wallet shipping destinations.
  std::set<std::string> allowed_shipping_countries_;

  // Whether Google Wallet allows American Express card for this merchant.
  AmexPermission amex_permission_;

  DISALLOW_COPY_AND_ASSIGN(WalletItems);
};

}  // namespace wallet
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_BROWSER_WALLET_WALLET_ITEMS_H_
