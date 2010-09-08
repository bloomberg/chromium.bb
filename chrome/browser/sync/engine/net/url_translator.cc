// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Contains the definition of a few helper functions used for generating sync
// URLs.

#include "chrome/browser/sync/engine/net/url_translator.h"

#include "base/basictypes.h"
#include "base/port.h"
#include "net/base/escape.h"

using std::string;

namespace browser_sync {

namespace {
// Parameters that the server understands. (here, a-Z)
const char kParameterAuthToken[] = "auth";
const char kParameterClientID[] = "client_id";
}

// Convenience wrappers around CgiEscapePath().
string CgiEscapeString(const char* src) {
  return CgiEscapeString(string(src));
}

string CgiEscapeString(const string& src) {
  return EscapeUrlEncodedData(src);
}

// This method appends the query string to the sync server path.
string MakeSyncServerPath(const string& path, const string& query_string) {
  string result = path;
  result.append("?");
  result.append(query_string);
  return result;
}

string MakeSyncQueryString(const string& client_id) {
  string query;
  query += kParameterClientID;
  query += "=" + CgiEscapeString(client_id);
  return query;
}

}  // namespace browser_sync
