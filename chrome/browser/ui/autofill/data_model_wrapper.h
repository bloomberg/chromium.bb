// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_DATA_MODEL_WRAPPER_H_
#define CHROME_BROWSER_UI_AUTOFILL_DATA_MODEL_WRAPPER_H_

#include "base/compiler_specific.h"
#include "base/string16.h"
#include "chrome/browser/autofill/field_types.h"
#include "chrome/browser/autofill/wallet/wallet_items.h"
#include "chrome/browser/ui/autofill/autofill_dialog_types.h"

class CreditCard;
class FormGroup;
class FormStructure;

namespace gfx {
class Image;
}

namespace wallet {
class Address;
}

namespace autofill {

// A glue class that allows uniform interactions with autocomplete data sources,
// regardless of their type. Implementations are intended to be lightweight and
// copyable, only holding weak references to their backing model.
class DataModelWrapper {
 public:
  virtual ~DataModelWrapper();

  // Returns the data for a specific autocomplete type.
  virtual string16 GetInfo(AutofillFieldType type) = 0;

  // Returns the icon, if any, that represents this model.
  virtual gfx::Image GetIcon() = 0;

  // Fills in |inputs| with the data that this model contains (|inputs| is an
  // out-param).
  virtual void FillInputs(DetailInputs* inputs) = 0;

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
  // Fills in |field| with data from the model.
  virtual void FillFormField(AutofillField* field) = 0;
};

// A DataModelWrapper for Autofill data.
class AutofillDataModelWrapper : public DataModelWrapper {
 public:
  AutofillDataModelWrapper(const FormGroup* form_group, size_t variant);
  virtual ~AutofillDataModelWrapper();

  virtual string16 GetInfo(AutofillFieldType type) OVERRIDE;
  virtual gfx::Image GetIcon() OVERRIDE;
  virtual void FillInputs(DetailInputs* inputs) OVERRIDE;

 protected:
  virtual void FillFormField(AutofillField* field) OVERRIDE;

 private:
  const FormGroup* form_group_;
  const size_t variant_;
};

// A DataModelWrapper specifically for Autofill CreditCard data.
class AutofillCreditCardWrapper : public AutofillDataModelWrapper {
 public:
  explicit AutofillCreditCardWrapper(const CreditCard* card);
  virtual ~AutofillCreditCardWrapper();

  virtual gfx::Image GetIcon() OVERRIDE;
  virtual string16 GetDisplayText() OVERRIDE;

 protected:
  virtual void FillFormField(AutofillField* field) OVERRIDE;

 private:
  const CreditCard* card_;
};

// A DataModelWrapper for Wallet addresses.
class WalletAddressWrapper : public DataModelWrapper {
 public:
  explicit WalletAddressWrapper(const wallet::Address* address);
  virtual ~WalletAddressWrapper();

  virtual string16 GetInfo(AutofillFieldType type) OVERRIDE;
  virtual gfx::Image GetIcon() OVERRIDE;
  virtual void FillInputs(DetailInputs* inputs) OVERRIDE;

 protected:
  virtual void FillFormField(AutofillField* field) OVERRIDE;

 private:
  const wallet::Address* address_;
};

// A DataModelWrapper for Wallet instruments.
class WalletInstrumentWrapper : public DataModelWrapper {
 public:
  explicit WalletInstrumentWrapper(
      const wallet::WalletItems::MaskedInstrument* instrument);
  virtual ~WalletInstrumentWrapper();

  virtual string16 GetInfo(AutofillFieldType type) OVERRIDE;
  virtual gfx::Image GetIcon() OVERRIDE;
  virtual void FillInputs(DetailInputs* inputs) OVERRIDE;
  virtual string16 GetDisplayText() OVERRIDE;

 protected:
  virtual void FillFormField(AutofillField* field) OVERRIDE;

 private:
  const wallet::WalletItems::MaskedInstrument* instrument_;
};

}

#endif  // CHROME_BROWSER_UI_AUTOFILL_DATA_MODEL_WRAPPER_H_
