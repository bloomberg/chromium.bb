// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/notifier/listener/xml_element_util.h"

#include <sstream>
#include <string>

#include "base/string_util.h"
#include "talk/xmllite/qname.h"
#include "talk/xmllite/xmlconstants.h"
#include "talk/xmllite/xmlelement.h"
#include "talk/xmllite/xmlprinter.h"

namespace browser_sync {

std::string XmlElementToString(const buzz::XmlElement& xml_element) {
  std::ostringstream xml_stream;
  buzz::XmlPrinter::PrintXml(&xml_stream, &xml_element);
  return xml_stream.str();
}

buzz::XmlElement* MakeBoolXmlElement(const char* name, bool value) {
  const buzz::QName elementQName(true, buzz::STR_EMPTY, name);
  const buzz::QName boolAttrQName(true, buzz::STR_EMPTY, "bool");
  buzz::XmlElement* bool_xml_element =
      new buzz::XmlElement(elementQName, true);
  bool_xml_element->AddAttr(boolAttrQName, value ? "true" : "false");
  return bool_xml_element;
}

buzz::XmlElement* MakeIntXmlElement(const char* name, int value) {
  const buzz::QName elementQName(true, buzz::STR_EMPTY, name);
  const buzz::QName intAttrQName(true, buzz::STR_EMPTY, "int");
  buzz::XmlElement* int_xml_element =
      new buzz::XmlElement(elementQName, true);
  int_xml_element->AddAttr(intAttrQName, IntToString(value));
  return int_xml_element;
}

buzz::XmlElement* MakeStringXmlElement(const char* name, const char* value) {
  const buzz::QName elementQName(true, buzz::STR_EMPTY, name);
  const buzz::QName dataAttrQName(true, buzz::STR_EMPTY, "data");
  buzz::XmlElement* data_xml_element =
      new buzz::XmlElement(elementQName, true);
  data_xml_element->AddAttr(dataAttrQName, value);
  return data_xml_element;
}

}  // namespace browser_sync
