// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/form_structure.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/sha1.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/field_types.h"
#include "chrome/browser/autofill/form_field.h"
#include "third_party/libjingle/files/talk/xmllite/xmlelement.h"
#include "webkit/glue/form_field.h"
#include "webkit/glue/form_field_values.h"

const char* kFormMethodGet = "get";
const char* kFormMethodPost = "post";

// XML attribute names
const char* const kAttributeClientVersion = "clientversion";
const char* const kAttributeAutoFillUsed = "autofillused";
const char* const kAttributeSignature = "signature";
const char* const kAttributeFormSignature = "formsignature";
const char* const kAttributeDataPresent = "datapresent";

const char* const kXMLElementForm = "form";
const char* const kXMLElementField = "field";
const char* const kAttributeAutoFillType = "autofilltype";

namespace {

static std::string Hash64Bit(const std::string& str) {
  std::string hash_bin = base::SHA1HashString(str);
  DCHECK(hash_bin.length() == 20);

  uint64 hash64 = (((static_cast<uint64>(hash_bin[0])) & 0xFF) << 56) |
                  (((static_cast<uint64>(hash_bin[1])) & 0xFF) << 48) |
                  (((static_cast<uint64>(hash_bin[2])) & 0xFF) << 40) |
                  (((static_cast<uint64>(hash_bin[3])) & 0xFF) << 32) |
                  (((static_cast<uint64>(hash_bin[4])) & 0xFF) << 24) |
                  (((static_cast<uint64>(hash_bin[5])) & 0xFF) << 16) |
                  (((static_cast<uint64>(hash_bin[6])) & 0xFF) << 8) |
                   ((static_cast<uint64>(hash_bin[7])) & 0xFF);

  return Uint64ToString(hash64);
}

}  // namespace

FormStructure::FormStructure(const webkit_glue::FormFieldValues& values)
    : form_name_(UTF16ToUTF8(values.form_name)),
      source_url_(values.source_url),
      target_url_(values.target_url) {
  // Copy the form fields.
  std::vector<webkit_glue::FormField>::const_iterator field;
  for (field = values.elements.begin();
       field != values.elements.end(); field++) {
    // Generate a unique name for this field by appending a counter to the name.
    string16 unique_name = field->name() + IntToString16(fields_.size() + 1);
    fields_.push_back(new AutoFillField(*field, unique_name));
  }

  // Terminate the vector with a NULL item.
  fields_.push_back(NULL);

  std::string method = UTF16ToUTF8(values.method);
  if (method == kFormMethodPost) {
    method_ = POST;
  } else {
    // Either the method is 'get', or we don't know.  In this case we default
    // to GET.
    method_ = GET;
  }
}

bool FormStructure::EncodeUploadRequest(bool auto_fill_used,
                                        std::string* encoded_xml) const {
  bool auto_fillable = IsAutoFillable();
  DCHECK(auto_fillable);  // Caller should've checked for search pages.
  if (!auto_fillable)
    return false;

  buzz::XmlElement autofill_upload(buzz::QName("autofillupload"));

  // Attributes for the <autofillupload> element.
  //
  // TODO(jhawkins): Work with toolbar devs to make a spec for autofill clients.
  // For now these values are hacked from the toolbar code.
  autofill_upload.SetAttr(buzz::QName(kAttributeClientVersion),
                          "6.1.1715.1442/en (GGLL)");

  autofill_upload.SetAttr(buzz::QName(kAttributeFormSignature),
                          FormSignature());

  autofill_upload.SetAttr(buzz::QName(kAttributeAutoFillUsed),
                          auto_fill_used ? "true" : "false");

  // TODO(jhawkins): Hook this up to the personal data manager.
  // personaldata_manager_->GetDataPresent();
  autofill_upload.SetAttr(buzz::QName(kAttributeDataPresent), "");

  // Add the child nodes for the form fields.
  for (size_t index = 0; index < field_count(); index++) {
    const AutoFillField* field = fields_[index];
    FieldTypeSet types = field->possible_types();
    for (FieldTypeSet::const_iterator type = types.begin();
         type != types.end(); type++) {
      buzz::XmlElement *field_element = new buzz::XmlElement(
          buzz::QName(kXMLElementField));

      field_element->SetAttr(buzz::QName(kAttributeSignature),
                             field->FieldSignature());

      field_element->SetAttr(buzz::QName(kAttributeAutoFillType),
                             IntToString(*type));

      autofill_upload.AddElement(field_element);
    }
  }

  // Obtain the XML structure as a string.
  *encoded_xml = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>";
  *encoded_xml += autofill_upload.Str().c_str();

  return true;
}

void FormStructure::GetHeuristicAutoFillTypes() {
  has_credit_card_field_ = false;
  has_autofillable_field_ = false;

  FieldTypeMap field_type_map;
  GetHeuristicFieldInfo(&field_type_map);

  for (size_t index = 0; index < field_count(); index++) {
    AutoFillField* field = fields_[index];
    FieldTypeMap::iterator iter = field_type_map.find(field->unique_name());

    AutoFillFieldType heuristic_auto_fill_type;
    if (iter == field_type_map.end())
      heuristic_auto_fill_type = UNKNOWN_TYPE;
    else
      heuristic_auto_fill_type = iter->second;

    field->set_heuristic_type(heuristic_auto_fill_type);

    AutoFillType autofill_type(field->type());
    if (autofill_type.group() == AutoFillType::CREDIT_CARD)
      has_credit_card_field_ = true;
    if (autofill_type.field_type() != UNKNOWN_TYPE)
      has_autofillable_field_ = true;
  }
}

std::string FormStructure::FormSignature() const {
  std::string form_string = target_url_.host() +
                            "&" +
                            form_name_ +
                            form_signature_field_names_;

  return Hash64Bit(form_string);
}

bool FormStructure::IsAutoFillable() const {
  if (fields_.size() == 0)
    return false;

  // Rule out http(s)://*/search?...
  //  e.g. http://www.google.com/search?q=...
  //       http://search.yahoo.com/search?p=...
  if (target_url_.path() == "/search")
    return false;

  // Disqualify all forms that are likely to be search boxes (like google.com).
  if (fields_.size() == 1) {
    std::string name = UTF16ToUTF8(fields_[0]->name());
    if (name == "q")
      return false;
  }

  if (method_ == GET)
    return false;

  return true;
}

void FormStructure::set_possible_types(int index, const FieldTypeSet& types) {
  int num_fields = static_cast<int>(fields_.size());
  DCHECK(index >= 0 && index < num_fields);
  if (index >= 0 && index < num_fields)
    fields_[index]->set_possible_types(types);
}

AutoFillField* FormStructure::field(int index) {
  return fields_[index];
}

size_t FormStructure::field_count() const {
  // Don't count the NULL terminator.
  return fields_.size() - 1;
}

void FormStructure::GetHeuristicFieldInfo(FieldTypeMap* field_type_map) {
  FormFieldSet fields = FormFieldSet(this);

  FormFieldSet::const_iterator field;
  for (field = fields.begin(); field != fields.end(); field++) {
    bool ok = (*field)->GetFieldInfo(field_type_map);
    DCHECK(ok);
  }
}
