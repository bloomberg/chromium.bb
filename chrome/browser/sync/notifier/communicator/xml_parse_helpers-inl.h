// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_NOTIFIER_COMMUNICATOR_XML_PARSE_HELPERS_INL_H_
#define CHROME_BROWSER_SYNC_NOTIFIER_COMMUNICATOR_XML_PARSE_HELPERS_INL_H_

#include <sstream>

#include "chrome/browser/sync/notifier/communicator/xml_parse_helpers.h"
#include "talk/xmllite/xmlelement.h"

namespace notifier {

template<class T>
void SetAttr(buzz::XmlElement* xml, const buzz::QName& name, const T& data) {
  std::ostringstream ost;
  ost << data;
  xml->SetAttr(name, ost.str());
}

} // namespace notifier

#endif  // CHROME_BROWSER_SYNC_NOTIFIER_COMMUNICATOR_XML_PARSE_HELPERS_INL_H_
