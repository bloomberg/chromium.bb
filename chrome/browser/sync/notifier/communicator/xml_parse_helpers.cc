// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/notifier/communicator/xml_parse_helpers.h"
#include "chrome/browser/sync/notifier/communicator/xml_parse_helpers-inl.h"

#include <string>

#include "chrome/browser/sync/notifier/base/string.h"
#include "talk/base/basicdefs.h"
#include "talk/base/stream.h"
#include "talk/xmllite/xmlbuilder.h"
#include "talk/xmllite/xmlelement.h"
#include "talk/xmllite/xmlparser.h"
#include "talk/xmllite/xmlprinter.h"
#include "talk/xmpp/jid.h"

namespace notifier {

buzz::XmlElement* ReadXmlFromStream(talk_base::StreamInterface* stream) {
  buzz::XmlBuilder builder;
  buzz::XmlParser parser(&builder);

  const int kBufferSize = 4 * 1024;
  char buf[kBufferSize];

  talk_base::StreamResult result = talk_base::SR_SUCCESS;
  while(true) {
    size_t read = 0;

    // Read a chunk.
    result = stream->Read(buf, kBufferSize, &read, NULL);
    if (result != talk_base::SR_SUCCESS)
      break;

    // Pass it to the parser.
    parser.Parse(buf, read, false);
  }

  if (result == talk_base::SR_EOS) {
    parser.Parse(NULL, 0, true);
    return builder.CreateElement();
  }

  return NULL;
}

bool ParseInt64Attr(const buzz::XmlElement* element,
                    const buzz::QName& attribute, int64* result) {
  if (!element->HasAttr(attribute))
    return false;
  std::string text = element->Attr(attribute);
  char* error = NULL;
#ifdef POSIX
  *result = atoll(text.c_str());
#else
  *result = _strtoi64(text.c_str(), &error, 10);
#endif
  return text.c_str() != error;
}

bool ParseIntAttr(const buzz::XmlElement* element, const buzz::QName& attribute,
                  int* result) {
  if (!element->HasAttr(attribute))
    return false;
  std::string text = element->Attr(attribute);
  char* error = NULL;
  *result = static_cast<int>(strtol(text.c_str(), &error, 10));
  return text.c_str() != error;
}

bool ParseBoolAttr(const buzz::XmlElement* element,
                   const buzz::QName& attribute, bool* result) {
  int int_value = 0;
  if (!ParseIntAttr(element, attribute, &int_value))
    return false;
  *result = int_value != 0;
  return true;
}

bool ParseStringAttr(const buzz::XmlElement* element,
                     const buzz::QName& attribute, std::string* result) {
  if (!element->HasAttr(attribute))
    return false;
  *result = element->Attr(attribute);
  return true;
}

void WriteXmlToStream(talk_base::StreamInterface* stream,
                      const buzz::XmlElement* xml) {
  // Save it all to a string and then write that string out to disk.
  //
  // This is probably really inefficient in multiple ways.  We probably have an
  // entire string copy of the XML in memory twice -- once in the stream and
  // once in the string.  There is probably a way to get the data directly out
  // of the stream but I don't have the time to decode the stream classes right
  // now.
  std::ostringstream s;
  buzz::XmlPrinter::PrintXml(&s, xml);
  std::string output_string = s.str();
  stream->WriteAll(output_string.data(), output_string.length(), NULL, NULL);
}

bool SetInt64Attr(buzz::XmlElement* element, const buzz::QName& attribute,
                  int64 value) {
  if (!element->HasAttr(attribute))
    return false;
  element->AddAttr(attribute, Int64ToString(value).c_str());
  return true;
}

bool SetIntAttr(buzz::XmlElement* element, const buzz::QName& attribute,
                int value) {
  if (!element->HasAttr(attribute))
    return false;
  element->AddAttr(attribute, IntToString(value).c_str());
  return true;
}

bool SetBoolAttr(buzz::XmlElement* element, const buzz::QName& attribute,
                 bool value) {
  int int_value = 0;
  if (value) {
    int_value = 1;
  }
  return SetIntAttr(element, attribute, int_value);
}

bool SetStringAttr(buzz::XmlElement* element, const buzz::QName& attribute,
                   const std::string& value) {
  if (!element->HasAttr(attribute))
    return false;
  element->AddAttr(attribute, value);
  return true;
}

// XmlStream.
XmlStream::XmlStream()
    : state_(talk_base::SS_OPEN),
      builder_(new buzz::XmlBuilder()),
      parser_(new buzz::XmlParser(builder_.get())) {
}

XmlStream::~XmlStream() {
}

buzz::XmlElement* XmlStream::CreateElement() {
  if (talk_base::SS_OPEN == state_) {
    Close();
  }
  return builder_->CreateElement();
}

talk_base::StreamResult XmlStream::Read(void* buffer, size_t buffer_len,
                                        size_t* read, int* error) {
  if (error)
    *error = -1;
  return talk_base::SR_ERROR;
}

talk_base::StreamResult XmlStream::Write(const void* data, size_t data_len,
                                         size_t* written, int* error) {
  if (talk_base::SS_OPEN != state_) {
    if (error)
      *error = -1;
    return talk_base::SR_ERROR;
  }
  parser_->Parse(static_cast<const char*>(data), data_len, false);
  if (written)
    *written = data_len;
  return talk_base::SR_SUCCESS;
}

void XmlStream::Close() {
  if (talk_base::SS_OPEN != state_)
    return;

  parser_->Parse(NULL, 0, true);
  state_ = talk_base::SS_CLOSED;
}

} // namespace buzz
