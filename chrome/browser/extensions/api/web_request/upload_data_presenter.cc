// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/web_request/upload_data_presenter.h"

#include "base/file_path.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/web_request/form_data_parser.h"
#include "chrome/browser/extensions/api/web_request/web_request_api_constants.h"
#include "net/base/upload_element.h"
#include "net/url_request/url_request.h"

using base::BinaryValue;
using base::DictionaryValue;
using base::ListValue;
using base::StringValue;
using base::Value;

namespace keys = extension_web_request_api_constants;

namespace {

// Takes |dictionary| of <string, list of strings> pairs, and gets the list
// for |key|, creating it if necessary.
ListValue* GetOrCreateList(DictionaryValue* dictionary,
                           const std::string& key) {
  ListValue* list = NULL;
  if (!dictionary->GetList(key, &list)) {
    list = new ListValue();
    dictionary->Set(key, list);
  }
  return list;
}

}  // namespace

namespace extensions {

// Implementation of UploadDataPresenter.

UploadDataPresenter::~UploadDataPresenter() {}

// Implementation of RawDataPresenter.

RawDataPresenter::RawDataPresenter()
  : success_(true),
    list_(new base::ListValue) {
}
RawDataPresenter::~RawDataPresenter() {}

void RawDataPresenter::FeedNext(const net::UploadElement& element) {
  if (!success_)
    return;

  switch (element.type()) {
    case net::UploadElement::TYPE_BYTES:
      FeedNextBytes(element.bytes(), element.bytes_length());
      break;
    case net::UploadElement::TYPE_FILE:
      // Insert the file path instead of the contents, which may be too large.
      FeedNextFile(element.file_path().AsUTF8Unsafe());
      break;
  }
}

bool RawDataPresenter::Succeeded() {
  return success_;
}

scoped_ptr<Value> RawDataPresenter::Result() {
  if (!success_)
    return scoped_ptr<Value>();

  return list_.PassAs<Value>();
}

// static
void RawDataPresenter::AppendResultWithKey(
    ListValue* list, const char* key, Value* value) {
  DictionaryValue* dictionary = new DictionaryValue;
  dictionary->Set(key, value);
  list->Append(dictionary);
}

void RawDataPresenter::Abort() {
  success_ = false;
  list_.reset();
}

void RawDataPresenter::FeedNextBytes(const char* bytes, size_t size) {
  AppendResultWithKey(list_.get(), keys::kRequestBodyRawBytesKey,
                      BinaryValue::CreateWithCopiedBuffer(bytes, size));
}

void RawDataPresenter::FeedNextFile(const std::string& filename) {
  // Insert the file path instead of the contents, which may be too large.
  AppendResultWithKey(list_.get(), keys::kRequestBodyRawFileKey,
                      Value::CreateStringValue(filename));
}

// Implementation of ParsedDataPresenter.

ParsedDataPresenter::ParsedDataPresenter(const net::URLRequest* request)
  : parser_(FormDataParser::Create(request)),
    success_(parser_.get() != NULL),
    dictionary_(success_ ? new DictionaryValue() : NULL) {
}

ParsedDataPresenter::~ParsedDataPresenter() {}

void ParsedDataPresenter::FeedNext(const net::UploadElement& element) {
  if (!success_)
    return;

  if (element.type() != net::UploadElement::TYPE_BYTES) {
    return;
  }
  if (!parser_->SetSource(base::StringPiece(element.bytes(),
                                            element.bytes_length()))) {
    Abort();
    return;
  }

  FormDataParser::Result result;
  while (parser_->GetNextNameValue(&result)) {
    GetOrCreateList(dictionary_.get(), result.name())->Append(
        new StringValue(result.value()));
  }
}

bool ParsedDataPresenter::Succeeded() {
  if (success_ && !parser_->AllDataReadOK())
    Abort();
  return success_;
}

scoped_ptr<Value> ParsedDataPresenter::Result() {
  if (!success_)
    return scoped_ptr<Value>();

  return dictionary_.PassAs<Value>();
}

void ParsedDataPresenter::Abort() {
  success_ = false;
  dictionary_.reset();
  parser_.reset();
}

}  // namespace extensions
