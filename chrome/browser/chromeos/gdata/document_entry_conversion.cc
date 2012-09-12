// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/document_entry_conversion.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/gdata/drive.pb.h"
#include "chrome/browser/chromeos/gdata/drive_file_system_util.h"
#include "chrome/browser/chromeos/gdata/gdata_wapi_parser.h"
#include "googleurl/src/gurl.h"
#include "net/base/escape.h"

namespace gdata {

DriveEntryProto ConvertDocumentEntryToDriveEntryProto(
    const DocumentEntry& doc) {
  DriveEntryProto entry_proto;

  // For regular files, the 'filename' and 'title' attribute in the metadata
  // may be different (e.g. due to rename). To be consistent with the web
  // interface and other client to use the 'title' attribute, instead of
  // 'filename', as the file name in the local snapshot.
  entry_proto.set_title(UTF16ToUTF8(doc.title()));
  entry_proto.set_base_name(util::EscapeUtf8FileName(entry_proto.title()));

  entry_proto.set_resource_id(doc.resource_id());
  entry_proto.set_content_url(doc.content_url().spec());

  const Link* edit_link = doc.GetLinkByType(Link::LINK_EDIT);
  if (edit_link)
    entry_proto.set_edit_url(edit_link->href().spec());

  const Link* parent_link = doc.GetLinkByType(Link::LINK_PARENT);
  if (parent_link) {
    entry_proto.set_parent_resource_id(
        util::ExtractResourceIdFromUrl(parent_link->href()));
  }

  entry_proto.set_deleted(doc.deleted());
  entry_proto.set_kind(doc.kind());

  PlatformFileInfoProto* file_info = entry_proto.mutable_file_info();

  // TODO(satorux): The last modified and the last accessed time shouldn't
  // be treated as the same thing: crbug.com/148434
  file_info->set_last_modified(doc.updated_time().ToInternalValue());
  file_info->set_last_accessed(doc.updated_time().ToInternalValue());
  file_info->set_creation_time(doc.published_time().ToInternalValue());

  if (doc.is_file() || doc.is_hosted_document()) {
    DriveFileSpecificInfo* file_specific_info =
        entry_proto.mutable_file_specific_info();
    if (doc.is_file()) {
      file_info->set_size(doc.file_size());
      file_specific_info->set_file_md5(doc.file_md5());

      // The resumable-edit-media link should only be present for regular
      // files as hosted documents are not uploadable.
      const Link* upload_link = doc.GetLinkByType(
          Link::LINK_RESUMABLE_EDIT_MEDIA);
      if (upload_link)
        entry_proto.set_upload_url(upload_link->href().spec());
    } else if (doc.is_hosted_document()) {
      // Attach .g<something> extension to hosted documents so we can special
      // case their handling in UI.
      // TODO(zelidrag): Figure out better way how to pass entry info like kind
      // to UI through the File API stack.
      const std::string document_extension = doc.GetHostedDocumentExtension();
      entry_proto.set_base_name(
          util::EscapeUtf8FileName(entry_proto.title() + document_extension));

      // We don't know the size of hosted docs and it does not matter since
      // is has no effect on the quota.
      file_info->set_size(0);
    }
    file_specific_info->set_content_mime_type(doc.content_mime_type());
    file_specific_info->set_is_hosted_document(doc.is_hosted_document());

    const Link* thumbnail_link = doc.GetLinkByType(Link::LINK_THUMBNAIL);
    if (thumbnail_link)
      file_specific_info->set_thumbnail_url(thumbnail_link->href().spec());

    const Link* alternate_link = doc.GetLinkByType(Link::LINK_ALTERNATE);
    if (alternate_link)
      file_specific_info->set_alternate_url(alternate_link->href().spec());
  } else if (doc.is_folder()) {
    const Link* upload_link = doc.GetLinkByType(
        Link::LINK_RESUMABLE_CREATE_MEDIA);
    if (upload_link)
      entry_proto.set_upload_url(upload_link->href().spec());
  } else {
    NOTREACHED() << "Unknown DocumentEntry type";
  }

  return entry_proto;
}

}  // namespace gdata
