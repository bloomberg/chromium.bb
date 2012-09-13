// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_WEB_REQUEST_FORM_DATA_PARSER_H_
#define CHROME_BROWSER_EXTENSIONS_API_WEB_REQUEST_FORM_DATA_PARSER_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
// Cannot forward declare StringPiece because it is a typedef.
#include "base/string_piece.h"

namespace net {
class URLRequest;
}

namespace extensions {

// Interface for the form data parsers.
class FormDataParser {
 public:
  class Result {
   public:
    Result();
    ~Result();
    const std::string& name() const {
      return name_;
    }
    const std::string& value() const {
      return value_;
    }
    void set_name(const base::StringPiece& str) {
      str.CopyToString(&name_);
    }
    void set_value(const base::StringPiece& str) {
      str.CopyToString(&value_);
    }
    void set_name(const std::string& str) {
      name_ = str;
    }
    void set_value(const std::string& str) {
      value_ = str;
    }
    void Reset();

   private:
    std::string name_;
    std::string value_;

    DISALLOW_COPY_AND_ASSIGN(Result);
  };

  virtual ~FormDataParser();

  // Creates a correct parser instance based on the |request|. Returns NULL
  // on failure.
  static scoped_ptr<FormDataParser> Create(const net::URLRequest* request);

  // Creates a correct parser instance based on |content_type_header|, the
  // "Content-Type" request header value. If |content_type_header| is NULL, it
  // defaults to "application/x-www-form-urlencoded". Returns NULL on failure.
  static scoped_ptr<FormDataParser> Create(
      const std::string* content_type_header);

  // Returns true if there was some data, it was well formed and all was read.
  virtual bool AllDataReadOK() = 0;

  // Returns the next name-value pair as |result|. After SetSource has
  // succeeded, this allows to iterate over all pairs in the source.
  // Returns true as long as a new pair was successfully found.
  virtual bool GetNextNameValue(Result* result) = 0;

  // Sets the |source| of the data to be parsed. The ownership is left with the
  // caller and the source should live until |this| dies or |this->SetSource()|
  // is called again, whichever comes sooner. Returns true on success.
  virtual bool SetSource(const base::StringPiece& source) = 0;

 protected:
  FormDataParser();

 private:
  DISALLOW_COPY_AND_ASSIGN(FormDataParser);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_WEB_REQUEST_FORM_DATA_PARSER_H_
