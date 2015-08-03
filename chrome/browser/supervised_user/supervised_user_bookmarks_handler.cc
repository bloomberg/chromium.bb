// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_bookmarks_handler.h"

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/url_formatter/url_fixer.h"

namespace {

// Keys of relevant managed user settings.
const char kKeyLink[] = "SupervisedBookmarkLink";
const char kKeyFolder[] = "SupervisedBookmarkFolder";

// Keys for elements of the bookmarks json tree.
const char kId[] = "id";
const char kName[] = "name";
const char kUrl[] = "url";
const char kChildren[] = "children";

bool ExtractId(const std::string& key, int* id) {
  // |key| can be either "<ID>:<Value>" (for links and for "parentID:name"
  // pairs) or just "<ID>" (for folders).
  std::string id_str = key.substr(0, key.find_first_of(':'));
  if (!base::StringToInt(id_str, id)) {
    LOG(WARNING) << "Failed to parse id from " << key;
    return false;
  }
  LOG_IF(WARNING, *id < 0) << "IDs should be >= 0, but got "
                           << *id << " from " << key;
  return true;
}

bool ExtractValue(const std::string& key, std::string* value) {
  // |key| must be "<ID>:<Value>".
  size_t pos = key.find_first_of(':');
  if (pos == std::string::npos) {
    LOG(WARNING) << "Failed to parse value from " << key;
    return false;
  }
  *value = key.substr(pos + 1);
  return true;
}

bool ExtractIdAndValue(const std::string& key, int* id, std::string* value) {
  return ExtractId(key, id) && ExtractValue(key, value);
}

// Recursively searches the tree from |root| for a folder with the given |id|.
// Returns the list of children of that folder if found, otherwise returns null.
base::ListValue* FindFolder(base::ListValue* root, int id) {
  if (id == 0)  // We're looking for the root folder. Assume this is it.
    return root;

  for (size_t i = 0; i < root->GetSize(); ++i) {
    base::DictionaryValue* item = nullptr;
    root->GetDictionary(i, &item);
    DCHECK_NE(item, (base::DictionaryValue*)nullptr);

    base::ListValue* children;
    if (!item->GetList(kChildren, &children))
      continue;  // Skip bookmarks. Only interested in folders.

    // Is this it?
    int node_id;
    if (item->GetInteger(kId, &node_id) && node_id == id)
      return children;

    // Recurse.
    base::ListValue* result = FindFolder(children, id);
    if (result)
      return result;
  }
  return nullptr;
}

}  // namespace

SupervisedUserBookmarksHandler::Folder::Folder(
    int id, const std::string& name, int parent_id)
    : id(id), name(name), parent_id(parent_id) {
}

SupervisedUserBookmarksHandler::Link::Link(
    const std::string& url, const std::string& name, int parent_id)
    : url(url), name(name), parent_id(parent_id) {
}

SupervisedUserBookmarksHandler::SupervisedUserBookmarksHandler() {
}

SupervisedUserBookmarksHandler::~SupervisedUserBookmarksHandler() {
}

scoped_ptr<base::ListValue> SupervisedUserBookmarksHandler::BuildBookmarksTree(
    const base::DictionaryValue& settings) {
  SupervisedUserBookmarksHandler handler;
  handler.ParseSettings(settings);
  return handler.BuildTree();
}

void SupervisedUserBookmarksHandler::ParseSettings(
    const base::DictionaryValue& settings) {
  const base::DictionaryValue* folders;
  if (settings.GetDictionary(kKeyFolder, &folders))
    ParseFolders(*folders);

  const base::DictionaryValue* links;
  if (settings.GetDictionary(kKeyLink, &links))
    ParseLinks(*links);
}

void SupervisedUserBookmarksHandler::ParseFolders(
    const base::DictionaryValue& folders) {
  for (base::DictionaryValue::Iterator it(folders); !it.IsAtEnd();
       it.Advance()) {
    int id;
    if (!ExtractId(it.key(), &id))
      continue;
    std::string value;
    it.value().GetAsString(&value);
    std::string name;
    int parent_id;
    if (!ExtractIdAndValue(value, &parent_id, &name))
      continue;
    folders_.push_back(Folder(id, name, parent_id));
  }
}

void SupervisedUserBookmarksHandler::ParseLinks(
    const base::DictionaryValue& links) {
  for (base::DictionaryValue::Iterator it(links); !it.IsAtEnd(); it.Advance()) {
    std::string url;
    if (!ExtractValue(it.key(), &url))
      continue;
    std::string value;
    it.value().GetAsString(&value);
    std::string name;
    int parent_id;
    if (!ExtractIdAndValue(value, &parent_id, &name))
      continue;
    links_.push_back(Link(url, name, parent_id));
  }
}

scoped_ptr<base::ListValue> SupervisedUserBookmarksHandler::BuildTree() {
  root_.reset(new base::ListValue);
  AddFoldersToTree();
  AddLinksToTree();
  return root_.Pass();
}

void SupervisedUserBookmarksHandler::AddFoldersToTree() {
  // Go over all folders and try inserting them into the correct position in the
  // tree. This requires the respective parent folder to be there already. Since
  // the parent might appear later in |folders_|, we might need multiple rounds
  // until all folders can be added successfully.
  // To avoid an infinite loop in the case of a non-existing parent, we take
  // care to stop when no folders could be added in a round.
  std::vector<Folder> folders = folders_;
  std::vector<Folder> folders_failed;
  while (!folders.empty() && folders.size() != folders_failed.size()) {
    folders_failed.clear();
    for (const auto& folder : folders) {
      scoped_ptr<base::DictionaryValue> node(new base::DictionaryValue);
      node->SetIntegerWithoutPathExpansion(kId, folder.id);
      node->SetStringWithoutPathExpansion(kName, folder.name);
      node->SetWithoutPathExpansion(kChildren, new base::ListValue);
      if (!AddNodeToTree(folder.parent_id, node.Pass()))
        folders_failed.push_back(folder);
    }
    folders.swap(folders_failed);
  }
  if (!folders_failed.empty()) {
    LOG(WARNING) << "SupervisedUserBookmarksHandler::AddFoldersToTree"
                 << " failed adding the following folders (id,name,parent):";
    for (const Folder& folder : folders_failed) {
      LOG(WARNING) << folder.id << ", " << folder.name << ", "
                   << folder.parent_id;
    }
  }
}

void SupervisedUserBookmarksHandler::AddLinksToTree() {
  for (const auto& link : links_) {
    scoped_ptr<base::DictionaryValue> node(new base::DictionaryValue);
    GURL url = url_formatter::FixupURL(link.url, std::string());
    if (!url.is_valid()) {
      LOG(WARNING) << "Got invalid URL: " << link.url;
      continue;
    }
    node->SetStringWithoutPathExpansion(kUrl, url.spec());
    node->SetStringWithoutPathExpansion(kName, link.name);
    if (!AddNodeToTree(link.parent_id, node.Pass())) {
      LOG(WARNING) << "SupervisedUserBookmarksHandler::AddLinksToTree"
                   << " failed to add link (url,name,parent): "
                   << link.url << ", " << link.name << ", " << link.parent_id;
    }
  }
}

bool SupervisedUserBookmarksHandler::AddNodeToTree(
    int parent_id,
    scoped_ptr<base::DictionaryValue> node) {
  base::ListValue* parent = FindFolder(root_.get(), parent_id);
  if (!parent)
    return false;
  parent->Append(node.release());
  return true;
}
