// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_DATA_MODEL_WRAPPER_H_
#define CHROME_BROWSER_UI_AUTOFILL_DATA_MODEL_WRAPPER_H_

#include <vector>

#include "base/compiler_specific.h"
#include "base/strings/string16.h"
#include "chrome/browser/ui/autofill/autofill_dialog_types.h"
#include "components/autofill/content/browser/wallet/wallet_items.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"

namespace gfx {
class Image;
}

namespace i18n {
namespace addressinput {
struct AddressData;
}
}

namespace autofill {

class AutofillDataModel;
class AutofillProfile;
class AutofillType;
class CreditCard;
class FormStructure;

namespace wallet {
class Address;
class FullWallet;
}

// A glue class that allows uniform interactions with autocomplete data sources,
// regardless of their type. Implementations are intended to be lightweight and
// copyable, only holding weak references to their backing model.
class DataModelWrapper {
 public:
  virtual ~DataModelWrapper();

  // Fills in |inputs| with the data that this model contains (|inputs| is an
  // out-param).
  void FillInputs(DetailInputs* inputs);

  // Returns the data for a specific autocomplete type in a format for filling
  // into a web form.
  virtual base::string16 GetInfo(const AutofillType& type) const = 0;

  // Returns the data for a specified type in a format optimized for displaying
  // to the user.
  virtual base::string16 GetInfoForDisplay(const AutofillType& type) const;

  // Returns the icon, if any, that represents this model.
  virtual gfx::Image GetIcon();

  // Gets text to display to the user to summarize this data source. The
  // default implementation assumes this is an address. Both params are required
  // to be non-NULL and will be filled in with text that is vertically compact
  // (but may take up a lot of horizontal space) and horizontally compact (but
  // may take up a lot of vertical space) respectively. The return value will
  // be true and the outparams will be filled in only if the data represented is
  // complete and valid.
  virtual bool GetDisplayText(base::string16* vertically_compact,
                              base::string16* horizontally_compact);

  // Returns the BCP 47 language code that should be used for formatting the
  // data for display.
  virtual const std::string& GetLanguageCode() const = 0;

  // Fills in |form_structure| with the data that this model contains. |inputs|
  // and |comparator| are used to determine whether each field in the
  // FormStructure should be filled in or left alone. Returns whether any fields
  // in |form_structure| were found to be matching.
  bool FillFormStructure(
      const std::vector<ServerFieldType>& types,
      const FormStructure::InputFieldComparator& compare,
      FormStructure* form_structure) const;

 protected:
  DataModelWrapper();

 private:
  DISALLOW_COPY_AND_ASSIGN(DataModelWrapper);
};

// A DataModelWrapper for Autofill profiles.
class AutofillProfileWrapper : public DataModelWrapper {
 public:
  explicit AutofillProfileWrapper(const AutofillProfile* profile);
  AutofillProfileWrapper(const AutofillProfile* profile,
                         const AutofillType& variant_type,
                         size_t variant);
  virtual ~AutofillProfileWrapper();

  virtual base::string16 GetInfo(const AutofillType& type) const OVERRIDE;
  virtual base::string16 GetInfoForDisplay(const AutofillType& type) const
      OVERRIDE;
  virtual const std::string& GetLanguageCode() const OVERRIDE;

 protected:
  // Returns the variant that should be used when dealing with an element that
  // has the given |type|.
  size_t GetVariantForType(const AutofillType& type) const;

 private:
  const AutofillProfile* profile_;

  // The profile variant. |variant_| describes which variant of |variant_group_|
  // to use in the profile.
  FieldTypeGroup variant_group_;
  size_t variant_;

  DISALLOW_COPY_AND_ASSIGN(AutofillProfileWrapper);
};

// A DataModelWrapper specifically for shipping address profiles.
class AutofillShippingAddressWrapper : public AutofillProfileWrapper {
 public:
  explicit AutofillShippingAddressWrapper(const AutofillProfile* profile);
  virtual ~AutofillShippingAddressWrapper();

  virtual base::string16 GetInfo(const AutofillType& type) const OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(AutofillShippingAddressWrapper);
};

// A DataModelWrapper specifically for Autofill CreditCard data.
class AutofillCreditCardWrapper : public DataModelWrapper {
 public:
  explicit AutofillCreditCardWrapper(const CreditCard* card);
  virtual ~AutofillCreditCardWrapper();

