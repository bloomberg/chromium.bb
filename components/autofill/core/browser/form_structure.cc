// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_structure.h"

#include <utility>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/i18n/case_conversion.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/sha1.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/autofill_xml_parser.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_field.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_predictions.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/form_field_data_predictions.h"
#include "third_party/icu/source/i18n/unicode/regex.h"
#include "third_party/libjingle/source/talk/xmllite/xmlelement.h"

namespace autofill {
namespace {

// XML elements and attributes.
const char kAttributeAutofillUsed[] = "autofillused";
const char kAttributeAutofillType[] = "autofilltype";
const char kAttributeClientVersion[] = "clientversion";
const char kAttributeDataPresent[] = "datapresent";
const char kAttributeFieldID[] = "fieldid";
const char kAttributeFieldType[] = "fieldtype";
const char kAttributeFormSignature[] = "formsignature";
const char kAttributeName[] = "name";
const char kAttributeSignature[] = "signature";
const char kClientVersion[] = "6.1.1715.1442/en (GGLL)";
const char kXMLDeclaration[] = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>";
const char kXMLElementAutofillQuery[] = "autofillquery";
const char kXMLElementAutofillUpload[] = "autofillupload";
const char kXMLElementFieldAssignments[] = "fieldassignments";
const char kXMLElementField[] = "field";
const char kXMLElementFields[] = "fields";
const char kXMLElementForm[] = "form";
const char kBillingMode[] = "billing";
const char kShippingMode[] = "shipping";

// Stip away >= 5 consecutive digits.
const char kIgnorePatternInFieldName[] = "\\d{5,}+";

// Helper for |EncodeUploadRequest()| that creates a bit field corresponding to
// |available_field_types| and returns the hex representation as a string.
std::string EncodeFieldTypes(const ServerFieldTypeSet& available_field_types) {
  // There are |MAX_VALID_FIELD_TYPE| different field types and 8 bits per byte,
  // so we need ceil(MAX_VALID_FIELD_TYPE / 8) bytes to encode the bit field.
  const size_t kNumBytes = (MAX_VALID_FIELD_TYPE + 0x7) / 8;

  // Pack the types in |available_field_types| into |bit_field|.
  std::vector<uint8> bit_field(kNumBytes, 0);
  for (ServerFieldTypeSet::const_iterator field_type =
           available_field_types.begin();
       field_type != available_field_types.end();
       ++field_type) {
    // Set the appropriate bit in the field.  The bit we set is the one
    // |field_type| % 8 from the left of the byte.
    const size_t byte = *field_type / 8;
    const size_t bit = 0x80 >> (*field_type % 8);
    DCHECK(byte < bit_field.size());
    bit_field[byte] |= bit;
  }

  // Discard any trailing zeroes.
  // If there are no available types, we return the empty string.
  size_t data_end = bit_field.size();
  for (; data_end > 0 && !bit_field[data_end - 1]; --data_end) {
  }

  // Print all meaningfull bytes into a string.
  std::string data_presence;
  data_presence.reserve(data_end * 2 + 1);
  for (size_t i = 0; i < data_end; ++i) {
    base::StringAppendF(&data_presence, "%02x", bit_field[i]);
  }

  return data_presence;
}

// Helper for |EncodeFormRequest()| that creates XmlElements for the given field
// in upload xml, and also add them to the parent XmlElement.
void EncodeFieldForUpload(const AutofillField& field,
                          buzz::XmlElement* parent) {
  // Don't upload checkable fields.
  if (field.is_checkable)
    return;

  ServerFieldTypeSet types = field.possible_types();
  // |types| could be empty in unit-tests only.
  for (ServerFieldTypeSet::iterator field_type = types.begin();
       field_type != types.end(); ++field_type) {
    buzz::XmlElement *field_element = new buzz::XmlElement(
        buzz::QName(kXMLElementField));

    field_element->SetAttr(buzz::QName(kAttributeSignature),
                           field.FieldSignature());
    field_element->SetAttr(buzz::QName(kAttributeAutofillType),
                           base::IntToString(*field_type));
    parent->AddElement(field_element);
  }
}

// Helper for |EncodeFormRequest()| that creates XmlElement for the given field
// in query xml, and also add it to the parent XmlElement.
void EncodeFieldForQuery(const AutofillField& field,
                         buzz::XmlElement* parent) {
  buzz::XmlElement *field_element = new buzz::XmlElement(
      buzz::QName(kXMLElementField));
  field_element->SetAttr(buzz::QName(kAttributeSignature),
                         field.FieldSignature());
  parent->AddElement(field_element);
}

// Helper for |EncodeFormRequest()| that creates XmlElements for the given field
// in field assignments xml, and also add them to the parent XmlElement.
void EncodeFieldForFieldAssignments(const AutofillField& field,
                                    buzz::XmlElement* parent) {
  ServerFieldTypeSet types = field.possible_types();
  for (ServerFieldTypeSet::iterator field_type = types.begin();
       field_type != types.end(); ++field_type) {
    buzz::XmlElement *field_element = new buzz::XmlElement(
        buzz::QName(kXMLElementFields));

    field_element->SetAttr(buzz::QName(kAttributeFieldID),
                           field.FieldSignature());
    field_element->SetAttr(buzz::QName(kAttributeFieldType),
                           base::IntToString(*field_type));
    field_element->SetAttr(buzz::QName(kAttributeName),
                           base::UTF16ToUTF8(field.name));
    parent->AddElement(field_element);
  }
}

// Returns |true| iff the |token| is a type hint for a contact field, as
// specified in the implementation section of http://is.gd/whatwg_autocomplete
// Note that "fax" and "pager" are intentionally ignored, as Chrome does not
// support filling either type of information.
bool IsContactTypeHint(const std::string& token) {
  return token == "home" || token == "work" || token == "mobile";
}

// Returns |true| iff the |token| is a type hint appropriate for a field of the
// given |field_type|, as specified in the implementation section of
// http://is.gd/whatwg_autocomplete
bool ContactTypeHintMatchesFieldType(const std::string& token,
                                     HtmlFieldType field_type) {
  // The "home" and "work" type hints are only appropriate for email and phone
  // number field types.
  if (token == "home" || token == "work") {
    return field_type == HTML_TYPE_EMAIL ||
        (field_type >= HTML_TYPE_TEL &&
         field_type <= HTML_TYPE_TEL_LOCAL_SUFFIX);
  }

  // The "mobile" type hint is only appropriate for phone number field types.
  // Note that "fax" and "pager" are intentionally ignored, as Chrome does not
  // support filling either type of information.
  if (token == "mobile") {
    return field_type >= HTML_TYPE_TEL &&
        field_type <= HTML_TYPE_TEL_LOCAL_SUFFIX;
  }

  return false;
}

// Returns the Chrome Autofill-supported field type corresponding to the given
// |autocomplete_attribute_value|, if there is one, in the context of the given
// |field|.  Chrome Autofill supports a subset of the field types listed at
// http://is.gd/whatwg_autocomplete
HtmlFieldType FieldTypeFromAutocompleteAttributeValue(
    const std::string& autocomplete_attribute_value,
    const AutofillField& field) {
  if (autocomplete_attribute_value == "name")
    return HTML_TYPE_NAME;

  if (autocomplete_attribute_value == "given-name")
    return HTML_TYPE_GIVEN_NAME;

  if (autocomplete_attribute_value == "additional-name") {
    if (field.max_length == 1)
      return HTML_TYPE_ADDITIONAL_NAME_INITIAL;
    else
      return HTML_TYPE_ADDITIONAL_NAME;
  }

  if (autocomplete_attribute_value == "family-name")
    return HTML_TYPE_FAMILY_NAME;

  if (autocomplete_attribute_value == "organization")
    return HTML_TYPE_ORGANIZATION;

  if (autocomplete_attribute_value == "street-address")
    return HTML_TYPE_STREET_ADDRESS;

  if (autocomplete_attribute_value == "address-line1")
    return HTML_TYPE_ADDRESS_LINE1;

  if (autocomplete_attribute_value == "address-line2")
    return HTML_TYPE_ADDRESS_LINE2;

  if (autocomplete_attribute_value == "address-line3")
    return HTML_TYPE_ADDRESS_LINE3;

  // TODO(estade): remove support for "locality" and "region".
  if (autocomplete_attribute_value == "locality")
    return HTML_TYPE_ADDRESS_LEVEL2;

  if (autocomplete_attribute_value == "region")
    return HTML_TYPE_ADDRESS_LEVEL1;

  if (autocomplete_attribute_value == "address-level1")
    return HTML_TYPE_ADDRESS_LEVEL1;

  if (autocomplete_attribute_value == "address-level2")
    return HTML_TYPE_ADDRESS_LEVEL2;

  if (autocomplete_attribute_value == "address-level3")
    return HTML_TYPE_ADDRESS_LEVEL3;

  if (autocomplete_attribute_value == "country")
    return HTML_TYPE_COUNTRY_CODE;

  if (autocomplete_attribute_value == "country-name")
    return HTML_TYPE_COUNTRY_NAME;

  if (autocomplete_attribute_value == "postal-code")
    return HTML_TYPE_POSTAL_CODE;

  // content_switches.h isn't accessible from here, hence we have
  // to copy the string literal. This should be removed soon anyway.
  if (autocomplete_attribute_value == "address" &&
      CommandLine::ForCurrentProcess()->HasSwitch(
          "enable-experimental-web-platform-features")) {
    return HTML_TYPE_FULL_ADDRESS;
  }

  if (autocomplete_attribute_value == "cc-name")
    return HTML_TYPE_CREDIT_CARD_NAME;

  if (autocomplete_attribute_value == "cc-number")
    return HTML_TYPE_CREDIT_CARD_NUMBER;

  if (autocomplete_attribute_value == "cc-exp") {
    if (field.max_length == 5)
      return HTML_TYPE_CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR;
    else if (field.max_length == 7)
      return HTML_TYPE_CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR;
    else
      return HTML_TYPE_CREDIT_CARD_EXP;
  }

  if (autocomplete_attribute_value == "cc-exp-month")
    return HTML_TYPE_CREDIT_CARD_EXP_MONTH;

  if (autocomplete_attribute_value == "cc-exp-year") {
    if (field.max_length == 2)
      return HTML_TYPE_CREDIT_CARD_EXP_2_DIGIT_YEAR;
    else if (field.max_length == 4)
      return HTML_TYPE_CREDIT_CARD_EXP_4_DIGIT_YEAR;
    else
      return HTML_TYPE_CREDIT_CARD_EXP_YEAR;
  }

  if (autocomplete_attribute_value == "cc-csc")
    return HTML_TYPE_CREDIT_CARD_VERIFICATION_CODE;

  if (autocomplete_attribute_value == "cc-type")
    return HTML_TYPE_CREDIT_CARD_TYPE;

  if (autocomplete_attribute_value == "transaction-amount")
    return HTML_TYPE_TRANSACTION_AMOUNT;

  if (autocomplete_attribute_value == "transaction-currency")
    return HTML_TYPE_TRANSACTION_CURRENCY;

  if (autocomplete_attribute_value == "tel")
    return HTML_TYPE_TEL;

  if (autocomplete_attribute_value == "tel-country-code")
    return HTML_TYPE_TEL_COUNTRY_CODE;

  if (autocomplete_attribute_value == "tel-national")
    return HTML_TYPE_TEL_NATIONAL;

  if (autocomplete_attribute_value == "tel-area-code")
    return HTML_TYPE_TEL_AREA_CODE;

  if (autocomplete_attribute_value == "tel-local")
    return HTML_TYPE_TEL_LOCAL;

  if (autocomplete_attribute_value == "tel-local-prefix")
    return HTML_TYPE_TEL_LOCAL_PREFIX;

  if (autocomplete_attribute_value == "tel-local-suffix")
    return HTML_TYPE_TEL_LOCAL_SUFFIX;

  if (autocomplete_attribute_value == "email")
    return HTML_TYPE_EMAIL;

  return HTML_TYPE_UNKNOWN;
}

std::string StripDigitsIfRequired(const base::string16& input) {
  UErrorCode status = U_ZERO_ERROR;
  CR_DEFINE_STATIC_LOCAL(icu::UnicodeString, icu_pattern,
                         (kIgnorePatternInFieldName));
  CR_DEFINE_STATIC_LOCAL(icu::RegexMatcher, matcher,
                         (icu_pattern, UREGEX_CASE_INSENSITIVE, status));
  DCHECK_EQ(status, U_ZERO_ERROR);

  icu::UnicodeString icu_input(input.data(), input.length());
  matcher.reset(icu_input);

  icu::UnicodeString replaced_string = matcher.replaceAll("", status);

  std::string return_string;
  status = U_ZERO_ERROR;
  base::UTF16ToUTF8(replaced_string.getBuffer(),
                    static_cast<size_t>(replaced_string.length()),
                    &return_string);
  if (status != U_ZERO_ERROR) {
    DVLOG(1) << "Couldn't strip digits in " << base::UTF16ToUTF8(input);
    return base::UTF16ToUTF8(input);
  }

  return return_string;
}

}  // namespace

FormStructure::FormStructure(const FormData& form)
    : form_name_(form.name),
      source_url_(form.origin),
      target_url_(form.action),
      autofill_count_(0),
      active_field_count_(0),
      upload_required_(USE_UPLOAD_RATES),
      has_author_specified_types_(false) {
  // Copy the form fields.
  std::map<base::string16, size_t> unique_names;
  for (std::vector<FormFieldData>::const_iterator field =
           form.fields.begin();
       field != form.fields.end(); ++field) {
    if (!ShouldSkipField(*field)) {
      // Add all supported form fields (including with empty names) to the
      // signature.  This is a requirement for Autofill servers.
      form_signature_field_names_.append("&");
      form_signature_field_names_.append(StripDigitsIfRequired(field->name));

      ++active_field_count_;
    }

    // Generate a unique name for this field by appending a counter to the name.
    // Make sure to prepend the counter with a non-numeric digit so that we are
    // guaranteed to avoid collisions.
    if (!unique_names.count(field->name))
      unique_names[field->name] = 1;
    else
      ++unique_names[field->name];
    base::string16 unique_name = field->name + base::ASCIIToUTF16("_") +
        base::IntToString16(unique_names[field->name]);
    fields_.push_back(new AutofillField(*field, unique_name));
  }
}

FormStructure::~FormStructure() {}

void FormStructure::DetermineHeuristicTypes(
    const AutofillMetrics& metric_logger) {
  // First, try to detect field types based on each field's |autocomplete|
  // attribute value.  If there is at least one form field that specifies an
  // autocomplete type hint, don't try to apply other heuristics to match fields
  // in this form.
  bool has_author_specified_sections;
  ParseFieldTypesFromAutocompleteAttributes(&has_author_specified_types_,
                                            &has_author_specified_sections);

  if (!has_author_specified_types_) {
    ServerFieldTypeMap field_type_map;
    FormField::ParseFormFields(fields_.get(), &field_type_map);
    for (size_t i = 0; i < field_count(); ++i) {
      AutofillField* field = fields_[i];
      ServerFieldTypeMap::iterator iter =
          field_type_map.find(field->unique_name());
      if (iter != field_type_map.end())
        field->set_heuristic_type(iter->second);
    }
  }

  UpdateAutofillCount();
  IdentifySections(has_author_specified_sections);

  if (IsAutofillable()) {
    metric_logger.LogDeveloperEngagementMetric(
        AutofillMetrics::FILLABLE_FORM_PARSED);
    if (has_author_specified_types_) {
      metric_logger.LogDeveloperEngagementMetric(
          AutofillMetrics::FILLABLE_FORM_CONTAINS_TYPE_HINTS);
    }
  }
}

bool FormStructure::EncodeUploadRequest(
    const ServerFieldTypeSet& available_field_types,
    bool form_was_autofilled,
    std::string* encoded_xml) const {
  DCHECK(ShouldBeCrowdsourced());

  // Verify that |available_field_types| agrees with the possible field types we
  // are uploading.
  for (std::vector<AutofillField*>::const_iterator field = begin();
       field != end();
       ++field) {
    for (ServerFieldTypeSet::const_iterator type =
             (*field)->possible_types().begin();
         type != (*field)->possible_types().end();
         ++type) {
      DCHECK(*type == UNKNOWN_TYPE ||
             *type == EMPTY_TYPE ||
             available_field_types.count(*type));
    }
  }

  // Set up the <autofillupload> element and its attributes.
  buzz::XmlElement autofill_request_xml(
      (buzz::QName(kXMLElementAutofillUpload)));
  autofill_request_xml.SetAttr(buzz::QName(kAttributeClientVersion),
                               kClientVersion);
  autofill_request_xml.SetAttr(buzz::QName(kAttributeFormSignature),
                               FormSignature());
  autofill_request_xml.SetAttr(buzz::QName(kAttributeAutofillUsed),
                               form_was_autofilled ? "true" : "false");
  autofill_request_xml.SetAttr(buzz::QName(kAttributeDataPresent),
                               EncodeFieldTypes(available_field_types).c_str());

  if (!EncodeFormRequest(FormStructure::UPLOAD, &autofill_request_xml))
    return false;  // Malformed form, skip it.

  // Obtain the XML structure as a string.
  *encoded_xml = kXMLDeclaration;
  *encoded_xml += autofill_request_xml.Str().c_str();

  // To enable this logging, run with the flag --vmodule="form_structure=2".
  VLOG(2) << "\n" << *encoded_xml;

  return true;
}

bool FormStructure::EncodeFieldAssignments(
    const ServerFieldTypeSet& available_field_types,
    std::string* encoded_xml) const {
  DCHECK(ShouldBeCrowdsourced());

  // Set up the <fieldassignments> element and its attributes.
  buzz::XmlElement autofill_request_xml(
      (buzz::QName(kXMLElementFieldAssignments)));
  autofill_request_xml.SetAttr(buzz::QName(kAttributeFormSignature),
                               FormSignature());

  if (!EncodeFormRequest(FormStructure::FIELD_ASSIGNMENTS,
                         &autofill_request_xml))
    return false;  // Malformed form, skip it.

  // Obtain the XML structure as a string.
  *encoded_xml = kXMLDeclaration;
  *encoded_xml += autofill_request_xml.Str().c_str();

  return true;
}

// static
bool FormStructure::EncodeQueryRequest(
    const std::vector<FormStructure*>& forms,
    std::vector<std::string>* encoded_signatures,
    std::string* encoded_xml) {
  DCHECK(encoded_signatures);
  DCHECK(encoded_xml);
  encoded_xml->clear();
  encoded_signatures->clear();
  encoded_signatures->reserve(forms.size());

  // Set up the <autofillquery> element and attributes.
  buzz::XmlElement autofill_request_xml(
      (buzz::QName(kXMLElementAutofillQuery)));
  autofill_request_xml.SetAttr(buzz::QName(kAttributeClientVersion),
                               kClientVersion);

  // Some badly formatted web sites repeat forms - detect that and encode only
  // one form as returned data would be the same for all the repeated forms.
  std::set<std::string> processed_forms;
  for (ScopedVector<FormStructure>::const_iterator it = forms.begin();
       it != forms.end();
       ++it) {
    std::string signature((*it)->FormSignature());
    if (processed_forms.find(signature) != processed_forms.end())
      continue;
    processed_forms.insert(signature);
    scoped_ptr<buzz::XmlElement> encompassing_xml_element(
        new buzz::XmlElement(buzz::QName(kXMLElementForm)));
    encompassing_xml_element->SetAttr(buzz::QName(kAttributeSignature),
                                      signature);

    if (!(*it)->EncodeFormRequest(FormStructure::QUERY,
                                  encompassing_xml_element.get()))
      continue;  // Malformed form, skip it.

    autofill_request_xml.AddElement(encompassing_xml_element.release());
    encoded_signatures->push_back(signature);
  }

  if (!encoded_signatures->size())
    return false;

  // Note: Chrome used to also set 'accepts="e"' (where 'e' is for experiments),
  // but no longer sets this because support for experiments is deprecated.  If
  // it ever resurfaces, re-add code here to set the attribute accordingly.

  // Obtain the XML structure as a string.
  *encoded_xml = kXMLDeclaration;
  *encoded_xml += autofill_request_xml.Str().c_str();

  return true;
}

// static
void FormStructure::ParseQueryResponse(
    const std::string& response_xml,
    const std::vector<FormStructure*>& forms,
    const AutofillMetrics& metric_logger) {
  metric_logger.LogServerQueryMetric(AutofillMetrics::QUERY_RESPONSE_RECEIVED);

  // Parse the field types from the server response to the query.
  std::vector<AutofillServerFieldInfo> field_infos;
  UploadRequired upload_required;
  AutofillQueryXmlParser parse_handler(&field_infos,
                                       &upload_required);
  buzz::XmlParser parser(&parse_handler);
  parser.Parse(response_xml.c_str(), response_xml.length(), true);
  if (!parse_handler.succeeded())
    return;

  metric_logger.LogServerQueryMetric(AutofillMetrics::QUERY_RESPONSE_PARSED);

  bool heuristics_detected_fillable_field = false;
  bool query_response_overrode_heuristics = false;

  // Copy the field types into the actual form.
  std::vector<AutofillServerFieldInfo>::iterator current_info =
      field_infos.begin();
  for (std::vector<FormStructure*>::const_iterator iter = forms.begin();
       iter != forms.end(); ++iter) {
    FormStructure* form = *iter;
    form->upload_required_ = upload_required;

    for (std::vector<AutofillField*>::iterator field = form->fields_.begin();
         field != form->fields_.end(); ++field) {
      if (form->ShouldSkipField(**field))
        continue;

      // In some cases *successful* response does not return all the fields.
      // Quit the update of the types then.
      if (current_info == field_infos.end())
        break;

      // UNKNOWN_TYPE is reserved for use by the client.
      DCHECK_NE(current_info->field_type, UNKNOWN_TYPE);

      ServerFieldType heuristic_type = (*field)->heuristic_type();
      if (heuristic_type != UNKNOWN_TYPE)
        heuristics_detected_fillable_field = true;

      (*field)->set_server_type(current_info->field_type);
      if (heuristic_type != (*field)->Type().GetStorableType())
        query_response_overrode_heuristics = true;

      // Copy default value into the field if available.
      if (!current_info->default_value.empty())
        (*field)->set_default_value(current_info->default_value);

      ++current_info;
    }

    form->UpdateAutofillCount();
    form->IdentifySections(false);
  }

  AutofillMetrics::ServerQueryMetric metric;
  if (query_response_overrode_heuristics) {
    if (heuristics_detected_fillable_field) {
      metric = AutofillMetrics::QUERY_RESPONSE_OVERRODE_LOCAL_HEURISTICS;
    } else {
      metric = AutofillMetrics::QUERY_RESPONSE_WITH_NO_LOCAL_HEURISTICS;
    }
  } else {
    metric = AutofillMetrics::QUERY_RESPONSE_MATCHED_LOCAL_HEURISTICS;
  }
  metric_logger.LogServerQueryMetric(metric);
}

// static
void FormStructure::GetFieldTypePredictions(
    const std::vector<FormStructure*>& form_structures,
    std::vector<FormDataPredictions>* forms) {
  forms->clear();
  forms->reserve(form_structures.size());
  for (size_t i = 0; i < form_structures.size(); ++i) {
    FormStructure* form_structure = form_structures[i];
    FormDataPredictions form;
    form.data.name = form_structure->form_name_;
    form.data.origin = form_structure->source_url_;
    form.data.action = form_structure->target_url_;
    form.signature = form_structure->FormSignature();

    for (std::vector<AutofillField*>::const_iterator field =
             form_structure->fields_.begin();
         field != form_structure->fields_.end(); ++field) {
      form.data.fields.push_back(FormFieldData(**field));

      FormFieldDataPredictions annotated_field;
      annotated_field.signature = (*field)->FieldSignature();
      annotated_field.heuristic_type =
          AutofillType((*field)->heuristic_type()).ToString();
      annotated_field.server_type =
          AutofillType((*field)->server_type()).ToString();
      annotated_field.overall_type = (*field)->Type().ToString();
      form.fields.push_back(annotated_field);
    }

    forms->push_back(form);
  }
}

std::string FormStructure::FormSignature() const {
  std::string scheme(target_url_.scheme());
  std::string host(target_url_.host());

  // If target host or scheme is empty, set scheme and host of source url.
  // This is done to match the Toolbar's behavior.
  if (scheme.empty() || host.empty()) {
    scheme = source_url_.scheme();
    host = source_url_.host();
  }

  std::string form_string = scheme + "://" + host + "&" +
                            base::UTF16ToUTF8(form_name_) +
                            form_signature_field_names_;

  return Hash64Bit(form_string);
}

bool FormStructure::ShouldSkipField(const FormFieldData& field) const {
  return field.is_checkable;
}

bool FormStructure::IsAutofillable() const {
  if (autofill_count() < kRequiredAutofillFields)
    return false;

  return ShouldBeParsed();
}

void FormStructure::UpdateAutofillCount() {
  autofill_count_ = 0;
  for (std::vector<AutofillField*>::const_iterator iter = begin();
       iter != end(); ++iter) {
    AutofillField* field = *iter;
    if (field && field->IsFieldFillable())
      ++autofill_count_;
  }
}

bool FormStructure::ShouldBeParsed() const {
  if (active_field_count() < kRequiredAutofillFields)
    return false;

  // Rule out http(s)://*/search?...
  //  e.g. http://www.google.com/search?q=...
  //       http://search.yahoo.com/search?p=...
  if (target_url_.path() == "/search")
    return false;

  bool has_text_field = false;
  for (std::vector<AutofillField*>::const_iterator it = begin();
       it != end() && !has_text_field; ++it) {
    has_text_field |= (*it)->form_control_type != "select-one";
  }

  return has_text_field;
}

bool FormStructure::ShouldBeCrowdsourced() const {
  return !has_author_specified_types_ && ShouldBeParsed();
}

void FormStructure::UpdateFromCache(const FormStructure& cached_form) {
  // Map from field signatures to cached fields.
  std::map<std::string, const AutofillField*> cached_fields;
  for (size_t i = 0; i < cached_form.field_count(); ++i) {
    const AutofillField* field = cached_form.field(i);
    cached_fields[field->FieldSignature()] = field;
  }

  for (std::vector<AutofillField*>::const_iterator iter = begin();
       iter != end(); ++iter) {
    AutofillField* field = *iter;

    std::map<std::string, const AutofillField*>::const_iterator
        cached_field = cached_fields.find(field->FieldSignature());
    if (cached_field != cached_fields.end()) {
      if (field->form_control_type != "select-one" &&
          field->value == cached_field->second->value) {
        // From the perspective of learning user data, text fields containing
        // default values are equivalent to empty fields.
        field->value = base::string16();
      }

      field->set_heuristic_type(cached_field->second->heuristic_type());
      field->set_server_type(cached_field->second->server_type());
    }
  }

  UpdateAutofillCount();

  // The form signature should match between query and upload requests to the
  // server. On many websites, form elements are dynamically added, removed, or
  // rearranged via JavaScript between page load and form submission, so we
  // copy over the |form_signature_field_names_| corresponding to the query
  // request.
  DCHECK_EQ(cached_form.form_name_, form_name_);
  DCHECK_EQ(cached_form.source_url_, source_url_);
  DCHECK_EQ(cached_form.target_url_, target_url_);
  form_signature_field_names_ = cached_form.form_signature_field_names_;
}

void FormStructure::LogQualityMetrics(
    const AutofillMetrics& metric_logger,
    const base::TimeTicks& load_time,
    const base::TimeTicks& interaction_time,
    const base::TimeTicks& submission_time) const {
  size_t num_detected_field_types = 0;
  bool did_autofill_all_possible_fields = true;
  bool did_autofill_some_possible_fields = false;
  for (size_t i = 0; i < field_count(); ++i) {
    const AutofillField* field = this->field(i);

    // No further logging for empty fields nor for fields where the entered data
    // does not appear to already exist in the user's stored Autofill data.
    const ServerFieldTypeSet& field_types = field->possible_types();
    DCHECK(!field_types.empty());
    if (field_types.count(EMPTY_TYPE) || field_types.count(UNKNOWN_TYPE))
      continue;

    // Similarly, no further logging for password fields.  Those are primarily
    // related to a different feature code path, and so make more sense to track
    // outside of this metric.
    if (field->form_control_type == "password")
      continue;

    ++num_detected_field_types;
    if (field->is_autofilled)
      did_autofill_some_possible_fields = true;
    else
      did_autofill_all_possible_fields = false;

    // Collapse field types that Chrome treats as identical, e.g. home and
    // billing address fields.
    ServerFieldTypeSet collapsed_field_types;
    for (ServerFieldTypeSet::const_iterator it = field_types.begin();
         it != field_types.end();
         ++it) {
      // Since we currently only support US phone numbers, the (city code + main
      // digits) number is almost always identical to the whole phone number.
      // TODO(isherman): Improve this logic once we add support for
      // international numbers.
      if (*it == PHONE_HOME_CITY_AND_NUMBER)
        collapsed_field_types.insert(PHONE_HOME_WHOLE_NUMBER);
      else
        collapsed_field_types.insert(AutofillType(*it).GetStorableType());
    }

    // Capture the field's type, if it is unambiguous.
    ServerFieldType field_type = UNKNOWN_TYPE;
    if (collapsed_field_types.size() == 1)
      field_type = *collapsed_field_types.begin();

    ServerFieldType heuristic_type =
        AutofillType(field->heuristic_type()).GetStorableType();
    ServerFieldType server_type =
        AutofillType(field->server_type()).GetStorableType();
    ServerFieldType predicted_type = field->Type().GetStorableType();

    // Log heuristic, server, and overall type quality metrics, independently of
    // whether the field was autofilled.
    if (heuristic_type == UNKNOWN_TYPE) {
      metric_logger.LogHeuristicTypePrediction(AutofillMetrics::TYPE_UNKNOWN,
                                               field_type);
    } else if (field_types.count(heuristic_type)) {
      metric_logger.LogHeuristicTypePrediction(AutofillMetrics::TYPE_MATCH,
                                               field_type);
    } else {
      metric_logger.LogHeuristicTypePrediction(AutofillMetrics::TYPE_MISMATCH,
                                               field_type);
    }

    if (server_type == NO_SERVER_DATA) {
      metric_logger.LogServerTypePrediction(AutofillMetrics::TYPE_UNKNOWN,
                                            field_type);
    } else if (field_types.count(server_type)) {
      metric_logger.LogServerTypePrediction(AutofillMetrics::TYPE_MATCH,
                                            field_type);
    } else {
      metric_logger.LogServerTypePrediction(AutofillMetrics::TYPE_MISMATCH,
                                            field_type);
    }

    if (predicted_type == UNKNOWN_TYPE) {
      metric_logger.LogOverallTypePrediction(AutofillMetrics::TYPE_UNKNOWN,
                                             field_type);
    } else if (field_types.count(predicted_type)) {
      metric_logger.LogOverallTypePrediction(AutofillMetrics::TYPE_MATCH,
                                             field_type);
    } else {
      metric_logger.LogOverallTypePrediction(AutofillMetrics::TYPE_MISMATCH,
                                             field_type);
    }
  }

  if (num_detected_field_types < kRequiredAutofillFields) {
    metric_logger.LogUserHappinessMetric(
        AutofillMetrics::SUBMITTED_NON_FILLABLE_FORM);
  } else {
    if (did_autofill_all_possible_fields) {
      metric_logger.LogUserHappinessMetric(
          AutofillMetrics::SUBMITTED_FILLABLE_FORM_AUTOFILLED_ALL);
    } else if (did_autofill_some_possible_fields) {
      metric_logger.LogUserHappinessMetric(
          AutofillMetrics::SUBMITTED_FILLABLE_FORM_AUTOFILLED_SOME);
    } else {
      metric_logger.LogUserHappinessMetric(
          AutofillMetrics::SUBMITTED_FILLABLE_FORM_AUTOFILLED_NONE);
    }

    // Unlike the other times, the |submission_time| should always be available.
    DCHECK(!submission_time.is_null());

    // The |load_time| might be unset, in the case that the form was dynamically
    // added to the DOM.
    if (!load_time.is_null()) {
      // Submission should always chronologically follow form load.
      DCHECK(submission_time > load_time);
      base::TimeDelta elapsed = submission_time - load_time;
      if (did_autofill_some_possible_fields)
        metric_logger.LogFormFillDurationFromLoadWithAutofill(elapsed);
      else
        metric_logger.LogFormFillDurationFromLoadWithoutAutofill(elapsed);
    }

    // The |interaction_time| might be unset, in the case that the user
    // submitted a blank form.
    if (!interaction_time.is_null()) {
      // Submission should always chronologically follow interaction.
      DCHECK(submission_time > interaction_time);
      base::TimeDelta elapsed = submission_time - interaction_time;
      if (did_autofill_some_possible_fields) {
        metric_logger.LogFormFillDurationFromInteractionWithAutofill(elapsed);
      } else {
        metric_logger.LogFormFillDurationFromInteractionWithoutAutofill(
            elapsed);
      }
    }
  }
}

const AutofillField* FormStructure::field(size_t index) const {
  if (index >= fields_.size()) {
    NOTREACHED();
    return NULL;
  }

  return fields_[index];
}

AutofillField* FormStructure::field(size_t index) {
  return const_cast<AutofillField*>(
      static_cast<const FormStructure*>(this)->field(index));
}

size_t FormStructure::field_count() const {
  return fields_.size();
}

size_t FormStructure::active_field_count() const {
  return active_field_count_;
}

FormData FormStructure::ToFormData() const {
  // |data.user_submitted| will always be false.
  FormData data;
  data.name = form_name_;
  data.origin = source_url_;
  data.action = target_url_;

  for (size_t i = 0; i < fields_.size(); ++i) {
    data.fields.push_back(FormFieldData(*fields_[i]));
  }

  return data;
}

bool FormStructure::operator==(const FormData& form) const {
  // TODO(jhawkins): Is this enough to differentiate a form?
  if (form_name_ == form.name &&
      source_url_ == form.origin &&
      target_url_ == form.action) {
    return true;
  }

  // TODO(jhawkins): Compare field names, IDs and labels once we have labels
  // set up.

  return false;
}

bool FormStructure::operator!=(const FormData& form) const {
  return !operator==(form);
}

std::string FormStructure::Hash64Bit(const std::string& str) {
  std::string hash_bin = base::SHA1HashString(str);
  DCHECK_EQ(20U, hash_bin.length());

  uint64 hash64 = (((static_cast<uint64>(hash_bin[0])) & 0xFF) << 56) |
                  (((static_cast<uint64>(hash_bin[1])) & 0xFF) << 48) |
                  (((static_cast<uint64>(hash_bin[2])) & 0xFF) << 40) |
                  (((static_cast<uint64>(hash_bin[3])) & 0xFF) << 32) |
                  (((static_cast<uint64>(hash_bin[4])) & 0xFF) << 24) |
                  (((static_cast<uint64>(hash_bin[5])) & 0xFF) << 16) |
                  (((static_cast<uint64>(hash_bin[6])) & 0xFF) << 8) |
                   ((static_cast<uint64>(hash_bin[7])) & 0xFF);

  return base::Uint64ToString(hash64);
}

bool FormStructure::EncodeFormRequest(
    FormStructure::EncodeRequestType request_type,
    buzz::XmlElement* encompassing_xml_element) const {
  if (!field_count())  // Nothing to add.
    return false;

  // Some badly formatted web sites repeat fields - limit number of fields to
  // 48, which is far larger than any valid form and XML still fits into 2K.
  // Do not send requests for forms with more than this many fields, as they are
  // near certainly not valid/auto-fillable.
  const size_t kMaxFieldsOnTheForm = 48;
  if (field_count() > kMaxFieldsOnTheForm)
    return false;

  // Add the child nodes for the form fields.
  for (size_t index = 0; index < field_count(); ++index) {
    const AutofillField* field = fields_[index];
    switch (request_type) {
      case FormStructure::UPLOAD:
        EncodeFieldForUpload(*field, encompassing_xml_element);
        break;
      case FormStructure::QUERY:
        if (ShouldSkipField(*field))
          continue;
        EncodeFieldForQuery(*field, encompassing_xml_element);
        break;
      case FormStructure::FIELD_ASSIGNMENTS:
        EncodeFieldForFieldAssignments(*field, encompassing_xml_element);
        break;
    }
  }
  return true;
}

void FormStructure::ParseFieldTypesFromAutocompleteAttributes(
    bool* found_types,
    bool* found_sections) {
  const std::string kDefaultSection = "-default";

  *found_types = false;
  *found_sections = false;
  for (std::vector<AutofillField*>::iterator it = fields_.begin();
       it != fields_.end(); ++it) {
    AutofillField* field = *it;

    // To prevent potential section name collisions, add a default suffix for
    // other fields.  Without this, 'autocomplete' attribute values
    // "section--shipping street-address" and "shipping street-address" would be
    // parsed identically, given the section handling code below.  We do this
    // before any validation so that fields with invalid attributes still end up
    // in the default section.  These default section names will be overridden
    // by subsequent heuristic parsing steps if there are no author-specified
    // section names.
    field->set_section(kDefaultSection);

    // Canonicalize the attribute value by trimming whitespace, collapsing
    // non-space characters (e.g. tab) to spaces, and converting to lowercase.
    std::string autocomplete_attribute =
        base::CollapseWhitespaceASCII(field->autocomplete_attribute, false);
    autocomplete_attribute = base::StringToLowerASCII(autocomplete_attribute);

    // The autocomplete attribute is overloaded: it can specify either a field
    // type hint or whether autocomplete should be enabled at all.  Ignore the
    // latter type of attribute value.
    if (autocomplete_attribute.empty() ||
        autocomplete_attribute == "on" ||
        autocomplete_attribute == "off") {
      continue;
    }

    // Any other value, even it is invalid, is considered to be a type hint.
    // This allows a website's author to specify an attribute like
    // autocomplete="other" on a field to disable all Autofill heuristics for
    // the form.
    *found_types = true;

    // Tokenize the attribute value.  Per the spec, the tokens are parsed in
    // reverse order.
    std::vector<std::string> tokens;
    Tokenize(autocomplete_attribute, " ", &tokens);

    // The final token must be the field type.
    // If it is not one of the known types, abort.
    DCHECK(!tokens.empty());
    std::string field_type_token = tokens.back();
    tokens.pop_back();
    HtmlFieldType field_type =
        FieldTypeFromAutocompleteAttributeValue(field_type_token, *field);
    if (field_type == HTML_TYPE_UNKNOWN)
      continue;

    // The preceding token, if any, may be a type hint.
    if (!tokens.empty() && IsContactTypeHint(tokens.back())) {
      // If it is, it must match the field type; otherwise, abort.
      // Note that an invalid token invalidates the entire attribute value, even
      // if the other tokens are valid.
      if (!ContactTypeHintMatchesFieldType(tokens.back(), field_type))
        continue;

      // Chrome Autofill ignores these type hints.
      tokens.pop_back();
    }

    // The preceding token, if any, may be a fixed string that is either
    // "shipping" or "billing".  Chrome Autofill treats these as implicit
    // section name suffixes.
    DCHECK_EQ(kDefaultSection, field->section());
    std::string section = field->section();
    HtmlFieldMode mode = HTML_MODE_NONE;
    if (!tokens.empty()) {
      if (tokens.back() == kShippingMode)
        mode = HTML_MODE_SHIPPING;
      else if (tokens.back() == kBillingMode)
        mode = HTML_MODE_BILLING;
    }

    if (mode != HTML_MODE_NONE) {
      section = "-" + tokens.back();
      tokens.pop_back();
    }

    // The preceding token, if any, may be a named section.
    const std::string kSectionPrefix = "section-";
    if (!tokens.empty() &&
        StartsWithASCII(tokens.back(), kSectionPrefix, true)) {
      // Prepend this section name to the suffix set in the preceding block.
      section = tokens.back().substr(kSectionPrefix.size()) + section;
      tokens.pop_back();
    }

    // No other tokens are allowed.  If there are any remaining, abort.
    if (!tokens.empty())
      continue;

    if (section != kDefaultSection) {
      *found_sections = true;
      field->set_section(section);
    }

    // No errors encountered while parsing!
    // Update the |field|'s type based on what was parsed from the attribute.
    field->SetHtmlType(field_type, mode);
  }
}

bool FormStructure::FillFields(
    const std::vector<ServerFieldType>& types,
    const InputFieldComparator& matches,
    const base::Callback<base::string16(const AutofillType&)>& get_info,
    const std::string& address_language_code,
    const std::string& app_locale) {
  bool filled_something = false;
  for (size_t i = 0; i < field_count(); ++i) {
    for (size_t j = 0; j < types.size(); ++j) {
      if (matches.Run(types[j], *field(i))) {
        AutofillField::FillFormField(*field(i),
                                     get_info.Run(field(i)->Type()),
                                     address_language_code,
                                     app_locale,
                                     field(i));
        filled_something = true;
        break;
      }
    }
  }
  return filled_something;
}

std::set<base::string16> FormStructure::PossibleValues(ServerFieldType type) {
  std::set<base::string16> values;
  AutofillType target_type(type);
  for (std::vector<AutofillField*>::iterator iter = fields_.begin();
       iter != fields_.end(); ++iter) {
    AutofillField* field = *iter;
    if (field->Type().GetStorableType() != target_type.GetStorableType() ||
        field->Type().group() != target_type.group()) {
      continue;
    }

    // No option values; anything goes.
    if (field->option_values.empty())
      return std::set<base::string16>();

    for (size_t i = 0; i < field->option_values.size(); ++i) {
      if (!field->option_values[i].empty())
        values.insert(base::i18n::ToUpper(field->option_values[i]));
    }

    for (size_t i = 0; i < field->option_contents.size(); ++i) {
      if (!field->option_contents[i].empty())
        values.insert(base::i18n::ToUpper(field->option_contents[i]));
    }
  }

  return values;
}

base::string16 FormStructure::GetUniqueValue(HtmlFieldType type) const {
  base::string16 value;
  for (std::vector<AutofillField*>::const_iterator iter = fields_.begin();
       iter != fields_.end(); ++iter) {
    const AutofillField* field = *iter;
    if (field->html_type() != type)
      continue;

    // More than one value found; abort rather than choosing one arbitrarily.
    if (!value.empty() && !field->value.empty())
      return base::string16();

    value = field->value;
  }

  return value;
}

void FormStructure::IdentifySections(bool has_author_specified_sections) {
  if (fields_.empty())
    return;

  if (!has_author_specified_sections) {
    // Name sections after the first field in the section.
    base::string16 current_section = fields_.front()->unique_name();

    // Keep track of the types we've seen in this section.
    std::set<ServerFieldType> seen_types;
    ServerFieldType previous_type = UNKNOWN_TYPE;

    for (std::vector<AutofillField*>::iterator field = fields_.begin();
         field != fields_.end(); ++field) {
      const ServerFieldType current_type = (*field)->Type().GetStorableType();

      bool already_saw_current_type = seen_types.count(current_type) > 0;

      // Forms often ask for multiple phone numbers -- e.g. both a daytime and
      // evening phone number.  Our phone number detection is also generally a
      // little off.  Hence, ignore this field type as a signal here.
      if (AutofillType(current_type).group() == PHONE_HOME)
        already_saw_current_type = false;

      // Some forms have adjacent fields of the same type.  Two common examples:
      //  * Forms with two email fields, where the second is meant to "confirm"
      //    the first.
      //  * Forms with a <select> menu for states in some countries, and a
      //    freeform <input> field for states in other countries.  (Usually,
      //    only one of these two will be visible for any given choice of
      //    country.)
      // Generally, adjacent fields of the same type belong in the same logical
      // section.
      if (current_type == previous_type)
        already_saw_current_type = false;

      previous_type = current_type;

      if (current_type != UNKNOWN_TYPE && already_saw_current_type) {
        // We reached the end of a section, so start a new section.
        seen_types.clear();
        current_section = (*field)->unique_name();
      }

      seen_types.insert(current_type);
      (*field)->set_section(base::UTF16ToUTF8(current_section));
    }
  }

  // Ensure that credit card and address fields are in separate sections.
  // This simplifies the section-aware logic in autofill_manager.cc.
  for (std::vector<AutofillField*>::iterator field = fields_.begin();
       field != fields_.end(); ++field) {
    FieldTypeGroup field_type_group = (*field)->Type().group();
    if (field_type_group == CREDIT_CARD)
      (*field)->set_section((*field)->section() + "-cc");
    else
      (*field)->set_section((*field)->section() + "-default");
  }
}

}  // namespace autofill
