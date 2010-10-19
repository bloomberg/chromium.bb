// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// BookmarkCodec is responsible for encoding and decoding the BookmarkModel
// into JSON values. The encoded values are written to disk via the
// BookmarkService.

#ifndef CHROME_BROWSER_BOOKMARKS_BOOKMARK_CODEC_H_
#define CHROME_BROWSER_BOOKMARKS_BOOKMARK_CODEC_H_
#pragma once

#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/md5.h"
#include "base/string16.h"

class BookmarkModel;
class BookmarkNode;
class DictionaryValue;
class ListValue;
class Value;

// BookmarkCodec is responsible for encoding/decoding bookmarks into JSON
// values. BookmarkCodec is used by BookmarkService.

class BookmarkCodec {
 public:
  // Creates an instance of the codec. During decoding, if the IDs in the file
  // are not unique, we will reassign IDs to make them unique. There are no
  // guarantees on how the IDs are reassigned or about doing minimal
  // reassignments to achieve uniqueness.
  BookmarkCodec();
  ~BookmarkCodec();

  // Encodes the model to a JSON value. It's up to the caller to delete the
  // returned object. This is invoked to encode the contents of the bookmark bar
  // model and is currently a convenience to invoking Encode that takes the
  // bookmark bar node and other folder node.
  Value* Encode(BookmarkModel* model);

  // Encodes the bookmark bar and other folders returning the JSON value. It's
  // up to the caller to delete the returned object.
  // This method is public for use by StarredURLDatabase in migrating the
  // bookmarks out of the database.
  Value* Encode(const BookmarkNode* bookmark_bar_node,
                const BookmarkNode* other_folder_node);

  // Decodes the previously encoded value to the specified nodes as well as
  // setting |max_node_id| to the greatest node id. Returns true on success,
  // false otherwise. If there is an error (such as unexpected version) all
  // children are removed from the bookmark bar and other folder nodes. On exit
  // |max_node_id| is set to the max id of the nodes.
  bool Decode(BookmarkNode* bb_node,
              BookmarkNode* other_folder_node,
              int64* max_node_id,
              const Value& value);

  // Returns the checksum computed during last encoding/decoding call.
  const std::string& computed_checksum() const { return computed_checksum_; }

  // Returns the checksum that's stored in the file. After a call to Encode,
  // the computed and stored checksums are the same since the computed checksum
  // is stored to the file. After a call to decode, the computed checksum can
  // differ from the stored checksum if the file contents were changed by the
  // user.
  const std::string& stored_checksum() const { return stored_checksum_; }

  // Returns whether the IDs were reassigned during decoding. Always returns
  // false after encoding.
  bool ids_reassigned() const { return ids_reassigned_; }

  // Names of the various keys written to the Value.
  static const char* kRootsKey;
  static const char* kRootFolderNameKey;
  static const char* kOtherBookmarkFolderNameKey;
  static const char* kVersionKey;
  static const char* kChecksumKey;
  static const char* kIdKey;
  static const char* kTypeKey;
  static const char* kNameKey;
  static const char* kDateAddedKey;
  static const char* kURLKey;
  static const char* kDateModifiedKey;
  static const char* kChildrenKey;

  // Possible values for kTypeKey.
  static const char* kTypeURL;
  static const char* kTypeFolder;

 private:
  // Encodes node and all its children into a Value object and returns it.
  // The caller takes ownership of the returned object.
  Value* EncodeNode(const BookmarkNode* node);

  // Helper to perform decoding.
  bool DecodeHelper(BookmarkNode* bb_node,
                    BookmarkNode* other_folder_node,
                    const Value& value);

  // Decodes the children of the specified node. Returns true on success.
  bool DecodeChildren(const ListValue& child_value_list,
                      BookmarkNode* parent);

  // Reassigns bookmark IDs for all nodes.
  void ReassignIDs(BookmarkNode* bb_node, BookmarkNode* other_node);

  // Helper to recursively reassign IDs.
  void ReassignIDsHelper(BookmarkNode* node);

  // Decodes the supplied node from the supplied value. Child nodes are
  // created appropriately by way of DecodeChildren. If node is NULL a new
  // node is created and added to parent (parent must then be non-NULL),
  // otherwise node is used.
  bool DecodeNode(const DictionaryValue& value,
                  BookmarkNode* parent,
                  BookmarkNode* node);

  // Updates the check-sum with the given string.
  void UpdateChecksum(const std::string& str);
  void UpdateChecksum(const string16& str);

  // Updates the check-sum with the given contents of URL/folder bookmark node.
  // NOTE: These functions take in individual properties of a bookmark node
  // instead of taking in a BookmarkNode for efficiency so that we don't convert
  // various data-types to UTF16 strings multiple times - once for serializing
  // and once for computing the check-sum.
  // The url parameter should be a valid UTF8 string.
  void UpdateChecksumWithUrlNode(const std::string& id,
                                 const string16& title,
                                 const std::string& url);
  void UpdateChecksumWithFolderNode(const std::string& id,
                                    const string16& title);

  // Initializes/Finalizes the checksum.
  void InitializeChecksum();
  void FinalizeChecksum();

  // Whether or not IDs were reassigned by the codec.
  bool ids_reassigned_;

  // Whether or not IDs are valid. This is initially true, but set to false
  // if an id is missing or not unique.
  bool ids_valid_;

  // Contains the id of each of the nodes found in the file. Used to determine
  // if we have duplicates.
  std::set<int64> ids_;

  // MD5 context used to compute MD5 hash of all bookmark data.
  MD5Context md5_context_;

  // Checksums.
  std::string computed_checksum_;
  std::string stored_checksum_;

  // Maximum ID assigned when decoding data.
  int64 maximum_id_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkCodec);
};

#endif  // CHROME_BROWSER_BOOKMARKS_BOOKMARK_CODEC_H_
