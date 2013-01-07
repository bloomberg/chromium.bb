// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_WALLET_WALLET_ITEMS_H_
#define CHROME_BROWSER_AUTOFILL_WALLET_WALLET_ITEMS_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "chrome/browser/autofill/wallet/required_action.h"
#include "chrome/browser/autofill/wallet/wallet_address.h"

namespace base {
class DictionaryValue;
}

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
      BILLING_INCOMPLETE,
      DECLINED,
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

    const std::string& descriptive_name() const { return descriptive_name_; }
    const Type& type() const { return type_; }
    const std::vector<std::string>& supported_currencies() const {
      return supported_currencies_;
    }
    const std::string& last_four_digits() const { return last_four_digits_; }
    int expiration_month() const { return expiration_month_; }
    int expiration_year() const { return expiration_year_; }
    const std::string& brand() const { return brand_; }
    const Address& address() const { return *address_; }
    const Status& status() const { return status_; }
    const std::string& object_id() const { return object_id_; }

   private:
    friend class WalletItemsTest;
    FRIEND_TEST_ALL_PREFIXES(WalletItemsTest, CreateMaskedInstrument);
    FRIEND_TEST_ALL_PREFIXES(WalletItemsTest, CreateWalletItems);
    MaskedInstrument(const std::string& descriptve_name,
                     const Type& type,
                     const std::vector<std::string>& supported_currencies,
                     const std::string& last_four_digits,
                     int expiration_month,
                     int expiration_year,
                     const std::string& brand,
                     scoped_ptr<Address> address,
                     const Status& status,
                     const std::string& object_id);
    std::string descriptive_name_;
    Type type_;
    std::vector<std::string> supported_currencies_;
    std::string last_four_digits_;
    int expiration_month_;
    int expiration_year_;
    std::string brand_;
    scoped_ptr<Address> address_;
    Status status_;
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

    bool operator==(const LegalDocument& other) const;
    bool operator!=(const LegalDocument& other) const;

    const std::string& document_id() const { return document_id_; }
    const std::string& display_name() const { return display_name_; }
    const std::string& document_body() const { return document_body_; }

   private:
    friend class WalletItemsTest;
    FRIEND_TEST_ALL_PREFIXES(WalletItemsTest, CreateLegalDocument);
    FRIEND_TEST_ALL_PREFIXES(WalletItemsTest, CreateWalletItems);
    LegalDocument(const std::string& document_id,
                  const std::string& display_name,
                  const std::string& document_body);
    std::string document_id_;
    std::string display_name_;
    std::string document_body_;
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
  const std::string& default_address_id() const {
    return default_address_id_;
  }
  const std::vector<LegalDocument*>& legal_documents() const {
    return legal_documents_.get();
  }

 private:
  friend class WalletItemsTest;
  FRIEND_TEST_ALL_PREFIXES(WalletItemsTest, CreateWalletItems);
  FRIEND_TEST_ALL_PREFIXES(WalletItemsTest,
                           CreateWalletItemsWithRequiredActions);
  WalletItems(const std::vector<RequiredAction>& required_actions,
              const std::string& google_transaction_id,
              const std::string& default_instrument_id,
              const std::string& default_address_id);
  // Actions that must be completed by the user before a FullWallet can be
  // issued to them by the Online Wallet service.
  std::vector<RequiredAction> required_actions_;
  std::string google_transaction_id_;
  std::string default_instrument_id_;
  std::string default_address_id_;
  ScopedVector<MaskedInstrument> instruments_;
  ScopedVector<Address> addresses_;
  ScopedVector<LegalDocument> legal_documents_;
  DISALLOW_COPY_AND_ASSIGN(WalletItems);
};

}  // namespace wallet

#endif  // CHROME_BROWSER_AUTOFILL_WALLET_WALLET_ITEMS_H_

