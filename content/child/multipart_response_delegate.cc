// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/multipart_response_delegate.h"

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "net/base/net_util.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "third_party/WebKit/public/platform/WebHTTPHeaderVisitor.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/WebURLLoaderClient.h"

using blink::WebHTTPHeaderVisitor;
using blink::WebString;
using blink::WebURLLoader;
using blink::WebURLLoaderClient;
using blink::WebURLResponse;

namespace content {

namespace {

// The list of response headers that we do not copy from the original
// response when generating a WebURLResponse for a MIME payload.
const char* kReplaceHeaders[] = {
  "content-type",
  "content-length",
  "content-disposition",
  "content-range",
  "range",
  "set-cookie"
};

class HeaderCopier : public WebHTTPHeaderVisitor {
 public:
  HeaderCopier(WebURLResponse* response)
      : response_(response) {
  }
  void visitHeader(const WebString& name, const WebString& value) override {
    const std::string& name_utf8 = name.utf8();
    for (size_t i = 0; i < arraysize(kReplaceHeaders); ++i) {
      if (base::LowerCaseEqualsASCII(name_utf8, kReplaceHeaders[i]))
        return;
    }
    response_->setHTTPHeaderField(name, value);
  }
 private:
  WebURLResponse* response_;
};

}  // namespace

MultipartResponseDelegate::MultipartResponseDelegate(
    WebURLLoaderClient* client,
    WebURLLoader* loader,
    const WebURLResponse& response,
    const std::string& boundary)
    : client_(client),
      loader_(loader),
      original_response_(response),
      encoded_data_length_(0),
      boundary_("--"),
      first_received_data_(true),
      processing_headers_(false),
      stop_sending_(false),
      has_sent_first_response_(false) {
  // Some servers report a boundary prefixed with "--".  See bug 5786.
  if (base::StartsWith(boundary, "--", base::CompareCase::SENSITIVE)) {
    boundary_.assign(boundary);
  } else {
    boundary_.append(boundary);
  }
}

void MultipartResponseDelegate::OnReceivedData(const char* data,
                                               int data_len,
                                               int encoded_data_length) {
  // stop_sending_ means that we've already received the final boundary token.
  // The server should stop sending us data at this point, but if it does, we
  // just throw it away.
  if (stop_sending_)
    return;

  data_.append(data, data_len);
  encoded_data_length_ += encoded_data_length;
  if (first_received_data_) {
    // Some servers don't send a boundary token before the first chunk of
    // data.  We handle this case anyway (Gecko does too).
    first_received_data_ = false;

    // Eat leading \r\n
    int pos = PushOverLine(data_, 0);
    if (pos)
      data_ = data_.substr(pos);

    if (data_.length() < boundary_.length() + 2) {
      // We don't have enough data yet to make a boundary token.  Just wait
      // until the next chunk of data arrives.
      first_received_data_ = true;
      return;
    }

    if (0 != data_.compare(0, boundary_.length(), boundary_)) {
      data_ = boundary_ + "\n" + data_;
    }
  }
  DCHECK(!first_received_data_);

  // Headers
  if (processing_headers_) {
    // Eat leading \r\n
    int pos = PushOverLine(data_, 0);
    if (pos)
      data_ = data_.substr(pos);

    if (ParseHeaders()) {
      // Successfully parsed headers.
      processing_headers_ = false;
    } else {
      // Get more data before trying again.
      return;
    }
  }
  DCHECK(!processing_headers_);

  size_t boundary_pos;
  while ((boundary_pos = FindBoundary()) != std::string::npos) {
    if (client_) {
      // Strip out trailing \n\r characters in the buffer preceding the
      // boundary on the same lines as Firefox.
      size_t data_length = boundary_pos;
      if (boundary_pos > 0 && data_[boundary_pos - 1] == '\n') {
        data_length--;
        if (boundary_pos > 1 && data_[boundary_pos - 2] == '\r') {
          data_length--;
        }
      }
      if (data_length > 0) {
        // Send the last data chunk.
        client_->didReceiveData(loader_,
                                data_.data(),
                                static_cast<int>(data_length),
                                encoded_data_length_);
        encoded_data_length_ = 0;
      }
    }
    size_t boundary_end_pos = boundary_pos + boundary_.length();
    if (boundary_end_pos < data_.length() && '-' == data_[boundary_end_pos]) {
      // This was the last boundary so we can stop processing.
      stop_sending_ = true;
      data_.clear();
      return;
    }

    // We can now throw out data up through the boundary
    int offset = PushOverLine(data_, boundary_end_pos);
    data_ = data_.substr(boundary_end_pos + offset);

    // Ok, back to parsing headers
    if (!ParseHeaders()) {
      processing_headers_ = true;
      break;
    }
  }

  // At this point, we should send over any data we have, but keep enough data
  // buffered to handle a boundary that may have been truncated.
  if (!processing_headers_ && data_.length() > boundary_.length()) {
    // If the last character is a new line character, go ahead and just send
    // everything we have buffered.  This matches an optimization in Gecko.
    int send_length = data_.length() - boundary_.length();
    if (data_.back() == '\n')
      send_length = data_.length();
    if (client_)
      client_->didReceiveData(loader_,
                              data_.data(),
                              send_length,
                              encoded_data_length_);
    data_ = data_.substr(send_length);
    encoded_data_length_ = 0;
  }
}

void MultipartResponseDelegate::OnCompletedRequest() {
  // If we have any pending data and we're not in a header, go ahead and send
  // it to WebCore.
  if (!processing_headers_ && !data_.empty() && !stop_sending_ && client_) {
    client_->didReceiveData(loader_,
                            data_.data(),
                            static_cast<int>(data_.length()),
                            encoded_data_length_);
    encoded_data_length_ = 0;
  }
}

int MultipartResponseDelegate::PushOverLine(const std::string& data,
                                            size_t pos) {
  int offset = 0;
  if (pos < data.length() && (data[pos] == '\r' || data[pos] == '\n')) {
    ++offset;
    if (pos + 1 < data.length() && data[pos + 1] == '\n')
      ++offset;
  }
  return offset;
}

bool MultipartResponseDelegate::ParseHeaders() {
  int headers_end_pos = net::HttpUtil::LocateEndOfAdditionalHeaders(
      data_.c_str(), data_.size(), 0);

  if (headers_end_pos < 0)
    return false;

  // Eat headers and prepend a status line as is required by
  // HttpResponseHeaders.
  std::string headers("HTTP/1.1 200 OK\r\n");
  headers.append(data_, 0, headers_end_pos);
  data_ = data_.substr(headers_end_pos);

  scoped_refptr<net::HttpResponseHeaders> response_headers =
      new net::HttpResponseHeaders(
          net::HttpUtil::AssembleRawHeaders(headers.c_str(), headers.size()));

  // Create a WebURLResponse based on the original set of headers + the
  // replacement headers. We only replace the same few headers that gecko
  // does. See netwerk/streamconv/converters/nsMultiMixedConv.cpp.
  WebURLResponse response(original_response_.url());

  std::string mime_type;
  response_headers->GetMimeType(&mime_type);
  response.setMIMEType(WebString::fromUTF8(mime_type));

  std::string charset;
  response_headers->GetCharset(&charset);
  response.setTextEncodingName(WebString::fromUTF8(charset));

  // Copy the response headers from the original response.
  HeaderCopier copier(&response);
  original_response_.visitHTTPHeaderFields(&copier);

  // Replace original headers with multipart headers listed in kReplaceHeaders.
  for (size_t i = 0; i < arraysize(kReplaceHeaders); ++i) {
    std::string name(kReplaceHeaders[i]);
    std::string value;
    size_t iterator = 0;
    while (response_headers->EnumerateHeader(&iterator, name, &value)) {
      response.addHTTPHeaderField(WebString::fromLatin1(name),
                                  WebString::fromLatin1(value));
    }
  }
  // To avoid recording every multipart load as a separate visit in
  // the history database, we want to keep track of whether the response
  // is part of a multipart payload.  We do want to record the first visit,
  // so we only set isMultipartPayload to true after the first visit.
  response.setIsMultipartPayload(has_sent_first_response_);
  has_sent_first_response_ = true;
  // Send the response!
  if (client_)
    client_->didReceiveResponse(loader_, response);

  return true;
}

// Boundaries are supposed to be preceeded with --, but it looks like gecko
// doesn't require the dashes to exist.  See nsMultiMixedConv::FindToken.
size_t MultipartResponseDelegate::FindBoundary() {
  size_t boundary_pos = data_.find(boundary_);
  if (boundary_pos != std::string::npos) {
    // Back up over -- for backwards compat
    // TODO(tc): Don't we only want to do this once?  Gecko code doesn't seem
    // to care.
    if (boundary_pos >= 2) {
      if ('-' == data_[boundary_pos - 1] && '-' == data_[boundary_pos - 2]) {
        boundary_pos -= 2;
        boundary_ = "--" + boundary_;
      }
    }
  }
  return boundary_pos;
}

bool MultipartResponseDelegate::ReadContentRanges(
    const WebURLResponse& response,
    int64_t* content_range_lower_bound,
    int64_t* content_range_upper_bound,
    int64_t* content_range_instance_size) {
  std::string content_range = response.httpHeaderField("Content-Range").utf8();
  if (content_range.empty()) {
    content_range = response.httpHeaderField("Range").utf8();
  }

  if (content_range.empty()) {
    DLOG(WARNING) << "Failed to read content range from response.";
    return false;
  }

  size_t byte_range_lower_bound_start_offset = content_range.find(" ");
  if (byte_range_lower_bound_start_offset == std::string::npos) {
    return false;
  }

  // Skip over the initial space.
  byte_range_lower_bound_start_offset++;

  // Find the lower bound.
  size_t byte_range_lower_bound_end_offset =
      content_range.find("-", byte_range_lower_bound_start_offset);
  if (byte_range_lower_bound_end_offset == std::string::npos) {
    return false;
  }

  size_t byte_range_lower_bound_characters =
      byte_range_lower_bound_end_offset - byte_range_lower_bound_start_offset;
  std::string byte_range_lower_bound =
      content_range.substr(byte_range_lower_bound_start_offset,
                           byte_range_lower_bound_characters);

  // Find the upper bound.
  size_t byte_range_upper_bound_start_offset =
      byte_range_lower_bound_end_offset + 1;

  size_t byte_range_upper_bound_end_offset =
      content_range.find("/", byte_range_upper_bound_start_offset);
  if (byte_range_upper_bound_end_offset == std::string::npos) {
    return false;
  }

  size_t byte_range_upper_bound_characters =
      byte_range_upper_bound_end_offset - byte_range_upper_bound_start_offset;
  std::string byte_range_upper_bound =
      content_range.substr(byte_range_upper_bound_start_offset,
                           byte_range_upper_bound_characters);

  // Find the instance size.
  size_t byte_range_instance_size_start_offset =
      byte_range_upper_bound_end_offset + 1;

  size_t byte_range_instance_size_end_offset =
      content_range.length();

  size_t byte_range_instance_size_characters =
      byte_range_instance_size_end_offset -
      byte_range_instance_size_start_offset;
  std::string byte_range_instance_size =
      content_range.substr(byte_range_instance_size_start_offset,
                           byte_range_instance_size_characters);

  if (!base::StringToInt64(byte_range_lower_bound, content_range_lower_bound))
    return false;
  if (!base::StringToInt64(byte_range_upper_bound, content_range_upper_bound))
    return false;
  if (!base::StringToInt64(byte_range_instance_size,
                           content_range_instance_size)) {
    return false;
  }
  return true;
}

}  // namespace content
