// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/resource_entry_conversion.h"

#include <algorithm>
#include <string>

#include "base/logging.h"
#include "base/platform_file.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/drive/drive_api_util.h"
#include "chrome/browser/google_apis/gdata_wapi_parser.h"

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

bool ConvertToResourceEntry(const google_apis::ResourceEntry& input,
                            ResourceEntry* out_entry,
                            std::string* out_parent_resource_id) {
  DCHECK(out_entry);
  DCHECK(out_parent_resource_id);

  ResourceEntry converted;

  // For regular files, the 'filename' and 'title' attribute in the metadata
  // may be different (e.g. due to rename). To be consistent with the web
  // interface and other client to use the 'title' attribute, instead of
  // 'filename', as the file name in the local snapshot.
  converted.set_title(input.title());
  converted.set_base_name(util::NormalizeFileName(converted.title()));
  converted.set_resource_id(input.resource_id());

  // Gets parent Resource ID. On drive.google.com, a file can have multiple
  // parents or no parent, but we are forcing a tree-shaped structure (i.e. no
  // multi-parent or zero-parent entries). Therefore the first found "parent" is
  // used for the entry and if the entry has no parent, we assign a special ID
  // which represents no-parent entries. Tracked in http://crbug.com/158904.
  std::string parent_resource_id;
  const google_apis::Link* parent_link =
      input.GetLinkByType(google_apis::Link::LINK_PARENT);
  if (parent_link)
    parent_resource_id = util::ExtractResourceIdFromUrl(parent_link->href());

  // Apply mapping from an empty parent to the special dummy directory.
  if (parent_resource_id.empty())
    parent_resource_id = util::kDriveOtherDirLocalId;

  converted.set_deleted(input.deleted());
  converted.set_shared_with_me(HasSharedWithMeLabel(input));

  PlatformFileInfoProto* file_info = converted.mutable_file_info();

  file_info->set_last_modified(input.updated_time().ToInternalValue());
  // If the file has never been viewed (last_viewed_time().is_null() == true),
  // then we will set the last_accessed field in the protocol buffer to 0.
  file_info->set_last_accessed(input.last_viewed_time().ToInternalValue());
  file_info->set_creation_time(input.published_time().ToInternalValue());

  if (input.is_file() || input.is_hosted_document()) {
    FileSpecificInfo* file_specific_info =
        converted.mutable_file_specific_info();
    if (input.is_file()) {
      file_info->set_size(input.file_size());
      file_specific_info->set_md5(input.file_md5());

      // The resumable-edit-media link should only be present for regular
      // files as hosted documents are not uploadable.
    } else if (input.is_hosted_document()) {
      // Attach .g<something> extension to hosted documents so we can special
      // case their handling in UI.
      // TODO(satorux): Figure out better way how to pass input info like kind
      // to UI through the File API stack.
      const std::string document_extension = input.GetHostedDocumentExtension();
      file_specific_info->set_document_extension(document_extension);
      converted.set_base_name(
          util::NormalizeFileName(converted.title() + document_extension));

      // We don't know the size of hosted docs and it does not matter since
      // is has no effect on the quota.
      file_info->set_size(0);
    }
    file_info->set_is_directory(false);
    file_specific_info->set_content_mime_type(input.content_mime_type());
    file_specific_info->set_is_hosted_document(input.is_hosted_document());

    const google_apis::Link* alternate_link =
        input.GetLinkByType(google_apis::Link::LINK_ALTERNATE);
    if (alternate_link)
      file_specific_info->set_alternate_url(alternate_link->href().spec());

    const int64 image_width = input.image_width();
    if (image_width != -1)
      file_specific_info->set_image_width(image_width);

    const int64 image_height = input.image_height();
    if (image_height != -1)
      file_specific_info->set_image_height(image_height);

    const int64 image_rotation = input.image_rotation();
    if (image_rotation != -1)
      file_specific_info->set_image_rotation(image_rotation);
  } else if (input.is_folder()) {
    file_info->set_is_directory(true);
  } else {
    // There are two cases to reach here.
    // * The entry is something that doesn't map into files (i.e. sites).
    //   We don't handle these kind of entries hence return false.
    // * The entry is un-shared to you by other owner. In that case, we
    //   get an entry with only deleted() and resource_id() fields are
    //   filled. Since we want to delete such entries locally as well,
    //   in that case we need to return true to proceed.
    if (!input.deleted())
      return false;
  }

  out_entry->Swap(&converted);
  swap(*out_parent_resource_id, parent_resource_id);
  return true;
}

void ConvertResourceEntryToPlatformFileInfo(const ResourceEntry& entry,
                                            base::PlatformFileInfo* file_info) {
  file_info->size = entry.file_info().size();
  file_info->is_directory = entry.file_info().is_directory();
  file_info->is_symbolic_link = entry.file_info().is_symbolic_link();
  file_info->last_modified = base::Time::FromInternalValue(
      entry.file_info().last_modified());
  file_info->last_accessed = base::Time::FromInternalValue(
      entry.file_info().last_accessed());
  file_info->creation_time = base::Time::FromInternalValue(
      entry.file_info().creation_time());
}

void SetPlatformFileInfoToResourceEntry(const base::PlatformFileInfo& file_info,
                                        ResourceEntry* entry) {
  PlatformFileInfoProto* entry_file_info = entry->mutable_file_info();
  entry_file_info->set_size(file_info.size);
  entry_file_info->set_is_directory(file_info.is_directory);
  entry_file_info->set_is_symbolic_link(file_info.is_symbolic_link);
  entry_file_info->set_last_modified(file_info.last_modified.ToInternalValue());
  entry_file_info->set_last_accessed(file_info.last_accessed.ToInternalValue());
  entry_file_info->set_creation_time(file_info.creation_time.ToInternalValue());
}

}  // namespace drive
