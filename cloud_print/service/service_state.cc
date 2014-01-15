// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cloud_print/service/service_state.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "net/base/escape.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/base/request_priority.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/base/upload_data_stream.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"

namespace {

const char kCloudPrintJsonName[] = "cloud_print";
const char kEnabledOptionName[] = "enabled";

const char kEmailOptionName[] = "email";
const char kProxyIdOptionName[] = "proxy_id";
const char kRobotEmailOptionName[] = "robot_email";
const char kRobotTokenOptionName[] = "robot_refresh_token";
const char kAuthTokenOptionName[] = "auth_token";
const char kXmppAuthTokenOptionName[] = "xmpp_auth_token";

const char kClientLoginUrl[] = "https://www.google.com/accounts/ClientLogin";

const int64 kRequestTimeoutMs = 10 * 1000;

class ServiceStateURLRequestDelegate : public net::URLRequest::Delegate {
 public:
  virtual void OnResponseStarted(net::URLRequest* request) OVERRIDE {
    if (request->GetResponseCode() == 200) {
      Read(request);
      if (request->status().is_io_pending())
        return;
    }
    request->Cancel();
  }

  virtual void OnReadCompleted(net::URLRequest* request,
                               int bytes_read) OVERRIDE {
    Read(request);
    if (!request->status().is_io_pending())
      base::MessageLoop::current()->Quit();
  }

  const std::string& data() const {
    return data_;
  }

 private:
  void Read(net::URLRequest* request) {
    // Read as many bytes as are available synchronously.
    const int kBufSize = 100000;
    scoped_refptr<net::IOBuffer> buf(new net::IOBuffer(kBufSize));
    int num_bytes = 0;
    while (request->Read(buf.get(), kBufSize, &num_bytes)) {
      data_.append(buf->data(), buf->data() + num_bytes);
    }
  }
  std::string data_;
};


void SetNotEmptyJsonString(base::DictionaryValue* dictionary,
                           const std::string& name,
                           const std::string& value) {
  if (!value.empty())
    dictionary->SetString(name, value);
}

}  // namespace

ServiceState::ServiceState() {
  Reset();
}

ServiceState::~ServiceState() {
}

void ServiceState::Reset() {
  email_.clear();
  proxy_id_.clear();
  robot_email_.clear();
  robot_token_.clear();
  auth_token_.clear();
  xmpp_auth_token_.clear();
}

bool ServiceState::FromString(const std::string& json) {
  Reset();
  scoped_ptr<base::Value> data(base::JSONReader::Read(json));
  if (!data.get())
    return false;

  const base::DictionaryValue* services = NULL;
  if (!data->GetAsDictionary(&services))
    return false;

  const base::DictionaryValue* cloud_print = NULL;
  if (!services->GetDictionary(kCloudPrintJsonName, &cloud_print))
    return false;

  bool valid_file = true;
  // Don't exit on fail. Collect all data for re-use by user later.
  if (!cloud_print->GetBoolean(kEnabledOptionName, &valid_file))
    valid_file = false;

  cloud_print->GetString(kEmailOptionName, &email_);
  cloud_print->GetString(kProxyIdOptionName, &proxy_id_);
  cloud_print->GetString(kRobotEmailOptionName, &robot_email_);
  cloud_print->GetString(kRobotTokenOptionName, &robot_token_);
  cloud_print->GetString(kAuthTokenOptionName, &auth_token_);
  cloud_print->GetString(kXmppAuthTokenOptionName, &xmpp_auth_token_);

  return valid_file && IsValid();
}

bool ServiceState::IsValid() const {
  if (email_.empty() || proxy_id_.empty())
    return false;
  bool valid_robot = !robot_email_.empty() && !robot_token_.empty();
  bool valid_auth = !auth_token_.empty() && !xmpp_auth_token_.empty();
  return valid_robot || valid_auth;
}

std::string ServiceState::ToString() {
  scoped_ptr<base::DictionaryValue> services(new base::DictionaryValue());

  scoped_ptr<base::DictionaryValue> cloud_print(new base::DictionaryValue());
  cloud_print->SetBoolean(kEnabledOptionName, true);

  SetNotEmptyJsonString(cloud_print.get(), kEmailOptionName, email_);
  SetNotEmptyJsonString(cloud_print.get(), kProxyIdOptionName, proxy_id_);
  SetNotEmptyJsonString(cloud_print.get(), kRobotEmailOptionName, robot_email_);
  SetNotEmptyJsonString(cloud_print.get(), kRobotTokenOptionName, robot_token_);
  SetNotEmptyJsonString(cloud_print.get(), kAuthTokenOptionName, auth_token_);
  SetNotEmptyJsonString(cloud_print.get(), kXmppAuthTokenOptionName,
                        xmpp_auth_token_);

  services->Set(kCloudPrintJsonName, cloud_print.release());

  std::string json;
  base::JSONWriter::WriteWithOptions(services.get(),
                                     base::JSONWriter::OPTIONS_PRETTY_PRINT,
                                     &json);
  return json;
}

std::string ServiceState::LoginToGoogle(const std::string& service,
                                        const std::string& email,
                                        const std::string& password) {
  base::MessageLoopForIO loop;

  net::URLRequestContextBuilder builder;
  scoped_ptr<net::URLRequestContext> context(builder.Build());

  ServiceStateURLRequestDelegate fetcher_delegate;
  GURL url(kClientLoginUrl);

  std::string post_body;
  post_body += "accountType=GOOGLE";
  post_body += "&Email=" + net::EscapeUrlEncodedData(email, true);
  post_body += "&Passwd=" + net::EscapeUrlEncodedData(password, true);
  post_body += "&source=" + net::EscapeUrlEncodedData("CP-Service", true);
  post_body += "&service=" + net::EscapeUrlEncodedData(service, true);

  net::URLRequest request(
      url, net::DEFAULT_PRIORITY, &fetcher_delegate, context.get());
  int load_flags = request.load_flags();
  load_flags = load_flags | net::LOAD_DO_NOT_SEND_COOKIES;
  load_flags = load_flags | net::LOAD_DO_NOT_SAVE_COOKIES;
  request.SetLoadFlags(load_flags);

  scoped_ptr<net::UploadElementReader> reader(
      net::UploadOwnedBytesElementReader::CreateWithString(post_body));
  request.set_upload(make_scoped_ptr(
      net::UploadDataStream::CreateWithReader(reader.Pass(), 0)));
  request.SetExtraRequestHeaderByName(
      "Content-Type", "application/x-www-form-urlencoded", true);
  request.set_method("POST");
  request.Start();

  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::MessageLoop::QuitClosure(),
      base::TimeDelta::FromMilliseconds(kRequestTimeoutMs));

  base::MessageLoop::current()->Run();

  const char kAuthStart[] = "Auth=";
  std::vector<std::string> lines;
  Tokenize(fetcher_delegate.data(), "\r\n", &lines);
  for (size_t i = 0; i < lines.size(); ++i) {
    if (StartsWithASCII(lines[i], kAuthStart, false))
      return lines[i].substr(arraysize(kAuthStart) - 1);
  }

  return std::string();
}

bool ServiceState::Configure(const std::string& email,
                             const std::string& password,
                             const std::string& proxy_id) {
  robot_token_.clear();
  robot_email_.clear();
  email_ = email;
  proxy_id_ = proxy_id;
  auth_token_ = LoginToGoogle("cloudprint", email_, password);
  xmpp_auth_token_ = LoginToGoogle("chromiumsync", email_, password);
  return IsValid();
}
