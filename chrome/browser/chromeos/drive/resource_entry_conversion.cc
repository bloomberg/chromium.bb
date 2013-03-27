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
#include "chrome/browser/chromeos/drive/drive_file_system_util.h"
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

DriveEntryProto ConvertResourceEntryToDriveEntryProto(
    const google_apis::ResourceEntry& entry) {
  DriveEntryProto entry_proto;

  // For regular files, the 'filename' and 'title' attribute in the metadata
  // may be different (e.g. due to rename). To be consistent with the web
  // interface and other client to use the 'title' attribute, instead of
  // 'filename', as the file name in the local snapshot.
  entry_proto.set_title(entry.title());
  entry_proto.set_base_name(util::EscapeUtf8FileName(entry_proto.title()));

  entry_proto.set_resource_id(entry.resource_id());
  entry_proto.set_download_url(entry.download_url().spec());

  const google_apis::Link* edit_link =
      entry.GetLinkByType(google_apis::Link::LINK_EDIT);
  if (edit_link)
    entry_proto.set_edit_url(edit_link->href().spec());

  const google_apis::Link* parent_link =
      entry.GetLinkByType(google_apis::Link::LINK_PARENT);
  if (parent_link) {
    // TODO(haruki): Apply mapping from an empty parent to special dummy
    // directory. See http://crbug.com/174233. Until we implement it,
    // ChangeListProcessor ignores such "no parent" entries.
    entry_proto.set_parent_resource_id(
        util::ExtractResourceIdFromUrl(parent_link->href()));
  }

  entry_proto.set_deleted(entry.deleted());
  entry_proto.set_kind(entry.kind());
  entry_proto.set_shared_with_me(HasSharedWithMeLabel(entry));

  PlatformFileInfoProto* file_info = entry_proto.mutable_file_info();

  file_info->set_last_modified(entry.updated_time().ToInternalValue());
  // If the file has never been viewed (last_viewed_time().is_null() == true),
  // then we will set the last_accessed field in the protocol buffer to 0.
  file_info->set_last_accessed(entry.last_viewed_time().ToInternalValue());
  file_info->set_creation_time(entry.published_time().ToInternalValue());

  if (entry.is_file() || entry.is_hosted_document()) {
    DriveFileSpecificInfo* file_specific_info =
        entry_proto.mutable_file_specific_info();
    if (entry.is_file()) {
      file_info->set_size(entry.file_size());
      file_specific_info->set_file_md5(entry.file_md5());

      // The resumable-edit-media link should only be present for regular
      // files as hosted documents are not uploadable.
      const google_apis::Link* upload_link = entry.GetLinkByType(
          google_apis::Link::LINK_RESUMABLE_EDIT_MEDIA);
      if (upload_link && upload_link->href().is_valid())
        entry_proto.set_upload_url(upload_link->href().spec());
    } else if (entry.is_hosted_document()) {
      // Attach .g<something> extension to hosted documents so we can special
      // case their handling in UI.
      // TODO(satorux): Figure out better way how to pass entry info like kind
      // to UI through the File API stack.
      const std::string document_extension = entry.GetHostedDocumentExtension();
      file_specific_info->set_document_extension(document_extension);
      entry_proto.set_base_name(
          util::EscapeUtf8FileName(entry_proto.title() + document_extension));

      // We don't know the size of hosted docs and it does not matter since
      // is has no effect on the quota.
      file_info->set_size(0);
    }
    file_info->set_is_directory(false);
    file_specific_info->set_content_mime_type(entry.content_mime_type());
    file_specific_info->set_is_hosted_document(entry.is_hosted_document());

    const google_apis::Link* thumbnail_link = entry.GetLinkByType(
        google_apis::Link::LINK_THUMBNAIL);
    if (thumbnail_link)
      file_specific_info->set_thumbnail_url(thumbnail_link->href().spec());

    const google_apis::Link* alternate_link = entry.GetLinkByType(
        google_apis::Link::LINK_ALTERNATE);
    if (alternate_link)
      file_specific_info->set_alternate_url(alternate_link->href().spec());

    const google_apis::Link* share_link = entry.GetLinkByType(
        google_apis::Link::LINK_SHARE);
    if (share_link)
      file_specific_info->set_share_url(share_link->href().spec());
  } else if (entry.is_folder()) {
    file_info->set_is_directory(true);
    const google_apis::Link* upload_link = entry.GetLinkByType(
        google_apis::Link::LINK_RESUMABLE_CREATE_MEDIA);
    if (upload_link)
      entry_proto.set_upload_url(upload_link->href().spec());
  } else {
    // Some resource entries don't map into files (i.e. sites).
    return DriveEntryProto();
  }

  return entry_proto;
}

}  // namespace drive
