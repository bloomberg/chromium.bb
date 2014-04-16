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
#include "content/public/browser/browser_thread.h"
#include "google_apis/drive/drive_api_parser.h"
#include "google_apis/drive/gdata_wapi_parser.h"
#include "net/base/escape.h"
#include "third_party/re2/re2/re2.h"
#include "url/gurl.h"

namespace drive {
namespace util {
namespace {

// Google Apps MIME types:
const char kGoogleDocumentMimeType[] = "application/vnd.google-apps.document";
const char kGoogleDrawingMimeType[] = "application/vnd.google-apps.drawing";
const char kGooglePresentationMimeType[] =
    "application/vnd.google-apps.presentation";
const char kGoogleSpreadsheetMimeType[] =
    "application/vnd.google-apps.spreadsheet";
const char kGoogleTableMimeType[] = "application/vnd.google-apps.table";
const char kGoogleFormMimeType[] = "application/vnd.google-apps.form";
const char kDriveFolderMimeType[] = "application/vnd.google-apps.folder";

std::string GetMimeTypeFromEntryKind(google_apis::DriveEntryKind kind) {
  switch (kind) {
    case google_apis::ENTRY_KIND_DOCUMENT:
      return kGoogleDocumentMimeType;
    case google_apis::ENTRY_KIND_SPREADSHEET:
      return kGoogleSpreadsheetMimeType;
    case google_apis::ENTRY_KIND_PRESENTATION:
      return kGooglePresentationMimeType;
    case google_apis::ENTRY_KIND_DRAWING:
      return kGoogleDrawingMimeType;
    case google_apis::ENTRY_KIND_TABLE:
      return kGoogleTableMimeType;
    case google_apis::ENTRY_KIND_FORM:
      return kGoogleFormMimeType;
    default:
      return std::string();
  }
}

// Returns the argument string.
std::string Identity(const std::string& resource_id) { return resource_id; }

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
      base::kWhitespaceUTF16 +
      base::string16(1, static_cast<base::char16>('"')));

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

std::string ExtractResourceIdFromUrl(const GURL& url) {
  return net::UnescapeURLComponent(url.ExtractFileName(),
                                   net::UnescapeRule::URL_SPECIAL_CHARS);
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

ResourceIdCanonicalizer GetIdentityResourceIdCanonicalizer() {
  return base::Bind(&Identity);
}

const char kDocsListScope[] = "https://docs.google.com/feeds/";
const char kDriveAppsScope[] = "https://www.googleapis.com/auth/drive.apps";

void ParseShareUrlAndRun(const google_apis::GetShareUrlCallback& callback,
                         google_apis::GDataErrorCode error,
                         scoped_ptr<base::Value> value) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  if (!value) {
    callback.Run(error, GURL());
    return;
  }

  // Parsing ResourceEntry is cheap enough to do on UI thread.
  scoped_ptr<google_apis::ResourceEntry> entry =
      google_apis::ResourceEntry::ExtractAndParse(*value);
  if (!entry) {
    callback.Run(google_apis::GDATA_PARSE_ERROR, GURL());
    return;
  }

  const google_apis::Link* share_link =
      entry->GetLinkByType(google_apis::Link::LINK_SHARE);
  callback.Run(error, share_link ? share_link->href() : GURL());
}

scoped_ptr<google_apis::FileResource> ConvertResourceEntryToFileResource(
    const google_apis::ResourceEntry& entry) {
  scoped_ptr<google_apis::FileResource> file(new google_apis::FileResource);

  file->set_file_id(entry.resource_id());
  file->set_title(entry.title());
  file->set_created_date(entry.published_time());

  if (std::find(entry.labels().begin(), entry.labels().end(),
                "shared-with-me") != entry.labels().end()) {
    // Set current time to mark the file is shared_with_me, since ResourceEntry
    // doesn't have |shared_with_me_date| equivalent.
    file->set_shared_with_me_date(base::Time::Now());
  }

  file->set_shared(std::find(entry.labels().begin(), entry.labels().end(),
                             "shared") != entry.labels().end());

  if (entry.is_folder()) {
    file->set_mime_type(kDriveFolderMimeType);
  } else {
    std::string mime_type = GetMimeTypeFromEntryKind(entry.kind());
    if (mime_type.empty())
      mime_type = entry.content_mime_type();
    file->set_mime_type(mime_type);
  }

  file->set_md5_checksum(entry.file_md5());
  file->set_file_size(entry.file_size());

  file->mutable_labels()->set_trashed(entry.deleted());
  file->set_etag(entry.etag());

  google_apis::ImageMediaMetadata* image_media_metadata =
    file->mutable_image_media_metadata();
  image_media_metadata->set_width(entry.image_width());
  image_media_metadata->set_height(entry.image_height());
  image_media_metadata->set_rotation(entry.image_rotation());

  std::vector<google_apis::ParentReference>* parents = file->mutable_parents();
  for (size_t i = 0; i < entry.links().size(); ++i) {
    using google_apis::Link;
    const Link& link = *entry.links()[i];
    switch (link.type()) {
      case Link::LINK_PARENT: {
        google_apis::ParentReference parent;
        parent.set_parent_link(link.href());

        std::string file_id =
            drive::util::ExtractResourceIdFromUrl(link.href());
        parent.set_file_id(file_id);
        parents->push_back(parent);
        break;
      }
      case Link::LINK_ALTERNATE:
        file->set_alternate_link(link.href());
        break;
      default:
        break;
    }
  }

  file->set_modified_date(entry.updated_time());
  file->set_last_viewed_by_me_date(entry.last_viewed_time());

  return file.Pass();
}

google_apis::DriveEntryKind GetKind(
    const google_apis::FileResource& file_resource) {
  if (file_resource.IsDirectory())
    return google_apis::ENTRY_KIND_FOLDER;

  const std::string& mime_type = file_resource.mime_type();
  if (mime_type == kGoogleDocumentMimeType)
    return google_apis::ENTRY_KIND_DOCUMENT;
  if (mime_type == kGoogleSpreadsheetMimeType)
    return google_apis::ENTRY_KIND_SPREADSHEET;
  if (mime_type == kGooglePresentationMimeType)
    return google_apis::ENTRY_KIND_PRESENTATION;
  if (mime_type == kGoogleDrawingMimeType)
    return google_apis::ENTRY_KIND_DRAWING;
  if (mime_type == kGoogleTableMimeType)
    return google_apis::ENTRY_KIND_TABLE;
  if (mime_type == kGoogleFormMimeType)
    return google_apis::ENTRY_KIND_FORM;
  if (mime_type == "application/pdf")
    return google_apis::ENTRY_KIND_PDF;
  return google_apis::ENTRY_KIND_FILE;
}

scoped_ptr<google_apis::ResourceEntry>
ConvertFileResourceToResourceEntry(
    const google_apis::FileResource& file_resource) {
  scoped_ptr<google_apis::ResourceEntry> entry(new google_apis::ResourceEntry);

  // ResourceEntry
  entry->set_resource_id(file_resource.file_id());
  entry->set_id(file_resource.file_id());
  entry->set_kind(GetKind(file_resource));
  entry->set_title(file_resource.title());
  entry->set_published_time(file_resource.created_date());

  std::vector<std::string> labels;
  if (!file_resource.shared_with_me_date().is_null())
    labels.push_back("shared-with-me");
  if (file_resource.shared())
    labels.push_back("shared");
  entry->set_labels(labels);

  // This should be the url to download the file_resource.
  {
    google_apis::Content content;
    content.set_mime_type(file_resource.mime_type());
    entry->set_content(content);
  }
  // TODO(kochi): entry->resource_links_

  // For file entries
  entry->set_filename(file_resource.title());
  entry->set_suggested_filename(file_resource.title());
  entry->set_file_md5(file_resource.md5_checksum());
  entry->set_file_size(file_resource.file_size());

  // If file is removed completely, that information is only available in
  // ChangeResource, and is reflected in |removed_|. If file is trashed, the
  // file entry still exists but with its "trashed" label true.
  entry->set_deleted(file_resource.labels().is_trashed());

  // ImageMediaMetadata
  entry->set_image_width(file_resource.image_media_metadata().width());
  entry->set_image_height(file_resource.image_media_metadata().height());
  entry->set_image_rotation(file_resource.image_media_metadata().rotation());

  // CommonMetadata
  entry->set_etag(file_resource.etag());
  // entry->authors_
  // entry->links_.
  ScopedVector<google_apis::Link> links;
  for (size_t i = 0; i < file_resource.parents().size(); ++i) {
    google_apis::Link* link = new google_apis::Link;
    link->set_type(google_apis::Link::LINK_PARENT);
    link->set_href(file_resource.parents()[i].parent_link());
    links.push_back(link);
  }
  if (!file_resource.alternate_link().is_empty()) {
    google_apis::Link* link = new google_apis::Link;
    link->set_type(google_apis::Link::LINK_ALTERNATE);
    link->set_href(file_resource.alternate_link());
    links.push_back(link);
  }
  entry->set_links(links.Pass());

  // entry->categories_
  entry->set_updated_time(file_resource.modified_date());
  entry->set_last_viewed_time(file_resource.last_viewed_by_me_date());

  entry->FillRemainingFields();
  return entry.Pass();
}

scoped_ptr<google_apis::ResourceEntry>
ConvertChangeResourceToResourceEntry(
    const google_apis::ChangeResource& change_resource) {
  scoped_ptr<google_apis::ResourceEntry> entry;
  if (change_resource.file())
    entry = ConvertFileResourceToResourceEntry(*change_resource.file()).Pass();
  else
    entry.reset(new google_apis::ResourceEntry);

  entry->set_resource_id(change_resource.file_id());
  // If |is_deleted()| returns true, the file is removed from Drive.
  entry->set_removed(change_resource.is_deleted());
  entry->set_changestamp(change_resource.change_id());
  entry->set_modification_date(change_resource.modification_date());

  return entry.Pass();
}

scoped_ptr<google_apis::ResourceList>
ConvertFileListToResourceList(const google_apis::FileList& file_list) {
  scoped_ptr<google_apis::ResourceList> feed(new google_apis::ResourceList);

  const ScopedVector<google_apis::FileResource>& items = file_list.items();
  ScopedVector<google_apis::ResourceEntry> entries;
  for (size_t i = 0; i < items.size(); ++i)
    entries.push_back(ConvertFileResourceToResourceEntry(*items[i]).release());
  feed->set_entries(entries.Pass());

  ScopedVector<google_apis::Link> links;
  if (!file_list.next_link().is_empty()) {
    google_apis::Link* link = new google_apis::Link;
    link->set_type(google_apis::Link::LINK_NEXT);
    link->set_href(file_list.next_link());
    links.push_back(link);
  }
  feed->set_links(links.Pass());

  return feed.Pass();
}

scoped_ptr<google_apis::ResourceList>
ConvertChangeListToResourceList(const google_apis::ChangeList& change_list) {
  scoped_ptr<google_apis::ResourceList> feed(new google_apis::ResourceList);

  const ScopedVector<google_apis::ChangeResource>& items = change_list.items();
  ScopedVector<google_apis::ResourceEntry> entries;
  for (size_t i = 0; i < items.size(); ++i) {
    entries.push_back(
        ConvertChangeResourceToResourceEntry(*items[i]).release());
  }
  feed->set_entries(entries.Pass());

  feed->set_largest_changestamp(change_list.largest_change_id());

  ScopedVector<google_apis::Link> links;
  if (!change_list.next_link().is_empty()) {
    google_apis::Link* link = new google_apis::Link;
    link->set_type(google_apis::Link::LINK_NEXT);
    link->set_href(change_list.next_link());
    links.push_back(link);
  }
  feed->set_links(links.Pass());

  return feed.Pass();
}

std::string GetMd5Digest(const base::FilePath& file_path) {
  const int kBufferSize = 512 * 1024;  // 512kB.

  base::File file(file_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid())
    return std::string();

  base::MD5Context context;
  base::MD5Init(&context);

  int64 offset = 0;
  scoped_ptr<char[]> buffer(new char[kBufferSize]);
  while (true) {
    int result = file.Read(offset, buffer.get(), kBufferSize);
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

const char kWapiRootDirectoryResourceId[] = "folder:root";

}  // namespace util
}  // namespace drive
