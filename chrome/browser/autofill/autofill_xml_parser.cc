// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_xml_parser.h"

#include <stdlib.h>
#include <string.h>

#include "base/logging.h"
#include "third_party/libjingle/source/talk/xmllite/qname.h"

AutofillXmlParser::AutofillXmlParser()
    : succeeded_(true) {
}

void AutofillXmlParser::CharacterData(
    buzz::XmlParseContext* context, const char* text, int len) {
}

void AutofillXmlParser::EndElement(buzz::XmlParseContext* context,
                                   const char* name) {
}

void AutofillXmlParser::Error(buzz::XmlParseContext* context,
                              XML_Error error_code) {
  succeeded_ = false;
}

AutofillQueryXmlParser::AutofillQueryXmlParser(
    std::vector<AutofillFieldType>* field_types,
    UploadRequired* upload_required,
    std::string* experiment_id)
    : field_types_(field_types),
      upload_required_(upload_required),
      experiment_id_(experiment_id) {
  DCHECK(upload_required_);
  DCHECK(experiment_id_);
}

void AutofillQueryXmlParser::StartElement(buzz::XmlParseContext* context,
                                          const char* name,
                                          const char** attrs) {
  buzz::QName qname = context->ResolveQName(name, false);
  const std::string& element = qname.LocalPart();
  if (element.compare("autofillqueryresponse") == 0) {
    // We check for the upload required attribute below, but if it's not
    // present, we use the default upload rates. Likewise, by default we assume
    // an empty experiment id.
    *upload_required_ = USE_UPLOAD_RATES;
    *experiment_id_ = std::string();

    // |attrs| is a NULL-terminated list of (attribute, value) pairs.
    while (*attrs) {
      buzz::QName attribute_qname = context->ResolveQName(attrs[0], true);
      const std::string& attribute_name = attribute_qname.LocalPart();
      if (attribute_name.compare("uploadrequired") == 0) {
        if (strcmp(attrs[1], "true") == 0)
          *upload_required_ = UPLOAD_REQUIRED;
        else if (strcmp(attrs[1], "false") == 0)
          *upload_required_ = UPLOAD_NOT_REQUIRED;
      } else if (attribute_name.compare("experimentid") == 0) {
        *experiment_id_ = attrs[1];
      }

      // Advance to the next (attribute, value) pair.
      attrs += 2;
    }
  } else if (element.compare("field") == 0) {
    if (!attrs[0]) {
      // Missing the "autofilltype" attribute, abort.
      context->RaiseError(XML_ERROR_ABORTED);
      return;
    }

    // Determine the field type from the attribute value.  There should be one
    // attribute (autofilltype) with an integer value.
    AutofillFieldType field_type = UNKNOWN_TYPE;
    buzz::QName attribute_qname = context->ResolveQName(attrs[0], true);
    const std::string& attribute_name = attribute_qname.LocalPart();

    if (attribute_name.compare("autofilltype") == 0) {
      int value = GetIntValue(context, attrs[1]);
      field_type = static_cast<AutofillFieldType>(value);
      if (field_type < 0 || field_type > MAX_VALID_FIELD_TYPE) {
        field_type = NO_SERVER_DATA;
      }
    }

    // Record this field type.
    field_types_->push_back(field_type);
  }
}

int AutofillQueryXmlParser::GetIntValue(buzz::XmlParseContext* context,
                                        const char* attribute) {
  char* attr_end = NULL;
  int value = strtol(attribute, &attr_end, 10);
  if (attr_end != NULL && attr_end == attribute) {
    context->RaiseError(XML_ERROR_SYNTAX);
    return 0;
  }
  return value;
}

AutofillUploadXmlParser::AutofillUploadXmlParser(double* positive_upload_rate,
                                                 double* negative_upload_rate)
    : succeeded_(false),
      positive_upload_rate_(positive_upload_rate),
      negative_upload_rate_(negative_upload_rate) {
  DCHECK(positive_upload_rate_);
  DCHECK(negative_upload_rate_);
}

void AutofillUploadXmlParser::StartElement(buzz::XmlParseContext* context,
                                           const char* name,
                                           const char** attrs) {
  buzz::QName qname = context->ResolveQName(name, false);
  const std::string &element = qname.LocalPart();
  if (element.compare("autofilluploadresponse") == 0) {
    // Loop over all attributes to get the upload rates.
    while (*attrs) {
      buzz::QName attribute_qname = context->ResolveQName(attrs[0], true);
      const std::string &attribute_name = attribute_qname.LocalPart();
      if (attribute_name.compare("positiveuploadrate") == 0) {
        *positive_upload_rate_ = GetDoubleValue(context, attrs[1]);
      } else if (attribute_name.compare("negativeuploadrate") == 0) {
        *negative_upload_rate_ = GetDoubleValue(context, attrs[1]);
      }
      attrs += 2;  // We peeked at attrs[0] and attrs[1], skip past both.
    }
  }
}

double AutofillUploadXmlParser::GetDoubleValue(buzz::XmlParseContext* context,
                                               const char* attribute) {
  char* attr_end = NULL;
  double value = strtod(attribute, &attr_end);
  if (attr_end != NULL && attr_end == attribute) {
    context->RaiseError(XML_ERROR_SYNTAX);
    return 0.0;
  }
  return value;
}
