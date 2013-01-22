// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/form_structure.h"

#include <utility>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/sha1.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/browser/autofill/autofill_metrics.h"
#include "chrome/browser/autofill/autofill_type.h"
#include "chrome/browser/autofill/autofill_xml_parser.h"
#include "chrome/browser/autofill/field_types.h"
#include "chrome/browser/autofill/form_field.h"
#include "chrome/common/form_data.h"
#include "chrome/common/form_data_predictions.h"
#include "chrome/common/form_field_data.h"
#include "chrome/common/form_field_data_predictions.h"
#include "third_party/libjingle/source/talk/xmllite/xmlelement.h"

namespace {

const char kFormMethodPost[] = "post";

// XML elements and attributes.
const char kAttributeAcceptedFeatures[] = "accepts";
const char kAttributeAutofillUsed[] = "autofillused";
const char kAttributeAutofillType[] = "autofilltype";
const char kAttributeClientVersion[] = "clientversion";
const char kAttributeDataPresent[] = "datapresent";
const char kAttributeFormSignature[] = "formsignature";
const char kAttributeSignature[] = "signature";
const char kAcceptedFeatures[] = "e"; // e=experiments
const char kClientVersion[] = "6.1.1715.1442/en (GGLL)";
const char kXMLDeclaration[] = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>";
const char kXMLElementAutofillQuery[] = "autofillquery";
const char kXMLElementAutofillUpload[] = "autofillupload";
const char kXMLElementForm[] = "form";
const char kXMLElementField[] = "field";

// The number of fillable fields necessary for a form to be fillable.
const size_t kRequiredFillableFields = 3;

// Helper for |EncodeUploadRequest()| that creates a bit field corresponding to
// |available_field_types| and returns the hex representation as a string.
std::string EncodeFieldTypes(const FieldTypeSet& available_field_types) {
  // There are |MAX_VALID_FIELD_TYPE| different field types and 8 bits per byte,
  // so we need ceil(MAX_VALID_FIELD_TYPE / 8) bytes to encode the bit field.
  const size_t kNumBytes = (MAX_VALID_FIELD_TYPE + 0x7) / 8;

  // Pack the types in |available_field_types| into |bit_field|.
  std::vector<uint8> bit_field(kNumBytes, 0);
  for (FieldTypeSet::const_iterator field_type = available_field_types.begin();
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
                                     AutofillFieldType field_type) {
  // The "home" and "work" type hints are only appropriate for email and phone
  // number field types.
  if (token == "home" || token == "work") {
    return field_type == EMAIL_ADDRESS ||
        (field_type >= PHONE_HOME_NUMBER &&
         field_type <= PHONE_HOME_WHOLE_NUMBER);
  }

  // The "mobile" type hint is only appropriate for phone number field types.
  // Note that "fax" and "pager" are intentionally ignored, as Chrome does not
  // support filling either type of information.
  if (token == "mobile") {
    return field_type >= PHONE_HOME_NUMBER &&
        field_type <= PHONE_HOME_WHOLE_NUMBER;
  }

  return false;
}

// Returns the Chrome Autofill-supported field type corresponding to the given
// |autocomplete_type|, if there is one, in the context of the given |field|.
// Chrome Autofill supports a subset of the field types listed at
// http://is.gd/whatwg_autocomplete
AutofillFieldType FieldTypeFromAutocompleteType(
    const std::string& autocomplete_type,
    const AutofillField& field) {
  if (autocomplete_type == "name")
    return NAME_FULL;

  if (autocomplete_type == "given-name")
    return NAME_FIRST;

  if (autocomplete_type == "additional-name") {
    if (field.max_length == 1)
      return NAME_MIDDLE_INITIAL;
    else
      return NAME_MIDDLE;
  }

  if (autocomplete_type == "family-name")
    return NAME_LAST;

  if (autocomplete_type == "honorific-suffix")
    return NAME_SUFFIX;

  if (autocomplete_type == "organization")
    return COMPANY_NAME;

  if (autocomplete_type == "street-address" ||
      autocomplete_type == "address-line1")
    return ADDRESS_HOME_LINE1;

  if (autocomplete_type == "address-line2")
    return ADDRESS_HOME_LINE2;

  if (autocomplete_type == "locality")
    return ADDRESS_HOME_CITY;

  if (autocomplete_type == "region")
    return ADDRESS_HOME_STATE;

  if (autocomplete_type == "country")
    return ADDRESS_HOME_COUNTRY;

  if (autocomplete_type == "postal-code")
    return ADDRESS_HOME_ZIP;

  if (autocomplete_type == "cc-name")
    return CREDIT_CARD_NAME;

  if (autocomplete_type == "cc-number")
    return CREDIT_CARD_NUMBER;

  if (autocomplete_type == "cc-exp") {
    if (field.max_length == 5)
      return CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR;
    else
      return CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR;
  }

  if (autocomplete_type == "cc-exp-month")
    return CREDIT_CARD_EXP_MONTH;

  if (autocomplete_type == "cc-exp-year") {
    if (field.max_length == 2)
      return CREDIT_CARD_EXP_2_DIGIT_YEAR;
    else
      return CREDIT_CARD_EXP_4_DIGIT_YEAR;
  }

  if (autocomplete_type == "cc-csc")
    return CREDIT_CARD_VERIFICATION_CODE;

  if (autocomplete_type == "tel")
    return PHONE_HOME_WHOLE_NUMBER;

  if (autocomplete_type == "tel-country-code")
    return PHONE_HOME_COUNTRY_CODE;

  if (autocomplete_type == "tel-national")
    return PHONE_HOME_CITY_AND_NUMBER;

  if (autocomplete_type == "tel-area-code")
    return PHONE_HOME_CITY_CODE;

  if (autocomplete_type == "tel-local")
    return PHONE_HOME_NUMBER;

  if (autocomplete_type == "tel-local-prefix")
    return PHONE_HOME_NUMBER;

  if (autocomplete_type == "tel-local-suffix")
    return PHONE_HOME_NUMBER;

  if (autocomplete_type == "email")
    return EMAIL_ADDRESS;

  return UNKNOWN_TYPE;
}

}  // namespace

FormStructure::FormStructure(const FormData& form)
    : form_name_(form.name),
      source_url_(form.origin),
      target_url_(form.action),
      autofill_count_(0),
      checkable_field_count_(0),
      upload_required_(USE_UPLOAD_RATES),
      server_experiment_id_("no server response"),
      current_page_number_(-1),
      total_pages_(-1),
      has_author_specified_types_(false),
      experimental_form_filling_enabled_(
          CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kEnableExperimentalFormFilling)) {
  // Copy the form fields.
  std::map<string16, size_t> unique_names;
  for (std::vector<FormFieldData>::const_iterator field =
           form.fields.begin();
       field != form.fields.end(); field++) {
    // Skipping checkable elements when flag is not set, else these fields will
    // interfere with existing field signatures with Autofill servers.
    // TODO(ramankk): Add checkable elements only on whitelisted pages
    if (!field->is_checkable || experimental_form_filling_enabled_) {
      // Add all supported form fields (including with empty names) to the
      // signature.  This is a requirement for Autofill servers.
      form_signature_field_names_.append("&");
      form_signature_field_names_.append(UTF16ToUTF8(field->name));
    }

    // Generate a unique name for this field by appending a counter to the name.
    // Make sure to prepend the counter with a non-numeric digit so that we are
    // guaranteed to avoid collisions.
    if (!unique_names.count(field->name))
      unique_names[field->name] = 1;
    else
      ++unique_names[field->name];
    string16 unique_name = field->name + ASCIIToUTF16("_") +
        base::IntToString16(unique_names[field->name]);
    fields_.push_back(new AutofillField(*field, unique_name));

    if (field->is_checkable)
      ++checkable_field_count_;
  }

  std::string method = UTF16ToUTF8(form.method);
  if (StringToLowerASCII(method) == kFormMethodPost) {
    method_ = POST;
  } else {
    // Either the method is 'get', or we don't know.  In this case we default
    // to GET.
    method_ = GET;
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
    FieldTypeMap field_type_map;
    FormField::ParseFormFields(fields_.get(), &field_type_map);
    for (size_t index = 0; index < field_count(); index++) {
      AutofillField* field = fields_[index];
      FieldTypeMap::iterator iter = field_type_map.find(field->unique_name());
      if (iter != field_type_map.end())
        field->set_heuristic_type(iter->second);
    }
  }

  UpdateAutofillCount();
  IdentifySections(has_author_specified_sections);

  if (IsAutofillable(true)) {
    metric_logger.LogDeveloperEngagementMetric(
        AutofillMetrics::FILLABLE_FORM_PARSED);
    if (has_author_specified_types_) {
      metric_logger.LogDeveloperEngagementMetric(
          AutofillMetrics::FILLABLE_FORM_CONTAINS_TYPE_HINTS);
    }
  }
}

