// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/fake_gaia.h"

#include "base/base_paths.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "url/url_parse.h"

using namespace net::test_server;

namespace {
const base::FilePath::CharType kServiceLogin[] =
    FILE_PATH_LITERAL("google_apis/test/service_login.html");
}

FakeGaia::AccessTokenInfo::AccessTokenInfo()
  : expires_in(3600) {}

FakeGaia::AccessTokenInfo::~AccessTokenInfo() {}

FakeGaia::FakeGaia() {
  base::FilePath source_root_dir;
  PathService::Get(base::DIR_SOURCE_ROOT, &source_root_dir);
  CHECK(base::ReadFileToString(
      source_root_dir.Append(base::FilePath(kServiceLogin)),
      &service_login_response_));
}

FakeGaia::~FakeGaia() {}

scoped_ptr<HttpResponse> FakeGaia::HandleRequest(const HttpRequest& request) {
  GaiaUrls* gaia_urls = GaiaUrls::GetInstance();

  // The scheme and host of the URL is actually not important but required to
  // get a valid GURL in order to parse |request.relative_url|.
  GURL request_url = GURL("http://localhost").Resolve(request.relative_url);
  std::string request_path = request_url.path();

  scoped_ptr<BasicHttpResponse> http_response(new BasicHttpResponse());
  if (request_path == gaia_urls->service_login_url().path()) {
    http_response->set_code(net::HTTP_OK);
    http_response->set_content(service_login_response_);
    http_response->set_content_type("text/html");
  } else if (request_path == gaia_urls->service_login_auth_url().path()) {
    std::string continue_url = gaia_urls->service_login_url().spec();
    GetQueryParameter(request.content, "continue", &continue_url);
    http_response->set_code(net::HTTP_OK);
    const std::string redirect_js =
        "document.location.href = '" + continue_url + "'";
    http_response->set_content(
        "<HTML><HEAD><SCRIPT>\n" + redirect_js + "\n</SCRIPT></HEAD></HTML>");
    http_response->set_content_type("text/html");
  } else if (request_path == gaia_urls->oauth2_token_url().path()) {
    std::string refresh_token;
    std::string client_id;
    std::string scope;
    const AccessTokenInfo* token_info = NULL;
    GetQueryParameter(request.content, "scope", &scope);
    if (GetQueryParameter(request.content, "refresh_token", &refresh_token) &&
        GetQueryParameter(request.content, "client_id", &client_id) &&
        (token_info = GetAccessTokenInfo(refresh_token, client_id, scope))) {
      base::DictionaryValue response_dict;
      response_dict.SetString("access_token", token_info->token);
      response_dict.SetInteger("expires_in", 3600);
      FormatJSONResponse(response_dict, http_response.get());
    } else {
      http_response->set_code(net::HTTP_BAD_REQUEST);
    }
  } else if (request_path == gaia_urls->oauth2_token_info_url().path()) {
    const AccessTokenInfo* token_info = NULL;
    std::string access_token;
    if (GetQueryParameter(request.content, "access_token", &access_token)) {
      for (AccessTokenInfoMap::const_iterator entry(
               access_token_info_map_.begin());
           entry != access_token_info_map_.end();
           ++entry) {
        if (entry->second.token == access_token) {
          token_info = &(entry->second);
          break;
        }
      }
    }

    if (token_info) {
      base::DictionaryValue response_dict;
      response_dict.SetString("issued_to", token_info->issued_to);
      response_dict.SetString("audience", token_info->audience);
      response_dict.SetString("user_id", token_info->user_id);
      std::vector<std::string> scope_vector(token_info->scopes.begin(),
                                            token_info->scopes.end());
      response_dict.SetString("scope", JoinString(scope_vector, " "));
      response_dict.SetInteger("expires_in", token_info->expires_in);
      response_dict.SetString("email", token_info->email);
      FormatJSONResponse(response_dict, http_response.get());
    } else {
      http_response->set_code(net::HTTP_BAD_REQUEST);
    }
  } else if (request_path == gaia_urls->oauth2_issue_token_url().path()) {
    std::string access_token;
    std::map<std::string, std::string>::const_iterator auth_header_entry =
        request.headers.find("Authorization");
    if (auth_header_entry != request.headers.end()) {
      if (StartsWithASCII(auth_header_entry->second, "Bearer ", true))
        access_token = auth_header_entry->second.substr(7);
    }

    std::string scope;
    std::string client_id;
    const AccessTokenInfo* token_info = NULL;
    if (GetQueryParameter(request.content, "scope", &scope) &&
        GetQueryParameter(request.content, "client_id", &client_id) &&
        (token_info = GetAccessTokenInfo(access_token, client_id, scope))) {
      base::DictionaryValue response_dict;
      response_dict.SetString("issueAdvice", "auto");
      response_dict.SetString("expiresIn",
                              base::IntToString(token_info->expires_in));
      response_dict.SetString("token", token_info->token);
      FormatJSONResponse(response_dict, http_response.get());
    } else {
      http_response->set_code(net::HTTP_BAD_REQUEST);
    }
  } else {
    // Request not understood.
    return scoped_ptr<HttpResponse>();
  }

  return http_response.PassAs<HttpResponse>();
}

void FakeGaia::IssueOAuthToken(const std::string& auth_token,
                               const AccessTokenInfo& token_info) {
  access_token_info_map_.insert(std::make_pair(auth_token, token_info));
}

void FakeGaia::FormatJSONResponse(const base::DictionaryValue& response_dict,
                                  BasicHttpResponse* http_response) {
  std::string response_json;
  base::JSONWriter::Write(&response_dict, &response_json);
  http_response->set_content(response_json);
  http_response->set_code(net::HTTP_OK);
}

const FakeGaia::AccessTokenInfo* FakeGaia::GetAccessTokenInfo(
    const std::string& auth_token,
    const std::string& client_id,
    const std::string& scope_string) const {
  if (auth_token.empty() || client_id.empty())
    return NULL;

  std::vector<std::string> scope_list;
  base::SplitString(scope_string, ' ', &scope_list);
  ScopeSet scopes(scope_list.begin(), scope_list.end());

  for (AccessTokenInfoMap::const_iterator entry(
           access_token_info_map_.lower_bound(auth_token));
       entry != access_token_info_map_.upper_bound(auth_token);
       ++entry) {
    if (entry->second.audience == client_id &&
        (scope_string.empty() || entry->second.scopes == scopes)) {
      return &(entry->second);
    }
  }

  return NULL;
}

// static
bool FakeGaia::GetQueryParameter(const std::string& query,
                                 const std::string& key,
                                 std::string* value) {
  // Name and scheme actually don't matter, but are required to get a valid URL
  // for parsing.
  GURL query_url("http://localhost?" + query);
  return net::GetValueForKeyInQuery(query_url, key, value);
}
