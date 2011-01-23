// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/form_structure.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/sha1.h"
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

const char* kFormMethodPost = "post";

// XML attribute names.
const char* const kAttributeClientVersion = "clientversion";
const char* const kAttributeAutoFillUsed = "autofillused";
const char* const kAttributeSignature = "signature";
const char* const kAttributeFormSignature = "formsignature";
const char* const kAttributeDataPresent = "datapresent";

const char* const kXMLElementForm = "form";
const char* const kXMLElementField = "field";
const char* const kAttributeAutoFillType = "autofilltype";

// The list of form control types we handle.
const char* const kControlTypeSelect = "select-one";
const char* const kControlTypeText = "text";

// The number of fillable fields necessary for a form to be fillable.
const size_t kRequiredFillableFields = 3;

static std::string Hash64Bit(const std::string& str) {
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

}  // namespace

FormStructure::FormStructure(const FormData& form)
    : form_name_(form.name),
      source_url_(form.origin),
      target_url_(form.action),
      has_credit_card_field_(false),
      has_autofillable_field_(false),
      has_password_fields_(false),
      autofill_count_(0) {
  // Copy the form fields.
  std::vector<webkit_glue::FormField>::const_iterator field;
  for (field = form.fields.begin();
       field != form.fields.end(); field++) {
    // We currently only handle text and select fields; however, we need to
    // store all fields in order to match the fields stored in the FormManager.
    // We don't append other field types to the form signature though in order
    // to match the form signature of the AutoFill servers.
    if (LowerCaseEqualsASCII(field->form_control_type(), kControlTypeText) ||
        LowerCaseEqualsASCII(field->form_control_type(), kControlTypeSelect)) {
      // Add all supported form fields (including with empty names) to
      // signature.  This is a requirement for AutoFill servers.
      form_signature_field_names_.append("&");
      form_signature_field_names_.append(UTF16ToUTF8(field->name()));
    }

    // Generate a unique name for this field by appending a counter to the name.
    string16 unique_name = field->name() +
        base::IntToString16(fields_.size() + 1);
    fields_.push_back(new AutoFillField(*field, unique_name));
  }

  // Terminate the vector with a NULL item.
  fields_.push_back(NULL);
  GetHeuristicAutoFillTypes();

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

bool FormStructure::EncodeUploadRequest(bool auto_fill_used,
                                        std::string* encoded_xml) const {
  DCHECK(encoded_xml);
  encoded_xml->clear();
  bool auto_fillable = IsAutoFillable(false);
  DCHECK(auto_fillable);  // Caller should've checked for search pages.
  if (!auto_fillable)
    return false;

  buzz::XmlElement autofil_request_xml(buzz::QName("autofillupload"));

  // Attributes for the <autofillupload> element.
  //
  // TODO(jhawkins): Work with toolbar devs to make a spec for autofill clients.
  // For now these values are hacked from the toolbar code.
  autofil_request_xml.SetAttr(buzz::QName(kAttributeClientVersion),
                              "6.1.1715.1442/en (GGLL)");

  autofil_request_xml.SetAttr(buzz::QName(kAttributeFormSignature),
                              FormSignature());

  autofil_request_xml.SetAttr(buzz::QName(kAttributeAutoFillUsed),
                              auto_fill_used ? "true" : "false");

  // TODO(jhawkins): Hook this up to the personal data manager.
  // personaldata_manager_->GetDataPresent();
  autofil_request_xml.SetAttr(buzz::QName(kAttributeDataPresent), "");

  if (!EncodeFormRequest(FormStructure::UPLOAD, &autofil_request_xml))
    return false;  // Malformed form, skip it.

  // Obtain the XML structure as a string.
  *encoded_xml = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>";
  *encoded_xml += autofil_request_xml.Str().c_str();

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
  buzz::XmlElement autofil_request_xml(buzz::QName("autofillquery"));
  // Attributes for the <autofillquery> element.
  //
  // TODO(jhawkins): Work with toolbar devs to make a spec for autofill clients.
  // For now these values are hacked from the toolbar code.
  autofil_request_xml.SetAttr(buzz::QName(kAttributeClientVersion),
                              "6.1.1715.1442/en (GGLL)");
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
        new buzz::XmlElement(buzz::QName("form")));
    encompassing_xml_element->SetAttr(buzz::QName(kAttributeSignature),
                                      signature);

    if (!(*it)->EncodeFormRequest(FormStructure::QUERY,
                                  encompassing_xml_element.get()))
      continue;  // Malformed form, skip it.

    autofil_request_xml.AddElement(encompassing_xml_element.release());
    encoded_signatures->push_back(signature);
  }

  if (!encoded_signatures->size())
    return false;

  // Obtain the XML structure as a string.
  *encoded_xml = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>";
  *encoded_xml += autofil_request_xml.Str().c_str();

  return true;
}

