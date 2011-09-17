// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/form_structure.h"

#include <utility>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/sha1.h"
#include "base/stringprintf.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_metrics.h"
#include "chrome/browser/autofill/autofill_type.h"
#include "chrome/browser/autofill/autofill_xml_parser.h"
#include "chrome/browser/autofill/field_types.h"
#include "chrome/browser/autofill/form_field.h"
#include "third_party/libjingle/source/talk/xmllite/xmlelement.h"
#include "webkit/glue/form_data.h"
#include "webkit/glue/form_data_predictions.h"
#include "webkit/glue/form_field.h"
#include "webkit/glue/form_field_predictions.h"

using webkit_glue::FormData;
using webkit_glue::FormDataPredictions;
using webkit_glue::FormFieldPredictions;

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

bool UpdateFromAutocompleteType(const string16& autocomplete_type,
                                AutofillField* field) {
  if (autocomplete_type == ASCIIToUTF16("given-name")) {
    field->set_heuristic_type(NAME_FIRST);
    return true;
  }

  if (autocomplete_type == ASCIIToUTF16("middle-name")) {
    field->set_heuristic_type(NAME_MIDDLE);
    return true;
  }

  if (autocomplete_type == ASCIIToUTF16("middle-initial")) {
    field->set_heuristic_type(NAME_MIDDLE_INITIAL);
    return true;
  }

  if (autocomplete_type == ASCIIToUTF16("surname")) {
    field->set_heuristic_type(NAME_LAST);
    return true;
  }

  if (autocomplete_type == ASCIIToUTF16("full-name")) {
    field->set_heuristic_type(NAME_FULL);
    return true;
  }

  if (autocomplete_type == ASCIIToUTF16("street-address") ||
      autocomplete_type == ASCIIToUTF16("address-line1")) {
    field->set_heuristic_type(ADDRESS_HOME_LINE1);
    return true;
  }

  if (autocomplete_type == ASCIIToUTF16("address-line2")) {
    field->set_heuristic_type(ADDRESS_HOME_LINE2);
    return true;
  }

  if (autocomplete_type == ASCIIToUTF16("locality") ||
      autocomplete_type == ASCIIToUTF16("city")) {
    field->set_heuristic_type(ADDRESS_HOME_CITY);
    return true;
  }

  if (autocomplete_type == ASCIIToUTF16("administrative-area") ||
      autocomplete_type == ASCIIToUTF16("state") ||
      autocomplete_type == ASCIIToUTF16("province") ||
      autocomplete_type == ASCIIToUTF16("region")) {
    field->set_heuristic_type(ADDRESS_HOME_STATE);
    return true;
  }

  if (autocomplete_type == ASCIIToUTF16("postal-code")) {
    field->set_heuristic_type(ADDRESS_HOME_ZIP);
    return true;
  }

  if (autocomplete_type == ASCIIToUTF16("country")) {
    field->set_heuristic_type(ADDRESS_HOME_COUNTRY);
    return true;
  }

  if (autocomplete_type == ASCIIToUTF16("organization")) {
    field->set_heuristic_type(COMPANY_NAME);
    return true;
  }

  if (autocomplete_type == ASCIIToUTF16("email")) {
    field->set_heuristic_type(EMAIL_ADDRESS);
    return true;
  }

  if (autocomplete_type == ASCIIToUTF16("phone-full")) {
    field->set_heuristic_type(PHONE_HOME_WHOLE_NUMBER);
    return true;
  }

  if (autocomplete_type == ASCIIToUTF16("phone-country-code")) {
    field->set_heuristic_type(PHONE_HOME_COUNTRY_CODE);
    return true;
  }

  if (autocomplete_type == ASCIIToUTF16("phone-national")) {
    field->set_heuristic_type(PHONE_HOME_CITY_AND_NUMBER);
    return true;
  }

  if (autocomplete_type == ASCIIToUTF16("phone-area-code")) {
    field->set_heuristic_type(PHONE_HOME_CITY_CODE);
    return true;
  }

  if (autocomplete_type == ASCIIToUTF16("phone-local")) {
    field->set_heuristic_type(PHONE_HOME_NUMBER);
    return true;
  }

  if (autocomplete_type == ASCIIToUTF16("phone-local-prefix")) {
    field->set_heuristic_type(PHONE_HOME_NUMBER);
    field->set_phone_part(AutofillField::PHONE_PREFIX);
    return true;
  }

  if (autocomplete_type == ASCIIToUTF16("phone-local-suffix")) {
    field->set_heuristic_type(PHONE_HOME_NUMBER);
    field->set_phone_part(AutofillField::PHONE_SUFFIX);
    return true;
  }

  if (autocomplete_type == ASCIIToUTF16("cc-full-name")) {
    field->set_heuristic_type(CREDIT_CARD_NAME);
    return true;
  }

  if (autocomplete_type == ASCIIToUTF16("cc-number")) {
    field->set_heuristic_type(CREDIT_CARD_NUMBER);
    return true;
  }

  if (autocomplete_type == ASCIIToUTF16("cc-exp-month")) {
    field->set_heuristic_type(CREDIT_CARD_EXP_MONTH);
    return true;
  }

  if (autocomplete_type == ASCIIToUTF16("cc-exp-year")) {
    if (field->max_length == 2)
      field->set_heuristic_type(CREDIT_CARD_EXP_2_DIGIT_YEAR);
    else
      field->set_heuristic_type(CREDIT_CARD_EXP_4_DIGIT_YEAR);
    return true;
  }

  if (autocomplete_type == ASCIIToUTF16("cc-exp")) {
    if (field->max_length == 5)
      field->set_heuristic_type(CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR);
    else
      field->set_heuristic_type(CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR);
    return true;
  }

  return false;
}

}  // namespace

