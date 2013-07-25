// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profile_resetter/brandcode_config_fetcher.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profile_resetter/brandcoded_default_settings.h"
#include "libxml/parser.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_status.h"

namespace {

const int kDownloadTimeoutSec = 10;
const char kPostXml[] =
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"<request version=\"1.3.17.0\" protocol=\"3.0\" testsource=\"dev\" "
    "shell_version=\"1.2.3.5\">\n"
"  <os platform=\"win\" version=\"6.1\" sp=\"\" arch=\"x86\" />\n"
"  <app\n"
"    appid=\"{8A69D345-D564-463C-AFF1-A69D9E530F96}\"\n"
"    version=\"0.0.0.0\"\n"
"      >\n"
"    <updatecheck />\n"
"    <data name=\"install\" "
    "index=\"__BRANDCODE_PLACEHOLDER__\" />\n"
"  </app>\n"
"</request>";

// Returns the query to the server which can be used to retrieve the config.
// |brand| is a brand code, it mustn't be empty.
std::string GetUploadData(const std::string& brand) {
  DCHECK(!brand.empty());
  std::string data(kPostXml);
  const std::string placeholder("__BRANDCODE_PLACEHOLDER__");
  size_t placeholder_pos = data.find(placeholder);
  DCHECK(placeholder_pos != std::string::npos);
  data.replace(placeholder_pos, placeholder.size(), brand);
  return data;
}

// Extracts json master prefs from xml.
class XmlConfigParser {
 public:
  XmlConfigParser();
  ~XmlConfigParser();

  // Returns the content of /response/app/data tag.
  static void Parse(const std::string& input_buffer,
                    std::string* output_buffer);

 private:
  static XmlConfigParser* FromContext(void* ctx);
  static std::string XMLCharToString(const xmlChar* value);
  static void StartElementImpl(void* ctx,
                               const xmlChar* name,
                               const xmlChar** atts);
  static void EndElementImpl(void* ctx, const xmlChar* name);
  static void CharactersImpl(void* ctx, const xmlChar* ch, int len);

  bool IsParsingData() const;

  // Extracted json file.
  std::string master_prefs_;

  // Current stack of the elements being parsed.
  std::vector<std::string> elements_;

  DISALLOW_COPY_AND_ASSIGN(XmlConfigParser);
};

XmlConfigParser::XmlConfigParser() {}

XmlConfigParser::~XmlConfigParser() {}

void XmlConfigParser::Parse(const std::string& input_buffer,
                            std::string* output_buffer) {
  using logging::LOG_WARNING;

  DCHECK(output_buffer);
  xmlSAXHandler sax_handler = {};
  sax_handler.startElement = &XmlConfigParser::StartElementImpl;
  sax_handler.endElement = &XmlConfigParser::EndElementImpl;
  sax_handler.characters = &XmlConfigParser::CharactersImpl;
  XmlConfigParser parser;
  int error = xmlSAXUserParseMemory(&sax_handler,
                                    &parser,
                                    input_buffer.c_str(),
                                    input_buffer.size());
  if (error) {
    VLOG(LOG_WARNING) << "Error parsing brandcoded master prefs, err=" << error;
  } else {
    output_buffer->swap(parser.master_prefs_);
  }
}

XmlConfigParser* XmlConfigParser::FromContext(void* ctx) {
  return static_cast<XmlConfigParser*>(ctx);
}

std::string XmlConfigParser::XMLCharToString(const xmlChar* value) {
  return std::string(reinterpret_cast<const char*>(value));
}

void XmlConfigParser::StartElementImpl(void* ctx,
                                       const xmlChar* name,
                                       const xmlChar** atts) {
  std::string node_name(XMLCharToString(name));
  XmlConfigParser* context = FromContext(ctx);
  context->elements_.push_back(node_name);
  if (context->IsParsingData())
    context->master_prefs_.clear();
}

void XmlConfigParser::EndElementImpl(void* ctx, const xmlChar* name) {
  XmlConfigParser* context = FromContext(ctx);
  context->elements_.pop_back();
}

void XmlConfigParser::CharactersImpl(void* ctx, const xmlChar* ch, int len) {
  XmlConfigParser* context = FromContext(ctx);
  if (context->IsParsingData()) {
    context->master_prefs_ +=
        std::string(reinterpret_cast<const char*>(ch), len);
  }
}

bool XmlConfigParser::IsParsingData() const {
  const std::string data_path[] = {"response", "app", "data"};
  return elements_.size() == arraysize(data_path) &&
         std::equal(elements_.begin(), elements_.end(), data_path);
}

} // namespace

BrandcodeConfigFetcher::BrandcodeConfigFetcher(const FetchCallback& callback,
                                               const GURL& url,
                                               const std::string& brandcode)
    : fetch_callback_(callback) {
  DCHECK(!brandcode.empty());
  config_fetcher_.reset(net::URLFetcher::Create(0 /* ID used for testing */,
                                                url,
                                                net::URLFetcher::POST,
                                                this));
  config_fetcher_->SetRequestContext(
      g_browser_process->system_request_context());
  config_fetcher_->SetUploadData("text/xml", GetUploadData(brandcode));
  config_fetcher_->AddExtraRequestHeader("Accept: text/xml");
  config_fetcher_->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
                                net::LOAD_DO_NOT_SAVE_COOKIES |
                                net::LOAD_DISABLE_CACHE);
  config_fetcher_->Start();
  // Abort the download attempt if it takes too long.
  download_timer_.Start(FROM_HERE,
                        base::TimeDelta::FromSeconds(kDownloadTimeoutSec),
                        this,
                        &BrandcodeConfigFetcher::OnDownloadTimeout);
}

BrandcodeConfigFetcher::~BrandcodeConfigFetcher() {}

void BrandcodeConfigFetcher::SetCallback(const FetchCallback& callback) {
  fetch_callback_ = callback;
}

void BrandcodeConfigFetcher::OnURLFetchComplete(const net::URLFetcher* source) {
  if (source != config_fetcher_.get()) {
    NOTREACHED() << "Callback from foreign URL fetcher";
    return;
  }
  std::string response_string;
  std::string mime_type;
  if (config_fetcher_ &&
      config_fetcher_->GetStatus().is_success() &&
      config_fetcher_->GetResponseCode() == 200 &&
      config_fetcher_->GetResponseHeaders()->GetMimeType(&mime_type) &&
      mime_type == "text/xml" &&
      config_fetcher_->GetResponseAsString(&response_string)) {
    std::string master_prefs;
    XmlConfigParser::Parse(response_string, &master_prefs);
    default_settings_.reset(new BrandcodedDefaultSettings(master_prefs));
  }
  config_fetcher_.reset();
  download_timer_.Stop();
  fetch_callback_.Run();
}

void BrandcodeConfigFetcher::OnDownloadTimeout() {
  if (config_fetcher_) {
    config_fetcher_.reset();
    fetch_callback_.Run();
  }
}
