// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/resource_entry_conversion.h"

#include <algorithm>
#include <string>

#include "base/logging.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/google_apis/gdata_wapi_parser.h"
#include "googleurl/src/gurl.h"
#include "net/base/escape.h"

namespace drive {

namespace {

const char kSharedWithMeLabel[] = "shared-with-me";

// Checks if |entry| has a label "shared-with-me", which is added to entries
// shared with the user.
bool HasSharedWithMeLabel(const google_apis::ResourceEntry& entry) {
  std::vector<std::string>::const_iterator it =
      std::find(entry.labels().begin(), entry.labels().end(),
                kSharedWithMeLabel);
  return it != entry.labels().end();
}

}  // namespace

ResourceEntry ConvertToResourceEntry(
    const google_apis::ResourceEntry& input) {
  ResourceEntry output;

  // For regular files, the 'filename' and 'title' attribute in the metadata
  // may be different (e.g. due to rename). To be consistent with the web
  // interface and other client to use the 'title' attribute, instead of
  // 'filename', as the file name in the local snapshot.
  output.set_title(input.title());
  output.set_base_name(util::EscapeUtf8FileName(output.title()));

  output.set_resource_id(input.resource_id());
  output.set_download_url(input.download_url().spec());

  const google_apis::Link* edit_link =
      input.GetLinkByType(google_apis::Link::LINK_EDIT);
  if (edit_link)
    output.set_edit_url(edit_link->href().spec());

  // Sets parent Resource ID. On drive.google.com, a file can have multiple
  // parents or no parent, but we are forcing a tree-shaped structure (i.e. no
  // multi-parent or zero-parent entries). Therefore the first found "parent" is
  // used for the entry and if the entry has no parent, we assign a special ID
  // which represents no-parent entries. Tracked in http://crbug.com/158904.
  const google_apis::Link* parent_link =
      input.GetLinkByType(google_apis::Link::LINK_PARENT);
  if (parent_link) {
    output.set_parent_resource_id(
        util::ExtractResourceIdFromUrl(parent_link->href()));
  }
  // Apply mapping from an empty parent to the special dummy directory.
  if (output.parent_resource_id().empty())
    output.set_parent_resource_id(util::kDriveOtherDirSpecialResourceId);

  output.set_deleted(input.deleted());
  output.set_shared_with_me(HasSharedWithMeLabel(input));

  PlatformFileInfoProto* file_info = output.mutable_file_info();

  file_info->set_last_modified(input.updated_time().ToInternalValue());
  // If the file has never been viewed (last_viewed_time().is_null() == true),
  // then we will set the last_accessed field in the protocol buffer to 0.
  file_info->set_last_accessed(input.last_viewed_time().ToInternalValue());
  file_info->set_creation_time(input.published_time().ToInternalValue());

  if (input.is_file() || input.is_hosted_document()) {
    DriveFileSpecificInfo* file_specific_info =
        output.mutable_file_specific_info();
    if (input.is_file()) {
      file_info->set_size(input.file_size());
      file_specific_info->set_file_md5(input.file_md5());

      // The resumable-edit-media link should only be present for regular
      // files as hosted documents are not uploadable.
    } else if (input.is_hosted_document()) {
      // Attach .g<something> extension to hosted documents so we can special
      // case their handling in UI.
      // TODO(satorux): Figure out better way how to pass input info like kind
      // to UI through the File API stack.
      const std::string document_extension = input.GetHostedDocumentExtension();
      file_specific_info->set_document_extension(document_extension);
      output.set_base_name(
          util::EscapeUtf8FileName(output.title() + document_extension));

      // We don't know the size of hosted docs and it does not matter since
      // is has no effect on the quota.
      file_info->set_size(0);
    }
    file_info->set_is_directory(false);
    file_specific_info->set_content_mime_type(input.content_mime_type());
    file_specific_info->set_is_hosted_document(input.is_hosted_document());

    const google_apis::Link* thumbnail_link = input.GetLinkByType(
        google_apis::Link::LINK_THUMBNAIL);
    if (thumbnail_link)
      file_specific_info->set_thumbnail_url(thumbnail_link->href().spec());

    const google_apis::Link* alternate_link = input.GetLinkByType(
        google_apis::Link::LINK_ALTERNATE);
    if (alternate_link)
      file_specific_info->set_alternate_url(alternate_link->href().spec());

    const google_apis::Link* share_link = input.GetLinkByType(
        google_apis::Link::LINK_SHARE);
    if (share_link)
      file_specific_info->set_share_url(share_link->href().spec());
  } else if (input.is_folder()) {
    file_info->set_is_directory(true);
  } else {
    // Some resource entries don't map into files (i.e. sites).
    return ResourceEntry();
  }

  return output;
}

}  // namespace drive
