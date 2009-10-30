// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_codec.h"

#include <algorithm>

#include "app/l10n_util.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"

using base::Time;

const wchar_t* BookmarkCodec::kRootsKey = L"roots";
const wchar_t* BookmarkCodec::kRootFolderNameKey = L"bookmark_bar";
const wchar_t* BookmarkCodec::kOtherBookmarkFolderNameKey = L"other";
const wchar_t* BookmarkCodec::kVersionKey = L"version";
const wchar_t* BookmarkCodec::kChecksumKey = L"checksum";
const wchar_t* BookmarkCodec::kIdKey = L"id";
const wchar_t* BookmarkCodec::kTypeKey = L"type";
const wchar_t* BookmarkCodec::kNameKey = L"name";
const wchar_t* BookmarkCodec::kDateAddedKey = L"date_added";
const wchar_t* BookmarkCodec::kURLKey = L"url";
const wchar_t* BookmarkCodec::kDateModifiedKey = L"date_modified";
const wchar_t* BookmarkCodec::kChildrenKey = L"children";
const wchar_t* BookmarkCodec::kTypeURL = L"url";
const wchar_t* BookmarkCodec::kTypeFolder = L"folder";

// Current version of the file.
static const int kCurrentVersion = 1;

BookmarkCodec::BookmarkCodec()
    : ids_reassigned_(false),
      ids_valid_(true),
      maximum_id_(0) {
}

Value* BookmarkCodec::Encode(BookmarkModel* model) {
  return Encode(model->GetBookmarkBarNode(), model->other_node());
}

Value* BookmarkCodec::Encode(const BookmarkNode* bookmark_bar_node,
                             const BookmarkNode* other_folder_node) {
  ids_reassigned_ = false;
  InitializeChecksum();
  DictionaryValue* roots = new DictionaryValue();
  roots->Set(kRootFolderNameKey, EncodeNode(bookmark_bar_node));
  roots->Set(kOtherBookmarkFolderNameKey, EncodeNode(other_folder_node));

  DictionaryValue* main = new DictionaryValue();
  main->SetInteger(kVersionKey, kCurrentVersion);
  FinalizeChecksum();
  // We are going to store the computed checksum. So set stored checksum to be
  // the same as computed checksum.
  stored_checksum_ = computed_checksum_;
  main->Set(kChecksumKey, Value::CreateStringValue(computed_checksum_));
  main->Set(kRootsKey, roots);
  return main;
}

bool BookmarkCodec::Decode(BookmarkNode* bb_node,
                           BookmarkNode* other_folder_node,
                           int64* max_id,
                           const Value& value) {
  ids_.clear();
  ids_reassigned_ = false;
  ids_valid_ = true;
  maximum_id_ = 0;
  stored_checksum_.clear();
  InitializeChecksum();
  bool success = DecodeHelper(bb_node, other_folder_node, value);
  FinalizeChecksum();
  // If either the checksums differ or some IDs were missing/not unique,
  // reassign IDs.
  if (!ids_valid_ || computed_checksum() != stored_checksum())
    ReassignIDs(bb_node, other_folder_node);
  *max_id = maximum_id_ + 1;
  return success;
}

Value* BookmarkCodec::EncodeNode(const BookmarkNode* node) {
  DictionaryValue* value = new DictionaryValue();
  std::string id;
  id = Int64ToString(node->id());
  value->SetString(kIdKey, id);
  const std::wstring& title = node->GetTitle();
  value->SetString(kNameKey, title);
  value->SetString(kDateAddedKey,
                   Int64ToWString(node->date_added().ToInternalValue()));
  if (node->type() == BookmarkNode::URL) {
    value->SetString(kTypeKey, kTypeURL);
    std::wstring url = UTF8ToWide(node->GetURL().possibly_invalid_spec());
    value->SetString(kURLKey, url);
    UpdateChecksumWithUrlNode(id, title, url);
  } else {
    value->SetString(kTypeKey, kTypeFolder);
    value->SetString(kDateModifiedKey,
                     Int64ToWString(node->date_group_modified().
                                    ToInternalValue()));
    UpdateChecksumWithFolderNode(id, title);

    ListValue* child_values = new ListValue();
    value->Set(kChildrenKey, child_values);
    for (int i = 0; i < node->GetChildCount(); ++i)
      child_values->Append(EncodeNode(node->GetChild(i)));
  }
  return value;
}

