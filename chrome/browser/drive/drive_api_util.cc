// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/drive/drive_api_util.h"

#include <string>

#include "base/files/file.h"
#include "base/logging.h"
#include "base/md5.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "google_apis/drive/drive_api_parser.h"
#include "net/base/escape.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "storage/browser/fileapi/file_stream_reader.h"
#include "third_party/re2/re2/re2.h"
#include "url/gurl.h"

namespace drive {
namespace util {
namespace {

struct HostedDocumentKind {
  const char* mime_type;
  const char* extension;
};

const HostedDocumentKind kHostedDocumentKinds[] = {
    {kGoogleDocumentMimeType,     ".gdoc"},
    {kGoogleSpreadsheetMimeType,  ".gsheet"},
    {kGooglePresentationMimeType, ".gslides"},
    {kGoogleDrawingMimeType,      ".gdraw"},
    {kGoogleTableMimeType,        ".gtable"},
    {kGoogleFormMimeType,         ".gform"},
    {kGoogleMapMimeType,          ".gmaps"},
};

const char kUnknownHostedDocumentExtension[] = ".glink";

const int kMd5DigestBufferSize = 512 * 1024;  // 512 kB.

}  // namespace

std::string EscapeQueryStringValue(const std::string& str) {
  std::string result;
  result.reserve(str.size());
  for (size_t i = 0; i < str.size(); ++i) {
    if (str[i] == '\\' || str[i] == '\'') {
      result.push_back('\\');
    }
    result.push_back(str[i]);
  }
  return result;
}

std::string TranslateQuery(const std::string& original_query) {
  // In order to handle non-ascii white spaces correctly, convert to UTF16.
  base::string16 query = base::UTF8ToUTF16(original_query);
  const base::string16 kDelimiter(
      base::kWhitespaceUTF16 + base::ASCIIToUTF16("\""));

  std::string result;
  for (size_t index = query.find_first_not_of(base::kWhitespaceUTF16);
       index != base::string16::npos;
       index = query.find_first_not_of(base::kWhitespaceUTF16, index)) {
    bool is_exclusion = (query[index] == '-');
    if (is_exclusion)
      ++index;
    if (index == query.length()) {
      // Here, the token is '-' and it should be ignored.
      continue;
    }

    size_t begin_token = index;
    base::string16 token;
    if (query[begin_token] == '"') {
      // Quoted query.
      ++begin_token;
      size_t end_token = query.find('"', begin_token);
      if (end_token == base::string16::npos) {
        // This is kind of syntax error, since quoted string isn't finished.
        // However, the query is built by user manually, so here we treat
        // whole remaining string as a token as a fallback, by appending
        // a missing double-quote character.
        end_token = query.length();
        query.push_back('"');
      }

      token = query.substr(begin_token, end_token - begin_token);
      index = end_token + 1;  // Consume last '"', too.
    } else {
      size_t end_token = query.find_first_of(kDelimiter, begin_token);
      if (end_token == base::string16::npos) {
        end_token = query.length();
      }

      token = query.substr(begin_token, end_token - begin_token);
      index = end_token;
    }

    if (token.empty()) {
      // Just ignore an empty token.
      continue;
    }

    if (!result.empty()) {
      // If there are two or more tokens, need to connect with "and".
      result.append(" and ");
    }

    // The meaning of "fullText" should include title, description and content.
    base::StringAppendF(
        &result,
        "%sfullText contains \'%s\'",
        is_exclusion ? "not " : "",
        EscapeQueryStringValue(base::UTF16ToUTF8(token)).c_str());
  }

  return result;
}

std::string CanonicalizeResourceId(const std::string& resource_id) {
  // If resource ID is in the old WAPI format starting with a prefix like
  // "document:", strip it and return the remaining part.
  std::string stripped_resource_id;
  if (RE2::FullMatch(resource_id, "^[a-z-]+(?::|%3A)([\\w-]+)$",
                     &stripped_resource_id))
    return stripped_resource_id;
  return resource_id;
}

std::string GetMd5Digest(const base::FilePath& file_path) {
  base::File file(file_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid())
    return std::string();

  base::MD5Context context;
  base::MD5Init(&context);

  int64 offset = 0;
  scoped_ptr<char[]> buffer(new char[kMd5DigestBufferSize]);
  while (true) {
    int result = file.Read(offset, buffer.get(), kMd5DigestBufferSize);
    if (result < 0) {
      // Found an error.
      return std::string();
    }

    if (result == 0) {
      // End of file.
      break;
    }

    offset += result;
    base::MD5Update(&context, base::StringPiece(buffer.get(), result));
  }

  base::MD5Digest digest;
  base::MD5Final(&digest, &context);
  return MD5DigestToBase16(digest);
}

FileStreamMd5Digester::FileStreamMd5Digester()
    : buffer_(new net::IOBuffer(kMd5DigestBufferSize)) {
}

FileStreamMd5Digester::~FileStreamMd5Digester() {
}

void FileStreamMd5Digester::GetMd5Digest(
    scoped_ptr<storage::FileStreamReader> stream_reader,
    const ResultCallback& callback) {
  reader_ = stream_reader.Pass();
  base::MD5Init(&md5_context_);

  // Start the read/hash.
  ReadNextChunk(callback);
}

void FileStreamMd5Digester::ReadNextChunk(const ResultCallback& callback) {
  const int result =
      reader_->Read(buffer_.get(), kMd5DigestBufferSize,
                    base::Bind(&FileStreamMd5Digester::OnChunkRead,
                               base::Unretained(this), callback));
  if (result != net::ERR_IO_PENDING)
    OnChunkRead(callback, result);
}

void FileStreamMd5Digester::OnChunkRead(const ResultCallback& callback,
                                        int bytes_read) {
  if (bytes_read < 0) {
    // Error - just return empty string.
    callback.Run("");
    return;
  } else if (bytes_read == 0) {
    // EOF.
    base::MD5Digest digest;
    base::MD5Final(&digest, &md5_context_);
    std::string result = MD5DigestToBase16(digest);
    callback.Run(result);
    return;
  }

  // Read data and digest it.
  base::MD5Update(&md5_context_,
                  base::StringPiece(buffer_->data(), bytes_read));

  // Kick off the next read.
  ReadNextChunk(callback);
}

std::string GetHostedDocumentExtension(const std::string& mime_type) {
  for (size_t i = 0; i < arraysize(kHostedDocumentKinds); ++i) {
    if (mime_type == kHostedDocumentKinds[i].mime_type)
      return kHostedDocumentKinds[i].extension;
  }
  return kUnknownHostedDocumentExtension;
}

bool IsKnownHostedDocumentMimeType(const std::string& mime_type) {
  for (size_t i = 0; i < arraysize(kHostedDocumentKinds); ++i) {
    if (mime_type == kHostedDocumentKinds[i].mime_type)
      return true;
  }
  return false;
}

bool HasHostedDocumentExtension(const base::FilePath& path) {
  const std::string extension = base::FilePath(path.Extension()).AsUTF8Unsafe();
  for (size_t i = 0; i < arraysize(kHostedDocumentKinds); ++i) {
    if (extension == kHostedDocumentKinds[i].extension)
      return true;
  }
  return extension == kUnknownHostedDocumentExtension;
}

}  // namespace util
}  // namespace drive