FormStructure::FormStructure(const FormData& form)
    : form_name_(form.name),
      source_url_(form.origin),
      target_url_(form.action),
      autofill_count_(0),
      upload_required_(USE_UPLOAD_RATES),
      server_experiment_id_("no server response"),
      has_author_specified_types_(false) {
  // Copy the form fields.
  std::map<string16, size_t> unique_names;
  for (std::vector<webkit_glue::FormField>::const_iterator field =
           form.fields.begin();
       field != form.fields.end(); field++) {
    // Add all supported form fields (including with empty names) to the
    // signature.  This is a requirement for Autofill servers.
    form_signature_field_names_.append("&");
    form_signature_field_names_.append(UTF16ToUTF8(field->name));

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

void FormStructure::DetermineHeuristicTypes() {
  // First, try to detect field types based on the fields' |autocompletetype|
  // attributes.  If there is at least one form field with this attribute, don't
  // try to apply other heuristics to match fields in this form.
  bool found_sections;
  ParseAutocompletetypeAttributes(&has_author_specified_types_,
                                  &found_sections);

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

  if (!found_sections)
    IdentifySections();

  // Ensure that credit card and address fields are in separate sections.
  // This simplifies the section-aware logic in autofill_manager.cc.
  for (std::vector<AutofillField*>::iterator field = fields_->begin();
       field != fields_->end(); ++field) {
    AutofillType::FieldTypeGroup field_type_group =
        AutofillType((*field)->type()).group();
    if (field_type_group == AutofillType::CREDIT_CARD)
      (*field)->set_section((*field)->section() + ASCIIToUTF16("-cc"));
    else
      (*field)->set_section((*field)->section() + ASCIIToUTF16("-default"));
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

  return true;
}

// static
bool FormStructure::EncodeQueryRequest(
    const ScopedVector<FormStructure>& forms,
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
  std::vector<AutofillFieldType> field_types;
  UploadRequired upload_required;
  std::string experiment_id;
  AutofillQueryXmlParser parse_handler(&field_types, &upload_required,
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
  std::vector<AutofillFieldType>::iterator current_type = field_types.begin();
  for (std::vector<FormStructure*>::const_iterator iter = forms.begin();
       iter != forms.end(); ++iter) {
    FormStructure* form = *iter;
    form->upload_required_ = upload_required;
    form->server_experiment_id_ = experiment_id;

    for (std::vector<AutofillField*>::iterator field = form->fields_.begin();
         field != form->fields_.end(); ++field, ++current_type) {
      // In some cases *successful* response does not return all the fields.
      // Quit the update of the types then.
      if (current_type == field_types.end())
        break;

      // UNKNOWN_TYPE is reserved for use by the client.
      DCHECK_NE(*current_type, UNKNOWN_TYPE);

      AutofillFieldType heuristic_type = (*field)->type();
      if (heuristic_type != UNKNOWN_TYPE)
        heuristics_detected_fillable_field = true;

      (*field)->set_server_type(*current_type);
      if (heuristic_type != (*field)->type())
        query_response_overrode_heuristics = true;
    }

    form->UpdateAutofillCount();
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
      form.data.fields.push_back(webkit_glue::FormField(**field));

      FormFieldPredictions annotated_field;
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
  if (field_count() < kRequiredFillableFields)
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
    has_text_field |= (*it)->form_control_type != ASCIIToUTF16("select-one");
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
      if (field->form_control_type != ASCIIToUTF16("select-one") &&
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
    if (field->form_control_type == ASCIIToUTF16("select-one"))
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

std::string FormStructure::server_experiment_id() const {
  return server_experiment_id_;
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
      buzz::XmlElement *field_element = new buzz::XmlElement(
          buzz::QName(kXMLElementField));
      field_element->SetAttr(buzz::QName(kAttributeSignature),
                             field->FieldSignature());
      encompassing_xml_element->AddElement(field_element);
    }
  }
  return true;
}

void FormStructure::ParseAutocompletetypeAttributes(bool* found_attribute,
                                                    bool* found_sections) {
  *found_attribute = false;
  *found_sections = false;
  for (std::vector<AutofillField*>::iterator field = fields_->begin();
       field != fields_->end(); ++field) {
    if ((*field)->autocomplete_type.empty())
      continue;

    *found_attribute = true;
    std::vector<string16> types;
    Tokenize((*field)->autocomplete_type, ASCIIToUTF16(" "), &types);

    // Look for a named section.
    const string16 kSectionPrefix = ASCIIToUTF16("section-");
    if (!types.empty() && StartsWith(types.front(), kSectionPrefix, true)) {
      *found_sections = true;
      (*field)->set_section(types.front().substr(kSectionPrefix.size()));
    }

    // Look for specified types.
    for (std::vector<string16>::const_iterator type = types.begin();
         type != types.end(); ++type) {
      if (UpdateFromAutocompleteType(*type, *field))
        break;
    }
  }
}

void FormStructure::IdentifySections() {
  if (fields_.empty())
    return;

  // Name sections after the first field in the section.
  string16 current_section = fields_->front()->unique_name();

  // Keep track of the types we've seen in this section.
  std::set<AutofillFieldType> seen_types;
  AutofillFieldType previous_type = UNKNOWN_TYPE;

  for (std::vector<AutofillField*>::iterator field = fields_->begin();
       field != fields_->end(); ++field) {
    const AutofillFieldType current_type =
        AutofillType::GetEquivalentFieldType((*field)->type());

    bool already_saw_current_type = seen_types.count(current_type) > 0;

    // Forms often ask for multiple phone numbers -- e.g. both a daytime and
    // evening phone number.  Our phone number detection is also generally a
    // little off.  Hence, ignore this field type as a signal here.
    if (AutofillType(current_type).group() == AutofillType::PHONE_HOME)
      already_saw_current_type = false;

    // Some forms have adjacent fields of the same type.  Two common examples:
    //  * Forms with two email fields, where the second is meant to "confirm"
    //    the first.
    //  * Forms with a <select> menu for states in some countries, and a
    //    freeform <input> field for states in other countries.  (Usually, only
    //    one of these two will be visible for any given choice of country.)
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
    (*field)->set_section(current_section);
  }
}