bool BookmarkCodec::DecodeHelper(BookmarkNode* bb_node,
                                 BookmarkNode* other_folder_node,
                                 const Value& value) {
  if (value.GetType() != Value::TYPE_DICTIONARY)
    return false;  // Unexpected type.

  const DictionaryValue& d_value = static_cast<const DictionaryValue&>(value);

  int version;
  if (!d_value.GetInteger(kVersionKey, &version) || version != kCurrentVersion)
    return false;  // Unknown version.

  Value* checksum_value;
  if (d_value.Get(kChecksumKey, &checksum_value)) {
    if (checksum_value->GetType() != Value::TYPE_STRING)
      return false;
    StringValue* checksum_value_str = static_cast<StringValue*>(checksum_value);
    if (!checksum_value_str->GetAsString(&stored_checksum_))
      return false;
  }

  Value* roots;
  if (!d_value.Get(kRootsKey, &roots))
    return false;  // No roots.

  if (roots->GetType() != Value::TYPE_DICTIONARY)
    return false;  // Invalid type for roots.

  DictionaryValue* roots_d_value = static_cast<DictionaryValue*>(roots);
  Value* root_folder_value;
  Value* other_folder_value;
  if (!roots_d_value->Get(kRootFolderNameKey, &root_folder_value) ||
      root_folder_value->GetType() != Value::TYPE_DICTIONARY ||
      !roots_d_value->Get(kOtherBookmarkFolderNameKey, &other_folder_value) ||
      other_folder_value->GetType() != Value::TYPE_DICTIONARY)
    return false;  // Invalid type for root folder and/or other folder.

  DecodeNode(*static_cast<DictionaryValue*>(root_folder_value), NULL,
             bb_node);
  DecodeNode(*static_cast<DictionaryValue*>(other_folder_value), NULL,
             other_folder_node);
  // Need to reset the type as decoding resets the type to FOLDER. Similarly
  // we need to reset the title as the title is persisted and restored from
  // the file.
  bb_node->set_type(BookmarkNode::BOOKMARK_BAR);
  other_folder_node->set_type(BookmarkNode::OTHER_NODE);
  bb_node->SetTitle(l10n_util::GetString(IDS_BOOMARK_BAR_FOLDER_NAME));
  other_folder_node->SetTitle(
      l10n_util::GetString(IDS_BOOMARK_BAR_OTHER_FOLDER_NAME));

  return true;
}

bool BookmarkCodec::DecodeChildren(const ListValue& child_value_list,
                                   BookmarkNode* parent) {
  for (size_t i = 0; i < child_value_list.GetSize(); ++i) {
    Value* child_value;
    if (!child_value_list.Get(i, &child_value))
      return false;

    if (child_value->GetType() != Value::TYPE_DICTIONARY)
      return false;

    DecodeNode(*static_cast<DictionaryValue*>(child_value), parent, NULL);
  }
  return true;
}

