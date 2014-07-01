// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_in_memory_cache.h"

#include "base/files/file_enumerator.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "net/http/http_util.h"
#include "url/gurl.h"

using base::FilePath;
using base::StringPiece;
using std::string;

// Specifies the directory used during QuicInMemoryCache
// construction to seed the cache. Cache directory can be
// generated using `wget -p --save-headers <url>

namespace net {

FilePath::StringType g_quic_in_memory_cache_dir = FILE_PATH_LITERAL("");

QuicInMemoryCache::Response::Response() : response_type_(REGULAR_RESPONSE) {
}

QuicInMemoryCache::Response::~Response() {
}

// static
QuicInMemoryCache* QuicInMemoryCache::GetInstance() {
  return Singleton<QuicInMemoryCache>::get();
}

const QuicInMemoryCache::Response* QuicInMemoryCache::GetResponse(
    const GURL& url) const {
  ResponseMap::const_iterator it = responses_.find(GetKey(url));
  if (it == responses_.end()) {
    return NULL;
  }
  return it->second;
}

void QuicInMemoryCache::AddSimpleResponse(StringPiece path,
                                          StringPiece version,
                                          StringPiece response_code,
                                          StringPiece response_detail,
                                          StringPiece body) {
  GURL url("http://" + path.as_string());

  string status_line = version.as_string() + " " +
                       response_code.as_string() + " " +
                       response_detail.as_string();

  string header = "content-length: " +
                  base::Uint64ToString(static_cast<uint64>(body.length()));

  scoped_refptr<HttpResponseHeaders> response_headers =
      new HttpResponseHeaders(status_line + '\0' + header + '\0' + '\0');

  AddResponse(url, response_headers, body);
}

void QuicInMemoryCache::AddResponse(
    const GURL& url,
    scoped_refptr<HttpResponseHeaders> response_headers,
    StringPiece response_body) {
  string key = GetKey(url);
  VLOG(1) << "Adding response for: " << key;
  if (ContainsKey(responses_, key)) {
    LOG(DFATAL) << "Response for given request already exists!";
    return;
  }
  Response* new_response = new Response();
  new_response->set_headers(response_headers);
  new_response->set_body(response_body);
  responses_[key] = new_response;
}

void QuicInMemoryCache::AddSpecialResponse(StringPiece path,
                                           SpecialResponseType response_type) {
  GURL url("http://" + path.as_string());

  AddResponse(url, NULL, string());
  responses_[GetKey(url)]->response_type_ = response_type;
}

QuicInMemoryCache::QuicInMemoryCache() {
  Initialize();
}

void QuicInMemoryCache::ResetForTests() {
  STLDeleteValues(&responses_);
  Initialize();
}

void QuicInMemoryCache::Initialize() {
  // If there's no defined cache dir, we have no initialization to do.
  if (g_quic_in_memory_cache_dir.size() == 0) {
    VLOG(1) << "No cache directory found. Skipping initialization.";
    return;
  }
  VLOG(1) << "Attempting to initialize QuicInMemoryCache from directory: "
          << g_quic_in_memory_cache_dir;

  FilePath directory(g_quic_in_memory_cache_dir);
  base::FileEnumerator file_list(directory,
                                 true,
                                 base::FileEnumerator::FILES);

  FilePath file = file_list.Next();
  for (; !file.empty(); file = file_list.Next()) {
    // Need to skip files in .svn directories
    if (file.value().find(FILE_PATH_LITERAL("/.svn/")) != string::npos) {
      continue;
    }

    string file_contents;
    base::ReadFileToString(file, &file_contents);

    if (file_contents.length() > INT_MAX) {
      LOG(WARNING) << "File '" << file.value() << "' too large: "
                   << file_contents.length();
      continue;
    }
    int file_len = static_cast<int>(file_contents.length());

    int headers_end = HttpUtil::LocateEndOfHeaders(file_contents.data(),
                                                   file_len);
    if (headers_end < 1) {
      LOG(DFATAL) << "Headers invalid or empty, ignoring: " << file.value();
      continue;
    }

    string raw_headers =
        HttpUtil::AssembleRawHeaders(file_contents.data(), headers_end);

    scoped_refptr<HttpResponseHeaders> response_headers =
        new HttpResponseHeaders(raw_headers);

    string base;
    if (response_headers->GetNormalizedHeader("X-Original-Url", &base)) {
      response_headers->RemoveHeader("X-Original-Url");
      // Remove the protocol so we can add it below.
      if (StartsWithASCII(base, "https://", false)) {
        base = base.substr(8);
      } else if (StartsWithASCII(base, "http://", false)) {
        base = base.substr(7);
      }
    } else {
      base = file.AsUTF8Unsafe();
    }
    if (base.length() == 0 || base[0] == '/') {
      LOG(DFATAL) << "Invalid path, ignoring: " << base;
      continue;
    }
    if (base[base.length() - 1] == ',') {
      base = base.substr(0, base.length() - 1);
    }

    GURL url("http://" + base);

    VLOG(1) << "Inserting '" << GetKey(url) << "' into QuicInMemoryCache.";

    StringPiece body(file_contents.data() + headers_end,
                     file_contents.size() - headers_end);

    AddResponse(url, response_headers, body);
  }
}

QuicInMemoryCache::~QuicInMemoryCache() {
  STLDeleteValues(&responses_);
}

string QuicInMemoryCache::GetKey(const GURL& url) const {
  // Take everything but the scheme portion of the URL.
  return url.host() + url.PathForRequest();
}

}  // namespace net
