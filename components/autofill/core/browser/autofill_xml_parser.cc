// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_xml_parser.h"

#include <utility>

#include "base/strings/string_number_conversions.h"
#include "components/autofill/core/browser/autofill_server_field_info.h"
#include "components/autofill/core/browser/field_types.h"
#include "third_party/libxml/chromium/libxml_utils.h"

namespace autofill {

bool ParseAutofillQueryXml(std::string xml,
                           std::vector<AutofillServerFieldInfo>* field_infos,
                           UploadRequired* upload_required) {
  DCHECK(field_infos);
  DCHECK(upload_required);

  XmlReader reader;
  if (!reader.Load(xml))
    return false;

  // Seek to the first opening tag.
  if (!reader.Read())
    return false;  // Malformed input.

  if (reader.NodeName() != "autofillqueryresponse")
    return false;  // Malformed input.

  *upload_required = USE_UPLOAD_RATES;
  std::string upload_required_value;
  if (reader.NodeAttribute("uploadrequired", &upload_required_value)) {
    if (upload_required_value == "true")
      *upload_required = UPLOAD_REQUIRED;
    else if (upload_required_value == "false")
      *upload_required = UPLOAD_NOT_REQUIRED;
  }

  field_infos->clear();
  if (reader.IsClosingElement())
    return true;

  if (!reader.Read())
    return false;  // Malformed input.
  while (reader.NodeName() == "field") {
    AutofillServerFieldInfo field_info;
    field_info.field_type = UNKNOWN_TYPE;

    std::string autofill_type_value;
    if (!reader.NodeAttribute("autofilltype", &autofill_type_value))
      return false;  // Missing required attribute.
    int parsed_type = 0;
    if (!base::StringToInt(autofill_type_value, &parsed_type) ||
        parsed_type < 0 || parsed_type >= MAX_VALID_FIELD_TYPE) {
      // Invalid attribute value should not make the whole parse fail.
      field_info.field_type = NO_SERVER_DATA;
    } else {
      field_info.field_type = static_cast<ServerFieldType>(parsed_type);
    }

    std::string default_value;
    if (field_info.field_type == FIELD_WITH_DEFAULT_VALUE &&
        reader.NodeAttribute("defaultvalue", &default_value)) {
      field_info.default_value = std::move(default_value);
    }

    field_infos->push_back(field_info);

    if (!reader.Read())
      return false;  // Malformed input.
  }

  if (reader.NodeName() != "autofillqueryresponse" ||
      !reader.IsClosingElement()) {
    return false;  // Malformed input.
  }
  return true;
}

}  // namespace autofill