bool BookmarkCodec::DecodeNode(const DictionaryValue& value,
                               BookmarkNode* parent,
                               BookmarkNode* node) {
  // If no |node| is specified, we'll create one and add it to the |parent|.
  // Therefore, in that case, |parent| must be non-NULL.
  if (!node && !parent) {
    NOTREACHED();
    return false;
  }

  std::string id_string;
  int64 id = 0;
  if (ids_valid_) {
    if (!value.GetString(kIdKey, &id_string) ||
        !StringToInt64(id_string, &id) ||
        ids_.count(id) != 0) {
      ids_valid_ = false;
    } else {
      ids_.insert(id);
    }
  }

  maximum_id_ = std::max(maximum_id_, id);

  std::wstring title;
  value.GetString(kNameKey, &title);

  std::wstring date_added_string;
  if (!value.GetString(kDateAddedKey, &date_added_string))
    date_added_string = Int64ToWString(Time::Now().ToInternalValue());
  base::Time date_added = base::Time::FromInternalValue(
      StringToInt64(WideToUTF16Hack(date_added_string)));
#if !defined(OS_WIN)
  // We changed the epoch for dates on Mac & Linux from 1970 to the Windows
  // one of 1601. We assume any number we encounter from before 1970 is using
  // the old format, so we need to add the delta to it.
  //
  // This code should be removed at some point:
  // http://code.google.com/p/chromium/issues/detail?id=20264
  if (date_added.ToInternalValue() <
      base::Time::kWindowsEpochDeltaMicroseconds) {
    date_added = base::Time::FromInternalValue(date_added.ToInternalValue() +
        base::Time::kWindowsEpochDeltaMicroseconds);
  }
#endif

  std::wstring type_string;
  if (!value.GetString(kTypeKey, &type_string))
    return false;

  if (type_string != kTypeURL && type_string != kTypeFolder)
    return false;  // Unknown type.

  if (type_string == kTypeURL) {
    std::wstring url_string;
    if (!value.GetString(kURLKey, &url_string))
      return false;

    GURL url = GURL(WideToUTF8(url_string));
    if (!node && url.is_valid())
      node = new BookmarkNode(id, url);
    else
      return false;  // Node invalid.

    if (parent)
      parent->Add(parent->GetChildCount(), node);
    node->set_type(BookmarkNode::URL);
    UpdateChecksumWithUrlNode(id_string, title, url_string);
  } else {
    std::wstring last_modified_date;
    if (!value.GetString(kDateModifiedKey, &last_modified_date))
      last_modified_date = Int64ToWString(Time::Now().ToInternalValue());

    Value* child_values;
    if (!value.Get(kChildrenKey, &child_values))
      return false;

    if (child_values->GetType() != Value::TYPE_LIST)
      return false;

    if (!node) {
      node = new BookmarkNode(id, GURL());
    } else {
      // If a new node is not created, explicitly assign ID to the existing one.
      node->set_id(id);
    }

    node->set_type(BookmarkNode::FOLDER);
    node->set_date_group_modified(Time::FromInternalValue(
        StringToInt64(WideToUTF16Hack(last_modified_date))));

    if (parent)
      parent->Add(parent->GetChildCount(), node);

    UpdateChecksumWithFolderNode(id_string, title);

    if (!DecodeChildren(*static_cast<ListValue*>(child_values), node))
      return false;
  }

  node->SetTitle(title);
  node->set_date_added(date_added);
  return true;
}

void BookmarkCodec::ReassignIDs(BookmarkNode* bb_node,
                                BookmarkNode* other_node) {
  maximum_id_ = 0;
  ReassignIDsHelper(bb_node);
  ReassignIDsHelper(other_node);
  ids_reassigned_ = true;
}

void BookmarkCodec::ReassignIDsHelper(BookmarkNode* node) {
  DCHECK(node);
  node->set_id(++maximum_id_);
  for (int i = 0; i < node->GetChildCount(); ++i)
    ReassignIDsHelper(node->GetChild(i));
}

void BookmarkCodec::UpdateChecksum(const std::string& str) {
  MD5Update(&md5_context_, str.data(), str.length() * sizeof(char));
}

void BookmarkCodec::UpdateChecksum(const std::wstring& str) {
  MD5Update(&md5_context_, str.data(), str.length() * sizeof(wchar_t));
}

void BookmarkCodec::UpdateChecksumWithUrlNode(const std::string& id,
                                              const std::wstring& title,
                                              const std::wstring& url) {
  UpdateChecksum(id);
  UpdateChecksum(title);
  UpdateChecksum(kTypeURL);
  UpdateChecksum(url);
}

void BookmarkCodec::UpdateChecksumWithFolderNode(const std::string& id,
                                                 const std::wstring& title) {
  UpdateChecksum(id);
  UpdateChecksum(title);
  UpdateChecksum(kTypeFolder);
}

void BookmarkCodec::InitializeChecksum() {
  MD5Init(&md5_context_);
}

void BookmarkCodec::FinalizeChecksum() {
  MD5Digest digest;
  MD5Final(&digest, &md5_context_);
  computed_checksum_ = MD5DigestToBase16(digest);
}
