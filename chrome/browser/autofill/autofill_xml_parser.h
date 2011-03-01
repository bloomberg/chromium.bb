// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOFILL_XML_PARSER_H_
#define CHROME_BROWSER_AUTOFILL_AUTOFILL_XML_PARSER_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "chrome/browser/autofill/field_types.h"
#include "chrome/browser/autofill/form_structure.h"
#include "third_party/expat/files/lib/expat.h"
#include "third_party/libjingle/source/talk/xmllite/xmlparser.h"

// The base class that contains common functionality between
// AutoFillQueryXmlParser and AutoFillUploadXmlParser.
class AutoFillXmlParser : public buzz::XmlParseHandler {
 public:
  AutoFillXmlParser();

  // Returns true if no parsing errors were encountered.
  bool succeeded() const { return succeeded_; }

 private:
  // A callback for the end of an </element>, called by Expat.
  // |context| is a parsing context used to resolve element/attribute names.
  // |name| is the name of the element.
  virtual void EndElement(buzz::XmlParseContext* context, const char* name);

  // The callback for character data between tags (<element>text...</element>).
  // |context| is a parsing context used to resolve element/attribute names.
  // |text| is a pointer to the beginning of character data (not null
  // terminated).
  // |len| is the length of the string pointed to by text.
  virtual void CharacterData(buzz::XmlParseContext* context, const char* text,
                             int len);

  // The callback for parsing errors.
  // |context| is a parsing context used to resolve names.
  // |error_code| is a code representing the parsing error.
  virtual void Error(buzz::XmlParseContext* context, XML_Error error_code);

  // True if parsing succeeded.
  bool succeeded_;

  DISALLOW_COPY_AND_ASSIGN(AutoFillXmlParser);
};

// The XML parse handler for parsing AutoFill query responses.  A typical
// response looks like:
//
// <autofillqueryresponse experimentid="1">
//   <field autofilltype="0" />
//   <field autofilltype="1" />
//   <field autofilltype="3" />
//   <field autofilltype="2" />
// </autofillqueryresponse>
//
// Fields are returned in the same order they were sent to the server.
// autofilltype: The server's guess at what type of field this is.  0 is
// unknown, other types are documented in chrome/browser/autofill/field_types.h.
class AutoFillQueryXmlParser : public AutoFillXmlParser {
 public:
  AutoFillQueryXmlParser(std::vector<AutofillFieldType>* field_types,
                         UploadRequired* upload_required,
                         std::string* experiment_id);

 private:
  // A callback for the beginning of a new <element>, called by Expat.
  // |context| is a parsing context used to resolve element/attribute names.
  // |name| is the name of the element.
  // |attrs| is the list of attributes (names and values) for the element.
  virtual void StartElement(buzz::XmlParseContext* context, const char* name,
                            const char** attrs);

  // A helper function to retrieve integer values from strings.  Raises an
  // XML parse error if it fails.
  // |context| is the current parsing context.
  // |value| is the string to convert.
  int GetIntValue(buzz::XmlParseContext* context, const char* attribute);

  // The parsed field types.
  std::vector<AutofillFieldType>* field_types_;

  // A flag indicating whether the client should upload AutoFill data when this
  // form is submitted.
  UploadRequired* upload_required_;

  // The server experiment to which this query response belongs.
  // For the default server implementation, this is empty.
  std::string* experiment_id_;

  DISALLOW_COPY_AND_ASSIGN(AutoFillQueryXmlParser);
};

// The XML parser for handling AutoFill upload responses.  Typical upload
// responses look like:
//
// <autofilluploadresponse negativeuploadrate="0.00125" positiveuploadrate="1"/>
//
// The positive upload rate is the percentage of uploads to send to the server
// when something in the users profile matches what they have entered in a form.
// The negative upload rate is the percentage of uploads to send when nothing in
// the form matches what's in the users profile.
// The negative upload rate is typically much lower than the positive upload
// rate.
class AutoFillUploadXmlParser : public AutoFillXmlParser {
 public:
  AutoFillUploadXmlParser(double* positive_upload_rate,
                          double* negative_upload_rate);

 private:
  // A callback for the beginning of a new <element>, called by Expat.
  // |context| is a parsing context used to resolve element/attribute names.
  // |name| is the name of the element.
  // |attrs| is the list of attributes (names and values) for the element.
  virtual void StartElement(buzz::XmlParseContext* context, const char* name,
                            const char** attrs);

  // A helper function to retrieve double values from strings.  Raises an XML
  // parse error if it fails.
  // |context| is the current parsing context.
  // |value| is the string to convert.
  double GetDoubleValue(buzz::XmlParseContext* context, const char* attribute);

  // True if parsing succeeded.
  bool succeeded_;

  double* positive_upload_rate_;
  double* negative_upload_rate_;

  DISALLOW_COPY_AND_ASSIGN(AutoFillUploadXmlParser);
};

#endif  // CHROME_BROWSER_AUTOFILL_AUTOFILL_XML_PARSER_H_
