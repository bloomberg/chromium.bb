// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_BROWSER_FORM_STRUCTURE_H_
#define COMPONENTS_AUTOFILL_BROWSER_FORM_STRUCTURE_H_

#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "components/autofill/browser/autofill_field.h"
#include "components/autofill/browser/autofill_type.h"
#include "components/autofill/browser/field_types.h"
#include "components/autofill/common/web_element_descriptor.h"
#include "googleurl/src/gurl.h"

struct FormData;
struct FormDataPredictions;

enum RequestMethod {
  GET,
  POST
};

enum UploadRequired {
  UPLOAD_NOT_REQUIRED,
  UPLOAD_REQUIRED,
  USE_UPLOAD_RATES
};

class AutofillMetrics;

namespace autofill {
struct AutocheckoutPageMetaData;
}

namespace base {
class TimeTicks;
}

namespace buzz {
class XmlElement;
}

// FormStructure stores a single HTML form together with the values entered
// in the fields along with additional information needed by Autofill.
class FormStructure {
 public:
  FormStructure(const FormData& form,
                const std::string& autocheckout_url_prefix);
  virtual ~FormStructure();

  // Runs several heuristics against the form fields to determine their possible
  // types.
  void DetermineHeuristicTypes(const AutofillMetrics& metric_logger);

  // Encodes the XML upload request from this FormStructure.
  bool EncodeUploadRequest(const FieldTypeSet& available_field_types,
                           bool form_was_autofilled,
                           std::string* encoded_xml) const;

  // Encodes a XML block contains autofill field type from this FormStructure.
  // This XML will be written VLOG only, never be sent to server. It will
  // help make FieldAssignments and feed back to autofill server as
  // experiment data.
  bool EncodeFieldAssignments(const FieldTypeSet& available_field_types,
                              std::string* encoded_xml) const;

  // Encodes the XML query request for the set of forms.
  // All fields are returned in one XML. For example, there are three forms,
  // with 2, 4, and 3 fields. The returned XML would have type info for 9
  // fields, first two of which would be for the first form, next 4 for the
  // second, and the rest is for the third.
  static bool EncodeQueryRequest(const std::vector<FormStructure*>& forms,
                                 std::vector<std::string>* encoded_signatures,
                                 std::string* encoded_xml);

  // Parses the field types from the server query response. |forms| must be the
  // same as the one passed to EncodeQueryRequest when constructing the query.
  static void ParseQueryResponse(
      const std::string& response_xml,
      const std::vector<FormStructure*>& forms,
      autofill::AutocheckoutPageMetaData* page_meta_data,
      const AutofillMetrics& metric_logger);

  // Fills |forms| with the details from the given |form_structures| and their
  // fields' predicted types.
  static void GetFieldTypePredictions(
      const std::vector<FormStructure*>& form_structures,
      std::vector<FormDataPredictions>* forms);

  // The unique signature for this form, composed of the target url domain,
  // the form name, and the form field names in a 64-bit hash.
  std::string FormSignature() const;

  // Runs a quick heuristic to rule out forms that are obviously not
  // auto-fillable, like google/yahoo/msn search, etc. The requirement that the
  // form's method be POST is only applied if |require_method_post| is true.
  bool IsAutofillable(bool require_method_post) const;

  // Resets |autofill_count_| and counts the number of auto-fillable fields.
  // This is used when we receive server data for form fields.  At that time,
  // we may have more known fields than just the number of fields we matched
  // heuristically.
  void UpdateAutofillCount();

  // Returns true if this form matches the structural requirements for Autofill.
  // The requirement that the form's method be POST is only applied if
  // |require_method_post| is true.
  bool ShouldBeParsed(bool require_method_post) const;

  // Returns true if we should query the crowdsourcing server to determine this
  // form's field types.  If the form includes author-specified types, this will
  // return false.
  bool ShouldBeCrowdsourced() const;

  // Sets the field types and experiment id to be those set for |cached_form|.
  void UpdateFromCache(const FormStructure& cached_form);