// static
void FormStructure::ParseQueryResponse(const std::string& response_xml,
                                       const std::vector<FormStructure*>& forms,
                                       UploadRequired* upload_required,
                                       const AutoFillMetrics& metric_logger) {
  metric_logger.Log(AutoFillMetrics::QUERY_RESPONSE_RECEIVED);

  // Parse the field types from the server response to the query.
  std::vector<AutoFillFieldType> field_types;
  AutoFillQueryXmlParser parse_handler(&field_types, upload_required);
  buzz::XmlParser parser(&parse_handler);
  parser.Parse(response_xml.c_str(), response_xml.length(), true);
  if (!parse_handler.succeeded())
    return;

  metric_logger.Log(AutoFillMetrics::QUERY_RESPONSE_PARSED);

  bool heuristics_detected_fillable_field = false;
  bool query_response_overrode_heuristics = false;

  // Copy the field types into the actual form.
  std::vector<AutoFillFieldType>::iterator current_type = field_types.begin();
  for (std::vector<FormStructure*>::const_iterator iter = forms.begin();
       iter != forms.end(); ++iter) {
    FormStructure* form = *iter;

    if (form->has_autofillable_field_)
      heuristics_detected_fillable_field = true;

    form->has_credit_card_field_ = false;
    form->has_autofillable_field_ = false;

    for (std::vector<AutoFillField*>::iterator field = form->fields_.begin();
         field != form->fields_.end(); ++field, ++current_type) {
      // The field list is terminated by a NULL AutoFillField.
      if (!*field)
        break;

      // In some cases *successful* response does not return all the fields.
      // Quit the update of the types then.
      if (current_type == field_types.end())
        break;

      // UNKNOWN_TYPE is reserved for use by the client.
      DCHECK_NE(*current_type, UNKNOWN_TYPE);

      AutoFillFieldType heuristic_type = (*field)->type();
      (*field)->set_server_type(*current_type);
      if (heuristic_type != (*field)->type())
        query_response_overrode_heuristics = true;

      AutoFillType autofill_type((*field)->type());
      if (autofill_type.group() == AutoFillType::CREDIT_CARD)
        form->has_credit_card_field_ = true;
      if (autofill_type.field_type() != UNKNOWN_TYPE)
        form->has_autofillable_field_ = true;
    }

    form->UpdateAutoFillCount();
  }

  AutoFillMetrics::ServerQueryMetric metric;
  if (query_response_overrode_heuristics) {
    if (heuristics_detected_fillable_field) {
      metric = AutoFillMetrics::QUERY_RESPONSE_OVERRODE_LOCAL_HEURISTICS;
    } else {
      metric = AutoFillMetrics::QUERY_RESPONSE_WITH_NO_LOCAL_HEURISTICS;
    }
  } else {
    metric = AutoFillMetrics::QUERY_RESPONSE_MATCHED_LOCAL_HEURISTICS;
  }
  metric_logger.Log(metric);
}

std::string FormStructure::FormSignature() const {
  std::string form_string = target_url_.scheme() +
                            "://" +
                            target_url_.host() +
                            "&" +
                            UTF16ToUTF8(form_name_) +
                            form_signature_field_names_;

  return Hash64Bit(form_string);
}

