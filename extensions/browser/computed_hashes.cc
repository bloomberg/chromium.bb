// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/computed_hashes.h"

#include "base/base64.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"

namespace {
const char kPathKey[] = "path";
const char kBlockSizeKey[] = "block_size";
const char kBlockHashesKey[] = "block_hashes";
}

namespace extensions {

ComputedHashes::Reader::Reader() {
}
ComputedHashes::Reader::~Reader() {
}

bool ComputedHashes::Reader::InitFromFile(const base::FilePath& path) {
  std::string contents;
  if (!base::ReadFileToString(path, &contents))
    return false;

  base::ListValue* all_hashes = NULL;
  scoped_ptr<base::Value> value(base::JSONReader::Read(contents));
  if (!value.get() || !value->GetAsList(&all_hashes))
    return false;

  for (size_t i = 0; i < all_hashes->GetSize(); i++) {
    base::DictionaryValue* dictionary = NULL;
    if (!all_hashes->GetDictionary(i, &dictionary))
      return false;

    std::string relative_path_utf8;
    if (!dictionary->GetString(kPathKey, &relative_path_utf8))
      return false;

    int block_size;
    if (!dictionary->GetInteger(kBlockSizeKey, &block_size))
      return false;
    if (block_size <= 0 || ((block_size % 1024) != 0)) {
      LOG(ERROR) << "Invalid block size: " << block_size;
      block_size = 0;
      return false;
    }

    base::ListValue* hashes_list = NULL;
    if (!dictionary->GetList(kBlockHashesKey, &hashes_list))
      return false;

    base::FilePath relative_path =
        base::FilePath::FromUTF8Unsafe(relative_path_utf8);
    relative_path = relative_path.NormalizePathSeparatorsTo('/');

    data_[relative_path] = HashInfo(block_size, std::vector<std::string>());
    std::vector<std::string>* hashes = &(data_[relative_path].second);

    for (size_t j = 0; j < hashes_list->GetSize(); j++) {
      std::string encoded;
      if (!hashes_list->GetString(j, &encoded))
        return false;

      hashes->push_back(std::string());
      std::string* decoded = &hashes->back();
      if (!base::Base64Decode(encoded, decoded)) {
        hashes->clear();
        return false;
      }
    }
  }
  return true;
}

bool ComputedHashes::Reader::GetHashes(const base::FilePath& relative_path,
                                       int* block_size,
                                       std::vector<std::string>* hashes) {
  base::FilePath path = relative_path.NormalizePathSeparatorsTo('/');
  std::map<base::FilePath, HashInfo>::iterator i = data_.find(path);
  if (i == data_.end())
    return false;
  HashInfo& info = i->second;
  *block_size = info.first;
  *hashes = info.second;
  return true;
}

ComputedHashes::Writer::Writer() {
}
ComputedHashes::Writer::~Writer() {
}

void ComputedHashes::Writer::AddHashes(const base::FilePath& relative_path,
                                       int block_size,
                                       const std::vector<std::string>& hashes) {
  base::DictionaryValue* dict = new base::DictionaryValue();
  base::ListValue* block_hashes = new base::ListValue();
  file_list_.Append(dict);
  dict->SetString(kPathKey,
                  relative_path.NormalizePathSeparatorsTo('/').AsUTF8Unsafe());
  dict->SetInteger(kBlockSizeKey, block_size);
  dict->Set(kBlockHashesKey, block_hashes);

  for (std::vector<std::string>::const_iterator i = hashes.begin();
       i != hashes.end();
       ++i) {
    std::string encoded;
    base::Base64Encode(*i, &encoded);
    block_hashes->AppendString(encoded);
  }
}

bool ComputedHashes::Writer::WriteToFile(const base::FilePath& path) {
  std::string json;
  if (!base::JSONWriter::Write(&file_list_, &json))
    return false;
  int written = base::WriteFile(path, json.data(), json.size());
  if (static_cast<unsigned>(written) != json.size()) {
    LOG(ERROR) << "Error writing " << path.MaybeAsASCII()
               << " ; write result:" << written << " expected:" << json.size();
    return false;
  }
  return true;
}

}  // namespace extensions
