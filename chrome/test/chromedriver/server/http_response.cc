// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/server/http_response.h"

#include "base/format_macros.h"
#include "base/string_util.h"
#include "base/stringprintf.h"

const int HttpResponse::kOk = 200;
const int HttpResponse::kNoContent = 204;
const int HttpResponse::kSeeOther = 303;
const int HttpResponse::kNotModified = 304;
const int HttpResponse::kBadRequest = 400;
const int HttpResponse::kForbidden = 403;
const int HttpResponse::kNotFound = 404;
const int HttpResponse::kMethodNotAllowed = 405;
const int HttpResponse::kInternalServerError = 500;
const int HttpResponse::kNotImplemented = 501;

namespace {

const char* kContentLengthHeader = "content-length";

}  // namespace

HttpResponse::HttpResponse()
    : status_(kOk) {
}

HttpResponse::HttpResponse(int status)
    : status_(status) {
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

void HttpResponse::SetMimeType(const std::string& mime_type) {
  UpdateHeader("Content-Type", mime_type);
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
    case kForbidden:
      return "Forbidden";
    case kNotFound:
      return "Not Found";
    case kMethodNotAllowed:
      return "Method Not Allowed";
    case kInternalServerError:
      return "Internal Server Error";
    case kNotImplemented:
      return "Not Implemented";
    default:
      return "Unknown";
  }
}

void HttpResponse::GetData(std::string* data) const {
  *data += base::StringPrintf("HTTP/1.1 %d %s\r\n",
      status_, GetReasonPhrase().c_str());

  typedef HttpResponse::HeaderMap::const_iterator HeaderIter;
  for (HeaderIter header = headers_.begin(); header != headers_.end();
       ++header) {
    *data += header->first + ":" + header->second + "\r\n";
  }
  std::string length;
  if (!GetHeader(kContentLengthHeader, &length)) {
    *data += base::StringPrintf(
        "%s:%"PRIuS"\r\n",
        kContentLengthHeader, body_.length());
  }
  *data += "\r\n";

  if (body_.length())
    *data += body_;
}

int HttpResponse::status() const {
  return status_;
}

void HttpResponse::set_status(int status) {
  status_ = status;
}

const std::string& HttpResponse::body() const {
  return body_;
}

void HttpResponse::set_body(const std::string& body) {
  body_ = body;
}

void HttpResponse::UpdateHeader(const std::string& name,
                                const std::string& new_value) {
  RemoveHeader(name);
  AddHeader(name, new_value);
}
