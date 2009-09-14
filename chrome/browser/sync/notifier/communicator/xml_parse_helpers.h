// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_NOTIFIER_COMMUNICATOR_XML_PARSE_HELPERS_H_
#define CHROME_BROWSER_SYNC_NOTIFIER_COMMUNICATOR_XML_PARSE_HELPERS_H_

#include <string>

#include "talk/base/basictypes.h"
#include "talk/base/scoped_ptr.h"
#include "talk/base/stream.h"

namespace buzz {
class XmlBuilder;
class XmlElement;
class XmlParser;
class QName;
}

namespace notifier {

buzz::XmlElement* ReadXmlFromStream(talk_base::StreamInterface* stream);
bool ParseInt64Attr(const buzz::XmlElement* element,
                    const buzz::QName& attribute, int64* result);
bool ParseIntAttr(const buzz::XmlElement* element,
                  const buzz::QName& attribute, int* result);
bool ParseBoolAttr(const buzz::XmlElement* element,
                   const buzz::QName& attribute, bool* result);
bool ParseStringAttr(const buzz::XmlElement* element,
                     const buzz::QName& attribute, std::string* result);

void WriteXmlToStream(talk_base::StreamInterface* stream,
                      const buzz::XmlElement* xml);
bool SetInt64Attr(buzz::XmlElement* element, const buzz::QName& attribute,
                  int64 result);
bool SetIntAttr(buzz::XmlElement* element, const buzz::QName& attribute,
                int result);
bool SetBoolAttr(buzz::XmlElement* element, const buzz::QName& attribute,
                 bool result);
bool SetStringAttr(buzz::XmlElement* element, const buzz::QName& attribute,
                   const std::string& result);

template<class T>
void SetAttr(buzz::XmlElement* xml, const buzz::QName& name, const T& data);

///////////////////////////////////////////////////////////////////////////////
// XmlStream
///////////////////////////////////////////////////////////////////////////////

class XmlStream : public talk_base::StreamInterface {
 public:
  XmlStream();
  virtual ~XmlStream();

  buzz::XmlElement* CreateElement();

  virtual talk_base::StreamState GetState() const { return state_; }

  virtual talk_base::StreamResult Read(void* buffer, size_t buffer_len,
                                       size_t* read, int* error);
  virtual talk_base::StreamResult Write(const void* data, size_t data_len,
                                        size_t* written, int* error);
  virtual void Close();

 private:
  talk_base::StreamState state_;
  scoped_ptr<buzz::XmlBuilder> builder_;
  scoped_ptr<buzz::XmlParser> parser_;
};

///////////////////////////////////////////////////////////////////////////////

}  // namespace buzz

#endif  // CHROME_BROWSER_SYNC_NOTIFIER_COMMUNICATOR_XML_PARSE_HELPERS_H_
