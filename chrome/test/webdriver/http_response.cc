// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/http_response.h"

#include "base/format_macros.h"
#include "base/string_util.h"
#include "base/stringprintf.h"

namespace webdriver {

const int HttpResponse::kOk = 200;
const int HttpResponse::kNoContent = 204;
const int HttpResponse::kSeeOther = 303;
const int HttpResponse::kNotModified = 304;
const int HttpResponse::kBadRequest = 400;
const int HttpResponse::kNotFound = 404;
const int HttpResponse::kMethodNotAllowed = 405;
const int HttpResponse::kInternalServerError = 500;

HttpResponse::HttpResponse()
    : status_(kOk) {
}

HttpResponse::~HttpResponse() {
}

void HttpResponse::AddHeader(const std::string& name,
                             const std::string& value) {
  std::string lower_case_name(StringToLowerASCII(name));
  HeaderMap::iterator header = headers_.find(lower_case_name);
  if (header == headers_.end()) {
    headers_[lower_case_name] = value;
  } else {
    header->second.append("," + value);
  }
}

bool HttpResponse::GetHeader(const std::string& name,
                             std::string* value) const {
  std::string lower_case_name(StringToLowerASCII(name));
  HeaderMap::const_iterator header = headers_.find(lower_case_name);

  if (header == headers_.end()) {
    return false;
  }

  if (value) {
    *value = header->second;
  }

  return true;
}

bool HttpResponse::RemoveHeader(const std::string& name) {
  std::string lower_case_name(StringToLowerASCII(name));
  HeaderMap::iterator header = headers_.find(lower_case_name);

  if (header == headers_.end()) {
    return false;
  }

  headers_.erase(header);
  return true;
}

void HttpResponse::ClearHeaders() {
  headers_.clear();
}

void HttpResponse::UpdateHeader(const std::string& name,
                                const std::string& new_value) {
  RemoveHeader(name);
  AddHeader(name, new_value);
}

void HttpResponse::SetMimeType(const std::string& mime_type) {
  UpdateHeader("Content-Type", mime_type);
}

void HttpResponse::SetBody(const std::string& data) {
  SetBody(data.data(), data.length());
}

void HttpResponse::SetBody(const char* const data, size_t length) {
  data_ = std::string(data, length);
  UpdateHeader("Content-Length",
               base::StringPrintf("%"PRIuS"", data_.length()));
}

std::string HttpResponse::GetReasonPhrase() const {
  switch (status_) {
    case kOk:
      return "OK";
    case kNoContent:
      return "No Content";
    case kSeeOther:
      return "See Other";
    case kNotModified:
      return "Not Modified";
    case kBadRequest:
      return "Bad Request";
    case kNotFound:
      return "Not Found";
    case kMethodNotAllowed:
      return "Method Not Allowed";
    case kInternalServerError:
      return "Internal Server Error";
    default:
      return "Unknown";
  }
}

int HttpResponse::status() const {
  return status_;
}

void HttpResponse::set_status(int status) {
  status_ = status;
}

const HttpResponse::HeaderMap* const HttpResponse::headers() const {
  return &headers_;
}

const char* const HttpResponse::data() const {
  return data_.data();
}

size_t HttpResponse::length() const {
  return data_.length();
}

}  // namespace webdriver