  // Logs quality metrics for |this|, which should be a user-submitted form.
  // This method should only be called after the possible field types have been
  // set for each field.  |interaction_time| should be a timestamp corresponding
  // to the user's first interaction with the form.  |submission_time| should be
  // a timestamp corresponding to the form's submission.
  void LogQualityMetrics(const AutofillMetrics& metric_logger,
                         const base::TimeTicks& load_time,
                         const base::TimeTicks& interaction_time,
                         const base::TimeTicks& submission_time) const;

  // Classifies each field in |fields_| based upon its |autocomplete| attribute,
  // if the attribute is available.  The association is stored into the field's
  // |heuristic_type|.
  // Fills |found_types| with |true| if the attribute is available and neither
  // empty nor set to the special values "on" or "off" for at least one field.
  // Fills |found_sections| with |true| if the attribute specifies a section for
  // at least one field.
  void ParseFieldTypesFromAutocompleteAttributes(bool* found_types,
                                                 bool* found_sections);

  const AutofillField* field(size_t index) const;
  AutofillField* field(size_t index);
  size_t field_count() const;

  // Returns the number of fields that are able to be autofilled.
  size_t autofill_count() const { return autofill_count_; }

  // Used for iterating over the fields.
  std::vector<AutofillField*>::const_iterator begin() const {
    return fields_.begin();
  }
  std::vector<AutofillField*>::const_iterator end() const {
    return fields_.end();
  }

  const GURL& source_url() const { return source_url_; }

  UploadRequired upload_required() const { return upload_required_; }

  virtual std::string server_experiment_id() const;

  // Returns a FormData containing the data this form structure knows about.
  // |user_submitted| is currently always false.
  FormData ToFormData() const;

  bool operator==(const FormData& form) const;
  bool operator!=(const FormData& form) const;

 private:
  friend class FormStructureTest;
  FRIEND_TEST_ALL_PREFIXES(AutofillDownloadTest, QueryAndUploadTest);

  // 64-bit hash of the string - used in FormSignature and unit-tests.
  static std::string Hash64Bit(const std::string& str);

  enum EncodeRequestType {
    QUERY,
    UPLOAD,
    FIELD_ASSIGNMENTS,
  };

  // Adds form info to |encompassing_xml_element|. |request_type| indicates if
  // it is a query or upload.
  bool EncodeFormRequest(EncodeRequestType request_type,
                         buzz::XmlElement* encompassing_xml_element) const;

  // Classifies each field in |fields_| into a logical section.
  // Sections are identified by the heuristic that a logical section should not
  // include multiple fields of the same autofill type (with some exceptions, as
  // described in the implementation).  Sections are furthermore distinguished
  // as either credit card or non-credit card sections.
  // If |has_author_specified_sections| is true, only the second pass --
  // distinguishing credit card sections from non-credit card ones -- is made.
  void IdentifySections(bool has_author_specified_sections);

  bool IsAutocheckoutEnabled() const;

  // Returns the minimal number of fillable fields required to start autofill.
  size_t RequiredFillableFields() const;
  size_t active_field_count() const;

  // The name of the form.
  string16 form_name_;

  // The source URL.
  GURL source_url_;

  // The target URL.
  GURL target_url_;

  // The number of fields able to be auto-filled.
  size_t autofill_count_;

  // A vector of all the input fields in the form.
  ScopedVector<AutofillField> fields_;

  // The number of fields counted towards form signature and request to Autofill
  // server.
  size_t active_field_count_;

  // The names of the form input elements, that are part of the form signature.
  // The string starts with "&" and the names are also separated by the "&"
  // character. E.g.: "&form_input1_name&form_input2_name&...&form_inputN_name"
  std::string form_signature_field_names_;

  // Whether the server expects us to always upload, never upload, or default
  // to the stored upload rates.
  UploadRequired upload_required_;

  // The server experiment corresponding to the server types returned for this
  // form.
  std::string server_experiment_id_;

  // GET or POST.
  RequestMethod method_;

  // Whether the form includes any field types explicitly specified by the site
  // author, via the |autocompletetype| attribute.
  bool has_author_specified_types_;

  // The URL prefix matched in autocheckout whitelist. An empty string implies
  // autocheckout is not enabled for this form.
  std::string autocheckout_url_prefix_;

  DISALLOW_COPY_AND_ASSIGN(FormStructure);
};

#endif  // COMPONENTS_AUTOFILL_BROWSER_FORM_STRUCTURE_H_
