// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_ONC_ONC_VALIDATOR_H_
#define CHROMEOS_NETWORK_ONC_ONC_VALIDATOR_H_

#include "base/memory/scoped_ptr.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/network/onc/onc_mapper.h"

namespace base {
class Value;
class DictionaryValue;
}

namespace chromeos {
namespace onc {

struct OncValueSignature;

class CHROMEOS_EXPORT Validator : public Mapper {
 public:
  // Creates a Validator that searches for the following invalid cases:
  // - a field name is found that is not part of the signature
  //   (controlled by |error_on_unknown_field|)
  //
  // - a kRecommended array contains a field name that is not part of the
  //   enclosing object's signature or if that field is dictionary typed
  //   (controlled by |error_on_wrong_recommended|)
  //
  // - |managed_onc| is false and a field with name kRecommended is found
  //   (always ignored)
  //
  // - a required field is missing (controlled by |error_on_missing_field|)
  //
  // If one of these invalid cases occurs and the controlling flag is true, then
  // it is an error and the validation stops. The function
  // ValidateAndRepairObject returns NULL.
  //
  // If no error occurred, then a DeepCopy of the validated object is created,
  // which contains all but the invalid fields and values.
  Validator(bool error_on_unknown_field,
            bool error_on_wrong_recommended,
            bool error_on_missing_field,
            bool managed_onc);

  virtual ~Validator();

  // Validate the given |onc_object| according to |object_signature|. The
  // |object_signature| has to be a pointer to one of the signatures in
  // |onc_signature.h|. If an error is found, the function returns NULL. If
  // possible (no error encountered) a DeepCopy is created that contains all but
  // the invalid fields and values and returns this "repaired" object.
  // That means, if not handled as an error, then the following are ignored:
  // - unknown fields
  // - invalid field names in kRecommended arrays
  // - kRecommended fields in an unmanaged ONC
  // For details, see the comment at the Constructor.
  scoped_ptr<base::DictionaryValue> ValidateAndRepairObject(
      const OncValueSignature* object_signature,
      const base::DictionaryValue& onc_object);

 private:
  // Overriden from Mapper:
  // Compare |onc_value|s type with |onc_type| and validate/repair according to
  // |signature|. On error returns NULL.
  virtual scoped_ptr<base::Value> MapValue(
    const OncValueSignature& signature,
    const base::Value& onc_value) OVERRIDE;

  // Dispatch to the right validation function according to
  // |signature|. Iterates over all fields and recursively validates/repairs
  // these. All valid fields are added to the result dictionary. Returns the
  // repaired dictionary. On error returns NULL.
  virtual scoped_ptr<base::DictionaryValue> MapObject(
      const OncValueSignature& signature,
      const base::DictionaryValue& onc_object) OVERRIDE;

  // This is the default validation of objects/dictionaries. Validates
  // |onc_object| according to |object_signature|. |result| must point to a
  // dictionary into which the repaired fields are written.
  bool ValidateObjectDefault(
      const OncValueSignature& object_signature,
      const base::DictionaryValue& onc_object,
      base::DictionaryValue* result);

  // Validates/repairs the kRecommended array in |result| according to
  // |object_signature| of the enclosing object.
  bool ValidateRecommendedField(
      const OncValueSignature& object_signature,
      base::DictionaryValue* result);

  bool ValidateNetworkConfiguration(
      const base::DictionaryValue& onc_object,
      base::DictionaryValue* result);

  bool ValidateEthernet(
      const base::DictionaryValue& onc_object,
      base::DictionaryValue* result);

  bool ValidateIPConfig(
      const base::DictionaryValue& onc_object,
      base::DictionaryValue* result);

  bool ValidateWiFi(
      const base::DictionaryValue& onc_object,
      base::DictionaryValue* result);

  bool ValidateVPN(
      const base::DictionaryValue& onc_object,
      base::DictionaryValue* result);

  bool ValidateIPsec(
      const base::DictionaryValue& onc_object,
      base::DictionaryValue* result);

  bool ValidateOpenVPN(
      const base::DictionaryValue& onc_object,
      base::DictionaryValue* result);

  bool ValidateCertificatePattern(
      const base::DictionaryValue& onc_object,
      base::DictionaryValue* result);

  bool ValidateProxySettings(
      const base::DictionaryValue& onc_object,
      base::DictionaryValue* result);

  bool ValidateProxyLocation(
      const base::DictionaryValue& onc_object,
      base::DictionaryValue* result);

  bool ValidateEAP(
      const base::DictionaryValue& onc_object,
      base::DictionaryValue* result);

  bool ValidateCertificate(
      const base::DictionaryValue& onc_object,
      base::DictionaryValue* result);

  const bool error_on_unknown_field_;
  const bool error_on_wrong_recommended_;
  const bool error_on_missing_field_;
  const bool managed_onc_;

  DISALLOW_COPY_AND_ASSIGN(Validator);
};

}  // namespace onc
}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_ONC_ONC_VALIDATOR_H_
