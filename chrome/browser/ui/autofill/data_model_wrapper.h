// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_DATA_MODEL_WRAPPER_H_
#define CHROME_BROWSER_UI_AUTOFILL_DATA_MODEL_WRAPPER_H_

#include "base/compiler_specific.h"
#include "base/string16.h"
#include "chrome/browser/ui/autofill/autofill_dialog_types.h"
#include "components/autofill/browser/field_types.h"
#include "components/autofill/browser/wallet/wallet_items.h"

class AutofillProfile;
class CreditCard;
class FormGroup;
class FormStructure;

namespace gfx {
class Image;
}

namespace autofill {

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

  // Returns the data for a specific autocomplete type.
  virtual string16 GetInfo(AutofillFieldType type) = 0;

  // Returns the icon, if any, that represents this model.
  virtual gfx::Image GetIcon();

  // Fills in |inputs| with the data that this model contains (|inputs| is an
  // out-param).
  virtual void FillInputs(DetailInputs* inputs);

  // Returns text to display to the user to summarize this data source. The
  // default implementation assumes this is an address.
  virtual string16 GetDisplayText();

  // Fills in |form_structure| with the data that this model contains. |inputs|
  // and |comparator| are used to determine whether each field in the
  // FormStructure should be filled in or left alone.
  void FillFormStructure(
      const DetailInputs& inputs,
      const InputFieldComparator& compare,
      FormStructure* form_structure);

 protected:
  DataModelWrapper();

  // Fills in |field| with data from the model.
  virtual void FillFormField(AutofillField* field);

 private:
  DISALLOW_COPY_AND_ASSIGN(DataModelWrapper);
};

// A DataModelWrapper for Autofill data.
class AutofillFormGroupWrapper : public DataModelWrapper {
 public:
  AutofillFormGroupWrapper(const FormGroup* form_group, size_t variant);
  virtual ~AutofillFormGroupWrapper();

  virtual string16 GetInfo(AutofillFieldType type) OVERRIDE;

 protected:
  virtual void FillFormField(AutofillField* field) OVERRIDE;

  size_t variant() const { return variant_; }

 private:
  const FormGroup* form_group_;
  const size_t variant_;

  DISALLOW_COPY_AND_ASSIGN(AutofillFormGroupWrapper);
};

// A DataModelWrapper for Autofill profiles.
class AutofillProfileWrapper : public AutofillFormGroupWrapper {
 public:
  AutofillProfileWrapper(const AutofillProfile* profile, size_t variant);
  virtual ~AutofillProfileWrapper();

  virtual void FillInputs(DetailInputs* inputs) OVERRIDE;

 private:
  const AutofillProfile* profile_;

  DISALLOW_COPY_AND_ASSIGN(AutofillProfileWrapper);
};

// A DataModelWrapper specifically for Autofill CreditCard data.
class AutofillCreditCardWrapper : public AutofillFormGroupWrapper {
 public:
  explicit AutofillCreditCardWrapper(const CreditCard* card);
  virtual ~AutofillCreditCardWrapper();

  virtual string16 GetInfo(AutofillFieldType type) OVERRIDE;
  virtual gfx::Image GetIcon() OVERRIDE;
  virtual string16 GetDisplayText() OVERRIDE;

 protected:
  virtual void FillFormField(AutofillField* field) OVERRIDE;

 private:
  const CreditCard* card_;

  DISALLOW_COPY_AND_ASSIGN(AutofillCreditCardWrapper);
};

// A DataModelWrapper for Wallet addresses.
class WalletAddressWrapper : public DataModelWrapper {
 public:
  explicit WalletAddressWrapper(const wallet::Address* address);
  virtual ~WalletAddressWrapper();

  virtual string16 GetInfo(AutofillFieldType type) OVERRIDE;

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

  virtual string16 GetInfo(AutofillFieldType type) OVERRIDE;
  virtual gfx::Image GetIcon() OVERRIDE;
  virtual string16 GetDisplayText() OVERRIDE;

 private:
  const wallet::WalletItems::MaskedInstrument* instrument_;

  DISALLOW_COPY_AND_ASSIGN(WalletInstrumentWrapper);
};

// A DataModelWrapper for FullWallets billing data.
class FullWalletBillingWrapper : public DataModelWrapper {
 public:
  explicit FullWalletBillingWrapper(wallet::FullWallet* full_wallet);
  virtual ~FullWalletBillingWrapper();

  virtual string16 GetInfo(AutofillFieldType type) OVERRIDE;

 private:
  wallet::FullWallet* full_wallet_;

  DISALLOW_COPY_AND_ASSIGN(FullWalletBillingWrapper);
};

// A DataModelWrapper for FullWallets shipping data.
class FullWalletShippingWrapper : public DataModelWrapper {
 public:
  explicit FullWalletShippingWrapper(wallet::FullWallet* full_wallet);
  virtual ~FullWalletShippingWrapper();

  virtual string16 GetInfo(AutofillFieldType type) OVERRIDE;

 private:
  wallet::FullWallet* full_wallet_;

  DISALLOW_COPY_AND_ASSIGN(FullWalletShippingWrapper);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_DATA_MODEL_WRAPPER_H_
