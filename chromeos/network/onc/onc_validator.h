// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_ONC_ONC_VALIDATOR_H_
#define CHROMEOS_NETWORK_ONC_ONC_VALIDATOR_H_

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/network/onc/onc_constants.h"
#include "chromeos/network/onc/onc_mapper.h"

namespace base {
class DictionaryValue;
class Value;
}

namespace chromeos {
namespace onc {

struct OncValueSignature;

// The ONC Validator searches for the following invalid cases:
// - a value is found that has the wrong type or is not expected according to
//   the ONC spec (always an error)
//
// - a field name is found that is not part of the signature
//   (controlled by flag |error_on_unknown_field|)
//
// - a kRecommended array contains a field name that is not part of the
//   enclosing object's signature or if that field is dictionary typed
//   (controlled by flag |error_on_wrong_recommended|)
//
// - |managed_onc| is false and a field with name kRecommended is found
//   (always ignored)
//
// - a required field is missing (controlled by flag |error_on_missing_field|)
//
// If one of these invalid cases occurs and, in case of a controlling flag, that
// flag is true, then it is an error. The function ValidateAndRepairObject sets
// |result| to INVALID and returns NULL.
//
// Otherwise, a DeepCopy of the validated object is created, which contains
// all but the invalid fields and values.
//
// If one of the invalid cases occurs and the controlling flag is false, then
// it is a warning. The function ValidateAndRepairObject sets |result| to
// VALID_WITH_WARNINGS and returns the repaired copy.
//
// If no error occurred, |result| is set to VALID and an exact DeepCopy is
// returned.
class CHROMEOS_EXPORT Validator : public Mapper {
 public:
  enum Result {
    VALID,
    VALID_WITH_WARNINGS,
    INVALID
  };

  // See the class comment.
  Validator(bool error_on_unknown_field,
            bool error_on_wrong_recommended,
            bool error_on_missing_field,
            bool managed_onc);

  virtual ~Validator();

  // Sets the ONC source to |source|. If not set, defaults to ONC_SOURCE_NONE.
  // If the source is set to ONC_SOURCE_DEVICE_POLICY, validation additionally
  // checks:
  // - only the network types Wifi and Ethernet are allowed
  // - client certificate patterns are disallowed
  void SetOncSource(ONCSource source) {
    onc_source_ = source;
  }

  // Validate the given |onc_object| according to |object_signature|. The
  // |object_signature| has to be a pointer to one of the signatures in
  // |onc_signature.h|. If an error is found, the function returns NULL and sets
  // |result| to INVALID. If possible (no error encountered) a DeepCopy is
  // created that contains all but the invalid fields and values and returns
  // this "repaired" object. That means, if not handled as an error, then the
  // following are dropped from the copy:
  // - unknown fields
  // - invalid field names in kRecommended arrays
  // - kRecommended fields in an unmanaged ONC
  // If any of these cases occurred, sets |result| to VALID_WITH_WARNINGS and
  // otherwise to VALID.
  // For details, see the class comment.
  scoped_ptr<base::DictionaryValue> ValidateAndRepairObject(
      const OncValueSignature* object_signature,
      const base::DictionaryValue& onc_object,
      Result* result);

 private:
  // Overridden from Mapper:
  // Compare |onc_value|s type with |onc_type| and validate/repair according to
  // |signature|. On error returns NULL.
  virtual scoped_ptr<base::Value> MapValue(
    const OncValueSignature& signature,
    const base::Value& onc_value,
    bool* error) OVERRIDE;

  // Dispatch to the right validation function according to
  // |signature|. Iterates over all fields and recursively validates/repairs
  // these. All valid fields are added to the result dictionary. Returns the
  // repaired dictionary. Only on error returns NULL.
  virtual scoped_ptr<base::DictionaryValue> MapObject(
      const OncValueSignature& signature,
      const base::DictionaryValue& onc_object,
      bool* error) OVERRIDE;

  // Pushes/pops the |field_name| to |path_|, otherwise like |Mapper::MapField|.
  virtual scoped_ptr<base::Value> MapField(
      const std::string& field_name,
      const OncValueSignature& object_signature,
      const base::Value& onc_value,
      bool* found_unknown_field,
      bool* error) OVERRIDE;

  // Ignores nested errors in NetworkConfigurations and Certificates, otherwise
  // like |Mapper::MapArray|.
  virtual scoped_ptr<base::ListValue> MapArray(
      const OncValueSignature& array_signature,
      const base::ListValue& onc_array,
      bool* nested_error) OVERRIDE;

  // Pushes/pops the index to |path_|, otherwise like |Mapper::MapEntry|.
  virtual scoped_ptr<base::Value> MapEntry(
      int index,
      const OncValueSignature& signature,
      const base::Value& onc_value,
      bool* error) OVERRIDE;

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

  bool ValidateToplevelConfiguration(
      const base::DictionaryValue& onc_object,
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

  bool FieldExistsAndHasNoValidValue(const base::DictionaryValue& object,
                                     const std::string &field_name,
                                     const char** valid_values);

  bool FieldExistsAndIsNotInRange(const base::DictionaryValue& object,
                                  const std::string &field_name,
                                  int lower_bound,
                                  int upper_bound);

  bool RequireField(const base::DictionaryValue& dict, const std::string& key);

  bool CertPatternInDevicePolicy(const std::string& cert_type);

  std::string WarningHeader();
  std::string ErrorHeader();
  std::string MessageHeader(bool is_error);

  const bool error_on_unknown_field_;
  const bool error_on_wrong_recommended_;
  const bool error_on_missing_field_;
  const bool managed_onc_;

  ONCSource onc_source_;

  // The path of field names and indices to the current value. Indices
  // are stored as strings in decimal notation.
  std::vector<std::string> path_;

  // Tracks if an error or warning occurred within validation initiated by
  // function ValidateAndRepairObject.
  bool error_or_warning_found_;

  DISALLOW_COPY_AND_ASSIGN(Validator);
};

}  // namespace onc
}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_ONC_ONC_VALIDATOR_H_
