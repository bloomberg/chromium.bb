// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enhanced_bookmarks/metadata_accessor.h"

#include <iomanip>

#include "base/base64.h"
#include "base/rand_util.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/enhanced_bookmarks/proto/metadata.pb.h"
#include "ui/base/models/tree_node_iterator.h"

using namespace image::collections;

namespace {

// Helper method for working with bookmark metainfo.
std::string DataForMetaInfoField(const BookmarkNode* node,
                                 const std::string& field) {
  const BookmarkNode::MetaInfoMap* map = node->GetMetaInfoMap();
  if (!map)
    return "";

  BookmarkNode::MetaInfoMap::const_iterator it = map->find(field);
  if (it == map->end())
    return "";

  std::string decoded;
  bool result = base::Base64Decode((*it).second, &decoded);
  if (!result)
    return "";

  return decoded;
}

// Sets a new remote id on a bookmark.
std::string SetRemoteIdOnBookmark(BookmarkModel* bookmark_model,
                                  const BookmarkNode* node) {
  // Generate 16 digit hex string random id.
  std::stringstream random_id;
  random_id << std::hex << std::setfill('0') << std::setw(16);
  random_id << base::RandUint64() << base::RandUint64();
  std::string random_id_str = random_id.str();
  bookmark_model->SetNodeMetaInfo(
      node, enhanced_bookmarks::kIdDataKey, random_id_str);
  return random_id_str;
}

// Helper method for working with ImageData_ImageInfo.
bool PopulateImageData(const ImageData_ImageInfo& info,
                       GURL* out_url,
                       int* width,
                       int* height) {
  if (!info.has_url() || !info.has_width() || !info.has_height())
    return false;

  GURL url(info.url());
  if (!url.is_valid())
    return false;

  *out_url = url;
  *width = info.width();
  *height = info.height();
  return true;
}

}  // namespace

namespace enhanced_bookmarks {

const char* kPageDataKey = "stars.pageData";
const char* kImageDataKey = "stars.imageData";
const char* kIdDataKey = "stars.id";
const char* kNoteKey = "stars.note";

std::string RemoteIdFromBookmark(BookmarkModel* bookmark_model,
                                 const BookmarkNode* node) {
  const BookmarkNode::MetaInfoMap* map = node->GetMetaInfoMap();
  if (!map)
    return SetRemoteIdOnBookmark(bookmark_model, node);

  BookmarkNode::MetaInfoMap::const_iterator it = map->find(kIdDataKey);
  if (it == map->end())
    return SetRemoteIdOnBookmark(bookmark_model, node);

  DCHECK(it->second.length());
  return it->second;
}

void SetDescriptionForBookmark(BookmarkModel* bookmark_model,
                               const BookmarkNode* node,
                               const std::string& description) {
  bookmark_model->SetNodeMetaInfo(node, kNoteKey, description);
}

std::string DescriptionFromBookmark(const BookmarkNode* node) {
  const BookmarkNode::MetaInfoMap* map = node->GetMetaInfoMap();
  if (!map)
    return "";

  // First, look for a custom note set by the user.
  BookmarkNode::MetaInfoMap::const_iterator it = map->find(kNoteKey);
  if (it != map->end() && it->second != "")
    return it->second;

  // If none are present, return the snippet.
  return SnippetFromBookmark(node);
}

bool SetOriginalImageForBookmark(BookmarkModel* bookmark_model,
                                 const BookmarkNode* node,
                                 const GURL& url,
                                 int width,
                                 int height) {
  DCHECK(url.is_valid());

  std::string decoded(DataForMetaInfoField(node, kImageDataKey));
  ImageData data;

  // Try to populate the imageData with the existing data.
  if (decoded != "") {
    // If the parsing fails, something is wrong. Immediately fail.
    bool result = data.ParseFromString(decoded);
    if (!result)
      return false;
  }

  scoped_ptr<ImageData_ImageInfo> info(new ImageData_ImageInfo);
  info->set_url(url.spec());
  info->set_width(width);
  info->set_height(height);
  data.set_allocated_original_info(info.release());

  std::string output;
  bool result = data.SerializePartialToString(&output);
  if (!result)
    return false;

  std::string encoded;
  base::Base64Encode(output, &encoded);
  bookmark_model->SetNodeMetaInfo(node, kImageDataKey, encoded);
  // Ensure that the bookmark has a stars.id, to trigger the server processing.
  RemoteIdFromBookmark(bookmark_model, node);
  return true;
}

bool OriginalImageFromBookmark(const BookmarkNode* node,
                               GURL* url,
                               int* width,
                               int* height) {
  std::string decoded(DataForMetaInfoField(node, kImageDataKey));
  if (decoded == "")
    return false;

  ImageData data;
  bool result = data.ParseFromString(decoded);
  if (!result)
    return false;

  if (!data.has_original_info())
    return false;

  return PopulateImageData(data.original_info(), url, width, height);
}

bool ThumbnailImageFromBookmark(const BookmarkNode* node,
                                GURL* url,
                                int* width,
                                int* height) {
  std::string decoded(DataForMetaInfoField(node, kImageDataKey));
  if (decoded == "")
    return false;

  ImageData data;
  bool result = data.ParseFromString(decoded);
  if (!result)
    return false;

  if (!data.has_thumbnail_info())
    return false;

  return PopulateImageData(data.thumbnail_info(), url, width, height);
}

std::string SnippetFromBookmark(const BookmarkNode* node) {
  std::string decoded(DataForMetaInfoField(node, kPageDataKey));
  if (decoded == "")
    return decoded;

  PageData data;
  bool result = data.ParseFromString(decoded);
  if (!result)
    return "";

  return data.snippet();
}

bool SetAllImagesForBookmark(BookmarkModel* bookmark_model,
                             const BookmarkNode* node,
                             const GURL& image_url,
                             int image_width,
                             int image_height,
                             const GURL& thumbnail_url,
                             int thumbnail_width,
                             int thumbnail_height) {
  DCHECK(image_url.is_valid() || image_url.is_empty());
  DCHECK(thumbnail_url.is_valid() || thumbnail_url.is_empty());
  std::string decoded(DataForMetaInfoField(node, kImageDataKey));
  ImageData data;

  // Try to populate the imageData with the existing data.
  if (decoded != "") {
    // If the parsing fails, something is wrong. Immediately fail.
    bool result = data.ParseFromString(decoded);
    if (!result)
      return false;
  }

  if (image_url.is_empty()) {
    data.release_original_info();
  } else {
    // Regardless of whether an image info exists, we make a new one.
    // Intentially make a raw pointer.
    ImageData_ImageInfo* info = new ImageData_ImageInfo;
    info->set_url(image_url.spec());
    info->set_width(image_width);
    info->set_height(image_height);
    // This method consumes the raw pointer.
    data.set_allocated_original_info(info);
  }

  if (thumbnail_url.is_empty()) {
    data.release_thumbnail_info();
  } else {
    // Regardless of whether an image info exists, we make a new one.
    // Intentially make a raw pointer.
    ImageData_ImageInfo* info = new ImageData_ImageInfo;
    info->set_url(thumbnail_url.spec());
    info->set_width(thumbnail_width);
    info->set_height(thumbnail_height);
    // This method consumes the raw pointer.
    data.set_allocated_thumbnail_info(info);
  }
  std::string output;
  bool result = data.SerializePartialToString(&output);
  if (!result)
    return false;

  std::string encoded;
  base::Base64Encode(output, &encoded);
  bookmark_model->SetNodeMetaInfo(node, kImageDataKey, encoded);
  return true;
}

}  // namespace enhanced_bookmarks
