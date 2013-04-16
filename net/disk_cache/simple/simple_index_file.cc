// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/simple/simple_index_file.h"

#include "base/file_util.h"
#include "base/hash.h"
#include "base/logging.h"
#include "base/pickle.h"
#include "net/disk_cache/simple/simple_entry_format.h"
#include "net/disk_cache/simple/simple_index.h"
#include "net/disk_cache/simple/simple_util.h"
#include "third_party/zlib/zlib.h"


namespace {

const uint64 kMaxEntiresInIndex = 100000000;

uint32 CalculatePickleCRC(const Pickle& pickle) {
  return crc32(crc32(0, Z_NULL, 0),
               reinterpret_cast<const Bytef*>(pickle.payload()),
               pickle.payload_size());
}

}  // namespace

namespace disk_cache {

SimpleIndexFile::IndexMetadata::IndexMetadata() :
    magic_number_(kSimpleIndexMagicNumber),
    version_(kSimpleVersion),
    number_of_entries_(0),
    cache_size_(0) {}

SimpleIndexFile::IndexMetadata::IndexMetadata(
    uint64 number_of_entries, uint64 cache_size) :
    magic_number_(kSimpleIndexMagicNumber),
    version_(kSimpleVersion),
    number_of_entries_(number_of_entries),
    cache_size_(cache_size) {}

void SimpleIndexFile::IndexMetadata::Serialize(Pickle* pickle) const {
  DCHECK(pickle);
  pickle->WriteUInt64(magic_number_);
  pickle->WriteUInt32(version_);
  pickle->WriteUInt64(number_of_entries_);
  pickle->WriteUInt64(cache_size_);
}

bool SimpleIndexFile::IndexMetadata::Deserialize(PickleIterator* it) {
  DCHECK(it);
  return it->ReadUInt64(&magic_number_) &&
      it->ReadUInt32(&version_) &&
      it->ReadUInt64(&number_of_entries_)&&
      it->ReadUInt64(&cache_size_);
}

bool SimpleIndexFile::IndexMetadata::CheckIndexMetadata() {
  return number_of_entries_ <= kMaxEntiresInIndex &&
      magic_number_ == disk_cache::kSimpleIndexMagicNumber &&
      version_ == disk_cache::kSimpleVersion;
}

// static
scoped_ptr<SimpleIndex::EntrySet> SimpleIndexFile::LoadFromDisk(
    const base::FilePath& index_filename) {
  std::string contents;
  if(!file_util::ReadFileToString(index_filename, &contents)) {
    LOG(WARNING) << "Could not read Simple Index file.";
    return scoped_ptr<SimpleIndex::EntrySet>(NULL);
  }

  return SimpleIndexFile::Deserialize(contents.data(), contents.size());
}

// static
scoped_ptr<SimpleIndex::EntrySet> SimpleIndexFile::Deserialize(const char* data,
                                                               int data_len) {
  DCHECK(data);
  Pickle pickle(data, data_len);
  if (!pickle.data()) {
    LOG(WARNING) << "Corrupt Simple Index File.";
    return scoped_ptr<SimpleIndex::EntrySet>(NULL);
  }

  PickleIterator pickle_it(pickle);

  SimpleIndexFile::PickleHeader* header_p =
      pickle.headerT<SimpleIndexFile::PickleHeader>();
  const uint32 crc_read = header_p->crc;
  const uint32 crc_calculated = CalculatePickleCRC(pickle);

  if (crc_read != crc_calculated) {
    LOG(WARNING) << "Invalid CRC in Simple Index file.";
    return scoped_ptr<SimpleIndex::EntrySet>(NULL);
  }

  SimpleIndexFile::IndexMetadata index_metadata;
  if (!index_metadata.Deserialize(&pickle_it)) {
    LOG(ERROR) << "Invalid index_metadata on Simple Cache Index.";
    return scoped_ptr<SimpleIndex::EntrySet>(NULL);
  }

  if (!index_metadata.CheckIndexMetadata()) {
    LOG(ERROR) << "Invalid index_metadata on Simple Cache Index.";
    return scoped_ptr<SimpleIndex::EntrySet>(NULL);
  }

  scoped_ptr<SimpleIndex::EntrySet> index_file_entries(
      new SimpleIndex::EntrySet());
  while (index_file_entries->size() < index_metadata.GetNumberOfEntries()) {
    EntryMetadata entry_metadata;
    if (!entry_metadata.Deserialize(&pickle_it)) {
      LOG(WARNING) << "Invalid EntryMetadata in Simple Index file.";
      return scoped_ptr<SimpleIndex::EntrySet>(NULL);
    }
    SimpleIndex::InsertInEntrySet(entry_metadata, index_file_entries.get());
  }

  return index_file_entries.Pass();
}

// static
scoped_ptr<Pickle> SimpleIndexFile::Serialize(
    const SimpleIndexFile::IndexMetadata& index_metadata,
    const SimpleIndex::EntrySet& entries) {
  scoped_ptr<Pickle> pickle(new Pickle(sizeof(SimpleIndexFile::PickleHeader)));

  index_metadata.Serialize(pickle.get());
  for (SimpleIndex::EntrySet::const_iterator it = entries.begin();
       it != entries.end(); ++it) {
    it->second.Serialize(pickle.get());
  }
  SimpleIndexFile::PickleHeader* header_p =
      pickle->headerT<SimpleIndexFile::PickleHeader>();
  header_p->crc = CalculatePickleCRC(*pickle);
  return pickle.Pass();
}

// static
void SimpleIndexFile::WriteToDisk(const base::FilePath& index_filename,
                                  const Pickle& pickle) {
  const base::FilePath temp_filename =
      index_filename.DirName().AppendASCII("index_temp");
  int bytes_written = file_util::WriteFile(
      temp_filename,
      reinterpret_cast<const char*>(pickle.data()),
      pickle.size());
  DCHECK_EQ(bytes_written, implicit_cast<int>(pickle.size()));
  if (bytes_written != static_cast<int>(pickle.size())) {
    // TODO(felipeg): Add better error handling.
    LOG(ERROR) << "Could not write Simple Cache index to temporary file: "
               << temp_filename.value();
    file_util::Delete(temp_filename, /* recursive = */ false);
    return;
  }
  // Swap temp and index_file.
  bool result = file_util::ReplaceFile(temp_filename, index_filename);
  DCHECK(result);
}

}  // namespace disk_cache
