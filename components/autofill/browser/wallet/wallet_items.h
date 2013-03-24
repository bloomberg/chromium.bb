// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_BROWSER_WALLET_WALLET_ITEMS_H_
#define COMPONENTS_AUTOFILL_BROWSER_WALLET_WALLET_ITEMS_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/string16.h"
#include "components/autofill/browser/wallet/required_action.h"
#include "components/autofill/browser/wallet/wallet_address.h"

class GURL;

namespace base {
class DictionaryValue;
}

namespace gfx {
class Image;
}

namespace autofill {

FORWARD_DECLARE_TEST(WalletInstrumentWrapperTest, GetInfoCreditCardExpMonth);

namespace wallet {

class WalletItemsTest;

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
    string16 DisplayName() const;
    string16 DisplayNameDetail() const;

    // Gets info that corresponds with |type|.
    string16 GetInfo(AutofillFieldType type) const;

    const string16& descriptive_name() const { return descriptive_name_; }
    const Type& type() const { return type_; }
    const std::vector<string16>& supported_currencies() const {
      return supported_currencies_;
    }
    const string16& last_four_digits() const { return last_four_digits_; }
    int expiration_month() const { return expiration_month_; }
    int expiration_year() const { return expiration_year_; }
    const Address& address() const { return *address_; }
    const Status& status() const { return status_; }
    const std::string& object_id() const { return object_id_; }

   private:
    friend class WalletItemsTest;
    friend scoped_ptr<MaskedInstrument> GetTestMaskedInstrument();
    FRIEND_TEST_ALL_PREFIXES(::autofill::WalletInstrumentWrapperTest,
                             GetInfoCreditCardExpMonth);
    FRIEND_TEST_ALL_PREFIXES(WalletItemsTest, CreateMaskedInstrument);
    FRIEND_TEST_ALL_PREFIXES(WalletItemsTest, CreateWalletItems);

    MaskedInstrument(const string16& descriptve_name,
                     const Type& type,
                     const std::vector<string16>& supported_currencies,
                     const string16& last_four_digits,
                     int expiration_month,
                     int expiration_year,
                     scoped_ptr<Address> address,
                     const Status& status,
                     const std::string& object_id);

    // A user-provided description of the instrument. For example, "Google Visa
    // Card".
    string16 descriptive_name_;

    // The payment network of the instrument. For example, Visa.
    Type type_;

    // |supported_currencies_| are ISO 4217 currency codes, e.g. USD.
    std::vector<string16> supported_currencies_;

    // The last four digits of the primary account number of the instrument.
    string16 last_four_digits_;

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

    // Returns null if input is invalid or a valid legal document. Caller owns
    // returned pointer.
    static scoped_ptr<LegalDocument>
        CreateLegalDocument(const base::DictionaryValue& dictionary);

    // Get the url where this legal document is hosted.
    GURL GetUrl();

    bool operator==(const LegalDocument& other) const;
    bool operator!=(const LegalDocument& other) const;

    const std::string& document_id() const { return document_id_; }
    const std::string& display_name() const { return display_name_; }

   private:
    friend class WalletItemsTest;
    FRIEND_TEST_ALL_PREFIXES(WalletItemsTest, CreateLegalDocument);
    FRIEND_TEST_ALL_PREFIXES(WalletItemsTest, CreateWalletItems);
    FRIEND_TEST_ALL_PREFIXES(WalletItemsTest, LegalDocumentGetUrl);
    LegalDocument(const std::string& document_id,
                  const std::string& display_name);

    // Externalized Online Wallet id for the document.
    std::string document_id_;

    // User displayable name for the document.
    std::string display_name_;
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

  void AddInstrument(scoped_ptr<MaskedInstrument> instrument) {
    DCHECK(instrument.get());
    instruments_.push_back(instrument.release());
  }
  void AddAddress(scoped_ptr<Address> address) {
    DCHECK(address.get());
    addresses_.push_back(address.release());
  }
  void AddLegalDocument(scoped_ptr<LegalDocument> legal_document) {
    DCHECK(legal_document.get());
    legal_documents_.push_back(legal_document.release());
  }

  // Whether or not |action| is in |required_actions_|.
  bool HasRequiredAction(RequiredAction action) const;

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
  const std::string& obfuscated_gaia_id() const { return obfuscated_gaia_id_; }
  const std::vector<LegalDocument*>& legal_documents() const {
    return legal_documents_.get();
  }

 private:
  friend class WalletItemsTest;
  friend scoped_ptr<WalletItems> GetTestWalletItems();
  FRIEND_TEST_ALL_PREFIXES(WalletItemsTest, CreateWalletItems);
  FRIEND_TEST_ALL_PREFIXES(WalletItemsTest,
                           CreateWalletItemsWithRequiredActions);

  WalletItems(const std::vector<RequiredAction>& required_actions,
              const std::string& google_transaction_id,
              const std::string& default_instrument_id,
              const std::string& default_address_id,
              const std::string& obfuscated_gaia_id);

  // Actions that must be completed by the user before a FullWallet can be
  // issued to them by the Online Wallet service.
  std::vector<RequiredAction> required_actions_;

  // The id for this transaction issued by Google.
  std::string google_transaction_id_;

  // The id of the user's default instrument.
  std::string default_instrument_id_;

  // The id of the user's default address.
  std::string default_address_id_;

  // The externalized Gaia id of the user.
  std::string obfuscated_gaia_id_;

  // The user's backing instruments.
  ScopedVector<MaskedInstrument> instruments_;

  // The user's shipping addresses.
  ScopedVector<Address> addresses_;

  // Legal documents the user must accept before using Online Wallet.
  ScopedVector<LegalDocument> legal_documents_;

  DISALLOW_COPY_AND_ASSIGN(WalletItems);
};

}  // namespace wallet
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_BROWSER_WALLET_WALLET_ITEMS_H_