  virtual base::string16 GetInfo(const AutofillType& type) const OVERRIDE;
  virtual gfx::Image GetIcon() OVERRIDE;
  virtual bool GetDisplayText(base::string16* vertically_compact,
                              base::string16* horizontally_compact) OVERRIDE;
  virtual const std::string& GetLanguageCode() const OVERRIDE;

 private:
  const CreditCard* card_;

  DISALLOW_COPY_AND_ASSIGN(AutofillCreditCardWrapper);
};

// A DataModelWrapper for Wallet addresses.
class WalletAddressWrapper : public DataModelWrapper {
 public:
  explicit WalletAddressWrapper(const wallet::Address* address);
  virtual ~WalletAddressWrapper();

  virtual base::string16 GetInfo(const AutofillType& type) const OVERRIDE;
  virtual base::string16 GetInfoForDisplay(const AutofillType& type) const
      OVERRIDE;
  virtual bool GetDisplayText(base::string16* vertically_compact,
                              base::string16* horizontally_compact) OVERRIDE;
  virtual const std::string& GetLanguageCode() const OVERRIDE;

 private:
  const wallet::Address* address_;

  DISALLOW_COPY_AND_ASSIGN(WalletAddressWrapper);
};

// A DataModelWrapper for Wallet instruments.
class WalletInstrumentWrapper : public DataModelWrapper {
 public:
  explicit WalletInstrumentWrapper(
      const wallet::WalletItems::MaskedInstrument* instrument);
  virtual ~WalletInstrumentWrapper();

  virtual base::string16 GetInfo(const AutofillType& type) const OVERRIDE;
  virtual base::string16 GetInfoForDisplay(const AutofillType& type) const
      OVERRIDE;
  virtual gfx::Image GetIcon() OVERRIDE;
  virtual bool GetDisplayText(base::string16* vertically_compact,
                              base::string16* horizontally_compact) OVERRIDE;
  virtual const std::string& GetLanguageCode() const OVERRIDE;

 private:
  const wallet::WalletItems::MaskedInstrument* instrument_;

  DISALLOW_COPY_AND_ASSIGN(WalletInstrumentWrapper);
};

// A DataModelWrapper for FullWallet billing data.
class FullWalletBillingWrapper : public DataModelWrapper {
 public:
  explicit FullWalletBillingWrapper(wallet::FullWallet* full_wallet);
  virtual ~FullWalletBillingWrapper();

  virtual base::string16 GetInfo(const AutofillType& type) const OVERRIDE;
  virtual bool GetDisplayText(base::string16* vertically_compact,
                              base::string16* horizontally_compact) OVERRIDE;
  virtual const std::string& GetLanguageCode() const OVERRIDE;

 private:
  wallet::FullWallet* full_wallet_;

  DISALLOW_COPY_AND_ASSIGN(FullWalletBillingWrapper);
};

// A DataModelWrapper for FullWallet shipping data.
class FullWalletShippingWrapper : public DataModelWrapper {
 public:
  explicit FullWalletShippingWrapper(wallet::FullWallet* full_wallet);
  virtual ~FullWalletShippingWrapper();

  virtual base::string16 GetInfo(const AutofillType& type) const OVERRIDE;
  virtual const std::string& GetLanguageCode() const OVERRIDE;

 private:
  wallet::FullWallet* full_wallet_;

  DISALLOW_COPY_AND_ASSIGN(FullWalletShippingWrapper);
};

// A DataModelWrapper for ::i18n::addressinput::AddressData objects.
class I18nAddressDataWrapper : public DataModelWrapper {
 public:
  explicit I18nAddressDataWrapper(
      const ::i18n::addressinput::AddressData* address);
  virtual ~I18nAddressDataWrapper();

  virtual base::string16 GetInfo(const AutofillType& type) const OVERRIDE;
  virtual const std::string& GetLanguageCode() const OVERRIDE;

 private:
  const ::i18n::addressinput::AddressData* address_;

  DISALLOW_COPY_AND_ASSIGN(I18nAddressDataWrapper);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_DATA_MODEL_WRAPPER_H_