bool FormStructure::IsAutoFillable(bool require_method_post) const {
  if (autofill_count() < kRequiredFillableFields)
    return false;

  return ShouldBeParsed(require_method_post);
}

bool FormStructure::HasAutoFillableValues() const {
  if (autofill_count_ == 0)
    return false;

  for (std::vector<AutoFillField*>::const_iterator iter = begin();
       iter != end(); ++iter) {
    AutoFillField* field = *iter;
    if (field && !field->IsEmpty() && field->IsFieldFillable())
      return true;
  }

  return false;
}

void FormStructure::UpdateAutoFillCount() {
  autofill_count_ = 0;
  for (std::vector<AutoFillField*>::const_iterator iter = begin();
       iter != end(); ++iter) {
    AutoFillField* field = *iter;
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

void FormStructure::set_possible_types(int index, const FieldTypeSet& types) {
  int num_fields = static_cast<int>(field_count());
  DCHECK(index >= 0 && index < num_fields);
  if (index >= 0 && index < num_fields)
    fields_[index]->set_possible_types(types);
}

const AutoFillField* FormStructure::field(int index) const {
  return fields_[index];
}

size_t FormStructure::field_count() const {
  // Don't count the NULL terminator.
  size_t field_size = fields_.size();
  return (field_size == 0) ? 0 : field_size - 1;
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

void FormStructure::GetHeuristicAutoFillTypes() {
  has_credit_card_field_ = false;
  has_autofillable_field_ = false;

  FieldTypeMap field_type_map;
  GetHeuristicFieldInfo(&field_type_map);

  for (size_t index = 0; index < field_count(); index++) {
    AutoFillField* field = fields_[index];
    DCHECK(field);
    FieldTypeMap::iterator iter = field_type_map.find(field->unique_name());

    AutoFillFieldType heuristic_auto_fill_type;
    if (iter == field_type_map.end()) {
      heuristic_auto_fill_type = UNKNOWN_TYPE;
    } else {
      heuristic_auto_fill_type = iter->second;
      ++autofill_count_;
    }

    field->set_heuristic_type(heuristic_auto_fill_type);

    AutoFillType autofill_type(field->type());
    if (autofill_type.group() == AutoFillType::CREDIT_CARD)
      has_credit_card_field_ = true;
    if (autofill_type.field_type() != UNKNOWN_TYPE)
      has_autofillable_field_ = true;
  }
}

void FormStructure::GetHeuristicFieldInfo(FieldTypeMap* field_type_map) {
  FormFieldSet fields(this);

  FormFieldSet::const_iterator field;
  for (field = fields.begin(); field != fields.end(); field++) {
    bool ok = (*field)->GetFieldInfo(field_type_map);
    DCHECK(ok);
  }
}

bool FormStructure::EncodeFormRequest(
    FormStructure::EncodeRequestType request_type,
    buzz::XmlElement* encompassing_xml_element) const {
  if (!field_count())  // Nothing to add.
    return false;
  // Some badly formatted web sites repeat fields - limit number of fields to
  // 48, which is far larger than any valid form and XML still fits into 2K.
  const size_t kMaxFieldsOnTheForm = 48;
  if (field_count() > kMaxFieldsOnTheForm) {
    // This is not a valid form, most certainly. Do not send request for the
    // wrongly formatted forms.
    return false;
  }
  // Add the child nodes for the form fields.
  for (size_t index = 0; index < field_count(); ++index) {
    const AutoFillField* field = fields_[index];
    if (request_type == FormStructure::UPLOAD) {
      FieldTypeSet types = field->possible_types();
      for (FieldTypeSet::const_iterator type = types.begin();
           type != types.end(); type++) {
        buzz::XmlElement *field_element = new buzz::XmlElement(
            buzz::QName(kXMLElementField));

        field_element->SetAttr(buzz::QName(kAttributeSignature),
                               field->FieldSignature());
        field_element->SetAttr(buzz::QName(kAttributeAutoFillType),
                               base::IntToString(*type));
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