bool FormStructure::EncodeUploadRequest(
    const FieldTypeSet& available_field_types,
    bool form_was_autofilled,
    std::string* encoded_xml) const {
  if (!ShouldBeCrowdsourced()) {
    NOTREACHED();
    return false;
  }

  // Verify that |available_field_types| agrees with the possible field types we
  // are uploading.
  for (std::vector<AutofillField*>::const_iterator field = begin();
       field != end();
       ++field) {
    for (FieldTypeSet::const_iterator type = (*field)->possible_types().begin();
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
  encoded_xml->clear();
  *encoded_xml = kXMLDeclaration;
  *encoded_xml += autofill_request_xml.Str().c_str();

  // To enable this logging, run with the flag --vmodule="form_structure=2".
  VLOG(2) << "\n" << *encoded_xml;

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
  autofill_request_xml.SetAttr(buzz::QName(kAttributeAcceptedFeatures),
                               kAcceptedFeatures);

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

  // Obtain the XML structure as a string.
  *encoded_xml = kXMLDeclaration;
  *encoded_xml += autofill_request_xml.Str().c_str();

  return true;
}

// static
void FormStructure::ParseQueryResponse(const std::string& response_xml,
                                       const std::vector<FormStructure*>& forms,
                                       const AutofillMetrics& metric_logger) {
  metric_logger.LogServerQueryMetric(AutofillMetrics::QUERY_RESPONSE_RECEIVED);

  // Parse the field types from the server response to the query.
  std::vector<AutofillServerFieldInfo> field_infos;
  UploadRequired upload_required;
  std::string experiment_id;
  AutofillQueryXmlParser parse_handler(&field_infos, &upload_required,
                                       &experiment_id);
  buzz::XmlParser parser(&parse_handler);
  parser.Parse(response_xml.c_str(), response_xml.length(), true);
  if (!parse_handler.succeeded())
    return;

  metric_logger.LogServerQueryMetric(AutofillMetrics::QUERY_RESPONSE_PARSED);
  metric_logger.LogServerExperimentIdForQuery(experiment_id);

  bool heuristics_detected_fillable_field = false;
  bool query_response_overrode_heuristics = false;

  // Copy the field types into the actual form.
  std::vector<AutofillServerFieldInfo>::iterator current_info =
      field_infos.begin();
  for (std::vector<FormStructure*>::const_iterator iter = forms.begin();
       iter != forms.end(); ++iter) {
    FormStructure* form = *iter;
    form->upload_required_ = upload_required;
    form->server_experiment_id_ = experiment_id;
    form->current_page_number_ = parse_handler.current_page_number();
    form->total_pages_ = parse_handler.total_pages();

    for (std::vector<AutofillField*>::iterator field = form->fields_.begin();
         field != form->fields_.end(); ++field, ++current_info) {
      // In some cases *successful* response does not return all the fields.
      // Quit the update of the types then.
      if (current_info == field_infos.end())
        break;

      // UNKNOWN_TYPE is reserved for use by the client.
      DCHECK_NE(current_info->field_type, UNKNOWN_TYPE);

      AutofillFieldType heuristic_type = (*field)->type();
      if (heuristic_type != UNKNOWN_TYPE)
        heuristics_detected_fillable_field = true;

      (*field)->set_server_type(current_info->field_type);
      if (heuristic_type != (*field)->type())
        query_response_overrode_heuristics = true;

      // Copy default value into the field if available.
      if (!current_info->default_value.empty())
        (*field)->set_default_value(current_info->default_value);
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
    form.data.method =
        ASCIIToUTF16((form_structure->method_ == POST) ? "POST" : "GET");
    form.data.origin = form_structure->source_url_;
    form.data.action = form_structure->target_url_;
    form.signature = form_structure->FormSignature();
    form.experiment_id = form_structure->server_experiment_id_;

    for (std::vector<AutofillField*>::const_iterator field =
             form_structure->fields_.begin();
         field != form_structure->fields_.end(); ++field) {
      form.data.fields.push_back(FormFieldData(**field));

      FormFieldDataPredictions annotated_field;
      annotated_field.signature = (*field)->FieldSignature();
      annotated_field.heuristic_type =
          AutofillType::FieldTypeToString((*field)->heuristic_type());
      annotated_field.server_type =
          AutofillType::FieldTypeToString((*field)->server_type());
      annotated_field.overall_type =
          AutofillType::FieldTypeToString((*field)->type());
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
                            UTF16ToUTF8(form_name_) +
                            form_signature_field_names_;

  return Hash64Bit(form_string);
}

bool FormStructure::IsAutofillable(bool require_method_post) const {
  // TODO(ramankk): Remove this check once we have better way of identifying the
  // cases to trigger experimental form filling.
  if (experimental_form_filling_enabled_)
    return true;

  if (autofill_count() < kRequiredFillableFields)
    return false;

  return ShouldBeParsed(require_method_post);
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

bool FormStructure::ShouldBeParsed(bool require_method_post) const {
  // TODO(ramankk): Remove this check once we have better way of identifying the
  // cases to trigger experimental form filling.
  if (experimental_form_filling_enabled_)
    return true;

  // Ignore counting checkable elements towards minimum number of elements
  // required to parse. This avoids trying to crowdsource forms with few text
  // or select elements.
  if ((field_count() - checkable_field_count()) < kRequiredFillableFields)
    return false;

  // Rule out http(s)://*/search?...
  //  e.g. http://www.google.com/search?q=...
  //       http://search.yahoo.com/search?p=...
  if (target_url_.path() == "/search")
    return false;

  // Make sure there as at least one text field.
  bool has_text_field = false;
  for (std::vector<AutofillField*>::const_iterator it = begin();
       it != end() && !has_text_field; ++it) {
    has_text_field |= (*it)->form_control_type != "select-one";
  }
  if (!has_text_field)
    return false;

  return !require_method_post || (method_ == POST);
}

bool FormStructure::ShouldBeCrowdsourced() const {
  return !has_author_specified_types_ && ShouldBeParsed(true);
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
        field->value = string16();
      }

      field->set_heuristic_type(cached_field->second->heuristic_type());
      field->set_server_type(cached_field->second->server_type());
    }
  }

  UpdateAutofillCount();

  server_experiment_id_ = cached_form.server_experiment_id();

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
  std::string experiment_id = server_experiment_id();
  metric_logger.LogServerExperimentIdForUpload(experiment_id);

  size_t num_detected_field_types = 0;
  bool did_autofill_all_possible_fields = true;
  bool did_autofill_some_possible_fields = false;
  for (size_t i = 0; i < field_count(); ++i) {
    const AutofillField* field = this->field(i);
    metric_logger.LogQualityMetric(AutofillMetrics::FIELD_SUBMITTED,
                                   experiment_id);

    // No further logging for empty fields nor for fields where the entered data
    // does not appear to already exist in the user's stored Autofill data.
    const FieldTypeSet& field_types = field->possible_types();
    DCHECK(!field_types.empty());
    if (field_types.count(EMPTY_TYPE) || field_types.count(UNKNOWN_TYPE))
      continue;

    ++num_detected_field_types;
    if (field->is_autofilled)
      did_autofill_some_possible_fields = true;
    else
      did_autofill_all_possible_fields = false;

    // Collapse field types that Chrome treats as identical, e.g. home and
    // billing address fields.
    FieldTypeSet collapsed_field_types;
    for (FieldTypeSet::const_iterator it = field_types.begin();
         it != field_types.end();
         ++it) {
      // Since we currently only support US phone numbers, the (city code + main
      // digits) number is almost always identical to the whole phone number.
      // TODO(isherman): Improve this logic once we add support for
      // international numbers.
      if (*it == PHONE_HOME_CITY_AND_NUMBER)
        collapsed_field_types.insert(PHONE_HOME_WHOLE_NUMBER);
      else
        collapsed_field_types.insert(AutofillType::GetEquivalentFieldType(*it));
    }

    // Capture the field's type, if it is unambiguous.
    AutofillFieldType field_type = UNKNOWN_TYPE;
    if (collapsed_field_types.size() == 1)
      field_type = *collapsed_field_types.begin();

    AutofillFieldType heuristic_type = field->heuristic_type();
    AutofillFieldType server_type = field->server_type();
    AutofillFieldType predicted_type = field->type();

    // Log heuristic, server, and overall type quality metrics, independently of
    // whether the field was autofilled.
    if (heuristic_type == UNKNOWN_TYPE) {
      metric_logger.LogHeuristicTypePrediction(AutofillMetrics::TYPE_UNKNOWN,
                                               field_type, experiment_id);
    } else if (field_types.count(heuristic_type)) {
      metric_logger.LogHeuristicTypePrediction(AutofillMetrics::TYPE_MATCH,
                                               field_type, experiment_id);
    } else {
      metric_logger.LogHeuristicTypePrediction(AutofillMetrics::TYPE_MISMATCH,
                                               field_type, experiment_id);
    }

    if (server_type == NO_SERVER_DATA) {
      metric_logger.LogServerTypePrediction(AutofillMetrics::TYPE_UNKNOWN,
                                            field_type, experiment_id);
    } else if (field_types.count(server_type)) {
      metric_logger.LogServerTypePrediction(AutofillMetrics::TYPE_MATCH,
                                            field_type, experiment_id);
    } else {
      metric_logger.LogServerTypePrediction(AutofillMetrics::TYPE_MISMATCH,
                                            field_type, experiment_id);
    }

    if (predicted_type == UNKNOWN_TYPE) {
      metric_logger.LogOverallTypePrediction(AutofillMetrics::TYPE_UNKNOWN,
                                             field_type, experiment_id);
    } else if (field_types.count(predicted_type)) {
      metric_logger.LogOverallTypePrediction(AutofillMetrics::TYPE_MATCH,
                                             field_type, experiment_id);
    } else {
      metric_logger.LogOverallTypePrediction(AutofillMetrics::TYPE_MISMATCH,
                                             field_type, experiment_id);
    }

    // TODO(isherman): <select> fields don't support |is_autofilled()|, so we
    // have to skip them for the remaining metrics.
    if (field->form_control_type == "select-one")
      continue;

    if (field->is_autofilled) {
      metric_logger.LogQualityMetric(AutofillMetrics::FIELD_AUTOFILLED,
                                     experiment_id);
    } else {
      metric_logger.LogQualityMetric(AutofillMetrics::FIELD_NOT_AUTOFILLED,
                                     experiment_id);

      if (heuristic_type == UNKNOWN_TYPE) {
        metric_logger.LogQualityMetric(
            AutofillMetrics::NOT_AUTOFILLED_HEURISTIC_TYPE_UNKNOWN,
            experiment_id);
      } else if (field_types.count(heuristic_type)) {
        metric_logger.LogQualityMetric(
            AutofillMetrics::NOT_AUTOFILLED_HEURISTIC_TYPE_MATCH,
            experiment_id);
      } else {
        metric_logger.LogQualityMetric(
            AutofillMetrics::NOT_AUTOFILLED_HEURISTIC_TYPE_MISMATCH,
            experiment_id);
      }

      if (server_type == NO_SERVER_DATA) {
        metric_logger.LogQualityMetric(
            AutofillMetrics::NOT_AUTOFILLED_SERVER_TYPE_UNKNOWN,
            experiment_id);
      } else if (field_types.count(server_type)) {
        metric_logger.LogQualityMetric(
            AutofillMetrics::NOT_AUTOFILLED_SERVER_TYPE_MATCH,
            experiment_id);
      } else {
        metric_logger.LogQualityMetric(
            AutofillMetrics::NOT_AUTOFILLED_SERVER_TYPE_MISMATCH,
            experiment_id);
      }
    }
  }

  if (num_detected_field_types < kRequiredFillableFields) {
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

size_t FormStructure::checkable_field_count() const {
  return checkable_field_count_;
}

std::string FormStructure::server_experiment_id() const {
  return server_experiment_id_;
}

FormData FormStructure::ToFormData() const {
  // |data.user_submitted| will always be false.
  FormData data;
  data.name = form_name_;
  data.origin = source_url_;
  data.action = target_url_;
  data.method = ASCIIToUTF16(method_ == POST ? "POST" : "GET");

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
    if (request_type == FormStructure::UPLOAD) {
      // Don't upload checkable fields.
      if (field->is_checkable)
        continue;

      FieldTypeSet types = field->possible_types();
      // |types| could be empty in unit-tests only.
      for (FieldTypeSet::iterator field_type = types.begin();
           field_type != types.end(); ++field_type) {
        buzz::XmlElement *field_element = new buzz::XmlElement(
            buzz::QName(kXMLElementField));

        field_element->SetAttr(buzz::QName(kAttributeSignature),
                               field->FieldSignature());
        field_element->SetAttr(buzz::QName(kAttributeAutofillType),
                               base::IntToString(*field_type));
        encompassing_xml_element->AddElement(field_element);
      }
    } else {
      // Skip putting checkable fields in the request if the flag is not set.
      if (field->is_checkable && !experimental_form_filling_enabled_)
        continue;

      buzz::XmlElement *field_element = new buzz::XmlElement(
          buzz::QName(kXMLElementField));
      field_element->SetAttr(buzz::QName(kAttributeSignature),
                             field->FieldSignature());
      encompassing_xml_element->AddElement(field_element);
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
        CollapseWhitespaceASCII(field->autocomplete_attribute, false);
    autocomplete_attribute = StringToLowerASCII(autocomplete_attribute);

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
    AutofillFieldType field_type =
        FieldTypeFromAutocompleteType(field_type_token, *field);
    if (field_type == UNKNOWN_TYPE)
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
    if (!tokens.empty() &&
        (tokens.back() == "shipping" || tokens.back() == "billing")) {
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
    field->set_heuristic_type(field_type);
    if (field_type_token == "tel-local-prefix")
      field->set_phone_part(AutofillField::PHONE_PREFIX);
    else if (field_type_token == "tel-local-suffix")
      field->set_phone_part(AutofillField::PHONE_SUFFIX);
  }
}

bool FormStructure::IsStartOfAutofillableFlow() const {
  return current_page_number_ == 0 && total_pages_ > 0;
}

bool FormStructure::IsInAutofillableFlow() const {
  return current_page_number_ >= 0 && current_page_number_ < total_pages_;
}

void FormStructure::IdentifySections(bool has_author_specified_sections) {
  if (fields_.empty())
    return;

  if (!has_author_specified_sections) {
    // Name sections after the first field in the section.
    string16 current_section = fields_.front()->unique_name();

    // Keep track of the types we've seen in this section.
    std::set<AutofillFieldType> seen_types;
    AutofillFieldType previous_type = UNKNOWN_TYPE;

    for (std::vector<AutofillField*>::iterator field = fields_.begin();
         field != fields_.end(); ++field) {
      const AutofillFieldType current_type =
          AutofillType::GetEquivalentFieldType((*field)->type());

      bool already_saw_current_type = seen_types.count(current_type) > 0;

      // Forms often ask for multiple phone numbers -- e.g. both a daytime and
      // evening phone number.  Our phone number detection is also generally a
      // little off.  Hence, ignore this field type as a signal here.
      if (AutofillType(current_type).group() == AutofillType::PHONE)
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
      (*field)->set_section(UTF16ToUTF8(current_section));
    }
  }

  // Ensure that credit card and address fields are in separate sections.
  // This simplifies the section-aware logic in autofill_manager.cc.
  for (std::vector<AutofillField*>::iterator field = fields_.begin();
       field != fields_.end(); ++field) {
    AutofillType::FieldTypeGroup field_type_group =
        AutofillType((*field)->type()).group();
    if (field_type_group == AutofillType::CREDIT_CARD)
      (*field)->set_section((*field)->section() + "-cc");
    else
      (*field)->set_section((*field)->section() + "-default");
  }
}
