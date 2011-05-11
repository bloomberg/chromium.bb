// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/form_structure.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/sha1.h"
#include "base/stringprintf.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_metrics.h"
#include "chrome/browser/autofill/autofill_xml_parser.h"
#include "chrome/browser/autofill/field_types.h"
#include "chrome/browser/autofill/form_field.h"
#include "third_party/libjingle/source/talk/xmllite/xmlelement.h"
#include "webkit/glue/form_field.h"

using webkit_glue::FormData;

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

}  // namespace

FormStructure::FormStructure(const FormData& form)
    : form_name_(form.name),
      source_url_(form.origin),
      target_url_(form.action),
      autofill_count_(0) {
  // Copy the form fields.
  std::vector<webkit_glue::FormField>::const_iterator field;
  for (field = form.fields.begin();
       field != form.fields.end(); field++) {
    // Add all supported form fields (including with empty names) to the
    // signature.  This is a requirement for Autofill servers.
    form_signature_field_names_.append("&");
    form_signature_field_names_.append(UTF16ToUTF8(field->name));

    // Generate a unique name for this field by appending a counter to the name.
    string16 unique_name = field->name +
        base::IntToString16(fields_.size() + 1);
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
  autofill_count_ = 0;

  FieldTypeMap field_type_map;
  FormField::ParseFormFields(fields_.get(), &field_type_map);

  for (size_t index = 0; index < field_count(); index++) {
    AutofillField* field = fields_[index];
    FieldTypeMap::iterator iter = field_type_map.find(field->unique_name());

    AutofillFieldType heuristic_autofill_type;
    if (iter == field_type_map.end()) {
      heuristic_autofill_type = UNKNOWN_TYPE;
    } else {
      heuristic_autofill_type = iter->second;
      ++autofill_count_;
    }

    field->set_heuristic_type(heuristic_autofill_type);

    AutofillType autofill_type(field->type());
  }
}

bool FormStructure::EncodeUploadRequest(
    const FieldTypeSet& available_field_types,
    bool form_was_autofilled,
    std::string* encoded_xml) const {
  if (!ShouldBeParsed(true)) {
    NOTREACHED();  // Caller should've checked for search pages.
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
bool FormStructure::EncodeQueryRequest(const ScopedVector<FormStructure>& forms,
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
                                       UploadRequired* upload_required,
                                       const AutofillMetrics& metric_logger) {
  metric_logger.LogServerQueryMetric(AutofillMetrics::QUERY_RESPONSE_RECEIVED);

  // Parse the field types from the server response to the query.
  std::vector<AutofillFieldType> field_types;
  std::string experiment_id;
  AutofillQueryXmlParser parse_handler(&field_types, upload_required,
                                       &experiment_id);
  buzz::XmlParser parser(&parse_handler);
  parser.Parse(response_xml.c_str(), response_xml.length(), true);
  if (!parse_handler.succeeded())
    return;

  metric_logger.LogServerQueryMetric(AutofillMetrics::QUERY_RESPONSE_PARSED);

  bool heuristics_detected_fillable_field = false;
  bool query_response_overrode_heuristics = false;

  // Copy the field types into the actual form.
  std::vector<AutofillFieldType>::iterator current_type = field_types.begin();
  for (std::vector<FormStructure*>::const_iterator iter = forms.begin();
       iter != forms.end(); ++iter) {
    FormStructure* form = *iter;
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

  return !require_method_post || (method_ == POST);
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
      field->set_heuristic_type(cached_field->second->heuristic_type());
      field->set_server_type(cached_field->second->server_type());
    }
  }

  UpdateAutofillCount();

  server_experiment_id_ = cached_form.server_experiment_id();
}

void FormStructure::LogQualityMetrics(
    const AutofillMetrics& metric_logger) const {
  std::string experiment_id = server_experiment_id();
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
      else if (*it == PHONE_FAX_CITY_AND_NUMBER)
        collapsed_field_types.insert(PHONE_FAX_WHOLE_NUMBER);
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
}

void FormStructure::set_possible_types(size_t index,
                                       const FieldTypeSet& types) {
  if (index >= fields_.size()) {
    NOTREACHED();
    return;
  }

  fields_[index]->set_possible_types(types);
}

const AutofillField* FormStructure::field(size_t index) const {
  if (index >= fields_.size()) {
    NOTREACHED();
    return NULL;
  }

  return fields_[index];
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
